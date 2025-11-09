// Minimal inverted index for full-text search (std-only).
// Tokenizes ASCII words, builds postings, TF-IDF scoring at query time.

#ifndef UNIDICT_FULLTEXT_INDEX_STD_H
#define UNIDICT_FULLTEXT_INDEX_STD_H

#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

class FullTextIndexStd {
public:
    struct DocRef { int dict = -1; int word = -1; };

    FullTextIndexStd();

    // Add one document's text contents with a reference back to (dict_idx, word_idx).
    // Returns the internal doc id.
    int add_document(const std::string& text, DocRef ref);

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
    static inline bool is_word_char(unsigned char c);
    static std::vector<std::string> tokenize(const std::string& s);

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
    std::vector<std::string> substring_candidates(const std::string& tok, size_t cap = 256) const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_FULLTEXT_INDEX_STD_H
