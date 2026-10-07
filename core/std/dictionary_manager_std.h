// Qt-free dictionary manager: loads std parsers and indexes words.

#ifndef UNIDICT_DICTIONARY_MANAGER_STD_H
#define UNIDICT_DICTIONARY_MANAGER_STD_H

#include <cstdint>
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

    // 失败隔离两档（对齐 unidict_core.h:38-46 语义）：
    //   解析失败 → quarantined=true：持久隔离，重启后跳过解析（大词典
    //     反复失败代价高），显式 retry_failed_dictionary 才再试；
    //   文件丢失/扩展名不支持 → 不建记录（运行期档：显式添加直接失败
    //     返回，恢复语义由 load_state 在还原时逐次重查）。
    struct DictionaryFailureStd {
        std::string file_path;
        std::string reason;
        bool quarantined = false;  // true=持久隔离；false=运行期诊断
    };
    const std::vector<DictionaryFailureStd>& failed_dictionaries() const { return failures_; }
    // 显式重试：成功摘除失败记录；再失败刷新原因并确认隔离（quarantined=true）
    bool retry_failed_dictionary(const std::string& file_path);
    // 最近一次 add/retry 失败原因（与 legacy m_lastError 同位）
    const std::string& last_error() const { return last_error_; }

    // 状态持久化。文件格式与 legacy dictionary_state.json 的
    // dictionaries/quarantined 字段对齐（file_path/enabled/tags、
    // file_path/reason/quarantined）；priority 为 std 扩展字段；
    // history 不在 std 面（P-7 双存储收敛时定单一事实源）。
    // 手写 JSON 读写器复用 data_store_std 的口径（单入口字符串解析、
    // 字符串感知深度计数、受限转义表）。
    bool save_state(const std::string& state_file_path) const;
    // 还原语义对齐 legacy loadFromJson：隔离中的路径跳过解析；文件
    // 丢失/扩展名不支持记运行期诊断档（每次载入重查，文件回来自动
    // 恢复加载）；解析失败升级持久隔离。会先清空当前词典集合。
    bool load_state(const std::string& state_file_path);

    // 伴生 .mdd 资源访问（按词典名，跨该词典全部同名 .mdd 组合查询）
    bool has_resource(const std::string& dict_name, const std::string& key) const;
    std::vector<uint8_t> resource_data(const std::string& dict_name, const std::string& key) const;
    std::string resource_string(const std::string& dict_name, const std::string& key) const;

    // 词典源文件全量路径（装载序；未知名返回空）。伴生 .mdd 的
    // 同 stem 推导由此展开（mdx 与 .mdd 不同名时按源路径就近找）。
    std::vector<std::string> dictionary_source_paths(const std::string& dict_name) const;

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

    // searchGrouped 三层降级（P-3 3.3，对齐 legacy searchGrouped 契约，
    // TD-104 单口径准备）：层 0 词头精确（relevance 0）→ 层 1 词头前缀
    // （relevance 1，前缀索引取 12 候选逐词回查）→ 层 2 释义包含
    // （relevance 2 + fulltext 标注，全文兜底 12 条）；命中层即止。
    // 组按词典聚拢（查询视图序 = priority 降序稳定），组内同词头
    // （fold_key 折叠）去重保留首条。词典身份 = 词典名（std 面无独立
    // id，桥接层映射）。启用 + 标签过滤口径与其余查询一致。
    struct GroupedEntryStd {
        std::string word;
        std::string definition;
        int relevance = 0;      // 0 词头精确 / 1 词头前缀 / 2 释义包含
        bool fulltext = false;  // 层 2 置位
    };
    struct DictionaryGroupStd {
        std::string dictionary_name;
        std::vector<GroupedEntryStd> entries;
    };
    std::vector<DictionaryGroupStd> search_grouped(const std::string& word) const;

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
    std::vector<DictionaryFailureStd> failures_;
    std::string last_error_;
    std::vector<std::string> tag_filter_;
    IndexEngineStd index_;
    // 前缀 trie 自愈标记：add/load_state 后未显式 build_index 时，
    // search_grouped 的层 1 先补建（避免前缀层静默空手滑向层 2）
    mutable bool prefix_index_dirty_ = true;
    mutable std::unique_ptr<FullTextIndexStd> ft_index_; // built lazily
    void ensure_fulltext_index_built() const;
    const DictionaryStd* find_dictionary(const std::string& dict_name) const;
    int index_of_failure(const std::string& file_path) const;
    // 同路径已有记录则刷新原因/档位，否则追加
    void record_failure(const std::string& file_path, const std::string& reason, bool quarantined);
    // 标签过滤匹配（enabled 与否由调用方另行判定）
    bool participates(const DictionaryStd& d) const;
    // 查询/列表视图：priority 降序、同优先级保持装载序；dicts_ 本体
    // 始终保持装载序（全文索引 DocRef 下标依赖它）
    std::vector<const DictionaryStd*> ordered_dictionaries() const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_MANAGER_STD_H
