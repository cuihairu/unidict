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

    // ===== CJK 分词（tokenize v2）：文档侧 unigram+bigram、查询侧多字只发 bigram =====
    // 字面量全用字节转义（UTF-8），避开 MSVC 源码页对中日字面量的歧义。
    // 你=E4 BD A0 好=E5 A5 BD ；(U+FF1B)=EF BC 9B 招=E6 8B 9B 呼=E5 91 BC
    // 语=E8 AF AD 真=E7 9C 9F 棒=E6 A3 92 不=E4 B8 8D 周=E5 91 A8
    {
        FullTextIndexStd ft2;
        // d0：「你好；招呼语」——全角分号切段（标点分隔臂），两段 run
        ft2.add_document("int. \xE4\xBD\xA0\xE5\xA5\xBD\xEF\xBC\x9B\xE6\x8B\x9B\xE5\x91\xBC\xE8\xAF\xAD", {0, 0});
        // d1：「你真棒」——只含单字「你」，多字查询的精度对照（不应被「你好」召回）
        ft2.add_document("\xE4\xBD\xA0\xE7\x9C\x9F\xE6\xA3\x92", {0, 1});
        // d2：「招呼不周」——bigram 语义召回（与 d0 靠「招呼」共现）
        ft2.add_document("\xE6\x8B\x9B\xE5\x91\xBC\xE4\xB8\x8D\xE5\x91\xA8", {0, 2});
        ft2.finalize();
        // 多字查询只发 bigram：「你好」→ 只命中 d0；d1 含单字「你」不被召回
        auto q1 = ft2.search("\xE4\xBD\xA0\xE5\xA5\xBD", 10);
        assert(q1.size() == 1 && q1[0].word == 0);
        // 单字查询发 unigram：「你」→ d0 + d1
        auto q2 = ft2.search("\xE4\xBD\xA0", 10);
        assert(q2.size() == 2);
        // bigram 跨词典语义：「招呼」→ d0 + d2
        auto q3 = ft2.search("\xE6\x8B\x9B\xE5\x91\xBC", 10);
        assert(q3.size() == 2);
        // 三字查询（run≥2 臂同 bigram 口径）：「招呼语」→ d0 排第一
        auto q4 = ft2.search("\xE6\x8B\x9B\xE5\x91\xBC\xE8\xAF\xAD", 10);
        assert(!q4.empty() && q4[0].word == 0);
    }

    // ===== 分词边界：非 CJK 高字节并入词、标点分隔、非法字节当分隔符、4 字节码点 =====
    {
        FullTextIndexStd ft3;
        // café（2 字节非 CJK 高字节成词）+ emoji（4 字节非 CJK 并入词）
        ft3.add_document("caf\xC3\xA9 time \xF0\x9F\x98\x80 smile", {0, 0});
        // 坏字节序列集：\x80 续字节落单、\xC0\x80 超长编码、\xED\xA0\x80 代理区、
        // \xE4\x28 非法续字节、\xA9 落单续字节、尾部 \xE4\xBD 截断——全部当分隔符
        ft3.add_document("mix\x80" "A\xC0\x80" "B\xED\xA0\x80" "C\xE4\x28\xA9" "tail \xE4\xBD", {0, 1});
        // 𠮷 U+20BB7（4 字节 CJK 扩展 B）
        ft3.add_document("\xF0\xA0\xAD\xB7 is rare", {0, 2});
        // 「你好。全角ＡＢ ａｂ」——U+3002 句号切段（CJK 符号区臂）+ 单字 run
        // + 全角大写/小写字母并入词（is_sep_punct 的 alnum 三子句各臂）
        ft3.add_document("\xE4\xBD\xA0\xE5\xA5\xBD\xE3\x80\x82\xE5\x85\xA8\xE8\xA7\x92"
                         "\xEF\xBC\xA1\xEF\xBC\xA2 \xEF\xBD\x81\xEF\xBD\x82", {0, 3});
        ft3.finalize();
        // café 整词保留（字节级旧口径会把 é 切成 "caf" 单独成词）
        auto a = ft3.search("caf\xC3\xA9", 10);
        assert(!a.empty() && a[0].word == 0);
        // emoji 并入词后可整体查询
        auto b = ft3.search("\xF0\x9F\x98\x80", 10);
        assert(!b.empty() && b[0].word == 0);
        // 坏字节两侧断词：A/B/C/tail 各自成词可查（全部非法路径 return 行）
        for (const char* w : {"a", "b", "c", "tail", "mix"}) {
            auto r = ft3.search(w, 10);
            bool hit = false; for (auto& x : r) if (x.word == 1) hit = true;
            assert(hit);
        }
        // 扩展 B 4 字节 CJK
        auto c = ft3.search("\xF0\xA0\xAD\xB7", 10);
        assert(!c.empty() && c[0].word == 2);
        // 单字 run（查询与文档单字臂）：「全」
        auto d = ft3.search("\xE5\x85\xA8", 10);
        assert(!d.empty() && d[0].word == 3);
        // 全角大写字母并入词（不按标点切）
        auto e = ft3.search("\xEF\xBC\xA1\xEF\xBC\xA2", 10);
        assert(!e.empty() && e[0].word == 3);
        // 全角小写字母并入词（alnum 第三子句）
        auto f = ft3.search("\xEF\xBD\x81\xEF\xBD\x82", 10);
        assert(!f.empty() && f[0].word == 3);
        // 分词器版本与 fulltext 签名 TV= 联动：误 bump 会让所有 UDFT 缓存整体重建
        assert(FullTextIndexStd::kTokenizerVersion == 2);
    }

    std::cout << "OK\n";
    return 0;
}

