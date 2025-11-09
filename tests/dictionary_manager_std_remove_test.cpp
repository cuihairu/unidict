#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const std::string& name, const std::vector<std::pair<std::string,std::string>>& entries) {
    fs::path p = fs::current_path()/"build-local"/("dm_"+name+".json");
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc);
    out << "{\n  \"name\": \"" << name << "\",\n  \"entries\": [\n";
    for (size_t i=0;i<entries.size();++i) {
        out << "    {\"word\":\""<<entries[i].first<<"\",\"definition\":\""<<entries[i].second<<"\"}";
        if (i+1<entries.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return p;
}

int main() {
    auto p1 = write_json("A", {{"hello","def1"},{"apple","def2"}});
    auto p2 = write_json("B", {{"hello","defB"},{"banana","def3"}});

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p1.string()));
    assert(mgr.add_dictionary(p2.string()));
    mgr.build_index();

    auto ds = mgr.dictionaries_for_word("hello");
    bool hasA=false, hasB=false; for (auto& s:ds){ if(s=="A") hasA=true; if(s=="B") hasB=true; } assert(hasA&&hasB);

    // Remove dict B and verify index update
    bool removed = mgr.remove_dictionary("B"); assert(removed);
    mgr.build_index();
    ds = mgr.dictionaries_for_word("hello");
    hasA=false; hasB=false; for (auto& s:ds){ if(s=="A") hasA=true; if(s=="B") hasB=true; } assert(hasA && !hasB);

    auto metas = mgr.dictionaries_meta();
    assert(metas.size() == 1 && metas[0].name == "A" && metas[0].word_count == 2);

    std::cout << "OK\n";
    return 0;
}

