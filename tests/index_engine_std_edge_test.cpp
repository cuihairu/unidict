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

