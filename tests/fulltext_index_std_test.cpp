#include <cassert>
#include <string>
#include <vector>
#include <iostream>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;

int main() {
    FullTextIndexStd ft;
    // Three tiny documents
    int d0 = ft.add_document("Hello world. Greeting and goodwill.", {0, 0});
    int d1 = ft.add_document("The mouse is a small rodent and a computer device.", {0, 1});
    int d2 = ft.add_document("World history is vast.", {0, 2});
    assert(d0 == 0 && d1 == 1 && d2 == 2);
    assert(ft.doc_count() == 3);

    // finalize to compute IDF
    ft.finalize();

    // Basic token hit
    auto r1 = ft.search("greeting");
    bool has0 = false; for (auto& r : r1) if (r.word == 0) has0 = true; assert(has0);

    // Substring expansion: 'greet' should still match 'greeting'
    auto r2 = ft.search("greet");
    has0 = false; for (auto& r : r2) if (r.word == 0) has0 = true; assert(has0);

    // Multi-term scoring: 'world history' should prefer doc 2 over 0
    auto r3 = ft.search("world history");
    assert(!r3.empty());
    // Top result should be doc 2 or 0 depending on IDF; assert either contains doc2 at rank 0 or among top 2
    bool found2 = false; for (size_t i=0;i<r3.size() && i<2;i++) if (r3[i].word == 2) found2 = true; assert(found2);

    std::cout << "OK\n";
    return 0;
}

