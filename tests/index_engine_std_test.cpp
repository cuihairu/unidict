#include <cassert>
#include <iostream>
#include "std/index_engine_std.h"

using UnidictCoreStd::IndexEngineStd;

int main() {
    IndexEngineStd idx;
    idx.add_word("hello", "dict1");
    idx.add_word("hell", "dict1");
    idx.add_word("world", "dict2");
    idx.build_index();

    auto p = idx.prefix_search("he", 10);
    assert(!p.empty());

    auto fz = idx.fuzzy_search("hellp", 10);
    bool has_hello = false; for (auto& s : fz) if (s == "hello") has_hello = true;
    assert(has_hello);

    auto wc = idx.wildcard_search("he*o", 10);
    bool has_hello_wc = false; for (auto& s : wc) if (s == "hello") has_hello_wc = true;
    assert(has_hello_wc);

    auto rx = idx.regex_search("^a.*a$", 10);
    assert(rx.empty());

    auto d = idx.dictionaries_for_word("hello");
    bool found_dict1 = false; for (auto& s : d) if (s == "dict1") found_dict1 = true;
    assert(found_dict1);

    std::cout << "OK\n";
    return 0;
}
