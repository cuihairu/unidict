#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const fs::path& p, const std::string& name, const std::vector<std::pair<std::string,std::string>>& entries) {
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
    fs::path p1 = fs::current_path()/"build-local"/"sig_same1.json";
    fs::path p2 = fs::current_path()/"build-local"/"sig_same2.json";
    // Same content, different paths
    write_json(p1, "Same", {{"hello","greet"}});
    write_json(p2, "Same", {{"hello","greet"}});
    fs::path idxp = fs::current_path()/"build-local"/"ft_sig_path.index";

    DictionaryManagerStd m1; assert(m1.add_dictionary(p1.string()));
    auto r1 = m1.full_text_search("greet", 10); assert(!r1.empty());
    assert(m1.save_fulltext_index(idxp.string()));

    DictionaryManagerStd m2; assert(m2.add_dictionary(p2.string()));
    // Signature should differ because of path metadata
    bool ok = m2.load_fulltext_index(idxp.string());
    assert(!ok);
    return 0;
}

