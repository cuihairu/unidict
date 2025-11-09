#include <cassert>
#include <iostream>
#include "std/json_parser_std.h"

int main() {
    UnidictCoreStd::JsonParserStd jp;
    bool ok = false;
    const char* candidates[] = {"examples/dict.json", "../examples/dict.json", "../../examples/dict.json"};
    for (const char* p : candidates) { if (jp.load_dictionary(p)) { ok = true; break; } }
    assert(ok);
    assert(jp.is_loaded());
    assert(jp.word_count() >= 3);
    auto d = jp.lookup("hello");
    assert(!d.empty());
    auto sim = jp.find_similar("he", 10);
    bool has_hello = false; for (auto& s : sim) if (s == "hello") has_hello = true; assert(has_hello);
    std::cout << "OK\n";
    return 0;
}
