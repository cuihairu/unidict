#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "std/data_store_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

int main() {
    fs::path p = fs::current_path()/"build-local"/"ds_escape.json";
    fs::create_directories(p.parent_path());
    DataStoreStd ds;
    ds.set_storage_path(p.string());

    ds.clear_history();
    ds.clear_vocabulary();
    ds.add_search_history("path \\with backslash, and comma");
    ds.add_search_history("newline\\nnot real newline");
    auto h = ds.get_search_history(10);
    assert(h.size() >= 2);

    ds.add_vocabulary_item({"foo,bar", "def with,comma and \\backslash"});
    ds.add_vocabulary_item({"baz", "multi line \\n+ not actually newline"});

    fs::path csv = fs::current_path()/"build-local"/"ds_export.csv";
    bool ok = ds.export_vocabulary_csv(csv.string());
    assert(ok);
    // Basic sanity: first line is header
    std::ifstream in(csv, std::ios::binary);
    std::string line; std::getline(in, line);
    assert(line == "word,definition");
    std::cout << "OK\n";
    return 0;
}

