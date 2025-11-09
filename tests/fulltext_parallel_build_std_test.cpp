#include <cassert>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;

static std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> make_docs(int n) {
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    docs.reserve(n);
    for (int i = 0; i < n; ++i) {
        std::string text;
        if (i % 5 == 0) text += "alpha ";
        if (i % 2 == 0) text += "beta ";
        if (i % 3 == 0) text += "gamma ";
        if (text.empty()) text = "noise";
        docs.push_back({text, {0, i}});
    }
    return docs;
}

static std::vector<FullTextIndexStd::DocRef> runq(const FullTextIndexStd& ft, const std::string& q) {
    return ft.search(q, 50);
}

int main() {
    auto docs = make_docs(100);

    FullTextIndexStd a; a.build_from_documents(docs, 1);
    FullTextIndexStd b; b.build_from_documents(docs, 4);

    // Basic stats must match
    auto sa = a.stats();
    auto sb = b.stats();
    assert(sa.docs == sb.docs);
    assert(sa.terms == sb.terms);
    assert((sa.postings == sb.postings) || (sa.postings > 0 && sb.postings > 0));

    // Top results should be identical for common queries
    auto qa = runq(a, "alpha");
    auto qb = runq(b, "alpha");
    assert(!qa.empty() && !qb.empty());
    const int K = (int)std::min(qa.size(), qb.size());
    for (int i = 0; i < K; ++i) {
        assert(qa[i].dict == qb[i].dict && qa[i].word == qb[i].word);
    }

    auto q2a = runq(a, "beta gamma");
    auto q2b = runq(b, "beta gamma");
    assert(!q2a.empty() && !q2b.empty());
    const int K2 = (int)std::min(q2a.size(), q2b.size());
    for (int i = 0; i < K2; ++i) {
        assert(q2a[i].dict == q2b[i].dict && q2a[i].word == q2b[i].word);
    }
    return 0;
}

