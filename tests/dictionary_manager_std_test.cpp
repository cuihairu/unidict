#include <cassert>
#include <string>
#include "std/dictionary_manager_std.h"

int main() {
    UnidictCoreStd::DictionaryManagerStd mgr;
    bool ok = false;
    const char* candidates[] = {"examples/dict.json","../examples/dict.json","../../examples/dict.json"};
    for (auto p : candidates) { if (mgr.add_dictionary(p)) { ok = true; break; } }
    assert(ok);
    mgr.build_index();
    auto pref = mgr.prefix_search("he", 10);
    bool has_hello = false; for (auto& s : pref) if (s == "hello") has_hello = true;
    assert(has_hello);
    auto ex = mgr.exact_search("hello");
    assert(!ex.empty() && ex.front() == std::string("hello"));
    auto def = mgr.search_word("hello");
    assert(!def.empty());
    return 0;
}
