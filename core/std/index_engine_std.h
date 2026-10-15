// A Qt-free index engine implementation using only the C++ standard library.
// This is the reference path for making the core independent of Qt.

#ifndef UNIDICT_INDEX_ENGINE_STD_H
#define UNIDICT_INDEX_ENGINE_STD_H

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "text_norm_std.h"

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
    void clear();
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

    // P1 text-norm：换键归一化档位。词表（dict_ 原形）不动，作废
    // word_index_/trie_ 后按新档位重灌——add_word 的去重/归一全走
    // norm_options_，Exact 档位下 École 与 école 不再合并
    void set_norm_options(const TextNorm::Options& opt);

private:
    std::string normalize(const std::string& s) const;
    static int edit_distance(const std::string& a, const std::string& b);
    static bool wildcard_match(const std::string& word, const std::string& pattern);

    std::unique_ptr<TrieNode> trie_;
    std::unordered_map<std::string, IndexEntry> word_index_;                  // normalized -> entry
    std::unordered_map<std::string, std::unordered_set<std::string>> dict_;   // dictId -> set(words)
    TextNorm::Options norm_options_{}; // 默认全开 = 现行 fold_key 行为
    bool built_ = false;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_INDEX_ENGINE_STD_H
