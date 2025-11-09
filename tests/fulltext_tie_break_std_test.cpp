#include <cassert>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;

// When two docs have identical scores for a query term, tie-breaker prefers smaller docId.
int main() {
    FullTextIndexStd ft;
    // Two docs with the same term frequency for 'banana'
    ft.add_document("banana apple", {0, 0});
    ft.add_document("banana orange", {0, 1});
    ft.finalize();
    auto r = ft.search("banana", 10);
    assert(r.size() >= 2);
    // Expect docId 0 (word index 0) to rank ahead of docId 1
    assert(r[0].word == 0);
    assert(r[1].word == 1);
    return 0;
}

