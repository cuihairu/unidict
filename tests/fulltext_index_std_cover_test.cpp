// FullTextIndexStd 补覆盖：substring_candidates 的 2-gram / 1-char /
// prefix fallback 三条路径（经 search 精确 miss 触发）、varint 多字节
// 编解码往返（docId 间隔 >=128）、压缩 postings 末字节损坏时 vdecode
// 越界返回 false 的容错。

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;

static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static void write_file(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(data.data(), (std::streamsize)data.size());
    assert(out.good());
}

int main() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "ft_cover";
    fs::create_directories(base);

    // ===== 1) search 精确 miss 触发 substring_candidates 的三条路径 =====
    {
        FullTextIndexStd idx;
        std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
        docs.push_back({"tab apple apply zqq", {1, 1}});
        idx.build_from_documents(docs, 1);
        idx.finalize();

        assert(idx.doc_count() == 1);
        // "tab" 精确命中，不走进候选扩展
        auto exact = idx.search("tab", 10);
        assert(exact.size() == 1);
        // "ab"（2 字符）→ ngram2 路径扩展出 tab
        auto via2 = idx.search("ab", 10);
        assert(via2.size() == 1);
        // "a"（1 字符）→ char 索引路径
        auto via1 = idx.search("a", 10);
        assert(!via1.empty());
        // "apx"：3-gram 全 miss、2/1 字段不进 → prefix_index_ fallback
        auto viafb = idx.search("apx", 10);
        (void)viafb;
    }

    // ===== 2) varint 多字节往返：docId 间隔 >= 128 =====
    {
        FullTextIndexStd idx;
        std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
        // docId 1 与 200：delta=199 编码为两字节 varint
        docs.push_back({"gap word", {1, 1}});
        docs.push_back({"gap word", {200, 2}});
        idx.build_from_documents(docs, 1);
        idx.finalize();
        fs::path f = base / "gap.udft";
        assert(idx.save(f.string()));

        FullTextIndexStd back;
        assert(back.load(f.string()));
        auto hits = back.search("gap", 10);
        assert(hits.size() == 2);
        // docId 顺序保持：小 docId 先出
        assert(hits[0].dict == 1 && hits[0].word == 1);
        assert(hits[1].dict == 200 && hits[1].word == 2);
    }

    // ===== 3) 压缩 postings 末字节丢终止位：懒解码 vdecode 越界容错 =====
    {
        FullTextIndexStd idx;
        std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
        // 单一词项：save 顺序写且无目录段 → 文件末字节必属 "common"
        for (int i = 0; i < 4; ++i)
            docs.push_back({"common", {i + 1, i + 1}});
        idx.build_from_documents(docs, 1);
        idx.finalize();
        fs::path f = base / "corrupt.udft";
        assert(idx.save(f.string()));

        std::string data = read_file(f.string());
        assert(!data.empty());
        unsigned char last = (unsigned char)data.back();
        assert((last & 0x80) == 0);   // 确为终止字节
        data.back() = (char)(last | 0x80);
        write_file(f.string(), data);

        FullTextIndexStd back;
        assert(back.load(f.string()));   // 载入成功，解码是懒执行
        auto hits = back.search("common", 10);
        (void)hits;   // 容错路径不崩、返回部分/空结果均可
    }

    return 0;
}
