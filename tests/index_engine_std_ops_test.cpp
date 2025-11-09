#include <cassert>
#include <filesystem>
#include <string>
#include <vector>
#include <iostream>

#include "std/index_engine_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

int main() {
    IndexEngineStd idx;
    // Duplicates & normalization
    idx.add_word(" Hello ", "dA");
    idx.add_word("hello", "dB");
    idx.add_word("HELLO", "dA");
    idx.add_word("world", "dB");
    idx.build_index();
    assert(idx.word_count() == 2);
    auto dicts = idx.dictionaries_for_word("hello");
    bool hasA=false, hasB=false; for (auto& s:dicts){ if(s=="dA") hasA=true; if(s=="dB") hasB=true; } assert(hasA && hasB);

    // Remove and clear
    idx.remove_word("hello", "dA");
    dicts = idx.dictionaries_for_word("hello");
    hasA=false; hasB=false; for (auto& s:dicts){ if(s=="dA") hasA=true; if(s=="dB") hasB=true; } assert(!hasA && hasB);
    idx.clear_dictionary("dB");
    assert(idx.word_count() == 0);

    // Persistence roundtrip
    idx.add_word("alpha", "D1");
    idx.add_word("beta", "D1");
    idx.build_index();
    fs::path p = fs::current_path()/"build-local"/"idx_ops_test.index";
    fs::create_directories(p.parent_path());
    bool ok = idx.save_index(p.string());
    assert(ok);
    IndexEngineStd idx2;
    ok = idx2.load_index(p.string());
    assert(ok);
    auto pref = idx2.prefix_search("a", 10);
    bool hasAlpha = false; for (auto& s:pref) if (s=="alpha") hasAlpha=true; assert(hasAlpha);
    std::cout << "OK\n";
    return 0;
}

