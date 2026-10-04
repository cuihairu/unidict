// DictionaryManagerStd search_grouped 三层降级单测（P-3 3.3）：
// 契约对齐 legacy searchGrouped——层 0 词头精确 / 层 1 前缀（relevance 1）
// / 层 2 释义包含（relevance 2 + fulltext 标注），命中层即止；组按词典
// 聚拢；组内同词头折叠去重保留首条。

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr_grouped";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"g\",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) o << ",";
        o << "{\"word\":\"" << entries[i].first << "\",\"definition\":\"" << entries[i].second << "\"}";
    }
    o << "]}";
    return p;
}

static const DictionaryManagerStd::GroupedEntryStd*
find_entry(const DictionaryManagerStd::DictionaryGroupStd& g, const std::string& word) {
    for (const auto& e : g.entries) {
        if (e.word == word) return &e;
    }
    return nullptr;
}

int main() {
    auto a = write_json("a.json", "GA", {{"apple", "A def"},
                                         {"Apple", "A upper"},
                                         {"apple pie", "pie def"}});
    auto b = write_json("b.json", "GB", {{"apple", "B def"}, {"apply", "B apply"}});
    auto c = write_json("c.json", "GC", {{"Apple", "sweet greeting"}, {"APPLE", "another greeting"},
                                         {"问候", "hi"}});

    // --- T1 层 0：词头精确即止，两词典各一组 relevance 0 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        auto g = m.search_grouped("apple");
        assert(g.size() == 2);
        assert(g[0].dictionary_name == "GA" && g[1].dictionary_name == "GB");
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "apple" && g[0].entries[0].definition == "A def");
        assert(g[0].entries[0].relevance == 0 && !g[0].entries[0].fulltext);
        // 层级即止：全结果都是 relevance 0（无前缀/全文混入）
        for (const auto& grp : g) {
            for (const auto& e : grp.entries) assert(e.relevance == 0);
        }
    }

    // --- T2 层 1：前缀候选逐词回查 + trie 自愈补建（未显式 build_index）---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        auto g = m.search_grouped("app");
        assert(g.size() == 2);
        // 候选遍历序随 trie 子节点容器非确定 → 组定位按词典名，不按下标
        const DictionaryManagerStd::DictionaryGroupStd* ga = nullptr;
        const DictionaryManagerStd::DictionaryGroupStd* gb = nullptr;
        for (const auto& grp : g) {
            if (grp.dictionary_name == "GA") ga = &grp;
            if (grp.dictionary_name == "GB") gb = &grp;
        }
        assert(ga && gb);
        // GA：apple + apple pie（候选取 12 条，逐词回查命中进组）
        assert(ga->entries.size() == 2);
        const auto* pie = find_entry(*ga, "apple pie");
        assert(pie && pie->relevance == 1 && !pie->fulltext && pie->definition == "pie def");
        const auto* ap = find_entry(*ga, "apple");
        assert(ap && ap->relevance == 1);
        // GB：apple + apply
        assert(gb->entries.size() == 2);
        assert(find_entry(*gb, "apply") && find_entry(*gb, "apply")->relevance == 1);
    }

    // --- T3 层 2：释义包含兜底 + 组内折叠键去重（Apple/APPLE 同键）---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(c.string()));
        auto g = m.search_grouped("greeting");
        assert(g.size() == 1);
        assert(g[0].dictionary_name == "GC");
        assert(g[0].entries.size() == 1);  // Apple/APPLE 折叠同键，保留首条
        assert(g[0].entries[0].relevance == 2);
        assert(g[0].entries[0].fulltext);
    }

    // --- T4 查询词修剪：前后空白等价 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        auto g1 = m.search_grouped("apple");
        auto g2 = m.search_grouped("  apple \t");
        assert(g1.size() == g2.size());
        assert(g2[0].entries[0].definition == g1[0].entries[0].definition);
    }

    // --- T5 空/纯空白查询 → 空 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.search_grouped("").empty());
        assert(m.search_grouped("   ").empty());
    }

    // --- T6 禁用与标签过滤口径一致 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_enabled("GB", false));
        auto g = m.search_grouped("apple");
        assert(g.size() == 1 && g[0].dictionary_name == "GA");
        // 标签过滤：只留 GA
        assert(m.set_dictionary_tags("GA", {"keep"}));
        m.set_tag_filter({"keep"});
        assert(m.search_grouped("apple").size() == 1);
        m.set_tag_filter({"drop"});
        assert(m.search_grouped("apple").empty());
    }

    // --- T7 优先级决定组序 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_priority("GB", 5));
        auto g = m.search_grouped("apple");
        assert(g.size() == 2);
        assert(g[0].dictionary_name == "GB" && g[1].dictionary_name == "GA");
    }

    // --- T8 空定义不算命中：层 0 miss 滑层 1 ---
    {
        auto e = write_json("e.json", "GE", {{"ghost", ""}, {"ghostly", "near ghost"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(e.string()));
        auto g = m.search_grouped("ghost");
        assert(g.size() == 1);
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "ghostly" && g[0].entries[0].relevance == 1);
    }

    return 0;
}