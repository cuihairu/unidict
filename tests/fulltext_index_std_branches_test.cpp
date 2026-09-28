// fulltext_index_std 分支缺口补测（真实缺边 83 条，全库最大簇）。
//
// 缺边构成：持久化半边的截断矩阵（UDFT3 逐字段截断、UDFT1 posting 截断、
// 手写坏 varint）从未跑热；v1/v2 未压缩臂只有单句快乐路径；add_document
// 不 finalize 就 search 的 idf 兜底、查询去重/候选共享扩展、线程数
// 兜底/钳制臂、max_results<=0、save/load 的打开失败、幽灵词（n=0）
// 回存、空词项、词内非词字符的 gram 归档跳过臂、候选上限 256 截断。
// 本文件全部真实输入驱动（手改索引文件属于格式解析器的真实输入面）：
//   T1 tokenize 词字符臂（'_'/'-' 成词）
//   T2 build_from_documents 线程兜底/钳制 + max_results<=0
//   T3 查询去重、未 finalize 的 idf 兜底、候选共享扩展去重
//   T4 save/load 打开失败、坏 magic、签名往返
//   T5 UDFT3 逐字段截断矩阵 + UDFT1/2 完整与截断
//   T6 手写语义文件：坏 varint 三态、幽灵词回存、空词项、非词字符 gram
//   T7 候选扩展：ngram3 找最稀桶 + 全词不含 q 的 miss、ngram2/单字
//      路径、词表全单字时的索引空臂、256 上限截断、多字节 varint 往返

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path fpath(const char* tag) {
    fs::path p = fs::current_path() / "build-local" / ("ftbr_" + std::string(tag) + ".udft");
    fs::create_directories(p.parent_path());
    return p;
}

static void append_u32(std::string& b, uint32_t v) {
    b.push_back((char)(v & 0xFF));
    b.push_back((char)((v >> 8) & 0xFF));
    b.push_back((char)((v >> 16) & 0xFF));
    b.push_back((char)((v >> 24) & 0xFF));
}

static void write_bytes(const fs::path& p, const std::string& b) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(b.data(), (std::streamsize)b.size());
    assert(out.good());
}

// n 份 "alpha beta" 文档，docId 递增
static std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> mk_docs(int n) {
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    for (int i = 0; i < n; ++i) docs.push_back({"alpha beta", {0, i}});
    return docs;
}

// 完整 UDFT3：1 doc、1 词 "abc"、1 条压缩 posting（delta=1, tf=1）
static std::string udft3_body() {
    std::string b;
    b.append("UDFT3", 5);
    append_u32(b, 0);                       // siglen=0
    append_u32(b, 1);                       // docs
    append_u32(b, 0); append_u32(b, 0);     // docmap (0,0)
    append_u32(b, 1);                       // terms
    append_u32(b, 3); b.append("abc");      // term
    append_u32(b, 1);                       // n
    append_u32(b, 2);                       // blen
    b.append(std::string("\x00\x01", 2));   // delta=0（i==0 时 delta 即 docId）, tf=1
    return b;
}

// T1：'_'/'-' 成词（is_word_char 的真臂），'.' 切词（假臂）
static void test_tokenize_word_chars() {
    FullTextIndexStd ft;
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    docs.push_back({"hello_world foo-bar baz.qux", {0, 0}});
    ft.build_from_documents(docs, 1);
    auto r = ft.search("hello_world foo-bar", 10);
    assert(!r.empty() && r[0].word == 0);
    auto r2 = ft.search("baz qux", 10);
    assert(!r2.empty());
}

// T2：线程数兜底（<=0 → hardware_concurrency / 1）、超文档数钳制、
// 空文档集、max_results<=0
static void test_build_threads_and_limits() {
    {   // threads=0 → hardware_concurrency 兜底
        FullTextIndexStd ft;
        ft.build_from_documents(mk_docs(4), 0);
        assert(ft.doc_count() == 4);
        assert(ft.search("alpha", 10).size() == 4);
    }
    {   // threads 远超文档数 → 钳到 N
        FullTextIndexStd ft;
        ft.build_from_documents(mk_docs(2), 64);
        assert(ft.doc_count() == 2);
        assert(ft.search("beta", 10).size() == 2);
    }
    {   // 负线程数 → 同兜底臂
        FullTextIndexStd ft;
        ft.build_from_documents(mk_docs(3), -7);
        assert(ft.doc_count() == 3);
    }
    {   // 空文档集：finalize N=0 早退，检索空
        FullTextIndexStd ft;
        ft.build_from_documents({}, 1);
        assert(ft.doc_count() == 0);
        assert(ft.search("alpha", 10).empty());
    }
    {   // max_results<=0 → 空
        FullTextIndexStd ft;
        ft.build_from_documents(mk_docs(4), 1);
        assert(ft.search("alpha", 0).empty());
        assert(ft.search("alpha", -3).empty());
    }
}

// T3：查询词去重；add_document 后未 finalize（idf_ 空 → 1.0 兜底）；
// 两个查询词共享同一候选扩展（used_terms 去重）
static void test_search_dedup_and_unfinalized() {
    {   // 重复查询词：第二个走 seen_query_terms 去重 continue
        FullTextIndexStd ft;
        ft.add_document("alpha beta", {0, 0});
        ft.finalize();
        auto r = ft.search("alpha alpha", 10);
        assert(r.size() == 1);
    }
    {   // 未 finalize：postings_ 有词、idf_ 空 → idf 兜底 1.0 仍可检索
        FullTextIndexStd ft;
        ft.add_document("hello", {7, 9});
        auto r = ft.search("hello", 10);
        assert(r.size() == 1 && r[0].dict == 7 && r[0].word == 9);
    }
    {   // "abc" 精确 miss → ngram3 扩展出 "abcde"，与第一个查询词
        // "abcde" 的精确命中共享词条 → used_terms 去重 continue
        FullTextIndexStd ft;
        ft.add_document("abcde", {2, 2});
        ft.finalize();
        auto r = ft.search("abcde abc", 10);
        assert(r.size() == 1 && r[0].word == 2);
    }
}

// T4：save 打不开目标（目录）、load 不存在/空文件/坏 magic、签名往返
static void test_save_load_guards() {
    auto dir = fs::current_path() / "build-local" / "ftbr_dir";
    fs::create_directories(dir);
    FullTextIndexStd ft;
    ft.add_document("alpha", {0, 0});
    ft.finalize();
    assert(!ft.save(dir.string()));          // 目录打不开 → 假
    fs::remove_all(dir);

    FullTextIndexStd back;
    assert(!back.load(fpath("no_such").string()));
    assert(back.last_error() == "open failed");

    write_bytes(fpath("empty"), "");
    assert(!back.load(fpath("empty").string()));   // magic 都读不到

    write_bytes(fpath("badmagic"), "XXXXX");
    assert(!back.load(fpath("badmagic").string()));
    assert(back.last_error() == "unsupported format");

    // 签名非空 → siglen>0 臂 + 读回
    ft.set_signature("sig-xyz");
    assert(ft.save(fpath("sig").string()));
    FullTextIndexStd back2;
    assert(back2.load(fpath("sig").string()));
    assert(back2.version() == 3);
    assert(back2.signature() == "sig-xyz");
}

// T5：UDFT3 逐字段截断矩阵——每个 cut 停在读下一个字段的半截处，
// 对应 load 里各 read_u32/read 守卫的失败臂；UDFT1/2 完整与截断
static void test_truncation_matrix() {
    const std::string body = udft3_body();
    // 字段边界（偏移）：magic 0-4 | siglen 5-8 | docs 9-12 | docmap 13-20 |
    // terms 21-24 | len 25-28 | term 29-31 | n 32-35 | blen 36-39 | buf 40-41
    const size_t cuts[] = {5, 8, 12, 16, 20, 24, 28, 31, 35, 39,
                            40, 41};  // blen 完整、buf 缺/读半截 → compressed data 截断
    for (size_t cut : cuts) {
        write_bytes(fpath("cut"), body.substr(0, cut));
        FullTextIndexStd ft;
        assert(!ft.load(fpath("cut").string()));
    }
    // blen 读到但 buf 数据不齐：错误位落在 truncated (compressed data)
    {
        FullTextIndexStd ft;
        assert(!ft.load(fpath("cut").string()));  // cut=41（上一轮末值）
        assert(ft.last_error() == "truncated (compressed data)");
    }
    // siglen>0 但签名字节被截断 → truncated (sig) 臂
    {
        std::string b;
        b.append("UDFT3", 5);
        append_u32(b, 4); b.append("ab", 2);   // 声称 4 字节，只给 2
        write_bytes(fpath("sigcut"), b);
        FullTextIndexStd ft;
        assert(!ft.load(fpath("sigcut").string()));
        assert(ft.last_error() == "truncated (sig)");
    }
    // 完整文件往返：压缩 posting 懒解码
    write_bytes(fpath("cut"), body);
    FullTextIndexStd ok;
    assert(ok.load(fpath("cut").string()));
    assert(ok.version() == 3);
    auto r = ok.search("abc", 10);
    assert(r.size() == 1 && r[0].word == 0);

    // UDFT1：posting 表截断（docId 读一半）→ 失败；完整 → 未压缩臂检索
    {
        std::string b;
        b.append("UDFT1", 5);
        append_u32(b, 1);                       // docs
        append_u32(b, 0); append_u32(b, 0);     // docmap
        append_u32(b, 1);                       // terms
        append_u32(b, 2); b.append("xy");       // term
        append_u32(b, 1);                       // n
        write_bytes(fpath("u1cut"), b);         // docId/tf 缺失
        FullTextIndexStd ft;
        assert(!ft.load(fpath("u1cut").string()));

        append_u32(b, 0);                       // docId 完整、tf 缺 →
                                                // || 右操作数失败臂
        write_bytes(fpath("u1cut"), b);
        FullTextIndexStd ftTf;
        assert(!ftTf.load(fpath("u1cut").string()));
        assert(ftTf.last_error() == "truncated (posting)");

        append_u32(b, 3);                       // 补上 tf=3 → 完整
        write_bytes(fpath("u1cut"), b);
        FullTextIndexStd ft2;
        assert(ft2.load(fpath("u1cut").string()));
        assert(ft2.version() == 1);
        assert(ft2.search("xy", 10).size() == 1);
    }
    // UDFT2：带签名的未压缩格式
    {
        std::string b;
        b.append("UDFT2", 5);
        append_u32(b, 5); b.append("hello", 5); // 签名
        append_u32(b, 1);
        append_u32(b, 0); append_u32(b, 0);
        append_u32(b, 1);
        append_u32(b, 2); b.append("xy");
        append_u32(b, 1);
        append_u32(b, 0); append_u32(b, 1);
        write_bytes(fpath("u2"), b);
        FullTextIndexStd ft;
        assert(ft.load(fpath("u2").string()));
        assert(ft.version() == 2);
        assert(ft.signature() == "hello");
        assert(ft.search("xy", 10).size() == 1);
    }
}

// T6：手写语义文件——坏 varint 三态、幽灵词（n=0）回存、空词项、
// 词内非词字符的 gram 归档跳过臂
static void test_crafted_semantic_files() {
    // a) 坏 varint：cora=7 个连续续字节（第 7 个读入时 shift=42 越
    //    max_shift=35 上限臂）、corb=单续字节到尾（p>=end 臂）、
    //    corc=delta 正常 tf 越界（|| 右操作数失败臂）
    //    → 懒解码 break，检索空、不崩
    {
        std::string b;
        b.append("UDFT3", 5);
        append_u32(b, 0);
        append_u32(b, 1); append_u32(b, 0); append_u32(b, 0);  // 1 doc
        append_u32(b, 3);                                       // 3 terms
        for (int k = 0; k < 3; ++k) {
            std::string name = (k == 0) ? "cora" : (k == 1) ? "corb" : "corc";
            append_u32(b, (uint32_t)name.size()); b.append(name);
            append_u32(b, 1);                    // n=1
            if (k == 0) {
                append_u32(b, 7);
                b.append(std::string("\xff\xff\xff\xff\xff\xff\xff", 7));
            } else if (k == 1) {
                append_u32(b, 1); b.append("\xff");
            } else {
                append_u32(b, 1); b.append("\x05");
            }
        }
        write_bytes(fpath("corrupt"), b);
        FullTextIndexStd ft;
        assert(ft.load(fpath("corrupt").string()));
        auto s = ft.stats();
        assert(s.compressed_terms == 3 && s.compressed_bytes == 9);   // 懒解码前
        assert(ft.search("cora", 10).empty());
        assert(ft.search("corb", 10).empty());
        assert(ft.search("corc", 10).empty());
        auto s2 = ft.stats();
        assert(s2.compressed_terms == 0);   // 三个词都已按需解码（失败即空）
    }
    // b) 幽灵词 n=0 blen=0：载入后检索空；回存时 buf 为空走写省略臂
    {
        std::string b;
        b.append("UDFT3", 5);
        append_u32(b, 0);
        append_u32(b, 1); append_u32(b, 0); append_u32(b, 0);
        append_u32(b, 1);
        append_u32(b, 5); b.append("ghost");
        append_u32(b, 0);   // n=0
        append_u32(b, 0);   // blen=0
        write_bytes(fpath("ghost"), b);
        FullTextIndexStd ft;
        assert(ft.load(fpath("ghost").string()));
        assert(ft.search("ghost", 10).empty());
        assert(ft.save(fpath("ghost2").string()));   // 空 buf 不写内容
        FullTextIndexStd ft2;
        assert(ft2.load(fpath("ghost2").string()));
        assert(ft2.search("ghost", 10).empty());
    }
    // c) 只有一个空词项：三个候选索引全空 → 三条快速路的"索引空"臂
    {
        std::string b;
        b.append("UDFT3", 5);
        append_u32(b, 0);
        append_u32(b, 1); append_u32(b, 0); append_u32(b, 0);
        append_u32(b, 1);
        append_u32(b, 0);   // 空词名
        append_u32(b, 0);
        append_u32(b, 0);
        write_bytes(fpath("emptyterm"), b);
        FullTextIndexStd ft;
        assert(ft.load(fpath("emptyterm").string()));
        assert(ft.search("z", 10).empty());
        assert(ft.search("zz", 10).empty());
        assert(ft.search("zzz", 10).empty());
    }
    // d) 词内非词字符（'.'）：3-gram/2-gram/单字归档的逐位置跳过臂
    //    （词表来自手改文件，tokenize 产不出这种词）
    {
        std::string b;
        b.append("UDFT3", 5);
        append_u32(b, 0);
        append_u32(b, 1); append_u32(b, 0); append_u32(b, 0);
        append_u32(b, 3);
        const char* names[3] = {".ab", "a.b", "ab."};
        for (int k = 0; k < 3; ++k) {
            append_u32(b, 3); b.append(names[k]);
            append_u32(b, 1);
            append_u32(b, 2); b.append(std::string("\x00\x01", 2));
        }
        write_bytes(fpath("dots"), b);
        FullTextIndexStd ft;
        assert(ft.load(fpath("dots").string()));
        // "ab" 经 2-gram 桶合法命中 "a.b"（'.' 只被 3-gram 归档跳过）
        auto rd = ft.search("ab", 10);
        assert(rd.size() == 1 && rd[0].word == 0);
    }
}

// T7：候选扩展路径与上限
static void test_candidate_paths() {
    // a) ngram3：按最稀 3-gram 选桶；查询有 gram 命中但全词不含查询串
    {
        FullTextIndexStd ft;
        ft.add_document("xaaay", {0, 0});
        ft.finalize();
        assert(ft.search("aaa", 10).size() == 1);     // gram "aaa" 命中且含
        assert(ft.search("aaaz", 10).empty());        // gram "aaa" 命中但 find 落空
    }
    // b) ngram2 / 单字路径与 miss
    {
        FullTextIndexStd ft;
        ft.add_document("cat dog", {0, 0});
        ft.finalize();
        assert(ft.search("ca", 10).size() == 1);      // 2-gram 命中
        assert(ft.search("c", 10).size() == 1);       // 单字命中
        assert(ft.search("cx", 10).empty());          // 2-gram 桶不存在
        assert(ft.search("q", 10).empty());           // 单字桶不存在
    }
    // c) 词表全单字：ngram2/ngram3 索引为空的臂
    {
        FullTextIndexStd ft;
        ft.add_document("a", {0, 0});
        ft.add_document("b", {0, 1});
        ft.add_document("c", {0, 2});
        ft.finalize();
        assert(ft.search("a", 10).size() == 1);
        assert(ft.search("ab", 10).empty());          // ngram2 空
        assert(ft.search("abc", 10).empty());         // ngram3 空
        assert(ft.search("c", 10).size() == 1);
    }
    // d) 候选 256 上限截断（ngram2 桶 300 词）；单字桶同理
    {
        FullTextIndexStd ft;
        for (int i = 0; i < 300; ++i)
            ft.add_document("aa" + std::to_string(i), {0, i});
        ft.finalize();
        auto r = ft.search("aa", 5);
        assert(r.size() == 5);
    }
    {
        FullTextIndexStd ft;
        for (int i = 0; i < 300; ++i)
            ft.add_document("z" + std::to_string(i), {0, i});
        ft.finalize();
        auto r = ft.search("z", 5);
        assert(r.size() == 5);
    }
    // e) 多字节 varint 往返：docId 间隔 299（两字节）、tf=200（两字节）；
    //    单 posting（i==0 臂）与多 posting（prev 累加臂）
    {
        FullTextIndexStd ft;
        std::string big;
        for (int i = 0; i < 200; ++i) big += "big ";
        ft.add_document("zz " + big, {0, 0});
        for (int i = 1; i < 299; ++i) ft.add_document("fill", {0, i});
        ft.add_document("zz", {0, 299});
        ft.finalize();
        assert(ft.save(fpath("wide").string()));
        FullTextIndexStd back;
        assert(back.load(fpath("wide").string()));
        auto r = back.search("zz", 10);
        assert(r.size() == 2);
        assert(r[0].word == 0 && r[1].word == 299);
        assert(back.search("big", 10).size() == 1);
    }
    // f) 3-gram 扩展的 256 候选上限：300 词全含查询串 q，gram 桶
    //    全 miss（词表无 q 整串）→ 逐词 find 命中推到 cap 截断
    {
        FullTextIndexStd ft;
        for (int i = 0; i < 300; ++i)
            ft.add_document("aaax" + std::to_string(i), {0, i});
        ft.finalize();
        auto r = ft.search("aaax", 5);
        assert(r.size() == 5);
    }
}

int main() {
    test_tokenize_word_chars();
    test_build_threads_and_limits();
    test_search_dedup_and_unfinalized();
    test_save_load_guards();
    test_truncation_matrix();
    test_crafted_semantic_files();
    test_candidate_paths();
    std::cout << "OK\n";
    return 0;
}
