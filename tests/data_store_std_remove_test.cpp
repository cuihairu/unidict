#include <cassert>
#include <filesystem>
#include <string>
#include "std/data_store_std.h"

int main() {
    namespace fs = std::filesystem;
    using namespace UnidictCoreStd;
    fs::path p = fs::current_path() / "build-local" / "unidict_std_remove_test.json";
    fs::create_directories(p.parent_path());

    DataStoreStd ds;
    ds.set_storage_path(p.string());
    ds.clear_vocabulary();

    // Add and upsert same word (case-insensitive)
    ds.add_vocabulary_item({"Apple", "A fruit"});
    ds.add_vocabulary_item({"apple", "A tasty fruit"}); // should update not duplicate
    auto v = ds.get_vocabulary();
    assert(v.size() == 1);
    assert(v[0].word == "Apple" || v[0].word == "apple");
    assert(v[0].definition == "A tasty fruit");

    // Remove by word (case-insensitive)
    ds.remove_vocabulary_item("APPLE");
    v = ds.get_vocabulary();
    assert(v.empty());
    return 0;
}

