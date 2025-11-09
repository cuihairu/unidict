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
    fs::path idxp = fs::current_path()/"build-local"/"ft_mismatch.index";
    auto p1 = write_json("A", {{"hello","greet"}});
    auto p2 = write_json("B", {{"world","planet"}});

    // Save index for only dict A
    DictionaryManagerStd m1; assert(m1.add_dictionary(p1.string()));
    auto r1 = m1.full_text_search("greet", 10); assert(!r1.empty());
    bool ok = m1.save_fulltext_index(idxp.string());
    assert(ok);

    // Try to load into manager with only dict B -> mismatch, expect failure
    DictionaryManagerStd m2; assert(m2.add_dictionary(p2.string()));
    ok = m2.load_fulltext_index(idxp.string());
    assert(!ok);
    return 0;
}

