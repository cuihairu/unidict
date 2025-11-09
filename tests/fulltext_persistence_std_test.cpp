#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const std::string& name, const std::vector<std::pair<std::string,std::string>>& entries) {
    fs::path p = fs::current_path()/"build-local"/(name+".json");
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc);
    out << "{\n  \"name\": \""<<name<<"\",\n  \"entries\": [\n";
    for (size_t i=0;i<entries.size();++i) {
        out << "    {\"word\":\""<<entries[i].first<<"\",\"definition\":\""<<entries[i].second<<"\"}";
        if (i+1<entries.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
    return p;
}

int main() {
    auto p = write_json("ft_persist", {{"hello","A greeting and goodwill."},{"mouse","A small rodent and device."}});
    fs::path idxp = fs::current_path()/"build-local"/"ft.index";

    // Build and save
    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(p.string()));
    auto r1 = mgr.full_text_search("greet", 10);
    assert(!r1.empty());
    bool ok = mgr.save_fulltext_index(idxp.string());
    assert(ok);

    // Reload to new manager (with same dicts) and load index
    DictionaryManagerStd mgr2;
    assert(mgr2.add_dictionary(p.string()));
    ok = mgr2.load_fulltext_index(idxp.string());
    assert(ok);
    auto r2 = mgr2.full_text_search("device", 10);
    assert(!r2.empty());
    return 0;
}

