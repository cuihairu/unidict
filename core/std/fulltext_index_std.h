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

    // For diagnostics
    int doc_count() const;    

    void clear();

private:
    static inline bool is_word_char(unsigned char c);
    static std::vector<std::string> tokenize(const std::string& s);

    // Per-doc term frequencies
    // doc_tf_[docId][token] = count
    std::vector<std::unordered_map<std::string, int>> doc_tf_;
    std::vector<DocRef> doc_map_; // docId -> DocRef

    // Postings: token -> vector of (docId, tf)
    std::unordered_map<std::string, std::vector<std::pair<int,int>>> postings_;
    // IDF values for tokens
    std::unordered_map<std::string, double> idf_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_FULLTEXT_INDEX_STD_H
