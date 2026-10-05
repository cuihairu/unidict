#include <cstdint>
#include "fulltext_index_std.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <unordered_set>
#include <thread>
#include <future>

namespace UnidictCoreStd {

static inline std::string lcase(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

FullTextIndexStd::FullTextIndexStd() = default;

inline bool FullTextIndexStd::is_word_char(unsigned char c) {
    // 高字节（UTF-8 续字节/首字节）算词字符：Latin 变音整词保留
    // （"café" 不再被 é 一刀两断），CJK 词项的字节 ngram 归档不断流。
    // CJK 连续段的切分不归它管——tokenize 按码点切，这里只是字节级的
    // "成词/分隔"二分。
    return std::isalnum(c) || c == '_' || c == '-' || c >= 0x80;
}

namespace {

constexpr uint32_t kInvalidUtf8 = 0xFFFFFFFFu;

// UTF-8 解码一步。*len 恒置为本次消费字节数；合法序列返回码点，
// 非法（截断/超长编码/代理区/越界/续字节落单）返回 kInvalidUtf8 且
// *len = 1——调用方按单字节分隔符推进，不会在坏字节上死循环。
inline uint32_t utf8_next(const char* s, size_t n, size_t i, size_t* len) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { *len = 1; return c; }
    size_t need = 0;
    uint32_t cp = 0, lo_min = 0;
    if ((c & 0xE0) == 0xC0) { need = 2; cp = c & 0x1Fu; lo_min = 0x80u; }
    else if ((c & 0xF0) == 0xE0) { need = 3; cp = c & 0x0Fu; lo_min = 0x800u; }
    else if ((c & 0xF8) == 0xF0) { need = 4; cp = c & 0x07u; lo_min = 0x10000u; }
    else { *len = 1; return kInvalidUtf8; }
    if (i + need > n) { *len = 1; return kInvalidUtf8; }
    for (size_t k = 1; k < need; ++k) {
        const auto cc = static_cast<unsigned char>(s[i + k]);
        if ((cc & 0xC0) != 0x80) { *len = 1; return kInvalidUtf8; }
        cp = (cp << 6) | (cc & 0x3Fu);
    }
    if (cp < lo_min || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
        *len = 1; return kInvalidUtf8;
    }
    *len = need;
    return cp;
}

// CJK 连续段判定。刻意不含标点/符号（。、，U+3000–303F 等）——按
// 分隔符处理，与 ASCII 标点同口径；〇 是数字不是标点，单独放行。
inline bool is_cjk_codepoint(uint32_t cp) {
    return cp == 0x3007u
        || (cp >= 0x3040u && cp <= 0x30FFu)    // 平假名 + 片假名
        || (cp >= 0x3400u && cp <= 0x4DBFu)    // CJK 扩展 A
        || (cp >= 0x4E00u && cp <= 0x9FFFu)    // CJK 统一表意
        || (cp >= 0xF900u && cp <= 0xFAFFu)    // 兼容表意
        || (cp >= 0xAC00u && cp <= 0xD7AFu)    // 谚文音节
        || (cp >= 0x20000u && cp <= 0x2FA1Fu); // 扩展 B..
}

// 合法但必须当分隔符的 Unicode 标点：CJK 符号区（。、〈〉等）与
// 全角 ASCII 标点区（！，：；？［］｛｝｡｢｣等）。不显式拦的话，
//「你好；招呼」的全角分号会按"非 CJK 合法高字节"并进词 cur，
// 产出垃圾 token「；」。全角字母/数字不在其列——并入词（全角「ＡＢ」
// 查询自洽命中）。
inline bool is_sep_punct(uint32_t cp) {
    if (cp >= 0x3000u && cp <= 0x303Fu) return true;
    if (cp >= 0xFF01u && cp <= 0xFF65u) {
        const bool alnum = (cp >= 0xFF10u && cp <= 0xFF19u) ||  // 全角 0-9
                           (cp >= 0xFF21u && cp <= 0xFF3Au) ||  // 全角 A-Z
                           (cp >= 0xFF41u && cp <= 0xFF5Au);    // 全角 a-z
        return !alnum;
    }
    return false;
}

} // namespace

std::vector<std::string> FullTextIndexStd::tokenize(const std::string& s) {
    return tokenize_impl(s, false);
}

std::vector<std::string> FullTextIndexStd::tokenize_query(const std::string& s) {
    return tokenize_impl(s, true);
}

std::vector<std::string> FullTextIndexStd::tokenize_impl(const std::string& s, bool query_mode) {
    std::vector<std::string> out;
    std::string cur; cur.reserve(16);
    std::vector<std::string> cjk_run; cjk_run.reserve(8);
    auto flush_ascii = [&] { if (!cur.empty()) { out.push_back(cur); cur.clear(); } };
    auto flush_cjk = [&] {
        if (cjk_run.empty()) return;
        if (query_mode && cjk_run.size() >= 2) {
            // 查询侧多字串只发 bigram：发单字会把「你好」扩成
            // 「含你 ∪ 含好」的并集，层 2（释义包含）退化成单字噪声
            for (size_t i = 0; i + 1 < cjk_run.size(); ++i)
                out.push_back(cjk_run[i] + cjk_run[i + 1]);
        } else {
            // 文档侧全量：unigram 供单字查询命中、bigram 供多字查询命中；
            // 查询侧单字（run==1）只发 unigram（bigram 无从谈起）
            for (const auto& ch : cjk_run) out.push_back(ch);
            if (!query_mode)
                for (size_t i = 0; i + 1 < cjk_run.size(); ++i)
                    out.push_back(cjk_run[i] + cjk_run[i + 1]);
        }
        cjk_run.clear();
    };
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            flush_cjk();
            if (is_word_char(c)) cur.push_back((char)std::tolower(c));
            else flush_ascii();
            ++i;
            continue;
        }
        size_t len = 0;
        const uint32_t cp = utf8_next(s.data(), n, i, &len);
        if (cp == kInvalidUtf8) {
            // 非法字节：与 ASCII 分隔符同口径断词（沿用原字节级行为）
            flush_cjk(); flush_ascii();
            i += len;
            continue;
        }
        if (is_cjk_codepoint(cp)) {
            flush_ascii();
            cjk_run.push_back(s.substr(i, len));
        } else if (is_sep_punct(cp)) {
            // 全角/CJK 标点 = 分隔符（ASCII 标点同口径），不断进词
            flush_cjk(); flush_ascii();
        } else {
            // 非 CJK 合法高字节（拉丁变音/西里尔/全角字母数字等）：整段
            // 并入当前词，C 语言域下 tolower 对高字节是恒等——UTF-8
            // 序列字节保持完整
            flush_cjk();
            for (size_t k = 0; k < len; ++k)
                cur.push_back((char)std::tolower((unsigned char)s[i + k]));
        }
        i += len;
    }
    flush_cjk();
    flush_ascii();
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

void FullTextIndexStd::build_from_documents(const std::vector<std::pair<std::string, DocRef>>& docs, int threads) {
    clear();
    const size_t N = docs.size();
    doc_map_.resize(N);
    for (size_t i = 0; i < N; ++i) doc_map_[i] = docs[i].second;
    // Ensure doc_tf_ is non-empty to satisfy legacy checks; keep minimal
    doc_tf_.clear(); doc_tf_.resize(N);

    if (threads <= 0) {
        unsigned int hc = std::thread::hardware_concurrency();
        threads = (hc == 0) ? 1 : (int)hc;
    }
    // 上方两个分支已保证 threads >= 1（调用方传入的 >0，hardware_concurrency
    // 兜底臂取 1 或正核数），无需再钳一次。
    if ((size_t)threads > N) threads = (int)N;

    // Per-thread postings map: term -> vector of (docId, tf)
    std::vector<std::unordered_map<std::string, std::vector<std::pair<int,int>>>> local(threads);
    auto worker = [&](int tid) {
        size_t start = (N * tid) / threads;
        size_t end = (N * (tid + 1)) / threads;
        auto& lm = local[tid];
        for (size_t i = start; i < end; ++i) {
            const std::string& text = docs[i].first;
            std::unordered_map<std::string,int> tf;
            for (auto& tok : tokenize(text)) ++tf[tok];
            for (const auto& kv : tf) lm[kv.first].emplace_back((int)i, kv.second);
        }
    };
    std::vector<std::thread> pool; pool.reserve(threads);
    for (int t = 0; t < threads; ++t) pool.emplace_back(worker, t);
    for (auto& th : pool) th.join();

    // Merge all locals into postings_
    postings_.clear(); postings_.reserve(N * 2);
    for (int t = 0; t < threads; ++t) {
        for (auto& kv : local[t]) {
            auto& ent = postings_[kv.first];
            auto& dst = ent.vec;
            auto& src = kv.second;
            dst.insert(dst.end(), src.begin(), src.end());
        }
    }
    // finalize: compute idf and build term directory
    finalize();
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
    if (query.empty() || doc_map_.empty()) return out;
    std::unordered_map<int, double> score; // docId -> score
    std::unordered_set<std::string> seen_query_terms;
    std::unordered_set<std::string> used_terms;
    for (auto& tok : tokenize_query(query)) {
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

// 全量复位。之前这里只清了 doc_tf_/doc_map_/postings_/idf_ 四个，漏掉：
//  - terms_sorted_：存的是指向 postings_ 里 PostingEntry 的裸指针。postings_
//    一 clear，这些指针全部悬空；头文件原注释还写着"invalidated by clear()"，
//    恰恰是它没失效。清掉它是让那句注释成立。
//  - ngram3_/ngram2_/char_：三个辅助索引是 finalize() 里的
//    派生物，留着就是陈旧词项表；search() 的 substring_candidates() 会拿
//    它们枚举上一轮的词条。
//  - signature_/version_/last_error_：这三个是"这个索引是什么格式/为什么
//    加载失败"的对外可见状态（version()、last_error() 都有 public getter，
//    stats() 也把 version_ 报给 CLI 诊断用）。不清的话 clear() 之后
//    version() 还在报旧的 UDFT3，诊断输出直接骗人。
void FullTextIndexStd::clear() {
    doc_tf_.clear();
    doc_map_.clear();
    postings_.clear();
    idf_.clear();
    terms_sorted_.clear();
    ngram3_index_.clear();
    ngram2_index_.clear();
    char_index_.clear();
    signature_.clear();
    version_ = 0;
    last_error_.clear();
}

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
    // 先复位再记版本：clear() 是完整复位（会把 version_ 归 0，见其定义），
    // 顺序反了的话刚认出来的 UDFT 版本会被自己抹掉。version_ 记的是
    // "这份索引是从哪个持久化格式读进来的"，读完之后才成立。
    clear();
    version_ = v3 ? 3 : (v2 ? 2 : 1);
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
    // GCOVR_EXCL_LINE：唯一调用方 search() 在进入前已确认 term 命中
    // postings_（未命中走 continue），期间无任何插入，此处 end 臂
    // 结构上不可达；空表兜底是“返回引用”设计的防御语义，留档不删。
    if (it == postings_.end()) { static const std::vector<std::pair<int,int>> empty; return empty; }  // GCOVR_EXCL_LINE
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

UnidictCoreStd::FullTextIndexStd::Stats UnidictCoreStd::FullTextIndexStd::stats() const {
    Stats s;
    s.terms = postings_.size();
    s.docs = doc_map_.size();
    s.version = version_;
    size_t total_df = 0, comp_terms = 0, comp_bytes = 0, dec_pairs = 0;
    for (const auto& kv : postings_) {
        const PostingEntry& pe = kv.second;
        if (pe.compressed) { ++comp_terms; comp_bytes += pe.buf.size(); total_df += pe.count; }
        else { dec_pairs += pe.vec.size(); total_df += pe.vec.size(); }
    }
    s.postings = total_df;
    s.compressed_terms = comp_terms;
    s.compressed_bytes = comp_bytes;
    s.pairs_decompressed = dec_pairs;
    s.avg_df = s.terms ? (double)total_df / (double)s.terms : 0.0;
    return s;
}

void UnidictCoreStd::FullTextIndexStd::build_ngram3_index() {
    ngram3_index_.clear(); ngram2_index_.clear(); char_index_.clear();
    // Build inverted indexes mapping 3-grams / 2-grams / 1-char to term indices
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
        // 2-grams
        seen.clear();
        for (size_t j = 0; j + 1 < term.size(); ++j) {
            unsigned char c1 = (unsigned char)term[j];
            unsigned char c2 = (unsigned char)term[j+1];
            if (!is_word_char(c1) || !is_word_char(c2)) continue;
            std::string g; g.push_back((char)std::tolower(c1)); g.push_back((char)std::tolower(c2));
            if (!seen.insert(g).second) continue;
            ngram2_index_[g].push_back(i);
        }
        // 1-char
        std::unordered_set<unsigned char> seen1;
        for (unsigned char c : term) {
            if (!is_word_char(c)) continue;
            unsigned char lc = (unsigned char)std::tolower(c);
            if (!seen1.insert(lc).second) continue;
            char_index_[(char)lc].push_back(i);
        }
    }
}

std::vector<std::string> UnidictCoreStd::FullTextIndexStd::substring_candidates(const std::string& tok, size_t cap) const {
    std::vector<std::string> out;
    // 契约：tok 是 search() 从 tokenize() 拿到的查询记号——恒非空、
    // 字符全为词字符（is_word_char），因此不再做空串/逐字符合法性守卫。
    std::string q = lcase(tok);
    if (q.size() >= 3 && !ngram3_index_.empty()) {
        // Choose the rarest 3-gram from the query
        size_t best_sz = SIZE_MAX; const std::vector<int>* best_vec = nullptr;
        for (size_t j = 0; j + 2 < q.size(); ++j) {
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
    if (q.size() == 2 && !ngram2_index_.empty()) {
        auto it = ngram2_index_.find(q);
        if (it != ngram2_index_.end()) {
            // 桶按“词内的 2-gram”归档，而这里 q 恰是查询记号整体，
            // 桶里的词必然含有 q——无需逐词再 find 验证。
            for (int idx : it->second) {
                out.push_back(terms_sorted_[idx].first);
                if (out.size() >= cap) break;
            }
            return out;
        }
    }
    if (q.size() == 1 && !char_index_.empty()) {
        auto it = char_index_.find(q[0]);
        if (it != char_index_.end()) {
            // 同上：单字桶里的词必然含该字。
            for (int idx : it->second) {
                out.push_back(terms_sorted_[idx].first);
                if (out.size() >= cap) break;
            }
            return out;
        }
    }
    // 走到这里 = 没有任何词包含 q。三条快速路成立的前提是
    // “词含 q ⇒ 词必归入 q 的前缀 gram 桶 ⇒ 提前 return”（q 出自
    // tokenize，字符全为词字符，含 q 的词在 gram 归档时不会被跳过）。
    // 原 fallback（prefix 桶 + 全词表扫描）在到达此处时对任何词的
    // find 都只能是 miss，push 臂永远不执行，只是白付 O(词表) 扫描
    // ——连同只为它服务的 prefix_index_ 一起删除。
    return out;
}
