// Aggregated lookup unit tests (std-only).
// Tests for multi-dictionary result aggregation, deduplication, and relevance scoring.

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include "std/aggregate_lookup_std.h"
#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json_dict(const std::string& name, const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path p = fs::current_path() / "build-local" / ("agg_" + name + ".json");
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << "{\n  \"name\": \"" << name << "\",\n  \"entries\": [\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        out << "    {\"word\":\"" << entries[i].first << "\",\"definition\":\"" << entries[i].second << "\"}";
        if (i + 1 < entries.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return p;
}

void test_definition_hash() {
    // Same definition should produce same hash
    std::string def1 = "A definition of the word.";
    std::string def2 = "A definition of the word.";

    std::string hash1 = DictionaryAggregator::calculate_definition_hash(def1);
    std::string hash2 = DictionaryAggregator::calculate_definition_hash(def2);

    assert(!hash1.empty());
    assert(hash1 == hash2);

    // Different definition should produce different hash
    std::string def3 = "A different definition.";
    std::string hash3 = DictionaryAggregator::calculate_definition_hash(def3);

    assert(hash1 != hash3);
}

void test_definition_similarity() {
    // Identical definitions
    double sim1 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "The cat sat on the mat."
    );
    assert(sim1 > 0.9);

    // Similar definitions (one word different)
    double sim2 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "The cat sat on a mat."
    );
    assert(sim2 > 0.7);

    // Completely different
    double sim3 = DictionaryAggregator::definition_similarity(
        "The cat sat on the mat.",
        "Hello world programming."
    );
    assert(sim3 < 0.3);

    // Empty strings
    double sim4 = DictionaryAggregator::definition_similarity("", "");
    assert(sim4 == 0.0);
}

void test_entry_grouping() {
    DictionaryAggregator aggregator;

    std::vector<AggregatedEntry> entries;

    // Add entries for same word from different dictionaries
    AggregatedEntry e1;
    e1.word = "hello";
    e1.definition = "Definition 1";
    e1.source.dictionary_id = "dict1";
    e1.source.priority = 1;
    e1.relevance_score = 0.9;
    entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "hello";
    e2.definition = "Definition 2";
    e2.source.dictionary_id = "dict2";
    e2.source.priority = 2;
    e2.relevance_score = 0.8;
    entries.push_back(e2);

    AggregatedEntry e3;
    e3.word = "world";
    e3.definition = "Definition 3";
    e3.source.dictionary_id = "dict1";
    e3.source.priority = 1;
    e3.relevance_score = 0.7;
    entries.push_back(e3);

    auto groups = aggregator.group_entries(entries);

    assert(groups.size() == 2);

    // Check hello group
    assert(groups[0].word == "hello");
    assert(groups[0].dict_count == 2);
    assert(groups[0].entries.size() == 2);
    assert(groups[0].max_relevance == 0.9);
    assert(groups[0].best_entry != nullptr);
    assert(groups[0].best_entry->source.dictionary_id == "dict1");
}

void test_aggregated_lookup_builder() {
    AggregatedLookupBuilder builder;

    // Add individual entry
    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Test definition";
    e1.source.dictionary_id = "dict1";
    e1.source.dictionary_name = "Dictionary 1";
    e1.source.priority = 1;
    e1.relevance_score = 0.95;
    builder.add_entry(e1);

    // Add entries in bulk
    builder.add_entries("dict2", {"word1", "word2"}, {"def1", "def2"});

    // Set priorities
    builder.set_dictionary_priority("dict1", 1);
    builder.set_dictionary_priority("dict2", 2);

    // Build result
    AggregationResult result = builder.build("test");

    assert(result.query_word == "test");
    assert(result.total_matches == 3);  // 1 individual + 2 bulk
    assert(result.all_entries.size() == 3);
    assert(result.groups.size() >= 2);

    // Check best entry
    const AggregatedEntry* best = result.get_best();
    assert(best != nullptr);
    assert(best->relevance_score >= 0.95);
}

void test_relevance_calculation() {
    // DictionaryAggregator needs DictionaryManagerStd, skip for now
    // Test could be expanded when actual lookup is implemented
}

void test_deduplication() {
    // This tests the deduplication logic in aggregate_lookup_std.cpp
    // Actual implementation would require DictionaryManagerStd setup
}

void test_lookup_options() {
    LookupOptions options;

    // Defaults
    assert(options.deduplicate_definitions == true);
    assert(options.merge_similar_entries == true);
    assert(options.similarity_threshold == 0.85);
    assert(options.max_results_per_dictionary == -1);
    assert(options.max_total_results == -1);
    assert(options.sort_by_relevance == true);
    assert(options.include_disabled == false);

    // Custom values
    options.deduplicate_definitions = false;
    options.max_results_per_dictionary = 10;
    options.max_total_results = 100;

    assert(options.deduplicate_definitions == false);
    assert(options.max_results_per_dictionary == 10);
    assert(options.max_total_results == 100);
}

void test_dictionary_profile() {
    DictionaryProfile profile;

    profile.id = "english_learner";
    profile.name = "English Learner";
    profile.description = "Dictionaries for English learners";
    profile.dictionary_ids = {"oxford", "cambridge", "longman"};
    profile.priority = 1;
    profile.is_default = true;

    assert(profile.id == "english_learner");
    assert(profile.dictionary_ids.size() == 3);
    assert(profile.is_default);
}

void test_entry_source() {
    EntrySource source;

    source.dictionary_id = "oxford";
    source.dictionary_name = "Oxford English Dictionary";
    source.priority = 1;
    source.category = "en-en";
    source.is_enabled = true;

    assert(source.dictionary_id == "oxford");
    assert(source.priority == 1);
    assert(source.category == "en-en");
    assert(source.is_enabled);
}

void test_aggregated_entry() {
    AggregatedEntry entry;

    entry.word = "test";
    entry.definition = "A test entry.";
    entry.pronunciation = "/tɛst/";
    entry.part_of_speech = "noun";
    entry.examples = {"This is a test.", "Another example."};

    entry.source.dictionary_id = "test_dict";
    entry.source.dictionary_name = "Test Dictionary";
    entry.source.priority = 1;
    entry.relevance_score = 0.95;
    entry.definition_hash = "abc123";

    entry.metadata["key1"] = "value1";
    entry.metadata["key2"] = "value2";

    assert(entry.word == "test");
    assert(entry.examples.size() == 2);
    assert(entry.relevance_score == 0.95);
    assert(entry.metadata.size() == 2);
}

void test_aggregation_result() {
    AggregationResult result;

    result.query_word = "hello";
    result.total_matches = 5;
    result.dictionaries_queried = 3;
    result.dictionaries_with_matches = 2;
    result.query_time_ms = 42.5;

    result.match_counts_by_dict["dict1"] = 3;
    result.match_counts_by_dict["dict2"] = 2;

    assert(result.query_word == "hello");
    assert(result.total_matches == 5);
    assert(result.dictionaries_queried == 3);
    assert(result.match_counts_by_dict.size() == 2);

    // Empty result has no best entry
    assert(result.get_best() == nullptr);
}

void test_aggregation_result_with_entries() {
    AggregationResult result;

    result.query_word = "test";

    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Definition 1";
    e1.source.dictionary_id = "dict1";
    e1.relevance_score = 0.9;
    result.all_entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "test";
    e2.definition = "Definition 2";
    e2.source.dictionary_id = "dict2";
    e2.relevance_score = 0.95;
    result.all_entries.push_back(e2);

    // Get best entry (highest relevance)
    const AggregatedEntry* best = result.get_best();
    assert(best != nullptr);
    assert(best->relevance_score == 0.95);
    assert(best->source.dictionary_id == "dict2");
}

void test_get_entries_from_dictionary() {
    AggregationResult result;

    result.query_word = "test";

    // Add entries from different dictionaries
    AggregatedEntry e1;
    e1.word = "test";
    e1.definition = "Def 1";
    e1.source.dictionary_id = "dict1";
    result.all_entries.push_back(e1);

    AggregatedEntry e2;
    e2.word = "test";
    e2.definition = "Def 2";
    e2.source.dictionary_id = "dict1";
    result.all_entries.push_back(e2);

    AggregatedEntry e3;
    e3.word = "test";
    e3.definition = "Def 3";
    e3.source.dictionary_id = "dict2";
    result.all_entries.push_back(e3);

    // Get entries from dict1
    auto dict1_entries = result.get_from_dictionary("dict1");
    assert(dict1_entries.size() == 2);

    // Get entries from dict2
    auto dict2_entries = result.get_from_dictionary("dict2");
    assert(dict2_entries.size() == 1);

    // Get entries from non-existent dict
    auto dict3_entries = result.get_from_dictionary("dict3");
    assert(dict3_entries.size() == 0);
}

void test_aggregated_lookup_builder_clear() {
    AggregatedLookupBuilder builder;

    AggregatedEntry e;
    e.word = "test";
    e.definition = "test def";
    builder.add_entry(e);

    builder.add_entries("dict1", {"word1"}, {"def1"});

    assert(!builder.build("test").all_entries.empty());

    builder.clear();

    assert(builder.build("test").all_entries.empty());
}

void test_dictionary_priority() {
    EntrySource s1, s2, s3;

    s1.dictionary_id = "dict1";
    s1.priority = 3;

    s2.dictionary_id = "dict2";
    s2.priority = 1;

    s3.dictionary_id = "dict3";
    s3.priority = 2;

    // Lower number = higher priority
    assert(s2.priority < s1.priority);
    assert(s3.priority > s2.priority);
}

void test_enabled_dictionaries_filter() {
    LookupOptions options;

    // Enable specific dictionaries
    options.enabled_dictionaries = {"dict1", "dict2"};

    assert(options.enabled_dictionaries.size() == 2);
    assert(options.enabled_dictionaries[0] == "dict1");
}

void test_max_results_limits() {
    LookupOptions options;

    options.max_results_per_dictionary = 5;
    options.max_total_results = 20;

    assert(options.max_results_per_dictionary == 5);
    assert(options.max_total_results == 20);
}

void test_dictionary_enable_state() {
    auto p1 = write_json_dict("one", {{"hello", "def1"}});
    auto p2 = write_json_dict("two", {{"hello", "def2"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));

    DictionaryAggregator agg(&mgr);
    auto all_sources = agg.get_dictionary_sources();
    assert(all_sources.size() == 2);
    assert(all_sources[0].is_enabled);

    assert(mgr.set_dictionary_enabled("two", false));
    auto enabled_ids = agg.get_enabled_dictionary_ids();
    assert(enabled_ids.size() == 1);
    assert(enabled_ids[0] == "one");
    assert(agg.enabled_dictionaries() == 1);

    auto res = agg.lookup("hello");
    assert(!res.all_entries.empty());
    for (const auto& entry : res.all_entries) {
        assert(entry.source.dictionary_id != "two");
    }
}

void test_lookup_respects_enabled_dictionary_filter() {
    auto p1 = write_json_dict("dict1", {{"hello", "from dict1"}});
    auto p2 = write_json_dict("dict2", {{"hello", "from dict2"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.enabled_dictionaries = {"dict2"};

    auto res = agg.lookup("hello", options);
    assert(res.dictionaries_queried == 1);
    assert(res.all_entries.size() == 1);
    assert(res.all_entries[0].source.dictionary_id == "dict2");
}

void test_lookup_excludes_disabled_even_if_selected() {
    auto p1 = write_json_dict("dict1", {{"hello", "from dict1"}});
    auto p2 = write_json_dict("dict2", {{"hello", "from dict2"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    assert(mgr.set_dictionary_enabled("dict2", false));

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.enabled_dictionaries = {"dict2"};

    auto res = agg.lookup("hello", options);
    assert(res.dictionaries_queried == 0);
    assert(res.all_entries.empty());
}

void test_include_disabled_keeps_selected_dictionary_visible() {
    auto p1 = write_json_dict("dict1", {{"hello", "from dict1"}});
    auto p2 = write_json_dict("dict2", {{"hello", "from dict2"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    assert(mgr.set_dictionary_enabled("dict2", false));

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.enabled_dictionaries = {"dict2"};
    options.include_disabled = true;

    auto sources = agg.get_dictionary_sources();
    bool saw_disabled = false;
    for (const auto& source : sources) {
        if (source.dictionary_id == "dict2") {
            assert(!source.is_enabled);
            saw_disabled = true;
        }
    }
    assert(saw_disabled);

    auto res = agg.lookup("hello", options);
    assert(res.dictionaries_queried == 1);
    assert(res.all_entries.size() == 1);
    assert(res.all_entries[0].source.dictionary_id == "dict2");
    assert(!res.all_entries[0].source.is_enabled);
}

void test_include_disabled_keeps_disabled_dictionary_results_in_general_lookup() {
    auto p1 = write_json_dict("dict1", {{"hello", "from dict1"}});
    auto p2 = write_json_dict("dict2", {{"hello", "from dict2"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    assert(mgr.set_dictionary_enabled("dict2", false));

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.include_disabled = true;

    auto res = agg.lookup("hello", options);
    assert(res.dictionaries_queried == 2);
    assert(res.all_entries.size() == 2);

    bool saw_disabled = false;
    for (const auto& entry : res.all_entries) {
        if (entry.source.dictionary_id == "dict2") {
            assert(!entry.source.is_enabled);
            saw_disabled = true;
        }
    }
    assert(saw_disabled);
}

void test_prefix_lookup_total_limit() {
    auto p1 = write_json_dict("dict1", {
        {"hello", "from dict1"},
        {"help", "from dict1 second"}
    });
    auto p2 = write_json_dict("dict2", {
        {"hello", "from dict2"},
        {"help", "from dict2 second"}
    });

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    mgr.build_index();

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.max_total_results = 2;

    auto res = agg.prefix_lookup("he", options);
    assert(res.all_entries.size() == 2);
}

void test_prefix_lookup_per_dictionary_limit() {
    auto p1 = write_json_dict("dict1", {
        {"hello", "from dict1"},
        {"help", "from dict1 second"}
    });
    auto p2 = write_json_dict("dict2", {
        {"hello", "from dict2"},
        {"help", "from dict2 second"}
    });

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    mgr.build_index();

    DictionaryAggregator agg(&mgr);
    LookupOptions options;
    options.max_results_per_dictionary = 1;

    auto res = agg.prefix_lookup("he", options);
    assert(res.all_entries.size() == 2);
    int dict1_count = 0;
    int dict2_count = 0;
    for (const auto& entry : res.all_entries) {
        if (entry.source.dictionary_id == "dict1") ++dict1_count;
        if (entry.source.dictionary_id == "dict2") ++dict2_count;
    }
    assert(dict1_count == 1);
    assert(dict2_count == 1);
}

// ===== 聚合器全接口面：无/有 manager、profile、fuzzy、去重、限制、builder =====
void test_aggregator_manager_full_surface() {
    // --- 无 manager：全部空安全路径 ---
    {
        DictionaryAggregator bare;
        assert(!bare.has_dictionary("x"));
        bare.unregister_dictionary("x");
        bare.set_dictionary_manager(nullptr);
        assert(bare.get_dictionary_ids().empty());
        assert(bare.get_enabled_dictionary_ids().empty());
        assert(bare.get_dictionary_sources().empty());
        assert(bare.get_dictionary_source("d").is_enabled);
        assert(bare.enabled_dictionaries() == 0);
        assert(bare.total_dictionaries() == 0);
        assert(bare.total_words() == 0);
        assert(bare.lookup("w").all_entries.empty());
        assert(bare.prefix_lookup("w").all_entries.empty());
        assert(bare.fuzzy_lookup("w").all_entries.empty());
        bare.set_dictionary_priority("d", 1);   // 无 manager 早退
    }

    // --- 有 manager：两本词典 ---
    DictionaryManagerStd mgr;
    fs::path p1 = write_json_dict("agg_d1", {
        {"apple", "sweet red fruit"},
        {"application", std::string(120, 'x') + " long definition"},
    });
    fs::path p2 = write_json_dict("agg_d2", {
        {"apple", "red sweet fruit"},   // 同词集不同序：hash 不同、相似度 1.0
        {"banana", "yellow fruit"},
    });
    fs::path p3 = write_json_dict("agg_d3", {
        {"apple", "sweet red fruit"},   // 与 d1 完全同文 → hash 命中去重分支
    });
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    assert(mgr.add_dictionary(p3.string()));
    mgr.build_index();   // prefix 检索依赖 trie，add 之后必须重建

    DictionaryAggregator agg(&mgr);
    agg.set_dictionary_priority("agg_d1", 1);   // 有 manager 的 no-op 分支
    agg.set_dictionary_category("agg_d1", "en-en");

    // --- 聚合器禁用开关透传 manager ---
    {
        agg.set_dictionary_enabled("agg_d1", false);
        assert(!mgr.is_dictionary_enabled("agg_d1"));
        agg.set_dictionary_enabled("agg_d1", true);
        assert(mgr.is_dictionary_enabled("agg_d1"));
    }

    assert(agg.has_dictionary("agg_d1") && agg.has_dictionary("agg_d2"));
    assert(!agg.has_dictionary("ghost"));
    assert(agg.enabled_dictionaries() == 3);
    assert(agg.total_dictionaries() == 3);
    assert(agg.total_words() > 0);
    assert(agg.get_dictionary_ids().size() == 3);
    assert(agg.get_enabled_dictionary_ids().size() == 3);
    assert(agg.get_dictionary_sources().size() == 3);
    assert(agg.get_dictionary_source("agg_d1").is_enabled);
    assert(agg.get_dictionary_source("agg_d1").dictionary_id == "agg_d1");

    // --- 禁用词典：lookup 外层 continue ---
    {
        assert(mgr.set_dictionary_enabled("agg_d2", false));
        auto r = agg.lookup("banana");
        assert(r.dictionaries_queried == 2 && r.all_entries.empty());
        assert(mgr.set_dictionary_enabled("agg_d2", true));
    }

    // --- 去重：merge 关（不合并）/ merge 开（相似命中合并）---
    {
        LookupOptions off;
        off.deduplicate_definitions = true;
        off.merge_similar_entries = false;
        auto r = agg.lookup("apple", off);
        // merge_similar_entries=false 时 deduplicate_entries 直接短路返回：
        // 三本词典同词三条全保留（即使 deduplicate_definitions=true 也不去重）
        assert(r.total_matches == 3);
    }
    {
        auto r = agg.lookup("apple");   // 默认 merge_similar_entries=true
        assert(r.total_matches == 1);
        assert(r.dictionaries_with_matches == 1);
        assert(r.match_counts_by_dict.at("agg_d1") == 1);
    }

    // --- 相关度：超长定义拿满长度加成 ---
    {
        auto r = agg.lookup("application");
        assert(r.total_matches == 1);
        assert(r.all_entries[0].relevance_score > 0.9);
    }

    // --- 每词典/总量限制：0 即全过滤 / 首条 break ---
    {
        LookupOptions lim;
        lim.max_results_per_dictionary = 0;
        assert(agg.lookup("apple", lim).all_entries.empty());
        LookupOptions lim2;
        lim2.max_total_results = 0;
        assert(agg.lookup("apple", lim2).all_entries.empty());
    }

    // --- prefix：enabled_dictionaries 白名单过滤 ---
    {
        LookupOptions only1;
        only1.enabled_dictionaries = {"agg_d1"};
        auto r = agg.prefix_lookup("app", only1);
        assert(r.dictionaries_queried == 1);
        assert(!r.all_entries.empty());
        for (const auto& e : r.all_entries)
            assert(e.source.dictionary_id == "agg_d1");
    }

    // --- fuzzy：编辑距离 ≤2 命中 + 白名单 + 两个截断分支 ---
    {
        auto r = agg.fuzzy_lookup("aple");
        assert(!r.all_entries.empty());
        bool saw_apple = false;
        for (const auto& e : r.all_entries)
            if (e.word == "apple") saw_apple = true;
        assert(saw_apple);

        LookupOptions fonly;
        fonly.enabled_dictionaries = {"agg_d1"};
        auto rf = agg.fuzzy_lookup("aple", fonly);
        assert(rf.dictionaries_queried == 1);
        assert(!rf.all_entries.empty());

        LookupOptions cap1;
        cap1.max_results_per_dictionary = 0;
        assert(agg.fuzzy_lookup("aple", cap1).all_entries.empty());
        LookupOptions cap2;
        cap2.max_total_results = 0;
        assert(agg.fuzzy_lookup("aple", cap2).all_entries.empty());
    }

    // --- profile 全套：default 置位/删除/改设/查询 ---
    {
        DictionaryProfile p1;
        p1.id = "p1";
        p1.is_default = true;
        agg.create_profile(p1);
        DictionaryProfile p2;
        p2.id = "p2";
        agg.create_profile(p2);
        assert(agg.get_profiles().size() == 2);
        assert(agg.get_profiles_for_category("en-en").size() == 2);
        agg.set_default_profile("p2");
        agg.set_default_profile("ghost");   // 不存在：忽略
        agg.delete_profile("p2");           // 删除的正是当前默认 → 触发 default 清空分支
        agg.delete_profile("p1");
        agg.delete_profile("ghost");
        auto left = agg.get_profiles();
        assert(left.empty());
    }

    // --- builder：数量不等早退 + 同词多条目分组 ---
    {
        AggregatedLookupBuilder b;
        b.add_entries("d", {"w"}, {"a", "b"});   // words/definitions 数量不等 → 忽略
        EntrySource src;
        src.dictionary_id = "d1";
        AggregatedEntry e1;
        e1.word = "x";
        e1.source = src;
        e1.relevance_score = 0.5;
        AggregatedEntry e2;
        e2.word = "x";
        e2.source = src;
        // 差值 <0.001 时排序比较器视同相等、保持插入序 → e2 后到且更高，
        // 稳定触发 build() 分组里的 max/best 更新分支
        e2.relevance_score = 0.5005;
        b.add_entry(e1);
        b.add_entry(e2);
        auto r = b.build("x");
        assert(r.groups.size() == 1);
        assert(r.groups[0].entries.size() == 2);
        assert(r.groups[0].dict_count == 2);
        assert(r.groups[0].max_relevance == 0.5005);
        b.clear();
    }
}

int main() {
    test_definition_hash();
    test_definition_similarity();
    test_entry_grouping();
    test_aggregated_lookup_builder();
    test_lookup_options();
    test_dictionary_profile();
    test_entry_source();
    test_aggregated_entry();
    test_aggregation_result();
    test_aggregation_result_with_entries();
    test_get_entries_from_dictionary();
    test_aggregated_lookup_builder_clear();
    test_dictionary_priority();
    test_enabled_dictionaries_filter();
    test_max_results_limits();
    test_dictionary_enable_state();
    test_lookup_respects_enabled_dictionary_filter();
    test_lookup_excludes_disabled_even_if_selected();
    test_include_disabled_keeps_selected_dictionary_visible();
    test_include_disabled_keeps_disabled_dictionary_results_in_general_lookup();
    test_prefix_lookup_total_limit();
    test_prefix_lookup_per_dictionary_limit();
    test_aggregator_manager_full_surface();

    return 0;
}
