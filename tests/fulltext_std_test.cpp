#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

namespace fs = std::filesystem;
using namespace UnidictCoreStd;

static fs::path write_test_json() {
    fs::path p = fs::current_path() / "build-local" / "test_fulltext.json";
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << "{\n"
        << "  \"name\": \"FT Test\",\n"
        << "  \"description\": \"Fulltext test data\",\n"
        << "  \"entries\": [\n"
        << "    {\"word\":\"hello\",\"definition\":\"A greeting and expression of goodwill.\"},\n"
        << "    {\"word\":\"mouse\",\"definition\":\"A small rodent; also a computer input device.\"}\n"
        << "  ]\n"
        << "}\n";
    return p;
}

int main() {
    auto json_path = write_test_json();
    DictionaryManagerStd mgr;
    bool ok = mgr.add_dictionary(json_path.string());
    assert(ok);
    mgr.build_index();

    // Search substring in definition
    auto r1 = mgr.full_text_search("greet", 10);
    assert(!r1.empty());
    bool found_hello = false;
    for (auto& e : r1) if (e.word == "hello") found_hello = true;
    assert(found_hello);

    auto r2 = mgr.full_text_search("device", 10);
    assert(!r2.empty());
    bool found_mouse = false;
    for (auto& e : r2) if (e.word == "mouse") found_mouse = true;
    assert(found_mouse);

    std::cout << "ok\n";
    return 0;
}

