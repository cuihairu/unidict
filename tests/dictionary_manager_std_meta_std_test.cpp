#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const std::string& name, const std::vector<std::pair<std::string,std::string>>& entries) {
    fs::path p = fs::current_path()/"build-local"/(name+"_meta.json");
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc);
    out << "{\n  \"name\": \""<<name<<"\",\n  \"description\": \"meta test\",\n  \"entries\": [\n";
    for (size_t i=0;i<entries.size();++i) {
        out << "    {\"word\":\""<<entries[i].first<<"\",\"definition\":\""<<entries[i].second<<"\"}";
        if (i+1<entries.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return p;
}

int main() {
    auto p = write_json("MetaDict", {{"hello","greet"},{"world","planet"}});
    DictionaryManagerStd mgr;
    bool ok = mgr.add_dictionary(p.string());
    assert(ok);
    auto metas = mgr.dictionaries_meta();
    assert(metas.size() == 1);
    assert(metas[0].name == std::string("MetaDict"));
    assert(metas[0].word_count == 2);
    // 词典库 UI 面（gui 迁 std）：首源路径 + 展示名映射 + 标签/启停/优先级
    assert(metas[0].file_path == p.string());
    assert(metas[0].format == std::string("JSON"));
    assert(metas[0].enabled);
    assert(metas[0].priority == 0);
    assert(metas[0].tags.empty());
    assert(mgr.set_dictionary_tags("MetaDict", {"en", "test"}));
    assert(mgr.set_dictionary_enabled("MetaDict", false));
    metas = mgr.dictionaries_meta();
    assert(metas[0].tags.size() == 2);
    assert(metas[0].tags[0] == "en" && metas[0].tags[1] == "test");
    assert(!metas[0].enabled);
    // Also sanity-check dictionaries_for_word
    mgr.build_index();
    auto dicts = mgr.dictionaries_for_word("hello");
    bool has = false; for (auto& s : dicts) if (s == "MetaDict") has = true; assert(has);

    // 索引候选标签过滤后置：prefix 按当前 participates/启停逐词核对
    //（索引内容是构建期快照，运行期切组/禁用后仍与查询面同口径）
    {
        auto p1 = write_json("FilterEn", {{"apple","fruit"},{"banana","fruit"}});
        auto p2 = write_json("FilterZh", {{"applepie","dessert"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(p1.string()));
        assert(m.add_dictionary(p2.string()));
        assert(m.set_dictionary_tags("FilterEn", {"en"}));
        assert(m.set_dictionary_tags("FilterZh", {"zh"}));
        m.build_index();
        assert(m.prefix_search("app", 10).size() == 2);
        // 切组：en 组候选被后置过滤剔除
        m.set_tag_filter({"zh"});
        const auto zh = m.prefix_search("app", 10);
        assert(zh.size() == 1 && zh[0] == "applepie");
        // 禁用：无过滤查询同样剔除该典候选（构建期已入索引，运行期禁用生效）
        m.set_tag_filter({});
        assert(m.set_dictionary_enabled("FilterEn", false));
        const auto dis = m.prefix_search("app", 10);
        assert(dis.size() == 1 && dis[0] == "applepie");
    }
    return 0;
}

