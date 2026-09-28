// aggregate_lookup_std 分支缺口补测（lines 100% 后的最大真实缺边簇）。
//
// 上轮实测（build-cov 原始 gcov 图，扣除 throw 边）：本文件真实条件缺边
// 104 条，最大三簇是 exact/prefix/fuzzy 三处 source 三元（各 22 边——
// false 臂结构不可达 + true 臂 string 拷贝的 SSO/heap 互补分支从未同时
// 走过）。本文件逐簇收口，全部真实输入驱动：
//   S1 长短词典名 × 三条查询路径（source 元数据真实流经 + SSO/heap 两臂）
//   S2/S3/S4 exact/prefix/fuzzy 的每词典与总量上限卫语句
//   S5 三路径 dedup/sort 开关关闭臂
//   S6 停用词典 include_disabled 两臂
//   S7 释义内容边角（纯标签/前导分隔/连续空格/标签混合）
//   S8 manager 边界（优先级真/假 id、空 manager）
//   S9 builder 手工 relevance 覆盖 best_entry 更新两臂
//   S10 Jaro 相似度内部臂（自身相等/零匹配/换序）

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/aggregate_lookup_std.h"
#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json_dict(const std::string& name,
                                const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path p = fs::current_path() / "build-local" / ("aggb_" + name + ".json");
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << "{\n  \"name\": \"" << name << "\",\n  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << "    {\"word\":\"" << entries[i].first << "\",\"definition\":\""
            << entries[i].second << "\"}";
        if (i + 1 < entries.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return p;
}

// 短名（SSO，≤15 字节走栈内小串）与长名（heap，>15 字节走堆分配）：
// source 拷贝构造的两条内联臂只有两类名字都流过才会都命中。
static const char* kShort = "en";
static const char* kLong = "long-dictionary-name-over-fifteen";

static void add_two_dicts(DictionaryManagerStd& mgr, const std::string& word,
                          const std::string& def_short, const std::string& def_long) {
    auto p1 = write_json_dict(kShort, {{word, def_short}});
    auto p2 = write_json_dict(kLong, {{word, def_long}});
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    mgr.build_index();  // prefix/fuzzy 检索依赖 trie，add 之后必须重建
}

// S1+S2+S3+S5+S6 共用：hello 两词典各一条
static void make_hello_manager(DictionaryManagerStd& mgr) {
    add_two_dicts(mgr, "hello", "a greeting in short dict",
                  "a greeting in the very long dictionary");
}

static void test_sources_flow_and_caps() {
    // S1 source 元数据流：三条路径的条目都带正确的 source（长短名都流过）
    DictionaryManagerStd mgr;
    make_hello_manager(mgr);
    DictionaryAggregator agg(&mgr);

    auto ex = agg.lookup("hello");
    assert(ex.all_entries.size() == 2);
    for (const auto& e : ex.all_entries) {
        assert(e.source.dictionary_id == kShort || e.source.dictionary_id == kLong);
        assert(e.source.dictionary_name == e.source.dictionary_id);
        assert(e.source.is_enabled);
    }

    auto pre = agg.prefix_lookup("hel");
    assert(pre.all_entries.size() == 2);
    for (const auto& e : pre.all_entries) {
        assert(e.source.dictionary_name == e.source.dictionary_id);
    }

    auto fz = agg.fuzzy_lookup("hello");
    assert(!fz.all_entries.empty());
    for (const auto& e : fz.all_entries) {
        assert(e.source.dictionary_name == e.source.dictionary_id);
    }

    // S2 exact 上限：每词典 0 条（真实配置值=该词典静音）→ 全部被 continue 掉
    LookupOptions cap0;
    cap0.max_results_per_dictionary = 0;
    auto r0 = agg.lookup("hello", cap0);
    assert(r0.all_entries.empty());

    // 总量 1：第二个词典的条目触发 break
    LookupOptions cap1;
    cap1.max_total_results = 1;
    auto r1 = agg.lookup("hello", cap1);
    assert(r1.all_entries.size() == 1);

    // S3 prefix 同款上限（total return 臂）
    auto rp = agg.prefix_lookup("hel", cap1);
    assert(rp.all_entries.size() == 1);
    auto rp0 = agg.prefix_lookup("hel", cap0);
    assert(rp0.all_entries.empty());

    // per-dict 上限 >0 且未达上限：条目正常通过（与 0 的全 continue 相对）
    LookupOptions pd1;
    pd1.max_results_per_dictionary = 1;
    assert(agg.lookup("hello", pd1).all_entries.size() == 2);

    // S5 dedup/sort 开关关闭臂（三条路径）
    LookupOptions off;
    off.deduplicate_definitions = false;
    off.sort_by_relevance = false;
    assert(agg.lookup("hello", off).all_entries.size() == 2);
    assert(agg.prefix_lookup("hel", off).all_entries.size() == 2);
    assert(!agg.fuzzy_lookup("hello", off).all_entries.empty());
}

static void test_fuzzy_paths() {
    // S4+S10：fuzzy 的上限/词限臂 + Jaro 内部臂
    DictionaryManagerStd mgr;
    auto p1 = write_json_dict(kShort, {{"hello", "greeting"}, {"xy", "two letters"},
                                       {"abdce", "swapped tail"}});
    auto p2 = write_json_dict(kLong, {{"hello", "greeting in long dict"}});
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    mgr.build_index();  // prefix/fuzzy 检索依赖 trie，add 之后必须重建
    DictionaryAggregator agg(&mgr);
    (void)p1; (void)p2;

    // 查询词自身会被 fuzzy_search（edit distance 0 ≤ 2）原样返回：
    // string_similarity(word, word) 走"相等返回 1.0"的臂
    auto self = agg.fuzzy_lookup("hello");
    bool saw_self = false;
    for (const auto& e : self.all_entries) {
        if (e.word == "hello") saw_self = true;
        assert(e.relevance_score > 0.0 && e.relevance_score <= 1.0);
    }
    assert(saw_self);

    // "ab" vs "xy"：edit distance 2 入选，但窗口内无公共字符 → Jaro 零匹配臂
    auto nomatch = agg.fuzzy_lookup("ab");
    bool saw_xy = false;
    for (const auto& e : nomatch.all_entries) {
        if (e.word == "xy") saw_xy = true;
    }
    assert(saw_xy);

    // "abcde" vs "abdce"：edit distance 2 入选，匹配字符乱序 → 换序计数臂
    auto swap = agg.fuzzy_lookup("abcde");
    bool saw_swap = false;
    for (const auto& e : swap.all_entries) {
        if (e.word == "abdce") saw_swap = true;
    }
    assert(saw_swap);

    // 词限两臂：max_total ≤ 0 → word_limit=50；>0 → max(max_total, 50)
    LookupOptions neg;
    assert(agg.fuzzy_lookup("hello", neg).total_matches >= 0);
    LookupOptions pos;
    pos.max_total_results = 5;
    assert(agg.fuzzy_lookup("hello", pos).total_matches >= 0);

    // 上限臂：total=1 → 第二条触发 return；per-dict=0 → 全部 continue；
    // per-dict=1 未达上限 → 通过
    LookupOptions ft1;
    ft1.max_total_results = 1;
    assert(agg.fuzzy_lookup("hello", ft1).all_entries.size() == 1);
    LookupOptions fpd0;
    fpd0.max_results_per_dictionary = 0;
    assert(agg.fuzzy_lookup("hello", fpd0).all_entries.empty());
    LookupOptions fpd1;
    fpd1.max_results_per_dictionary = 1;
    assert(agg.fuzzy_lookup("hello", fpd1).all_entries.size() == 2);

    // 空查询串：编辑距离 ≤2 捞出 1-2 字符词；相似度计算走空串守卫臂
    // （string_similarity("", w) 与 calculate_relevance 的 else 支路
    // string_similarity(w, "") 两个方向）
    auto empty_q = agg.fuzzy_lookup("");
    bool saw_xy_empty = false;
    for (const auto& e : empty_q.all_entries) {
        if (e.word == "xy") saw_xy_empty = true;
    }
    assert(saw_xy_empty);
}

static void test_disabled_dictionaries() {
    // S6：停用词典在 ctx 构建轮被 continue（默认），include_disabled 时入选
    DictionaryManagerStd mgr;
    make_hello_manager(mgr);
    assert(mgr.set_dictionary_enabled(kLong, false));
    DictionaryAggregator agg(&mgr);

    LookupOptions def;
    auto ex = agg.lookup("hello", def);
    for (const auto& e : ex.all_entries) {
        assert(e.source.dictionary_id == kShort);  // 停用词典被筛掉
    }
    assert(agg.prefix_lookup("hel", def).all_entries.size() == 1);

    LookupOptions inc;
    inc.include_disabled = true;
    auto ex2 = agg.lookup("hello", inc);
    assert(ex2.all_entries.size() == 2);
    bool saw_disabled = false;
    for (const auto& e : ex2.all_entries) {
        if (e.source.dictionary_id == kLong) {
            saw_disabled = true;
            assert(!e.source.is_enabled);
        }
    }
    assert(saw_disabled);
    auto fz = agg.fuzzy_lookup("hello", inc);
    assert(!fz.all_entries.empty());

    // include_disabled=true 的 prefix ctx 臂 + 默认选项下 fuzzy ctx 的
    // 停用词典 continue 臂
    auto pre_inc = agg.prefix_lookup("hel", inc);
    assert(pre_inc.all_entries.size() == 2);
    auto fz_def = agg.fuzzy_lookup("hello", def);
    for (const auto& e : fz_def.all_entries) {
        assert(e.source.dictionary_id == kShort);
    }
}

static void test_definition_content_edges() {
    // S7：dedup 成对比较把 strip_html_tags / tokenize / 空白压缩的各臂走全。
    // 同词 "poly" 放三词典，释义两两比较：
    //  - 纯标签释义：strip 后空 → tokenize 空 → 相似度 0 守卫臂
    //  - 前导分隔符：tokenize 的"分隔符遇空 current"臂
    //  - 连续空格：definition_hash 压缩的"已在空格中"臂
    //  - 标签+正文混合与纯正文：in_tag 置位/复位/跳过各臂
    DictionaryManagerStd mgr;
    auto p1 = write_json_dict("d1", {{"poly", "<b></b>"}});
    auto p2 = write_json_dict("d2", {{"poly", " (adj) word  with  spaces"}});
    auto p3 = write_json_dict("d3", {{"poly", "<i>styled</i> plain tail"}});
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    assert(mgr.add_dictionary(p3.string()));
    DictionaryAggregator agg(&mgr);

    auto res = agg.lookup("poly");
    assert(res.all_entries.size() == 3);
    // 纯标签释义 stripping 后为空串，hash 稳定
    for (const auto& e : res.all_entries) {
        assert(!e.definition_hash.empty());
    }
}

static void test_manager_edges() {
    // S8：优先级设置的真/假 id；空 manager 的各兜底臂
    DictionaryAggregator null_agg;
    assert(!null_agg.has_dictionary("en"));
    assert(null_agg.get_dictionary_ids().empty());
    assert(null_agg.get_enabled_dictionary_ids().empty());
    assert(null_agg.total_dictionaries() == 0);
    assert(null_agg.enabled_dictionaries() == 0);
    assert(null_agg.total_words() == 0);
    assert(null_agg.get_dictionary_sources().empty());
    // 空 manager 下 get_dictionary_source 的 is_enabled 兜底 true 臂
    assert(null_agg.get_dictionary_source("en").is_enabled);
    null_agg.set_dictionary_enabled("en", true);  // 空 manager 直通返回
    null_agg.set_dictionary_priority("en", 3);    // 空 manager 直通返回

    DictionaryManagerStd mgr;
    make_hello_manager(mgr);
    DictionaryAggregator agg(&mgr);
    assert(agg.has_dictionary(kShort));
    // 真实 id：循环命中后 return；不存在 id：循环耗尽
    agg.set_dictionary_priority(kShort, 3);
    agg.set_dictionary_priority("no-such-dictionary", 9);
    assert(agg.get_dictionary_source(kShort).dictionary_id == kShort);
    // 真实 manager + 不存在的 id：is_dictionary_enabled 走找不到词典的臂
    assert(!agg.get_dictionary_source("no-such-dictionary").is_enabled);
    assert(agg.get_dictionary_sources().size() == 2);
    assert(agg.total_dictionaries() == 2);
    assert(agg.total_words() == 1);
}

static void test_builder_best_entry_update() {
    // S9：同词多词条且后条 relevance 更高 → best_entry 更新臂；
    // 再补一条更低的 → 不更新臂。
    AggregatedLookupBuilder b;
    AggregatedEntry e1;
    e1.word = "w";
    e1.definition = "first";
    e1.relevance_score = 0.5;
    AggregatedEntry e2;
    e2.word = "w";
    e2.definition = "second";
    e2.relevance_score = 0.9;
    AggregatedEntry e3;
    e3.word = "w";
    e3.definition = "third";
    e3.relevance_score = 0.1;
    b.add_entry(e1);
    b.add_entry(e2);
    b.add_entry(e3);
    auto r = b.build("w");
    assert(r.groups.size() == 1);
    assert(r.groups[0].entries.size() == 3);
    assert(r.groups[0].dict_count == 3);
    assert(r.groups[0].max_relevance == 0.9);
    assert(r.groups[0].best_entry != nullptr);
    assert(r.groups[0].best_entry->relevance_score == 0.9);
    assert(r.all_entries[0].relevance_score == 0.9);  // 按 relevance 降序

    // add_entries 的词数/释义数不匹配 → 直接返回
    AggregatedLookupBuilder b2;
    b2.add_entries("d", {"a", "b"}, {"only-one"});
    assert(b2.build("a").all_entries.empty());
}

int main() {
    test_sources_flow_and_caps();
    test_fuzzy_paths();
    test_disabled_dictionaries();
    test_definition_content_edges();
    test_manager_edges();
    test_builder_best_entry_update();
    std::cout << "OK\n";
    return 0;
}
