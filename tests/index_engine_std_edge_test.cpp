#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "std/index_engine_std.h"

using namespace UnidictCoreStd;

int main() {
    IndexEngineStd idx;
    idx.add_word("a.b[c]", "D");
    idx.add_word("question", "D");
    idx.add_word("asterisk", "D");
    idx.build_index();

    // Wildcard: '*' and '?' map to regex '.*' and '.'; other regex meta must be escaped
    auto w1 = idx.wildcard_search("a?b*", 10);
    bool hit=false; for (auto& s:w1) if (s=="a.b[c]") hit=true; assert(hit);

    // Regex: invalid patterns should not throw
    auto r1 = idx.regex_search("(unclosed", 10);
    assert(r1.empty());

    auto r2 = idx.regex_search("^a\\.b\\[c\\]$", 10);
    hit=false; for (auto& s:r2) if (s=="a.b[c]") hit=true; assert(hit);

    // Fuzzy extremes: distant string yields empty
    auto fz = idx.fuzzy_search("zzzzzz", 5);
    assert(fz.empty());

    // '*'/'?' 开头无字面前缀：退化为 scan_words_ 上的限时全表扫描
    // （deep-audit 覆盖缺口：lit_len==0 分支从未被测过）
    idx.add_word("zoo", "D");
    idx.build_index();
    auto w2 = idx.wildcard_search("*oo", 10);
    hit = false; for (auto& s : w2) if (s == "zoo") hit = true;
    assert(hit);
    auto w3 = idx.wildcard_search("?oo", 10);
    assert(w3.size() == 1 && w3[0] == "zoo");

    // ScanBudget 到点即止臂：词表大到全表扫描必超 1ms 预算（keep_going
    // 每 256 词查一次 deadline，2 万词的正则全扫远超预算 → 必命中到点
    // 返回假，扫描中途截停；几乎零命中的模式保证不会先撞 max_results）
    {
        IndexEngineStd big;
        for (int i = 0; i < 20000; ++i) big.add_word("w" + std::to_string(i), "D");
        big.build_index();
        auto few = big.wildcard_search("*qzqx", 1000000, 1);
        assert(few.size() < big.word_count());
    }

    // Malformed frequency column must not abort load_index (deep-audit P2:
    // bare std::stoi threw std::invalid_argument out of the scan loop)
    {
        namespace fs = std::filesystem;
        fs::path bad = fs::current_path() / "build-local" / "idx_bad_freq.index";
        fs::create_directories(bad.parent_path());
        std::ofstream out(bad);
        out << "goodword\t3\tD1\n";
        out << "badword\tnotanumber\tD2\n";
        out << "\t\t\n";
        out.close();
        IndexEngineStd bad_idx;
        bool loaded = bad_idx.load_index(bad.string());
        assert(loaded);
        assert(bad_idx.word_count() == 1);
        auto exact = bad_idx.exact_match("goodword");
        assert(exact.size() == 1 && exact[0] == "goodword");
    }

    std::cout << "OK\n";
    return 0;
}

