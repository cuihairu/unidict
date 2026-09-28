// mdd_resource_std 分支缺口补测（真实缺边 44 条，第五大簇）。
//
// 缺边构成：缓存名 file_extension 的无点/超长扩展名臂、decompress_zlib
// 的"Z_OK 但输出缓冲恰好填满"扩容臂、single_block 的正常退出（ftell>=
// end_pos 恰好 EOF）与短读/key_len==0/key_len>1024 断臂、multi_block 的
// RBLK 签名不符与块内 key_len==0/越界断臂、get_resource_info 未命中、
// extract_to_cache/extract_all 的失败计数臂、normalize_key 全斜杠臂、
// read_bytes 的 offset/size 越界臂、缓存名坏字符替换的 \0 臂、
// get_from_cache 的文件被删/变目录两态、manager 的缓存失效重取与未知
// 词条臂。全部真实输入驱动（手改容器字节、截断 zlib 流与文件系统状态
// 是解析器的真实输入面）。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/mdd_resource_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "mddbr";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

static fs::path write_bytes(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    assert(out.good());
    return p;
}

static void be16s(std::string& s, uint16_t v) {
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

static void be32s(std::string& s, uint32_t v) {
    s.push_back((char)((v >> 24) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF));
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

static void be64s(std::string& s, uint64_t v) {
    for (int i = 7; i >= 0; --i) s.push_back((char)((v >> (8 * i)) & 0xFF));
}

// SimpleKV 容器（fallback 解析器的真实输入面）：junk 前缀 +
// SIMPLEKV + be32 count + {be16 klen, key, be32 vlen, value}
static fs::path write_simplekv(const fs::path& p,
                               const std::vector<std::pair<std::string, std::string>>& kv) {
    std::string s = "this is 16 bytes pad";
    s += "SIMPLEKV";
    be32s(s, (uint32_t)kv.size());
    for (const auto& e : kv) {
        be16s(s, (uint16_t)e.first.size());
        s += e.first;
        be32s(s, (uint32_t)e.second.size());
        s += e.second;
    }
    return write_bytes(p, s);
}

// V1 条目：{be16 klen, key, be64 off, be64 sz}，每条 2 + klen + 16 字节
static void append_sb_entry(std::string& s, const std::string& key,
                            uint64_t off, uint64_t sz) {
    be16s(s, (uint16_t)key.size());
    s += key;
    be64s(s, off);
    be64s(s, sz);
}

static size_t sb_region_size(const std::vector<std::string>& keys) {
    size_t n = 0;
    for (const auto& k : keys) n += 2 + k.size() + 16;
    return n;
}

// V1 文件：magic(3) + header_len BE32 + version BE32 + 1 pad，条目区
// 从 header_len 起，blob 紧随条目区（调用方用 header_len + 条目区大小
// 算 blob 起始偏移回填到条目里）
static fs::path write_v1(const fs::path& p, const std::string& entries,
                         const std::string& blob, size_t header_len) {
    std::string body;
    body += "\x1b\x23\x45";
    be32s(body, (uint32_t)header_len);
    be32s(body, 1);
    body.push_back('\0');
    body.resize(header_len, '\0');
    body += entries;
    body += blob;
    return write_bytes(p, body);
}

// V2 文件：magic(3) + header_len BE16 + version BE16 + 1 pad，然后
// RBCT + be32 nblocks + 每个 { RBLK + be32 comp_len + deflate(data) }
static std::string deflate_data(const std::string& raw) {
    uLongf outlen = compressBound((uLong)raw.size());
    std::string out(outlen + 1, '\0');
    int rc = compress2((Bytef*)out.data(), &outlen, (const Bytef*)raw.data(),
                       (uLong)raw.size(), Z_DEFAULT_COMPRESSION);
    assert(rc == Z_OK);
    out.resize(outlen);
    return out;
}

static fs::path write_v2(const fs::path& p, const std::vector<std::string>& blocks) {
    std::string s;
    s += "\x1b\x23\x01";
    be16s(s, 8);   // header_len
    be16s(s, 2);   // version
    s.push_back('\0');
    s += "RBCT";
    be32s(s, (uint32_t)blocks.size());
    for (const auto& b : blocks) {
        s += "RBLK";
        std::string comp = deflate_data(b);
        be32s(s, (uint32_t)comp.size());
        s += comp;
    }
    return write_bytes(p, s);
}

// T1 SimpleKV fallback + 全斜杠键
static void test_simplekv_fallback() {
    fs::path dir = base_dir();
    fs::path mdd = write_simplekv(
        dir / "skv.mdd", {{"a.png", "PNGDATA"}, {"///", "SLASHKEY"}});
    MddResourceParser p;
    assert(p.load(mdd.string()));
    assert(p.is_loaded());
    assert(p.get_resource_as_string("a.png") == "PNGDATA");
    // 全斜杠键：normalize_key 的 find_first_not_of npos 臂（不去前导斜杠）
    assert(p.has_resource("///"));
    assert(p.get_resource_as_string("///") == "SLASHKEY");
    assert(!p.has_resource("nope.png"));
}

// T2 V1 single_block：正常退出、klen==0/key_len>1024 断臂、短读断臂、
// offset/size 越界、extract 混合成败、get_resource_info 未命中
static void test_v1_single_block() {
    fs::path dir = base_dir();
    const size_t H = 12;
    // 干净文件：条目区之后跟 blob。三个真实条目 + blob 尾部垃圾：
    // blob 字节被当成下一条目的 klen（0x424C > 1024）→ key_len>1024 断臂。
    // 条目含 size 越界（offset 合法、size 超出文件剩余）与 offset 越界
    std::string blob = "BLOB-CONTENT!";
    const uint64_t blob_off =
        H + sb_region_size({"res.png", "bigsz.png", "offb.png"});
    {
        std::string entries;
        append_sb_entry(entries, "res.png", blob_off, blob.size());
        append_sb_entry(entries, "bigsz.png", blob_off, 4096);  // size 越界
        append_sb_entry(entries, "offb.png", 9999999, 4);       // offset 越界
        fs::path v1 = write_v1(dir / "v1clean.mdd", entries, blob, H);
        MddResourceParser p;
        assert(p.load(v1.string()));
        assert(p.resource_count() == 3);
        assert(p.get_resource_as_string("res.png") == "BLOB-CONTENT!");
        // size 越界（≤10MB 但超出文件剩余）→ read_bytes 边界假
        assert(p.get_resource("bigsz.png").empty());
        // offset 越界 → read_bytes 边界假
        assert(p.get_resource("offb.png").empty());
        // get_resource_info 未命中 → 默认 entry（key 空）
        assert(p.get_resource_info("nokey.png").key.empty());
        // extract 混合成败：res.png 成、两个越界条目败 → 计数臂两态
        fs::path cache1 = dir / "cache1";
        assert(p.extract_to_cache("res.png", cache1.string()));
        assert(!p.extract_to_cache("bigsz.png", cache1.string()));
        assert(p.extract_all_to_cache(cache1.string()));
        assert(fs::exists(cache1 / "res.png"));
    }
    // 条目区恰好 EOF（无 blob）：while 的 ftell>=end_pos 正常退出臂
    {
        std::string entries;
        append_sb_entry(entries, "only.png", 0, 0);
        fs::path v1 = write_v1(dir / "v1eof.mdd", entries, "", H);
        MddResourceParser p;
        assert(p.load(v1.string()));
        assert(p.resource_count() == 1);
    }
    // klen==0 条目：key_len==0 断臂（blob 之后的字节不再被读）
    {
        const uint64_t boff = H + sb_region_size({"a.png"}) + 2;  // +klen0 垫
        std::string entries;
        append_sb_entry(entries, "a.png", boff, 4);
        be16s(entries, 0);
        fs::path v1 = write_v1(dir / "v1zerok.mdd", entries, "DATA", H);
        MddResourceParser p;
        assert(p.load(v1.string()));
        assert(p.resource_count() == 1);
        assert(p.get_resource_as_string("a.png") == "DATA");
    }
    // 条目区后只剩 1 个孤字节：fread(len_buf,2) 短读断臂
    {
        const uint64_t boff = H + sb_region_size({"a.png"});
        std::string entries;
        append_sb_entry(entries, "a.png", boff, 4);
        fs::path v1 =
            write_v1(dir / "v1odd.mdd", entries, std::string("OKAY\x7f", 5), H);
        MddResourceParser p;
        assert(p.load(v1.string()));
        assert(p.resource_count() == 1);
        assert(p.get_resource_as_string("a.png") == "OKAY");
    }
}

// T3 V2 multi_block：大块驱动解压扩容臂、截断 zlib 流、RBLK 签名不符、
// 块内 klen==0 与 klen 越界
static void test_v2_multi_block() {
    fs::path dir = base_dir();
    // 正常两块，第一块解压后 >1024 字节（multi_block 的 klen 无 1024
    // 上限，用超长键撑大块体，条目流仍然合法）：首缓冲填满 →
    // Z_OK+avail_out==0 扩容臂，续跑至 Z_STREAM_END
    {
        const std::string longkey(1100, 'a');
        std::string b1;
        append_sb_entry(b1, longkey + ".png", 0, 0);
        std::string b2;
        append_sb_entry(b2, "k2.png", 0, 0);
        fs::path v2 = write_v2(dir / "v2good.mdd", {b1, b2});
        MddResourceParser p;
        assert(p.load(v2.string()));
        assert(p.resource_count() == 2);
        assert(p.has_resource(longkey + ".png"));
        assert(p.has_resource("k2.png"));
        assert(p.header_info().num_blocks == 2);
    }
    // 截断 zlib 流：inflate 中途输入耗尽 → Z_BUF_ERROR ≠ Z_STREAM_END
    // → 解压假 → continue 臂，无条目 → load 假
    {
        std::string raw(2048, 'A');
        append_sb_entry(raw, "k1.png", 0, 0);
        std::string comp = deflate_data(raw);
        comp.resize(comp.size() / 2);
        std::string s;
        s += "\x1b\x23\x01";
        be16s(s, 8);
        be16s(s, 2);
        s.push_back('\0');
        s += "RBCT";
        be32s(s, 1);
        s += "RBLK";
        be32s(s, (uint32_t)comp.size());
        s += comp;
        write_bytes(dir / "v2trunc.mdd", s);
        MddResourceParser p;
        assert(!p.load((dir / "v2trunc.mdd").string()));
    }
    // 第二块签名不是 RBLK → memcmp 假臂 break，第一块条目保留
    {
        std::string b1;
        append_sb_entry(b1, "k1.png", 0, 0);
        std::string comp = deflate_data(b1);
        std::string s;
        s += "\x1b\x23\x01";
        be16s(s, 8);
        be16s(s, 2);
        s.push_back('\0');
        s += "RBCT";
        be32s(s, 2);
        s += "RBLK";
        be32s(s, (uint32_t)comp.size());
        s += comp;
        s += "XXXX";  // 坏签名
        be32s(s, 4);
        s += "junk";
        write_bytes(dir / "v2badsig.mdd", s);
        MddResourceParser p;
        assert(p.load((dir / "v2badsig.mdd").string()));
        assert(p.resource_count() == 1);
        assert(p.has_resource("k1.png"));
    }
    // 块循环中途 EOF：第二块签名 fread 不足 4 字节 → 短读真臂 break
    {
        std::string b1;
        append_sb_entry(b1, "k1.png", 0, 0);
        std::string comp = deflate_data(b1);
        std::string s;
        s += "\x1b\x23\x01";
        be16s(s, 8);
        be16s(s, 2);
        s.push_back('\0');
        s += "RBCT";
        be32s(s, 2);  // 宣称两块，只放一块，文件到此为止
        s += "RBLK";
        be32s(s, (uint32_t)comp.size());
        s += comp;
        write_bytes(dir / "v2eofsig.mdd", s);
        MddResourceParser p;
        assert(p.load((dir / "v2eofsig.mdd").string()));
        assert(p.resource_count() == 1);
        assert(p.has_resource("k1.png"));
    }
    // 块内 klen==0 与 klen 越界 → 内层 while 的两个 break 条件
    {
        std::string b;
        append_sb_entry(b, "k1.png", 0, 0);  // 先放合法条目
        be16s(b, 0);                          // klen 0 → break
        fs::path v2 = write_v2(dir / "v2zerok.mdd", {b});
        MddResourceParser p;
        assert(p.load(v2.string()));
        assert(p.resource_count() == 1);
    }
    {
        std::string b;
        append_sb_entry(b, "k1.png", 0, 0);
        be16s(b, 500);  // klen 超出块尾
        b += "short";   // 只给 5 字节
        fs::path v2 = write_v2(dir / "v2overk.mdd", {b});
        MddResourceParser p;
        assert(p.load(v2.string()));
        assert(p.resource_count() == 1);
    }
}

// T4 缓存：无点超长键、超长扩展名、内嵌 \0 键、缓存文件被删/变目录
static void test_cache_paths() {
    fs::path dir = base_dir();
    MddResourceCache cache((dir / "c").string());
    // 超长键（>200）无点：file_extension 的 npos 臂
    assert(cache.cache_resource(std::vector<uint8_t>(8, 1),
                                std::string(250, 'x') + "nodot",
                                "application/octet-stream"));
    // 超长键带 >12 字节扩展名：size-dot>12 臂
    assert(cache.cache_resource(std::vector<uint8_t>(8, 1),
                                std::string(250, 'y') + ".abcdefghijklmn",
                                "application/octet-stream"));
    // 内嵌 \0：strchr 命中（kBad 的收尾 NUL）但 c=='\0' 不替换的臂
    assert(cache.cache_resource(std::vector<uint8_t>(4, 2),
                                std::string("a\0b.png", 7), "image/png"));
    // 缓存文件被删：get_from_cache 的 !in 臂
    assert(cache.cache_resource(std::string("DELETED"), "gone", "image/png"));
    assert(cache.is_cached("gone"));
    fs::remove(fs::path(cache.get_cached_path("gone")));
    assert(cache.get_from_cache("gone").empty());
    // 缓存位被换成目录：!is_regular_file 臂（打不开目录的文件系统由
    // !in 同门短路）
    assert(cache.cache_resource(std::string("DIRSWAP"), "dirswap", "image/png"));
    fs::path dp(cache.get_cached_path("dirswap"));
    fs::remove(dp);
    fs::create_directories(dp);
    assert(cache.get_from_cache("dirswap").empty());
}

// T5 manager：未知词典、未知词条、缓存失效后重取
static void test_manager() {
    fs::path dir = base_dir();
    const size_t H = 12;
    std::string blob = "MGRBLOB";
    const uint64_t blob_off = H + sb_region_size({"res.png"});
    std::string entries;
    append_sb_entry(entries, "res.png", blob_off, blob.size());
    fs::path v1 = write_v1(dir / "mgr.mdd", entries, blob, H);

    MddResourceManager mgr;
    mgr.set_cache_directory((dir / "mc").string());
    assert(mgr.load_mdd(v1.string(), "d1"));
    assert(mgr.has_mdd("d1"));
    // 未知词典
    assert(mgr.get_resource_path("x", "nope").empty());
    assert(mgr.get_resource_data("x", "nope").empty());
    // 未知词条：get_resource_info 未命中 → key 空短路
    assert(mgr.get_resource_path("nokey.png", "d1").empty());
    assert(mgr.get_resource_data("nokey.png", "d1").empty());
    // 正常取用 → 缓存
    std::string p1 = mgr.get_resource_path("res.png", "d1");
    assert(!p1.empty() && fs::exists(p1));
    auto data = mgr.get_resource_data("res.png", "d1");
    assert(std::string(data.begin(), data.end()) == "MGRBLOB");
    // 缓存命中路径：文件未删再取 → get_resource_path 直接返回缓存路径
    std::string p1b = mgr.get_resource_path("res.png", "d1");
    assert(p1b == p1);
    // 缓存命中路径：get_resource_data 直接读缓存文件
    data = mgr.get_resource_data("res.png", "d1");
    assert(std::string(data.begin(), data.end()) == "MGRBLOB");
    // 全新词典 id 的首次取用：缓存元数据为空 → cached.empty() 真臂
    assert(mgr.load_mdd(v1.string(), "d2"));
    data = mgr.get_resource_data("res.png", "d2");
    assert(std::string(data.begin(), data.end()) == "MGRBLOB");
    // 缓存文件被删后重取：exists 假臂 → 回落解析器重新提取
    fs::remove(fs::path(p1));
    std::string p2 = mgr.get_resource_path("res.png", "d1");
    assert(!p2.empty() && fs::exists(p2));
    fs::remove(fs::path(p2));
    data = mgr.get_resource_data("res.png", "d1");
    assert(std::string(data.begin(), data.end()) == "MGRBLOB");
    // 卸载
    assert(mgr.unload_mdd("d1"));
    assert(!mgr.has_mdd("d1"));
    assert(!mgr.unload_mdd("d1"));
}

int main() {
    test_simplekv_fallback();
    test_v1_single_block();
    test_v2_multi_block();
    test_cache_paths();
    test_manager();
    std::cout << "OK\n";
    return 0;
}
