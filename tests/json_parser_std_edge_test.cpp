#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "std/json_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const std::string& content, const std::string& name) {
    fs::path p = fs::current_path()/"build-local"/name;
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc); out << content;
    return p;
}

int main() {
    // Minimal valid entries without name/description
    auto p1 = write_json("{\n  \"entries\": [ {\"word\":\"a\",\"definition\":\"x\"} ]\n}\n", "jp_edge1.json");
    JsonParserStd jp1; bool ok = jp1.load_dictionary(p1.string()); assert(ok);
    assert(jp1.word_count() == 1);
    assert(jp1.lookup("a") == std::string("x"));

    // Whitespace and extra fields should be ignored
    auto p2 = write_json("{\n  \"name\": \"N\", \n  \"description\": \"D\",\n  \"entries\": [ \n    { \n      \"word\": \"term\" , \n      \"definition\": \"def\", \n      \"extra\": 123 \n    } \n  ]\n}\n", "jp_edge2.json");
    JsonParserStd jp2; ok = jp2.load_dictionary(p2.string()); assert(ok);
    assert(jp2.word_count() == 1);
    assert(jp2.lookup("term") == std::string("def"));
    auto sim = jp2.find_similar("te", 10); assert(!sim.empty());

    std::cout << "OK\n";
    return 0;
}

