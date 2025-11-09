#include <cassert>
#include <filesystem>
#include <iostream>
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;

int main() {
    namespace fs = std::filesystem;
    auto p = (fs::current_path() / "build-local" / "unidict_std_test.json").string();
    fs::create_directories(fs::path(p).parent_path());
    DataStoreStd ds;
    ds.set_storage_path(p);
    ds.clear_history();
    ds.add_search_history("hello");
    ds.add_search_history("world");
    ds.add_search_history("hello");
    auto h = ds.get_search_history(10);
    assert(h.size() == 2);
    assert(h[0] == "world" && h[1] == "hello");

    ds.clear_vocabulary();
    ds.add_vocabulary_item({"foo", "bar"});
    auto v = ds.get_vocabulary();
    bool found = false; for (auto& it : v) if (it.word == "foo" && it.definition == "bar") found = true;
    assert(found);

    std::cout << "OK\n";
    return 0;
}
