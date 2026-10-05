// Minimal inverted index for full-text search (std-only).
// Tokenizes ASCII words and CJK runs (unigram+bigram), builds postings,
// TF-IDF scoring at query time.

#ifndef UNIDICT_FULLTEXT_INDEX_STD_H
#define UNIDICT_FULLTEXT_INDEX_STD_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

class FullTextIndexStd {
public:
    struct DocRef { int dict = -1; int word = -1; };

    // 分词器行为版本：tokenize 的产词规则变更（如 CJK 分词接入、大小写
    // 折叠口径）时递增。DictionaryManager 的全文签名（TV=）掺它，旧 UDFT
    // 缓存签名失配自动重建——分词器换了、缓存词表不换是静默坏数据。
    static constexpr int kTokenizerVersion = 2;

    FullTextIndexStd();

    // Add one document's text contents with a reference back to (dict_idx, word_idx).
    // Returns the internal doc id.
    int add_document(const std::string& text, DocRef ref);
    // Parallel builder: build index from a batch of documents (definition texts) and their refs.
    // If threads <= 0, uses hardware_concurrency or 1.
    void build_from_documents(const std::vector<std::pair<std::string, DocRef>>& docs, int threads = 0);

    // Once all documents are added, call finalize() to compute IDF.
    void finalize();

    // Query using simple tokenization; returns DocRefs ordered by score desc.
    std::vector<DocRef> search(const std::string& query, int max_results = 20) const;
    bool save(const std::string& path) const;
    bool load(const std::string& path);
    void set_signature(const std::string& sig) { signature_ = sig; }
    const std::string& signature() const { return signature_; }
    int version() const { return version_; }
    const std::string& last_error() const { return last_error_; }

    // For diagnostics
    int doc_count() const;    

    void clear();

private:
    // 高字节（UTF-8 多字节序列）也算词字符：拉丁变音整词保留、CJK 词项
    // 的字节 ngram 归档不断流（否则中文词项的 substring 后备整体失效）
    static inline bool is_word_char(unsigned char c);
    // 文档侧：ASCII 词 + CJK 连续段的 unigram+bigram（单字查询和多字
    // 查询都命中）。查询侧走 tokenize_query()，口径不同。
    static std::vector<std::string> tokenize(const std::string& s);
    // 查询侧：ASCII 同文档侧；CJK 连续段 ≥2 字只发 bigram——发单字会把
    // 「你好」扩成「含你 ∪ 含好」，层 2（释义包含）直接退化成单字噪声
    static std::vector<std::string> tokenize_query(const std::string& s);
    // 两者共用的实现：query_mode 只改 CJK 连续段的发词口径（见上）
    static std::vector<std::string> tokenize_impl(const std::string& s, bool query_mode);

    // Per-doc term frequencies (only used when building from scratch)
    // doc_tf_[docId][token] = count
    std::vector<std::unordered_map<std::string, int>> doc_tf_;
    std::vector<DocRef> doc_map_; // docId -> DocRef

    struct PostingEntry {
        std::vector<std::pair<int,int>> vec; // decompressed postings
        std::string buf;                     // compressed postings (UDFT3)
        uint32_t count = 0;                  // expected number of postings
        bool compressed = false;             // true if using buf
    };

    // Postings: token -> postings entry
    std::unordered_map<std::string, PostingEntry> postings_;
    // IDF values for tokens
    std::unordered_map<std::string, double> idf_;
    std::string signature_;
    int version_ = 0; // 0=unset, 1=UDFT1, 2=UDFT2
    std::string last_error_;

    // Helper: ensure postings for a term are decompressed (if stored compressed)
    const std::vector<std::pair<int,int>>& ensure_postings(const std::string& term) const;

    // Term directory for faster scans (prefix/substring candidates)
    // Built in finalize(). Points into postings_ entries; invalidated by clear().
    std::vector<std::pair<std::string, PostingEntry*>> terms_sorted_;
    void build_term_directory();

    // 3-gram inverted index over terms for fast substring candidate lookup
    std::unordered_map<std::string, std::vector<int>> ngram3_index_;
    std::unordered_map<std::string, std::vector<int>> ngram2_index_;
    std::unordered_map<char, std::vector<int>> char_index_;
    void build_ngram3_index();
    // 契约：tok 必须来自 tokenize()/tokenize_query()（非空、全词字符）。
    std::vector<std::string> substring_candidates(const std::string& tok, size_t cap = 256) const;

public:
    struct Stats {
        size_t terms = 0;
        size_t docs = 0;
        size_t postings = 0;            // sum of df across terms
        size_t compressed_terms = 0;    // number of terms still compressed
        size_t compressed_bytes = 0;    // total bytes of compressed buffers (if any)
        size_t pairs_decompressed = 0;  // total decompressed pairs available in memory
        double avg_df = 0.0;
        int version = 0;
    };
    Stats stats() const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_FULLTEXT_INDEX_STD_H
