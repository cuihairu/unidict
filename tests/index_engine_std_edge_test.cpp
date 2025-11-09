#include <cassert>
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

    std::cout << "OK\n";
    return 0;
}

