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
    // Also sanity-check dictionaries_for_word
    mgr.build_index();
    auto dicts = mgr.dictionaries_for_word("hello");
    bool has = false; for (auto& s : dicts) if (s == "MetaDict") has = true; assert(has);
    return 0;
}

