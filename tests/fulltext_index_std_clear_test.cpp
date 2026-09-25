// clear() 必须是完整复位：它原来只清了 4 个成员，把四个派生索引
// （terms_sorted_/ngram3_/ngram2_/char_/prefix_index_）和三个对外可见的状态
// （signature_/version_/last_error_）全留在旧值上。
//
// terms_sorted_ 那条尤其要命：它存的是指向 postings_ 里 PostingEntry 的裸
// 指针，postings_ 一 clear 指针就全部悬空。这份测试就是钉死"clear() 之后
// 索引里不残留任何上一轮的东西"这个契约——不只看 doc_count 归零，还要看
// version()/signature()/stats() 这些诊断出口不再说谎。

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

using UnidictCoreStd::FullTextIndexStd;

namespace {

std::string tmp_dir() {
    const char* t = std::getenv("TMPDIR");
    return (t && *t) ? t : "/tmp";
}

// 建一个有实质内容的索引：多文档、多次 finalize（走遍 4 个辅助索引），
// 再 save/load 一轮让 version_/signature_ 也带上值。
void seed(FullTextIndexStd& ft) {
    ft.add_document("Hello world. Greeting and goodwill.", {0, 0});
    ft.add_document("The mouse is a small rodent and a computer device.", {0, 1});
    ft.add_document("World history is vast.", {0, 2});
    ft.finalize();
    ft.set_signature("sig-v3:foldkey2");
}

void test_clear_resets_documents() {
    FullTextIndexStd ft;
    seed(ft);
    assert(ft.doc_count() == 3);
    assert(!ft.search("world").empty());

    ft.clear();

    assert(ft.doc_count() == 0);
    assert(ft.search("world").empty());
    // 精确词与子串扩展都要空：后者会走 substring_candidates()，
    // 正是那四个辅助索引的读取路径。
    assert(ft.search("worl").empty());
    assert(ft.search("wo").empty());
    assert(ft.search("w").empty());
    assert(ft.search("").empty());
}

void test_clear_resets_version_and_signature() {
    // version_ 记的是"从哪个持久化格式读进来"，只有 load() 会置位，
    // 所以这里要走一遍 save/load 才拿得到非零值。
    const std::string path = std::string(tmp_dir()) + "/ft_clear_roundtrip.udft";
    FullTextIndexStd ft;
    seed(ft);
    assert(ft.save(path));
    assert(ft.load(path));
    assert(ft.version() == 3);  // save 写 UDFT3
    assert(ft.signature() == "sig-v3:foldkey2");
    assert(ft.doc_count() == 3);

    ft.clear();

    // version_ 归 0（"unset"）而不是留旧值：CLI 的 --diagnostics 之类出口
    // 直接读它，留旧值等于对外谎报索引格式。
    assert(ft.version() == 0);
    assert(ft.signature().empty());
    assert(ft.last_error().empty());
    std::remove(path.c_str());
}

void test_load_records_version_after_clear() {
    // clear() 复位 version_ 之后，load() 仍必须把认出的 UDFT 版本记回来。
    // 这条是回归钉子：clear() 变成完整复位后，load() 里"先记版本再 clear"
    // 的旧顺序会把刚读出的版本自己抹掉。
    const std::string path = std::string(tmp_dir()) + "/ft_clear_version.udft";
    FullTextIndexStd ft;
    seed(ft);
    assert(ft.save(path));

    // 复用同一个对象：先 load 再 clear 再 load，version 每轮都得对
    assert(ft.load(path));
    assert(ft.version() == 3);
    ft.clear();
    assert(ft.version() == 0);
    assert(ft.load(path));
    assert(ft.version() == 3);
    assert(ft.signature() == "sig-v3:foldkey2");
    assert(ft.doc_count() == 3);
    assert(!ft.search("world").empty());
    std::remove(path.c_str());
}

void test_clear_resets_stats() {
    const std::string path = std::string(tmp_dir()) + "/ft_clear_stats.udft";
    FullTextIndexStd ft;
    seed(ft);
    // 走 load() 让 stats().version 非零（version_ 只由 load 置位）
    assert(ft.save(path));
    assert(ft.load(path));
    const auto before = ft.stats();
    assert(before.terms > 0);
    assert(before.docs == 3);
    assert(before.version == 3);

    ft.clear();

    // stats() 是 CLI 诊断的另一个出口，四个计数 + version 都要归零。
    const auto after = ft.stats();
    assert(after.terms == 0);
    assert(after.docs == 0);
    assert(after.postings == 0);
    assert(after.compressed_terms == 0);
    assert(after.compressed_bytes == 0);
    assert(after.pairs_decompressed == 0);
    assert(after.avg_df == 0.0);
    assert(after.version == 0);
    std::remove(path.c_str());
}

void test_clear_then_reuse_indexes_rebuilt() {
    FullTextIndexStd ft;
    seed(ft);
    ft.clear();

    // 复用：清干净的索引要能当新的用，且结果与全新实例一致——
    // 这条同时验证 terms_sorted_ 的悬空指针没被 search() 走到
    // （如果 clear() 没清 terms_sorted_，这里枚举旧词项会拿到已释放的
    //  PostingEntry 指针）。
    ft.add_document("a fresh document about greeting", {1, 0});
    ft.add_document("another fresh greeting world", {1, 1});
    ft.finalize();

    const auto r = ft.search("greeting");
    assert(r.size() == 2);
    for (const auto& d : r) assert(d.dict == 1);

    // 子串扩展在新数据上仍工作（辅助索引被 finalize 重建过）
    assert(ft.search("greet").size() == 2);
    assert(ft.doc_count() == 2);
    // 重新 build 不改持久化格式版本（version_ 只由 load 置位）
    assert(ft.version() == 0);

    // 与全新实例逐项对照：clear() 后复用 == 从零开始
    FullTextIndexStd fresh;
    fresh.add_document("a fresh document about greeting", {1, 0});
    fresh.add_document("another fresh greeting world", {1, 1});
    fresh.finalize();
    const auto rs = fresh.stats();
    const auto rt = ft.stats();
    assert(rs.terms == rt.terms);
    assert(rs.docs == rt.docs);
    assert(rs.postings == rt.postings);
    assert(fresh.search("greet").size() == ft.search("greet").size());
}

void test_clear_twice_is_idempotent() {
    FullTextIndexStd ft;
    seed(ft);
    ft.clear();
    ft.clear();  // 幂等：重复复位不该崩、不该改状态
    assert(ft.doc_count() == 0);
    assert(ft.version() == 0);
    assert(ft.signature().empty());
    assert(ft.stats().terms == 0);
    assert(ft.search("world").empty());
}

void test_clear_on_fresh_instance() {
    // 空索引上直接 clear：所有清理路径都是空操作，不能崩
    FullTextIndexStd ft;
    ft.clear();
    assert(ft.doc_count() == 0);
    assert(ft.version() == 0);
    assert(ft.stats().terms == 0);
}

}  // namespace

int main() {
    test_clear_resets_documents();
    test_clear_resets_version_and_signature();
    test_load_records_version_after_clear();
    test_clear_resets_stats();
    test_clear_then_reuse_indexes_rebuilt();
    test_clear_twice_is_idempotent();
    test_clear_on_fresh_instance();
    std::printf("OK\n");
    return 0;
}
