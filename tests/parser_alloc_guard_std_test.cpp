// 解析器分配守卫回归钉（2026-11-02 代码深审 P1 反序列化 OOM/DoS 族）。
//
// 根因：各解析器从文件控制的长度/计数字段（u32/u64）直接 reserve/resize，
// 未与剩余缓冲区/文件尺寸比对——小文件 + 超大计数字段驱动 ~160GB 级
// 分配（bad_alloc/length_error 中止）。修复为「先按剩余字节/最小项大小
// 钳制再分配」。本测试以真实畸形输入驱动，验证不 OOM 且快速失败：
//   - MDX KIDX/KEYB/KBIX/KBIX-multirb：count=0xFFFFFFFF + 极短体 → 该解析
//     器臂失败后落到种子兜底，load 正常返回（修复前此处 bad_alloc 中止）。
//   - fulltext UDFT2：docs/siglen 超剩余字节 → load 返回 false（截断），
//     不分配超大表（修复前 doc_tf_/doc_map_ resize 4e9 项 OOM）。
//   - StarDict：.idx 记录 sz 超出 .dict 尺寸 → lookup 返回空（修复前
//     out.resize(sz) 4GB 分配 OOM）。
//
// 注：MDXK/MDXR 与 KBIX-multirb 的「累计解码上限」（MAX_TOTAL_DECODED
// 512MB）需 >512MB 解压数据才触发，单测内存预算不可承受，按仓例
// GCOVR_EXCL_LINE（见 mdict_parser_std.cpp 相应注释），此处只钉计数钳制。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/mdict_parser_std.h"
#include "std/fulltext_index_std.h"
#include "std/stardict_parser_std.h"

namespace fs = std::filesystem;

static void u32be(std::string& s, uint32_t v) {
    s.push_back((char)((v >> 24) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF));
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}
static void u16be(std::string& s, uint16_t v) {
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}
static std::string zlib_compress(const std::string& in) {
    uLongf out_len = compressBound((uLong)in.size());
    std::string out; out.resize(out_len);
    int rc = compress2((Bytef*)out.data(), &out_len,
                       (const Bytef*)in.data(), (uLong)in.size(), Z_BEST_SPEED);
    assert(rc == Z_OK);
    out.resize(out_len);
    return out;
}
static void write_file(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    assert(out.good());
}

int main() {
    const fs::path dir = fs::current_path() / "build-local" / "allocguard";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // ---- MDX：KIDX/KEYB/KBIX/KBIX-multirb 的 count=0xFFFFFFFF ----
    const uint32_t HUGE_COUNT = 0xFFFFFFFFu;
    auto mdx_case = [&](const std::string& name, const std::string& body) {
        const fs::path mdx = dir / name;
        write_file(mdx, "<Dictionary title=\"Bad\"/>\n" + body);
        UnidictCoreStd::MdictParserStd mp;
        bool ok = mp.load_dictionary(mdx.string());
        assert(ok);                 // 落到种子兜底，不中止
        assert(mp.is_loaded());
        assert(!mp.all_words().empty());
    };
    // KIDX：count=HUGE_COUNT，仅 1 条短项（修复前 reserve 4e9*Item 中止）
    { std::string b; b += "KIDX"; u32be(b, HUGE_COUNT); u16be(b, 3); b += "abc";
      u32be(b, 0); u32be(b, 3); b += "RDEF"; b += zlib_compress("xyz");
      mdx_case("huge_kidx.mdx", b); }
    // KEYB：同口径
    { std::string b; b += "KEYB"; u32be(b, HUGE_COUNT); u16be(b, 3); b += "abc";
      u32be(b, 0); u32be(b, 3); b += "RECB"; b += zlib_compress("xyz");
      mdx_case("huge_keyb.mdx", b); }
    // KBIX/RBIX：同口径
    { std::string b; b += "KBIX"; u32be(b, HUGE_COUNT); u16be(b, 3); b += "abc";
      u32be(b, 0); u32be(b, 3); b += "RBIX"; b += zlib_compress("xyz");
      mdx_case("huge_kbix.mdx", b); }
    // KBIX-multirb：count=1（合法单项）→ RBCT blocks=HUGE_COUNT，触发每块 ≥9
    // 字节守卫钳制；单块累计上限另见 mdict_parser_std.cpp GCOVR_EXCL_LINE
    { std::string b; b += "KBIX"; u32be(b, 1); u16be(b, 3); b += "abc";
      u32be(b, 0); u32be(b, 0); u32be(b, 3); b += "RBCT"; u32be(b, HUGE_COUNT);
      b += "RBLK"; u32be(b, 0);  // comp_len=0 → safe_inflate 立即失败
      mdx_case("huge_multirb.mdx", b); }

    // ---- fulltext UDFT2：docs=0xFFFFFFFF（docmap 每条 8 字节，超出剩余）----
    {
        const fs::path ft = dir / "huge_docs.ftidx";
        std::string b; b += "UDFT2";
        u32be(b, 0);                 // siglen=0
        u32be(b, HUGE_COUNT);              // docs
        write_file(ft, b);
        UnidictCoreStd::FullTextIndexStd fi;
        assert(!fi.load(ft.string()));
        assert(fi.last_error() == "truncated (docmap)");
    }

    // ---- StarDict：.idx 记录 sz 超出 .dict 尺寸 → 空返回，不 4GB resize ----
    {
        const fs::path dict = dir / "bad.dict";
        const fs::path idx = dir / "bad.idx";
        const fs::path ifo = dir / "bad.ifo";
        write_file(ifo, "bookname=Bad\nwordcount=1\nsametypesequence=m\n");
        // .idx：词头 "hello\0"（7 字节） + off=0（u32） + sz=0xFFFFFFFF（u32）
        std::string idxb; idxb += "hello"; idxb.push_back('\0');
        u32be(idxb, 0); u32be(idxb, HUGE_COUNT);
        write_file(idx, idxb);
        write_file(dict, "short");   // .dict 仅 5 字节
        UnidictCoreStd::StarDictParserStd sp;
        assert(sp.load_dictionary(ifo.string()));
        // 查询命中该词头，但 sz 越界 → 守卫返回空（修复前 out.resize(4GB) OOM）
        assert(sp.lookup("hello").empty());
    }

    return 0;
}
