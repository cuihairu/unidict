#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

int main() {
    DictionaryManagerStd mgr;
    // Use example JSON dict
    bool ok = false;
    const char* cands[] = {"examples/dict.json","../examples/dict.json","../../examples/dict.json"};
    for (auto p : cands) if (mgr.add_dictionary(p)) { ok = true; break; }
    assert(ok);

    // Save full-text index
    fs::path out = fs::current_path()/"build-local"/"udft3.index";
    fs::create_directories(out.parent_path());
    ok = mgr.save_fulltext_index(out.string());
    assert(ok);
    // Check magic header
    std::ifstream in(out, std::ios::binary);
    assert((bool)in);
    char magic[5]; in.read(magic, 5);
    assert(in.gcount() == 5);
    assert(std::string(magic, 5) == std::string("UDFT3", 5));

    // Load back and run a query
    DictionaryManagerStd mgr2; ok = false;
    for (auto p : cands) if (mgr2.add_dictionary(p)) { ok = true; break; }
    assert(ok);
    ok = mgr2.load_fulltext_index(out.string());
    assert(ok);
    auto r = mgr2.full_text_search("greeting", 10);
    assert(!r.empty());
    return 0;
}

