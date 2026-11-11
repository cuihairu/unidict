// A Qt-free index engine implementation using only the C++ standard library.
// This is the reference path for making the core independent of Qt.

#ifndef UNIDICT_INDEX_ENGINE_STD_H
#define UNIDICT_INDEX_ENGINE_STD_H

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "text_norm_std.h"

namespace UnidictCoreStd {

struct IndexEntry {
    // 扫描缓存槽位（IndexEngineStd::scan_words_ 下标）；UINT32_MAX = 无。
    // 摘除时把槽位置空即可，无需在连续数组里移动元素
    static constexpr std::uint32_t kNoScanSlot = 0xFFFFFFFFu;

    std::string word;
    std::string normalized_word;
    std::vector<std::string> dictionary_ids;
    int frequency = 0;
    std::uint32_t scan_id = kNoScanSlot;
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
    // 全表扫描层（模糊/通配/子串）带单次耗时上限：budget_ms>0 时到点即止，
    // 大词典不卡调用线程；budget_ms<=0 = 不设上限（测试/CLI 的确定性口径）
    std::vector<std::string> fuzzy_search(const std::string& word, int max_results = 10,
                                           int budget_ms = 0) const;
    std::vector<std::string> wildcard_search(const std::string& pattern, int max_results = 10,
                                             int budget_ms = 0) const;
    std::vector<std::string> regex_search(const std::string& pattern, int max_results = 10) const;
    // 乱序容错（anagram）：同字母异序词，hlelo→hello。键 = 排序后的字母串，
    // 导入时 O(词数) 建表、add/remove 对称维护，查询 O(键长 + 桶内词数)——
    // 与前缀层同级，不扫全表。返回值不含查询词自身
    std::vector<std::string> anagram_search(const std::string& word, int max_results = 10) const;
    // 忘词容错：子串命中（ello→hello）。无前缀可走 trie，按需扫 + 单次
    // 耗时上限（budget_ms，<=0 不设上限）
    std::vector<std::string> substring_search(const std::string& sub, int max_results = 10,
                                              int budget_ms = 0) const;

    std::vector<std::string> all_words() const;
    std::vector<std::string> dictionaries_for_word(const std::string& word) const;
    int word_count() const;
    // 词频（词条缺失/未登记返回 0）——模糊层同分排序用
    int frequency_of(const std::string& word) const;

    // Persistence (simple line format)
    bool save_index(const std::string& file_path) const;
    bool load_index(const std::string& file_path);

    // P1 text-norm：换键归一化档位。词表（dict_ 原形）不动，作废
    // word_index_/trie_ 后按新档位重灌——add_word 的去重/归一全走
    // norm_options_，Exact 档位下 École 与 école 不再合并
    void set_norm_options(const TextNorm::Options& opt);

private:
    std::string normalize(const std::string& s) const;
    // 模糊层专用距离：a 已小写、b 现场小写比较（免逐候选 lcase 分配），
    // prev/cur 由调用方复用（免每词两次 vector 分配——121k 词即 24 万次）
    static int edit_distance_buf(const std::string& a_lower, const std::string& b,
                                 std::vector<int>& prev, std::vector<int>& cur);
    // 通配模式 → 正则串（元字符只留 * ?，其余逐个转义）；wildcard_search
    // 一次编译复用，不再逐词编译
    static std::string wildcard_to_regex(const std::string& pattern);
    // 乱序键 = 小写化后按字节排序的字母串（与前缀 trie 的小写口径一致，
    // UTF-8 中文按字节排序仍能同字集合聚桶）
    static std::string anagram_key(const std::string& word);

    std::unique_ptr<TrieNode> trie_;
    std::unordered_map<std::string, IndexEntry> word_index_;                  // normalized -> entry
    std::unordered_map<std::string, std::unordered_set<std::string>> dict_;   // dictId -> set(words)
    // 乱序索引：排序字母键 -> 词（与词表同生死：add_word 增量插、
    // remove_word 对称摘、build_index/clear 整体重建）
    std::unordered_map<std::string, std::vector<std::string>> anagram_index_;
    // 全表扫描缓存：词指针的连续数组（下标 = IndexEntry::scan_id）。
    // word_index_ 按桶序遍历是随机访存，实测比连续数组慢一个量级——
    // 子串/模糊/正则等必扫路径走这里。槽位摘除置空不移动元素，
    // build_index 整体重建时压实
    std::vector<const std::string*> scan_words_;
    TextNorm::Options norm_options_{}; // 默认全开 = 现行 fold_key 行为
    bool built_ = false;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_INDEX_ENGINE_STD_H
