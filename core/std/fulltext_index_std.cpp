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
    for (const auto& kv : tf) postings_[kv.first].emplace_back(docId, kv.second);
    return docId;
}

void FullTextIndexStd::finalize() {
    idf_.clear();
    const double N = (double)doc_tf_.size();
    if (N <= 0.0) return;
    idf_.reserve(postings_.size());
    for (const auto& kv : postings_) {
        const double df = (double)kv.second.size();
        // Smooth IDF; add 1 to avoid div by zero; +1 to keep positive
        double val = std::log((N + 1.0) / (df + 1.0)) + 1.0;
        idf_.emplace(kv.first, val);
    }
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
            // Expand: scan postings keys for tokens containing the query token
            for (const auto& kv : postings_) {
                if (kv.first.find(tok) != std::string::npos) terms.push_back(kv.first);
            }
        }
        for (const auto& term : terms) {
            if (!used_terms.insert(term).second) continue; // avoid double-count when multiple query tokens share expansions
            auto pit = postings_.find(term);
            if (pit == postings_.end()) continue;
            double idf = 1.0;
            auto ii = idf_.find(term);
            if (ii != idf_.end()) idf = ii->second;
            for (auto& p : pit->second) {
                // raw tf * idf scoring
                score[p.first] += (double)p.second * idf;
            }
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
        std::vector<std::pair<int,int>> postings = kv.second;
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
        auto& vec = postings_[term]; vec.reserve(n);
        if (v3) {
            uint32_t blen = 0; if (!read_u32(in, blen)) { last_error_ = "truncated (compressed len)"; return false; }
            std::string buf; buf.resize(blen);
            if (blen && !in.read(buf.data(), (std::streamsize)blen)) { last_error_ = "truncated (compressed data)"; return false; }
            const unsigned char* p = (const unsigned char*)buf.data();
            const unsigned char* end = p + buf.size();
            uint32_t prev = 0;
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t delta=0, tf=0;
                if (!vdecode_u32(p, end, delta) || !vdecode_u32(p, end, tf)) { last_error_ = "decode(varint)"; return false; }
                uint32_t docId = (i == 0) ? delta : (prev + delta);
                prev = docId;
                vec.emplace_back((int)docId, (int)tf);
                if (docId < doc_tf_.size()) doc_tf_[docId][term] = (int)tf;
            }
        } else {
            for (uint32_t i = 0; i < n; ++i) {
                uint32_t docId=0, tf=0; if (!read_u32(in, docId) || !read_u32(in, tf)) { last_error_ = "truncated (posting)"; return false; }
                vec.emplace_back((int)docId, (int)tf);
                // Also rebuild doc_tf_ per doc
                if (docId < doc_tf_.size()) doc_tf_[docId][term] = (int)tf;
            }
        }
    }
    finalize();
    return true;
}

} // namespace UnidictCoreStd
