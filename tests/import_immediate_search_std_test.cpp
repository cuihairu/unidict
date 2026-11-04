// 导入即索引 prove-red 回归（用户反馈：导入字典后搜索根本不能用）：
// add_dictionary 返回后不调用 build_index，所有查询面立即可用——
// 前缀/模糊/通配/全文/三层降级。修前复现：前缀层静默空手（trie 依赖
// 调用方显式 build_index，gui 重启还原态路径 if(added) 分支不补建）。
//
// 修复口径：
//   1. add_word 增量维护前缀 trie（core/std/index_engine_std.cpp）——
//      导入即索引，trie 与词表同生死（remove_word 同步摘除）；
//   2. 查询面自愈兜底（load_index 整表替换置脏后 prefix_search /
//      search_all 层 1 补建，search_grouped/suggest 同款）。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path dir = fs::current_path() / "build-local" / "import_immediate_search";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"t\",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) o << ",";
        o << "{\"word\":\"" << entries[i].first << "\",\"definition\":\"" << entries[i].second << "\"}";
    }
    o << "]}";
    return p;
}

static bool contains(const std::vector<std::string>& v, const std::string& w) {
    for (const auto& s : v) if (s == w) return true;
    return false;
}

int main() {
    const auto p = write_json("imm.json", "ImmDict", {
        {"hello", "int. 你好；招呼语"},
        {"helmet", "n. 头盔"},
        {"helium", "n. 氦"},
        {"world", "n. 世界"},
    });

    // ---- RED 前提：导入后不做任何显式 build_index，直接查询 ----
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(p.string()));

        // 前缀（修前 0 命中：trie 依赖调用方补建）
        auto pref = m.prefix_search("hel", 10);
        assert(contains(pref, "hello"));
        assert(contains(pref, "helmet"));
        assert(contains(pref, "helium"));

        // 精确（词头索引，修前即可用——回归护栏）
        assert(contains(m.exact_search("hello"), "hello"));

        // 模糊 / 通配（word_index_ 面，修前即可用——回归护栏）
        assert(contains(m.fuzzy_search("helo", 10), "hello"));
        assert(contains(m.wildcard_search("hel*", 10), "helmet"));

        // 全文（修前慢：首查付全量倒排构建；warm 后即查即得）
        m.warm_fulltext_index();
        auto ft = m.full_text_search("你好", 10);
        bool ft_hit = false;
        for (const auto& e : ft) if (e.word == "hello") ft_hit = true;
        assert(ft_hit);

        // 三层降级首组即精确命中（回归护栏）
        auto g = m.search_grouped("hello");
        assert(!g.empty() && g.front().entries.front().relevance == 0);

        // search_all 层 1：词头前缀命中兜底（有装载词典，逐词回查有释义）
        auto sa = m.search_all("hel", false, true);
        assert(!sa.empty());
        bool sa_prefix_hit = false;
        for (const auto& e : sa) if (e.word == "helium") sa_prefix_hit = true;
        assert(sa_prefix_hit);
    }

    // ---- load_index 整表替换后公开 prefix_search 自愈（置脏路径） ----
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(p.string()));
        const auto idx_file = fs::current_path() / "build-local" / "import_immediate_search" / "imm.idx";
        assert(m.save_index(idx_file.string()));

        DictionaryManagerStd m2;
        assert(m2.load_index(idx_file.string()));  // 整表替换，置脏
        // 修复后查询面自愈补建；修前此断言红（trie 空 → prefix 0 命中）。
        // 注意 search_all 在纯索引 manager 上恒空（词条释义在 DictionaryStd
        // 里，历史口径如此），这里只验前缀/精确/模糊候选面
        assert(contains(m2.prefix_search("hel", 10), "hello"));
        assert(contains(m2.exact_search("hello"), "hello"));
        assert(contains(m2.fuzzy_search("helo", 10), "hello"));
    }

    // ---- remove 后 trie 同步摘除（增量维护不残留鬼影） ----
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(p.string()));
        assert(m.remove_dictionary("ImmDict"));
        assert(m.prefix_search("hel", 10).empty());
        assert(m.prefix_search("world", 10).empty());
    }

    std::cout << "import_immediate_search_std_test: all assertions passed\n";
    return 0;
}
