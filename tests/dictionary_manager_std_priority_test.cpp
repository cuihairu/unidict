// DictionaryManagerStd 优先级排序 + tags/tagFilter 单测（P-3 3.2a）：
// priority 降序（同优先级保持装载序）、tags+tagFilter 只影响查询路径。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        const std::string& common_def) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr_prio";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"p\",\"entries\":["
      << "{\"word\":\"common\",\"definition\":\"" << common_def << "\"}"
      << "]}";
    return p;
}

int main() {
    // 三本词典都含 common，释义各不相同便于断言顺序
    auto a = write_json("a.json", "A", "from A");
    auto b = write_json("b.json", "B", "from B");
    auto c = write_json("c.json", "C", "from C");

    // --- T1 默认（全 0 优先级）：装载序，行为与既有兼容 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.add_dictionary(c.string()));
        assert(m.loaded_dictionaries() ==
               std::vector<std::string>({"A", "B", "C"}));
        auto all = m.search_all("common");
        assert(all.size() == 3);
        assert(all[0].definition == "from A");
        assert(all[1].definition == "from B");
        assert(all[2].definition == "from C");
    }

    // --- T2 优先级排序：B=5、C=1、A=0 → B C A；search_word 取最高 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.add_dictionary(c.string()));
        assert(m.set_dictionary_priority("B", 5));
        assert(m.set_dictionary_priority("C", 1));
        assert(m.loaded_dictionaries() ==
               std::vector<std::string>({"B", "C", "A"}));
        auto all = m.search_all("common");
        assert(all.size() == 3);
        assert(all[0].definition == "from B");
        assert(all[1].definition == "from C");
        assert(all[2].definition == "from A");
        assert(m.search_word("common") == "from B");
        // 平局稳定：A、C 同为 1 → 装载序 A 在 C 前
        assert(m.set_dictionary_priority("A", 1));
        assert(m.loaded_dictionaries() ==
               std::vector<std::string>({"B", "A", "C"}));
        // 未知名失败臂 + 查询缺省值
        assert(!m.set_dictionary_priority("nope", 3));
        assert(m.dictionary_priority("nope") == 0);
        assert(m.dictionary_priority("A") == 1);
    }

    // --- T3 tags + tagFilter：过滤只影响查询路径 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.add_dictionary(c.string()));
        m.build_index();  // 前缀降级层走 trie，需显式构建（惰性语义）
        assert(m.set_dictionary_tags("A", {"general"}));
        assert(m.set_dictionary_tags("B", {"tech", "shared"}));
        assert(m.set_dictionary_tags("nope", {"x"}) == false);
        assert((m.dictionary_tags("B") == std::vector<std::string>{"tech", "shared"}));
        assert(m.dictionary_tags("nope").empty());

        // 过滤 tech：只有 B 参与
        m.set_tag_filter({"tech"});
        assert(m.tag_filter().size() == 1);
        auto all = m.search_all("common");
        assert(all.size() == 1 && all[0].dict_name == "B");
        // 前缀降级层同样被过滤
        auto pref = m.search_all("comm", false, true);
        assert(pref.size() == 1 && pref[0].dict_name == "B");
        // 全文兜底层同样被过滤
        m.set_tag_filter({"nomatch"});
        assert(m.full_text_search("from", 12).empty());
        // 清空过滤 → 全量回归
        m.set_tag_filter({});
        assert(m.search_all("common").size() == 3);
        // 过滤不改变装载/启用列表
        assert(m.loaded_dictionaries().size() == 3);
        assert(m.enabled_dictionaries().size() == 3);
    }

    // --- T4 tagFilter 与 enabled 正交：禁用照旧挡查询、列表不受过滤 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_tags("B", {"tech"}));
        assert(m.set_dictionary_enabled("A", false));
        m.set_tag_filter({"tech"});
        auto all = m.search_all("common");
        assert(all.size() == 1 && all[0].dict_name == "B");
        // include_disabled=true 也要过标签闸
        assert(m.search_all("common", true).size() == 1);
        // 列表不受标签过滤影响
        assert(m.enabled_dictionaries().size() == 1);
    }

    // --- T5 fullText×tagFilter 正侧：过滤在倒排命中之后逐条裁决
    //（索引仍按全量已启用词典构建），摘除过滤即全量回归、无重建 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_tags("A", {"general"}));
        assert(m.set_dictionary_tags("B", {"tech"}));
        // 无过滤：两部词典的释义都含 "from"，倒排双双命中
        assert(m.full_text_search("from", 12).size() == 2);
        // 上 tech 过滤：只剩 B 的命中，且归属正确
        m.set_tag_filter({"tech"});
        auto hits = m.full_text_search("from", 12);
        assert(hits.size() == 1 && hits[0].dict_name == "B");
        // 摘除过滤：同一索引立即全量回归
        m.set_tag_filter({});
        assert(m.full_text_search("from", 12).size() == 2);
    }

    return 0;
}