#include <cassert>
#include <iostream>
#include "std/index_engine_std.h"

using UnidictCoreStd::IndexEngineStd;

int main() {
    IndexEngineStd idx;
    idx.add_word("hello", "dict1");
    idx.add_word("hell", "dict1");
    idx.add_word("world", "dict2");
    idx.build_index();

    auto p = idx.prefix_search("he", 10);
    assert(!p.empty());

    auto fz = idx.fuzzy_search("hellp", 10);
    bool has_hello = false; for (auto& s : fz) if (s == "hello") has_hello = true;
    assert(has_hello);

    auto wc = idx.wildcard_search("he*o", 10);
    bool has_hello_wc = false; for (auto& s : wc) if (s == "hello") has_hello_wc = true;
    assert(has_hello_wc);

    auto rx = idx.regex_search("^a.*a$", 10);
    assert(rx.empty());

    auto d = idx.dictionaries_for_word("hello");
    bool found_dict1 = false; for (auto& s : d) if (s == "dict1") found_dict1 = true;
    assert(found_dict1);

    // ---- norm profile：Options 进引擎，切换即按 dict_ 原始词重建（P1） ----
    {
        using UnidictCoreStd::TextNorm::Options;
        using UnidictCoreStd::TextNorm::Profile;
        using UnidictCoreStd::TextNorm::options_for_profile;

        IndexEngineStd e;
        e.add_word("École", "dictA");  // 重音词两形分属两词典
        e.add_word("école", "dictB");
        e.add_word("hello", "dictA");
        e.build_index();
        // 默认全折：两形一个键，代表词 = 先登记者，词典归属合并
        assert(e.word_count() == 2);
        auto hit = e.exact_match("école");
        assert(hit.size() == 1 && hit[0] == "École");
        auto both = e.dictionaries_for_word("ÉCOLE");
        assert(both.size() == 2);

        // Exact：原样键，两形独立、词典归属随重建保持
        e.set_norm_options(options_for_profile(Profile::Exact));
        assert(e.word_count() == 3);
        auto a = e.exact_match("École"), b = e.exact_match("école");
        assert(a.size() == 1 && a[0] == "École");
        assert(b.size() == 1 && b[0] == "école");
        auto da = e.dictionaries_for_word("École");
        assert(da.size() == 1 && da[0] == "dictA");

        // 切回宽容：重建回折叠键（代表词与遍历序相关，不钉具体形）
        e.set_norm_options(options_for_profile(Profile::Tolerant));
        assert(e.word_count() == 2);
        auto h2 = e.exact_match("ÉCOLE");
        assert(h2.size() == 1 && (h2[0] == "École" || h2[0] == "école"));

        // 保守档：É 折小写但不折重音，é 与 e 分键
        IndexEngineStd c;
        c.add_word("école", "d1");
        c.add_word("ecole", "d2");
        c.set_norm_options(options_for_profile(Profile::Conservative));
        assert(c.word_count() == 2);
        auto x = c.exact_match("École"), y = c.exact_match("ecole");
        assert(x.size() == 1 && x[0] == "école");
        assert(y.size() == 1 && y[0] == "ecole");
    }

    std::cout << "OK\n";
    return 0;
}
