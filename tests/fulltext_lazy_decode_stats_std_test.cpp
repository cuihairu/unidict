#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

// Validate UDFT3 lazy-decompression and stats counters.
// 1) Build small index in-memory and save (UDFT3).
// 2) Load back and check that terms are stored compressed (stats).
// 3) Run a query to trigger on-demand decompression for the hit term.
// 4) Verify stats reflect a decrease in compressed_terms and an increase in pairs_decompressed.

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path out_path() {
    fs::path p = fs::current_path()/"build-local"/"ft_lazy.index";
    fs::create_directories(p.parent_path());
    return p;
}

int main() {
    // Build tiny corpus and save as UDFT3
    {
        FullTextIndexStd ft;
        ft.add_document("Hello world greeting.", {0,0});
        ft.add_document("Another greeting appears here.", {0,1});
        ft.finalize();
        bool ok = ft.save(out_path().string());
        assert(ok);
    }

    // Load back and inspect stats pre-search (should be fully compressed)
    FullTextIndexStd ft2;
    bool ok = ft2.load(out_path().string());
    assert(ok);
    assert(ft2.version() == 3);
    auto s1 = ft2.stats();
    assert(s1.terms > 0);
    assert(s1.docs == 2);
    assert(s1.compressed_terms == s1.terms);
    // Should have some compressed bytes > 0 and no decompressed pairs yet
    assert(s1.compressed_bytes > 0);
    assert(s1.pairs_decompressed == 0);

    // A search for a term present in postings should force on-demand decompress for that term
    auto hits = ft2.search("greeting", 10);
    assert(!hits.empty());
    auto s2 = ft2.stats();
    assert(s2.terms == s1.terms);
    assert(s2.docs == s1.docs);
    // At least one term got decompressed
    assert(s2.compressed_terms < s1.compressed_terms);
    assert(s2.pairs_decompressed > 0);
    return 0;
}

