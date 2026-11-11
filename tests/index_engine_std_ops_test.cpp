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

    // Fuzzy rank frequency weighting (UNI-FUZZY-RANK-001)：距离同分按词频
    // 降序、同频回退字典序。纯字典序把高频词（实大词典下 apple 之于
    // aple 排 38 位）挤出 CLI top-5 与强候选窗口（12）
    {
        IndexEngineStd f;
        for (int i = 0; i < 9; ++i) f.add_word("apple", "d1");   // freq 9
        f.add_word("apla", "d1");                                // freq 1
        f.add_word("aplh", "d1");                                // freq 1
        f.build_index();
        const auto r = f.fuzzy_search("aple", 10, 0);
        // 三者距 aple 均 1；旧序（距离+字典序）= apla, aplh, apple
        assert(r.size() == 3);
        assert(r[0] == "apple");                    // 高频词置顶
        assert(r[1] == "apla" && r[2] == "aplh");   // 同频兜底仍按字典序
    }
    std::cout << "OK\n";
    return 0;
}

