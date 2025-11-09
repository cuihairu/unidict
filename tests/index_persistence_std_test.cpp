#include <cassert>
#include <filesystem>
#include <string>
#include "std/dictionary_manager_std.h"

int main() {
    namespace fs = std::filesystem;
    fs::path idx = fs::current_path() / "build-local" / "idx_std_test.index";
    fs::create_directories(idx.parent_path());

    {
        UnidictCoreStd::DictionaryManagerStd mgr;
        bool ok = false;
        const char* cands[] = {"examples/dict.json","../examples/dict.json","../../examples/dict.json"};
        for (auto p : cands) if (mgr.add_dictionary(p)) { ok = true; break; }
        assert(ok);
        mgr.build_index();
        assert(mgr.indexed_word_count() > 0);
        mgr.save_index(idx.string());
    }

    {
        UnidictCoreStd::DictionaryManagerStd mgr;
        bool ok = mgr.load_index(idx.string());
        assert(ok);
        assert(mgr.indexed_word_count() > 0);
        auto pref = mgr.prefix_search("he", 10);
        bool has_he = false; for (auto& s : pref) if (!s.empty() && s[0]=='h' && s[1]=='e') { has_he = true; break; }
        assert(has_he);
    }
    return 0;
}

