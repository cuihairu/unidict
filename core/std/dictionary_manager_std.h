// Qt-free dictionary manager: loads std parsers and indexes words.

#ifndef UNIDICT_DICTIONARY_MANAGER_STD_H
#define UNIDICT_DICTIONARY_MANAGER_STD_H

#include <memory>
#include <string>
#include <vector>

#include "index_engine_std.h"
#include "dictionary_std.h"
#include "fulltext_index_std.h"

namespace UnidictCoreStd {

struct DictEntryStd { std::string dict_name; std::string word; std::string definition; };

class DictionaryManagerStd {
public:
    DictionaryManagerStd();

    bool add_dictionary(const std::string& path);
    bool remove_dictionary(const std::string& dict_name);
    void clear_dictionaries();
    std::vector<std::string> loaded_dictionaries() const;
    std::vector<std::string> enabled_dictionaries() const;
    bool set_dictionary_enabled(const std::string& dict_name, bool enabled);
    bool is_dictionary_enabled(const std::string& dict_name) const;
    // 优先级：数值大的词典在查词/列表序中靠前（同优先级保持装载序，
    // 默认 0 = 装载序，与既有行为兼容）
    bool set_dictionary_priority(const std::string& dict_name, int priority);
    int dictionary_priority(const std::string& dict_name) const;
    bool set_dictionary_tags(const std::string& dict_name, std::vector<std::string> tags);
    std::vector<std::string> dictionary_tags(const std::string& dict_name) const;
    // 标签过滤：空 = 全部参与查词；非空 = 词典带任一过滤标签才参与
    // （只影响查询路径，不改变装载/索引集合）
    void set_tag_filter(std::vector<std::string> tags);
    const std::vector<std::string>& tag_filter() const;
    struct DictMeta { std::string name; int word_count; std::string description; };
    std::vector<DictMeta> dictionaries_meta() const;

    std::string search_word(const std::string& word, bool include_disabled = false) const; // returns first match
    // search_word / search_all(…, allow_fulltext_fallback=true) 与 Qt 面
    // searchWord/searchAll 同口径：词头全 miss 时用释义全文兜底（汉英词典
    // 查英文，词只在释义里）。**默认 false**：prefix/fuzzy 路径是拿候选词
    // 逐个回调本方法要释义，兜底会在候选词本身没命中时塞进无关条目
    std::vector<DictEntryStd> search_all(const std::string& word, bool include_disabled = false,
                                         bool allow_fulltext_fallback = false) const;

    // Indexed searches
    void build_index();
    std::vector<std::string> exact_search(const std::string& word) const;
    std::vector<std::string> prefix_search(const std::string& prefix, int max_results = 10) const;
    std::vector<std::string> fuzzy_search(const std::string& word, int max_results = 10) const;
    std::vector<std::string> wildcard_search(const std::string& pattern, int max_results = 10) const;
    std::vector<std::string> regex_search(const std::string& pattern, int max_results = 10) const;
    std::vector<std::string> dictionaries_for_word(const std::string& word) const;
    std::vector<std::string> all_indexed_words() const;
    int indexed_word_count() const;
    bool save_index(const std::string& file) const;
    bool load_index(const std::string& file);

    // Minimal full-text search (MVP): scans definitions for substring matches.
    // Returns matching entries across all loaded dictionaries, in load order.
    std::vector<DictEntryStd> full_text_search(const std::string& query, int max_results = 10) const;

    // Full-text inverted index persistence (must match the same dictionary set/order)
    bool save_fulltext_index(const std::string& file) const;
    bool load_fulltext_index(const std::string& file);
    // Load full-text index without signature check (for legacy/loose compatibility).
    // accept_version:
    //   0 —— 接受任何能解析的版本（loose 语义，调用方自己负责判风险）
    //   1 —— 只接受 legacy v1（无签名的老索引）。v2/v3 索引带签名，既然
    //        走到这个兜底就说明签名没通过 strict 校验，也就是它与当前词典
    //        不是同一套；此时**必须拒绝且不把索引装进内存**。否则会出现
    //        "调用方以为加载成功、实际全文检索在用一套旧索引"的静默错误。
    bool load_fulltext_index_relaxed(const std::string& file, int* out_version = nullptr,
                                     std::string* out_error = nullptr,
                                     int accept_version = 0);

    // Deterministic signature of currently loaded dictionary set/order.
    std::string fulltext_signature() const;
    // Stats of the currently loaded full-text index (empty if none loaded/built)
    FullTextIndexStd::Stats fulltext_stats() const;

private:
    std::vector<DictionaryStd> dicts_;
    std::vector<std::string> tag_filter_;
    IndexEngineStd index_;
    mutable std::unique_ptr<FullTextIndexStd> ft_index_; // built lazily
    void ensure_fulltext_index_built() const;
    const DictionaryStd* find_dictionary(const std::string& dict_name) const;
    // 标签过滤匹配（enabled 与否由调用方另行判定）
    bool participates(const DictionaryStd& d) const;
    // 查询/列表视图：priority 降序、同优先级保持装载序；dicts_ 本体
    // 始终保持装载序（全文索引 DocRef 下标依赖它）
    std::vector<const DictionaryStd*> ordered_dictionaries() const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_MANAGER_STD_H
