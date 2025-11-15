// A Qt-free index engine implementation using only the C++ standard library.
// This is the reference path for making the core independent of Qt.

#ifndef UNIDICT_INDEX_ENGINE_STD_H
#define UNIDICT_INDEX_ENGINE_STD_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace UnidictCoreStd {

struct IndexEntry {
    std::string word;
    std::string normalized_word;
    std::vector<std::string> dictionary_ids;
    int frequency = 0;
};

class TrieNode {
public:
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    std::unordered_set<std::string> words; // store full words ending at or under this node

    void insert(const std::string& word);
    void collect(const std::string& prefix, std::vector<std::string>& out, int& count, int max_results) const;
};

class IndexEngineStd {
public:
    IndexEngineStd();
    ~IndexEngineStd() = default;

    // Mutations
    void add_word(const std::string& word, const std::string& dictionary_id);
    void remove_word(const std::string& word, const std::string& dictionary_id);
    void clear_dictionary(const std::string& dictionary_id);
    void build_index();

    // Queries
    std::vector<std::string> exact_match(const std::string& word) const;
    std::vector<std::string> prefix_search(const std::string& prefix, int max_results = 10) const;
    std::vector<std::string> fuzzy_search(const std::string& word, int max_results = 10) const;
    std::vector<std::string> wildcard_search(const std::string& pattern, int max_results = 10) const;
    std::vector<std::string> regex_search(const std::string& pattern, int max_results = 10) const;

    std::vector<std::string> all_words() const;
    std::vector<std::string> dictionaries_for_word(const std::string& word) const;
    int word_count() const;

    // Persistence (simple line format)
    bool save_index(const std::string& file_path) const;
    bool load_index(const std::string& file_path);

private:
    static std::string normalize(const std::string& s);
    static int edit_distance(const std::string& a, const std::string& b);
    static bool wildcard_match(const std::string& word, const std::string& pattern);

    std::unique_ptr<TrieNode> trie_;
    std::unordered_map<std::string, IndexEntry> word_index_;                  // normalized -> entry
    std::unordered_map<std::string, std::unordered_set<std::string>> dict_;   // dictId -> set(words)
    bool built_ = false;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_INDEX_ENGINE_STD_H

