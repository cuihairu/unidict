#include "fulltext_index_std.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <unordered_set>

namespace UnidictCoreStd {

static inline std::string lcase(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

FullTextIndexStd::FullTextIndexStd() = default;

inline bool FullTextIndexStd::is_word_char(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-';
}

std::vector<std::string> FullTextIndexStd::tokenize(const std::string& s) {
    std::vector<std::string> out;
    std::string cur; cur.reserve(16);
    for (unsigned char c : s) {
        if (is_word_char(c)) {
            cur.push_back((char)std::tolower(c));
        } else if (!cur.empty()) {
            out.push_back(cur); cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int FullTextIndexStd::add_document(const std::string& text, DocRef ref) {
    const int docId = (int)doc_tf_.size();
    doc_tf_.emplace_back();
    doc_map_.push_back(ref);
    auto& tf = doc_tf_.back();
    for (auto& tok : tokenize(text)) ++tf[tok];
    for (const auto& kv : tf) postings_[kv.first].vec.emplace_back(docId, kv.second);
    return docId;
}

void FullTextIndexStd::finalize() {
    idf_.clear();
    const double N = (double)doc_map_.size();
    if (N <= 0.0) return;
    idf_.reserve(postings_.size());
    for (auto& kv : postings_) {
        PostingEntry& pe = kv.second;
        if (!pe.compressed) pe.count = (uint32_t)pe.vec.size();
        double df = pe.compressed ? (double)pe.count : (double)pe.vec.size();
        double val = std::log((N + 1.0) / (df + 1.0)) + 1.0;
        idf_.emplace(kv.first, val);
    }
    build_term_directory();
}

std::vector<FullTextIndexStd::DocRef> FullTextIndexStd::search(const std::string& query, int max_results) const {
    std::vector<DocRef> out;
    if (query.empty() || doc_tf_.empty()) return out;
    std::unordered_map<int, double> score; // docId -> score
    std::unordered_set<std::string> seen_query_terms;
    std::unordered_set<std::string> used_terms;
    for (auto& tok : tokenize(query)) {
        if (!seen_query_terms.insert(tok).second) continue; // de-dup query term
        // Collect exact token and, if missing, substring matches to approximate substring search
        std::vector<std::string> terms; terms.push_back(tok);
        if (postings_.find(tok) == postings_.end()) {
            const size_t kCap = 256;
            auto cand = substring_candidates(tok, kCap);
            terms.insert(terms.end(), cand.begin(), cand.end());
        }
        for (const auto& term : terms) {
            if (!used_terms.insert(term).second) continue; // avoid double-count when multiple query tokens share expansions
            auto pit = postings_.find(term);
            if (pit == postings_.end()) continue;
            double idf = 1.0;
            auto ii = idf_.find(term);
            if (ii != idf_.end()) idf = ii->second;
            const auto& pl = ensure_postings(term);
            for (auto& p : pl) { score[p.first] += (double)p.second * idf; }
        }
    }
    if (score.empty()) return out;
    std::vector<std::pair<int,double>> ranked; ranked.reserve(score.size());
    for (auto& kv : score) ranked.emplace_back(kv.first, kv.second);
    std::sort(ranked.begin(), ranked.end(), [](const auto& a, const auto& b){
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first; // tie-breaker: smaller docId first
    });
    const int n = (int)std::min<size_t>(ranked.size(), (size_t)std::max(0, max_results));
    out.reserve(n);
    for (int i = 0; i < n; ++i) out.push_back(doc_map_[ranked[i].first]);
    return out;
}

int FullTextIndexStd::doc_count() const { return (int)doc_tf_.size(); }

void FullTextIndexStd::clear() { doc_tf_.clear(); doc_map_.clear(); postings_.clear(); idf_.clear(); }

static inline void write_u32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v & 0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>24)&0xFF) };
    out.write((const char*)b, 4);
}
static inline bool read_u32(std::ifstream& in, uint32_t& v) {
    unsigned char b[4]; if (!in.read((char*)b, 4)) return false; v = (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24); return true;
}

// Simple varint (LEB128-like) encode/decode for 32-bit unsigned integers
static inline void vencode_u32(uint32_t v, std::string& out) {
    while (v >= 0x80) { out.push_back((char)((v & 0x7F) | 0x80)); v >>= 7; }
    out.push_back((char)(v & 0x7F));
}

static inline bool vdecode_u32(const unsigned char*& p, const unsigned char* end, uint32_t& v) {
    uint32_t result = 0; int shift = 0; const int max_shift = 35; // up to 5 bytes
    while (p < end && shift <= max_shift) {
        unsigned char b = *p++;
        result |= (uint32_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) { v = result; return true; }
        shift += 7;
    }
    return false;
}

bool FullTextIndexStd::save(const std::string& path) const {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    // UDFT3: compressed postings with varint + docId delta
    const char magic[5] = {'U','D','F','T','3'};
    out.write(magic, 5);
    // signature block
    write_u32(out, (uint32_t)signature_.size());
    if (!signature_.empty()) out.write(signature_.data(), (std::streamsize)signature_.size());
    write_u32(out, (uint32_t)doc_tf_.size());
    // Doc map
    for (const auto& r : doc_map_) {
        write_u32(out, (uint32_t)r.dict);
        write_u32(out, (uint32_t)r.word);
    }
    // Postings count
    write_u32(out, (uint32_t)postings_.size());
    for (const auto& kv : postings_) {
        const std::string& term = kv.first;
        write_u32(out, (uint32_t)term.size());
        out.write(term.data(), (std::streamsize)term.size());
        // compress postings: sort by docId, delta-encode docId and varint(docDelta, tf)
        std::vector<std::pair<int,int>> postings = kv.second.vec;
        std::sort(postings.begin(), postings.end(), [](auto& a, auto& b){ return a.first < b.first; });
        std::string buf; buf.reserve(postings.size() * 2);
        uint32_t prev = 0;
        for (size_t i = 0; i < postings.size(); ++i) {
            uint32_t did = (uint32_t)postings[i].first;
            uint32_t tf = (uint32_t)postings[i].second;
            uint32_t delta = (i == 0) ? did : (did - prev);
            vencode_u32(delta, buf);
            vencode_u32(tf, buf);
            prev = did;
        }
        // write count and compressed buffer
        write_u32(out, (uint32_t)postings.size());
        write_u32(out, (uint32_t)buf.size());
        if (!buf.empty()) out.write(buf.data(), (std::streamsize)buf.size());
    }
    return (bool)out;
}

bool FullTextIndexStd::load(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { last_error_ = "open failed"; return false; }
    char magic[5]; if (!in.read(magic, 5)) return false;
    std::string mg(magic, 5);
    bool v1 = (mg == std::string("UDFT1",5));
    bool v2 = (mg == std::string("UDFT2",5));
    bool v3 = (mg == std::string("UDFT3",5));
    if (!v1 && !v2 && !v3) { last_error_ = "unsupported format"; return false; }
    version_ = v3 ? 3 : (v2 ? 2 : 1);
    clear();
    if (v2 || v3) {
        uint32_t siglen = 0; if (!read_u32(in, siglen)) { last_error_ = "truncated (siglen)"; return false; }
        signature_.clear(); signature_.resize(siglen);
        if (siglen) { if (!in.read(signature_.data(), (std::streamsize)siglen)) { last_error_ = "truncated (sig)"; return false; } }
    } else {
        signature_.clear();
    }
    uint32_t docs = 0; if (!read_u32(in, docs)) { last_error_ = "truncated (docs)"; return false; }
    doc_tf_.resize(docs);
    doc_map_.resize(docs);
    for (uint32_t i = 0; i < docs; ++i) {
        uint32_t d, w; if (!read_u32(in, d) || !read_u32(in, w)) { last_error_ = "truncated (docmap)"; return false; }
        doc_map_[i] = {(int)d, (int)w};
    }
    uint32_t terms = 0; if (!read_u32(in, terms)) { last_error_ = "truncated (terms)"; return false; }
    for (uint32_t t = 0; t < terms; ++t) {
        uint32_t len = 0; if (!read_u32(in, len)) { last_error_ = "truncated (term len)"; return false; }
        std::string term; term.resize(len); if (!in.read(term.data(), (std::streamsize)len)) { last_error_ = "truncated (term)"; return false; }
        uint32_t n = 0; if (!read_u32(in, n)) { last_error_ = "truncated (postings count)"; return false; }
        auto& ent = postings_[term];
        if (v3) {
            uint32_t blen = 0; if (!read_u32(in, blen)) { last_error_ = "truncated (compressed len)"; return false; }
            std::string buf; buf.resize(blen);
            if (blen && !in.read(buf.data(), (std::streamsize)blen)) { last_error_ = "truncated (compressed data)"; return false; }
            ent.buf = std::move(buf); ent.compressed = true; ent.count = n; // lazy decode later
        } else {
            ent.vec.reserve(n);
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t docId=0, tf=0; if (!read_u32(in, docId) || !read_u32(in, tf)) { last_error_ = "truncated (posting)"; return false; }
                ent.vec.emplace_back((int)docId, (int)tf);
            }
        }
    }
    finalize();
    return true;
}

} // namespace UnidictCoreStd
const std::vector<std::pair<int,int>>& UnidictCoreStd::FullTextIndexStd::ensure_postings(const std::string& term) const {
    auto it = postings_.find(term);
    if (it == postings_.end()) { static const std::vector<std::pair<int,int>> empty; return empty; }
    PostingEntry& pe = const_cast<PostingEntry&>(it->second);
    if (!pe.compressed) return pe.vec;
    // Decode varint compressed buffer into vec
    pe.vec.clear(); pe.vec.reserve(pe.count);
    const unsigned char* p = (const unsigned char*)pe.buf.data();
    const unsigned char* end = p + pe.buf.size();
    uint32_t prev = 0;
    for (uint32_t i = 0; i < pe.count; ++i) {
        uint32_t delta=0, tf=0;
        if (!vdecode_u32(p, end, delta) || !vdecode_u32(p, end, tf)) { break; }
        uint32_t docId = (i == 0) ? delta : (prev + delta);
        prev = docId; pe.vec.emplace_back((int)docId, (int)tf);
    }
    pe.compressed = false; pe.buf.clear();
    return pe.vec;
}

void UnidictCoreStd::FullTextIndexStd::build_term_directory() {
    terms_sorted_.clear(); terms_sorted_.reserve(postings_.size());
    for (auto& kv : postings_) terms_sorted_.push_back({kv.first, &kv.second});
    std::sort(terms_sorted_.begin(), terms_sorted_.end(), [](const auto& a, const auto& b){ return a.first < b.first; });
    build_ngram3_index();
}

void UnidictCoreStd::FullTextIndexStd::build_ngram3_index() {
    ngram3_index_.clear();
    // Build an inverted index mapping 3-grams to term indices in terms_sorted_
    for (int i = 0; i < (int)terms_sorted_.size(); ++i) {
        const std::string& term = terms_sorted_[i].first;
        if (term.size() < 3) continue;
        // avoid duplicates per term
        std::unordered_set<std::string> seen;
        for (size_t j = 0; j + 2 < term.size(); ++j) {
            unsigned char c1 = (unsigned char)term[j];
            unsigned char c2 = (unsigned char)term[j+1];
            unsigned char c3 = (unsigned char)term[j+2];
            if (!is_word_char(c1) || !is_word_char(c2) || !is_word_char(c3)) continue;
            std::string g; g.push_back((char)std::tolower(c1)); g.push_back((char)std::tolower(c2)); g.push_back((char)std::tolower(c3));
            if (!seen.insert(g).second) continue;
            ngram3_index_[g].push_back(i);
        }
    }
}

std::vector<std::string> UnidictCoreStd::FullTextIndexStd::substring_candidates(const std::string& tok, size_t cap) const {
    std::vector<std::string> out;
    if (tok.empty()) return out;
    std::string q = lcase(tok);
    if (q.size() >= 3 && !ngram3_index_.empty()) {
        // Choose the rarest 3-gram from the query
        size_t best_sz = SIZE_MAX; const std::vector<int>* best_vec = nullptr;
        for (size_t j = 0; j + 2 < q.size(); ++j) {
            if (!is_word_char((unsigned char)q[j]) || !is_word_char((unsigned char)q[j+1]) || !is_word_char((unsigned char)q[j+2])) continue;
            std::string g = q.substr(j, 3);
            auto it = ngram3_index_.find(g);
            if (it == ngram3_index_.end()) continue;
            if (it->second.size() < best_sz) { best_sz = it->second.size(); best_vec = &it->second; }
        }
        if (best_vec) {
            for (int idx : *best_vec) {
                const std::string& term = terms_sorted_[idx].first;
                if (term.find(q) != std::string::npos) { out.push_back(term); if (out.size() >= cap) break; }
            }
            return out;
        }
    }
    // Fallback: scan sorted terms (bounded)
    size_t added = 0;
    for (const auto& pr : terms_sorted_) {
        if (pr.first.find(q) != std::string::npos) { out.push_back(pr.first); if (++added >= cap) break; }
    }
    return out;
}
