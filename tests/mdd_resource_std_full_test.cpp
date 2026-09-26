// mdd_resource_std 全接口覆盖：V1/V2 头、single/multi block、SIMPLEKV
// fallback、错误路径、normalize_key、MIME 探测、缓存与资源管理器
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>
#include <zlib.h>

#include "std/mdd_resource_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static void set_env(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

static void unset_env(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");  // MSVC：置空即删除变量
#else
    ::unsetenv(key);
#endif
}

static void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    v.push_back(static_cast<unsigned char>(x & 0xFF));
}
static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 3; i >= 0; --i)
        v.push_back(static_cast<unsigned char>((x >> (i * 8)) & 0xFF));
}
static void be64w(std::vector<unsigned char>& v, uint64_t x) {
    for (int i = 7; i >= 0; --i)
        v.push_back(static_cast<unsigned char>((x >> (i * 8)) & 0xFF));
}

static void write_file(const fs::path& path, const std::vector<unsigned char>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    assert(out.good());
}

// V2 头按格式写实：magic(3) + header_len(2) + version(2)，头部共 kHeaderLen
// 字节，索引表紧跟其后，资源值再往后。
//
// 这里原来写的是"头区固定 6947 字节"——因为解析器当时从 buf[0] 取
// header_len，把 magic 的头两字节 0x1b23=6947 当成了头长度，于是只有把
// 头撑到 6947 字节、让索引表正好落在 6947 上，文件才能加载。那等于把
// 解析器的越界偏移当成了格式。header_len 改为从 buf+3 取之后，头部就是
// 正常的几十字节，索引表从 kHeaderLen 开始。
static constexpr size_t kV2HeaderLen = 16;

// 裸 V2 头（kV2HeaderLen 字节），供"手工拼畸形块区"的用例当前缀用
static std::vector<unsigned char> v2_header() {
    std::vector<unsigned char> h;
    h.push_back(0x1b);
    h.push_back(0x23);
    h.push_back(0x01);
    be16w(h, static_cast<uint16_t>(kV2HeaderLen));
    be16w(h, 0x2d);
    while (h.size() < kV2HeaderLen) h.push_back(0);
    return h;
}

// 索引表（不含资源值）的字节数；offset 要等表长定下来才能算，故两遍
static std::vector<unsigned char> build_v2_table(
    const std::vector<std::tuple<std::string, uint64_t, uint64_t>>& placed,
    bool multi_block) {
    std::vector<unsigned char> table;
    if (!multi_block) {
        for (const auto& p : placed) {
            const std::string& k = std::get<0>(p);
            be16w(table, static_cast<uint16_t>(k.size()));
            table.insert(table.end(), k.begin(), k.end());
            be64w(table, std::get<1>(p));
            be64w(table, std::get<2>(p));
        }
    } else {
        // RBCT + 块数 + RBLK 块；USE_ZLIB 已接上，块数据是真 zlib 流
        const unsigned char sig_rbct[4] = {'R', 'B', 'C', 'T'};
        const unsigned char sig_rblk[4] = {'R', 'B', 'L', 'K'};
        table.insert(table.end(), sig_rbct, sig_rbct + 4);
        be32w(table, 1);
        std::vector<unsigned char> entries;
        for (const auto& p : placed) {
            const std::string& k = std::get<0>(p);
            be16w(entries, static_cast<uint16_t>(k.size()));
            entries.insert(entries.end(), k.begin(), k.end());
            be64w(entries, std::get<1>(p));
            be64w(entries, std::get<2>(p));
        }
        uLongf bound = compressBound(static_cast<uLong>(entries.size()));
        std::vector<unsigned char> comp(bound);
        uLongf clen = bound;
        assert(compress2(comp.data(), &clen, entries.data(), entries.size(),
                         Z_DEFAULT_COMPRESSION) == Z_OK);
        comp.resize(clen);
        table.insert(table.end(), sig_rblk, sig_rblk + 4);
        be32w(table, static_cast<uint32_t>(clen));
        table.insert(table.end(), comp.begin(), comp.end());
    }
    return table;
}

static std::vector<unsigned char> build_v2_mdd(
    const std::vector<std::pair<std::string, std::string>>& resources,
    bool multi_block = false) {
    // offset 要等索引表长度定下来才能算，而 multi_block 的索引表是 zlib
    // 压缩的——压缩后的长度又依赖 offset 的数值，两者是相互依赖的。用一个
    // 固定点迭代收敛：压缩长度对 offset 的敏感度只有字节级，几轮就稳定。
    std::vector<std::tuple<std::string, uint64_t, uint64_t>> placed;
    for (const auto& r : resources) {
        placed.emplace_back(r.first, 0, r.second.size());
    }

    size_t table_len = 0;
    std::string blob;
    for (int iter = 0; iter < 8; ++iter) {
        uint64_t data_off = kV2HeaderLen + table_len;
        blob.clear();
        for (size_t i = 0; i < resources.size(); ++i) {
            std::get<1>(placed[i]) = data_off;
            blob += resources[i].second;
            data_off += resources[i].second.size();
        }
        const size_t n = build_v2_table(placed, multi_block).size();
        if (n == table_len) break;
        table_len = n;
    }

    std::vector<unsigned char> body = v2_header();
    const auto table = build_v2_table(placed, multi_block);
    assert(table.size() == table_len);
    body.insert(body.end(), table.begin(), table.end());
    body.insert(body.end(), blob.begin(), blob.end());
    return body;
}

static std::vector<unsigned char> make_simplekv(
    const std::vector<std::pair<std::string, std::string>>& kv) {
    std::vector<unsigned char> v;
    const std::string magic = "SIMPLEKV";
    v.insert(v.end(), magic.begin(), magic.end());
    be32w(v, static_cast<uint32_t>(kv.size()));
    for (const auto& p : kv) {
        be16w(v, static_cast<uint16_t>(p.first.size()));
        v.insert(v.end(), p.first.begin(), p.first.end());
        be32w(v, static_cast<uint32_t>(p.second.size()));
        v.insert(v.end(), p.second.begin(), p.second.end());
    }
    return v;
}

int main() {
    fs::path base = fs::current_path() / "build-local" / "mdd_full";
    fs::create_directories(base);
    set_env("UNIDICT_CACHE_DIR", (base / "cache_env").string());

    const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "IMAGEDATA...";
    const std::string mp3 = "ID3-FAKE-MP3-AUDIO";
    const std::vector<unsigned char> png_vec(png.begin(), png.end());

    // ===== V2 + single block：完整读取链 =====
    {
        auto bytes = build_v2_mdd({{"pics/a.png", png}, {"snd/hello.mp3", mp3}});
        auto v2path = base / "v2_single.mdd";
        write_file(v2path, bytes);

        MddResourceParser p;
        assert(p.load(v2path.string()));
        assert(p.is_loaded());
        assert(p.resource_count() == 2);
        assert(p.file_path() == v2path.string());

        const auto& h = p.header_info();
        assert(h.magic.size() == 3);
        // header_len / version 来自头部字段本身（buf+3 起），不再是 magic 的头两字节
        assert(h.header_len == kV2HeaderLen);
        assert(h.version == 0x2d);
        assert(h.total_size == bytes.size());

        auto got = p.get_resource("pics/a.png");
        assert(std::string(got.begin(), got.end()) == png);
        assert(p.get_resource_as_string("snd/hello.mp3") == mp3);
        assert(p.get_resource("nope.bin").empty());
        assert(p.get_resource_as_string("nope.bin").empty());

        // normalize_key：反斜杠/前导斜杠/协议前缀/query/fragment/大小写
        assert(p.has_resource("pics\\a.png"));
        assert(p.has_resource("/pics/a.png"));
        assert(p.has_resource("file://pics/a.png"));
        assert(p.has_resource("sound://pics/a.png"));
        assert(p.has_resource("entry://pics/a.png"));
        assert(p.has_resource("bword://pics/a.png"));
        assert(p.has_resource("gxres://pics/a.png"));
        assert(p.has_resource("mdd://pics/a.png"));
        assert(p.has_resource("pics/a.png?x=1&y=2"));
        assert(p.has_resource("pics/a.png#frag"));
        assert(p.has_resource("PICS/A.PNG"));
        assert(!p.has_resource("pics/missing.png"));

        auto info = p.get_resource_info("pics/a.png");
        assert(info.key == "pics/a.png");
        assert(info.size == png.size());
        assert(!info.is_compressed);
        assert(p.get_resource_info("nope").key.empty());

        assert(p.list_resources().size() == 2);
        auto filtered = p.list_resources("pics/");
        assert(filtered.size() == 1 && filtered[0] == "pics/a.png");

        fs::path exdir = base / "extract";
        assert(p.extract_to_cache("pics/a.png", exdir.string()));
        assert(fs::exists(exdir / "pics-a.png"));
        assert(!p.extract_to_cache("nope.bin", exdir.string()));
        assert(p.extract_all_to_cache(exdir.string()));
        assert(p.extract_all_to_cache(exdir.string(), 1));
        p.unload();
        assert(!p.is_loaded());
    }

    // ===== V2 + multi block（RBCT/RBLK + zlib）=====
    {
        auto bytes = build_v2_mdd(
            {{"img/one.gif", "GIF89a-x"}, {"img/two.css", "body{}"}}, true);
        auto v2m = base / "v2_multi.mdd";
        write_file(v2m, bytes);

        MddResourceParser p;
        assert(p.load(v2m.string()));
        assert(p.resource_count() == 2);
        assert(p.header_info().num_blocks == 1);
        assert(p.get_resource_as_string("img/one.gif") == "GIF89a-x");
        assert(p.get_resource_as_string("img/two.css") == "body{}");
    }

    // ===== load 错误路径 =====
    {
        MddResourceParser p;
        assert(!p.load((base / "missing.mdd").string()));
        assert(!p.is_loaded());
        // 空文件：魔数不足 4 字节，SIMPLEKV 也失败（size<=0）
        {
            std::ofstream touch(base / "empty.mdd", std::ios::binary | std::ios::trunc);
            assert(touch.good());
        }
        assert(!p.load((base / "empty.mdd").string()));
        // V1 魔数（header_len 域被魔数前 4 字节顶成超大值）→ 索引区不可读
        write_file(base / "v1.mdd", {0x1b, 0x23, 0x45, 0x01, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        assert(!p.load((base / "v1.mdd").string()));
        // V1 魔数但文件不足 12 字节
        write_file(base / "v1_short.mdd",
                   {0x1b, 0x23, 0x45, 0x01, 0, 0, 0, 0, 0, 0});
        assert(!p.load((base / "v1_short.mdd").string()));
        // 乱字节：无魔数无 SIMPLEKV
        write_file(base / "junk.mdd", {'x', 'y', 'z', 'z', 'y', 'y', 1, 2, 3, 4});
        assert(!p.load((base / "junk.mdd").string()));
    }

    // ===== SIMPLEKV fallback：成功 / 截断 / vlen 超界 / 跨 64KB 块找魔数 =====
    {
        write_file(base / "kv.mdd",
                   make_simplekv({{"a/b.css", "p{color:red}"}, {"c.js", "x()"}}));
        MddResourceParser p;
        assert(p.load((base / "kv.mdd").string()));
        assert(p.resource_count() == 2);
        assert(p.get_resource_as_string("a/b.css") == "p{color:red}");

        // 截断：声称 3 条只放 1 条 → 第二条读取失败
        auto truncated = make_simplekv({{"only.css", "a{}"}});
        truncated[9] = 3;
        write_file(base / "kv_trunc.mdd", truncated);
        MddResourceParser p2;
        assert(!p2.load((base / "kv_trunc.mdd").string()));

        // vlen 超界（value 长度越过文件尾）
        auto overrun = make_simplekv({{"ok.css", "a{}"}});
        for (int i = 0; i < 4; ++i) overrun[20 + i] = 0xFF;
        write_file(base / "kv_vlen.mdd", overrun);
        MddResourceParser p3;
        assert(!p3.load((base / "kv_vlen.mdd").string()));

        // >64KB 垃圾前缀：魔数定位跨 chunk + overlap 拼接
        std::vector<unsigned char> big(70 * 1024, 'Z');
        auto kv_body = make_simplekv({{"deep/i.ico", "ICON"}});
        big.insert(big.end(), kv_body.begin(), kv_body.end());
        write_file(base / "kv_big.mdd", big);
        MddResourceParser p4;
        assert(p4.load((base / "kv_big.mdd").string()));
        assert(p4.get_resource_as_string("deep/i.ico") == "ICON");
    }

    // ===== 超大条目：size 越过 MAX_RESOURCE_SIZE 直接返回空 =====
    {
        auto bytes = build_v2_mdd({{"big.bin", "XY"}});
        // 索引表在 kV2HeaderLen：be16 klen + key(7) + be64 off + be64 size；
        // size 的第 5 字节置 0x0B → 约 184MB > 10MB 上限
        size_t size_pos = kV2HeaderLen + 2 + 7 + 8;
        bytes[size_pos + 4] = 0x0B;
        write_file(base / "big_entry.mdd", bytes);
        MddResourceParser p;
        assert(p.load((base / "big_entry.mdd").string()));
        assert(p.get_resource("big.bin").empty());
    }

    // ===== detect_mime_type 全类型 =====
    {
        assert(MddResourceParser::detect_mime_type("a.png") == "image/png");
        assert(MddResourceParser::detect_mime_type("a.jpg") == "image/jpeg");
        assert(MddResourceParser::detect_mime_type("a.JPEG") == "image/jpeg");
        assert(MddResourceParser::detect_mime_type("a.gif") == "image/gif");
        assert(MddResourceParser::detect_mime_type("a.svg") == "image/svg+xml");
        assert(MddResourceParser::detect_mime_type("a.webp") == "image/webp");
        assert(MddResourceParser::detect_mime_type("a.bmp") == "image/bmp");
        assert(MddResourceParser::detect_mime_type("a.ico") == "image/x-icon");
        assert(MddResourceParser::detect_mime_type("a.mp3") == "audio/mpeg");
        assert(MddResourceParser::detect_mime_type("a.wav") == "audio/wav");
        assert(MddResourceParser::detect_mime_type("a.ogg") == "audio/ogg");
        assert(MddResourceParser::detect_mime_type("a.m4a") == "audio/mp4");
        assert(MddResourceParser::detect_mime_type("a.aac") == "audio/aac");
        assert(MddResourceParser::detect_mime_type("a.flac") == "audio/flac");
        assert(MddResourceParser::detect_mime_type("a.mp4") == "video/mp4");
        assert(MddResourceParser::detect_mime_type("a.webm") == "video/webm");
        assert(MddResourceParser::detect_mime_type("a.ogv") == "video/ogg");
        assert(MddResourceParser::detect_mime_type("a.avi") == "video/x-msvideo");
        assert(MddResourceParser::detect_mime_type("a.dat") == "application/octet-stream");
        assert(MddResourceParser::detect_mime_type("noext") == "application/octet-stream");
    }

    // ===== MddResourceCache =====
    {
        fs::path cdir = base / "cache1";
        MddResourceCache cache(cdir.string());
        assert(cache.get_cache_directory() == cdir.string());

        std::vector<unsigned char> d1 = {'a', 'b', 'c'};
        std::vector<unsigned char> no_data;
        assert(!cache.cache_resource(no_data, "k0", "x/y"));
        assert(cache.cache_resource(d1, "k1", "image/png"));
        assert(cache.cache_resource(std::string("hello"), "k2", "text/plain"));

        assert(cache.is_cached("k1"));
        assert(!cache.is_cached("k3"));
        assert(cache.get_cached_path("k1").find("k1") != std::string::npos);
        assert(fs::exists(cache.get_cached_path("k1")));
        assert(cache.get_cached_path("k3").empty());

        assert(cache.get_from_cache("k1") == d1);
        assert(cache.get_from_cache("k3").empty());

        assert(cache.get_cached_count() == 2);
        assert(cache.get_cache_size() == 8);
        assert(cache.get_cache_info().size() == 2);

        cache.update_access_time("k1");
        cache.increment_access_count("k1");
        for (const auto& ci : cache.get_cache_info()) {
            if (ci.key == "k1") assert(ci.access_count == 2);
        }

        // 缓存文件被外部删除 → 读缓存得空
        fs::remove(cache.get_cached_path("k2"));
        assert(cache.get_from_cache("k2").empty());

        // 按 size 裁剪到 0（不依赖 last_used 排序稳定性）
        cache.prune_by_size(1);
        assert(cache.get_cached_count() == 0);

        // 按 age 裁剪：跨秒后 age(0) 必删
        assert(cache.cache_resource(d1, "k1", "image/png"));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        cache.prune_by_age(0);
        assert(cache.get_cached_count() == 0);

        // 按访问次数裁剪
        assert(cache.cache_resource(d1, "k1", "image/png"));
        cache.prune_by_access(5);
        assert(cache.get_cached_count() == 0);

        // clear：按前缀 / 全部（文件一并删除）
        assert(cache.cache_resource(d1, "k1", "image/png"));
        assert(cache.cache_resource(d1, "k2", "image/png"));
        cache.clear_cache("k1");
        assert(cache.get_cached_count() == 1 && cache.is_cached("k2"));
        std::string k2path = cache.get_cached_path("k2");
        cache.clear_cache();
        assert(cache.get_cached_count() == 0);
        assert(!fs::exists(k2path));
    }

    // ===== MddResourceManager 全链 =====
    {
        auto v2path = base / "v2_single.mdd";
        assert(fs::exists(v2path));

        MddResourceManager mgr;
        mgr.set_cache_directory((base / "cache_mgr").string());
        assert(!mgr.has_mdd("d1"));
        assert(!mgr.unload_mdd("d1"));
        assert(!mgr.load_mdd((base / "junk.mdd").string(), "d_bad"));

        assert(mgr.load_mdd(v2path.string(), "d1"));
        assert(mgr.has_mdd("d1"));
        assert(mgr.get_total_resource_count() == 2);

        assert(mgr.has_resource("pics/a.png", "d1"));
        assert(!mgr.has_resource("pics/a.png", "d9"));

        assert(mgr.list_resources("d1", "pics/").size() == 1);
        assert(mgr.list_resources("d9").empty());

        // 首次取路径：parser → 写缓存 → 路径落盘；二次缓存命中
        auto p1 = mgr.get_resource_path("pics/a.png", "d1");
        assert(!p1.empty() && fs::exists(p1));
        assert(p1 == mgr.get_resource_path("pics/a.png", "d1"));
        assert(mgr.get_resource_path("nope", "d1").empty());
        assert(mgr.get_resource_path("pics/a.png", "d9").empty());

        auto data = mgr.get_resource_data("snd/hello.mp3", "d1");
        assert(std::string(data.begin(), data.end()) == mp3);
        assert(mgr.get_resource_data("pics/a.png", "d1") == png_vec);
        assert(mgr.get_resource_data("pics/a.png", "d9").empty());

        assert(mgr.get_total_cache_size() > 0);

        // 按词典清缓存（前缀 "d1_"）→ 全量清 → 裁剪
        mgr.clear_cache("d1");
        assert(mgr.get_total_cache_size() == 0);
        mgr.get_resource_path("pics/a.png", "d1");
        mgr.clear_cache();
        assert(mgr.get_total_cache_size() == 0);
        mgr.get_resource_path("pics/a.png", "d1");
        mgr.prune_cache(1);
        assert(mgr.get_total_cache_size() == 0);

        assert(mgr.unload_mdd("d1"));
        assert(!mgr.has_mdd("d1"));
    }

    // ===== 解析层错误路径补齐：各截断/畸形分支 =====
    {
        MddResourceParser p;
        // V2 魔数但文件不足 8 字节 → parse_v2_header 的 fread 失败
        write_file(base / "v2_short.mdd", {0x1b, 0x23, 0x01, 0x2d});
        assert(!p.load((base / "v2_short.mdd").string()));

        // 6 字节垃圾：SIMPLEKV 扫描时 chunk 不足 magic 长度的 overlap 分支
        write_file(base / "junk6.mdd", {'q', 'w', 'e', 'r', 't', 'y'});
        assert(!p.load((base / "junk6.mdd").string()));

        // SIMPLEKV 魔数后 EOF → count 域读取失败
        write_file(base / "kv_magic_only.mdd",
                   {'S', 'I', 'M', 'P', 'L', 'E', 'K', 'V'});
        assert(!p.load((base / "kv_magic_only.mdd").string()));

        // count=1 但 klen 域截断
        {
            auto v = make_simplekv({{"a.css", "x"}});
            v.resize(12);
            write_file(base / "kv_klen_trunc.mdd", v);
            assert(!p.load((base / "kv_klen_trunc.mdd").string()));
        }
        // klen==0 拒绝
        {
            std::vector<unsigned char> v;
            const std::string magic = "SIMPLEKV";
            v.insert(v.end(), magic.begin(), magic.end());
            be32w(v, 1);
            be16w(v, 0);
            write_file(base / "kv_klen0.mdd", v);
            assert(!p.load((base / "kv_klen0.mdd").string()));
        }
        // klen 声称 100 但 key 只有 3 字节
        {
            auto v = make_simplekv({{"abc", "x"}});
            v[12] = 0; v[13] = 100;
            write_file(base / "kv_key_trunc.mdd", v);
            assert(!p.load((base / "kv_key_trunc.mdd").string()));
        }
        // vlen 域截断
        {
            auto v = make_simplekv({{"ok", "x"}});
            v.resize(12 + 2 + 2);
            write_file(base / "kv_vlen_trunc.mdd", v);
            assert(!p.load((base / "kv_vlen_trunc.mdd").string()));
        }

        // single_block：表尾多 1 字节 → 下一条 klen 读取失败 break（文件仍合法加载）
        {
            auto bytes = build_v2_mdd({{"a.png", "X"}});
            bytes.push_back(0xAA);
            write_file(base / "sb_tail1.mdd", bytes);
            assert(p.load((base / "sb_tail1.mdd").string()));
            assert(p.resource_count() == 1);
        }
        // 表尾 klen 声称 100 但 key 只有 3 字节 → key 读取 break
        {
            auto bytes = build_v2_mdd({{"a.png", "X"}});
            be16w(bytes, 100);
            bytes.insert(bytes.end(), {'a', 'b', 'c'});
            write_file(base / "sb_key_trunc.mdd", bytes);
            assert(p.load((base / "sb_key_trunc.mdd").string()));
            assert(p.resource_count() == 1);
        }
        // 表尾条目缺 offset/size → 16 字节读取 break
        {
            auto bytes = build_v2_mdd({{"a.png", "X"}});
            be16w(bytes, 2);
            bytes.insert(bytes.end(), {'k', '1'});
            bytes.insert(bytes.end(), 10, 0);
            write_file(base / "sb_entry_trunc.mdd", bytes);
            assert(p.load((base / "sb_entry_trunc.mdd").string()));
            assert(p.resource_count() == 1);
        }

        // RBCT 签名后 EOF → num_blocks 读取失败
        {
            auto bytes = v2_header();
            const unsigned char rbct[4] = {'R', 'B', 'C', 'T'};
            bytes.insert(bytes.end(), rbct, rbct + 4);
            write_file(base / "mb_sig_only.mdd", bytes);
            assert(!p.load((base / "mb_sig_only.mdd").string()));
        }
        // num_blocks=2 但块 1 后截断 → 第二块签名读取 break，块 0 条目保留
        {
            auto bytes = build_v2_mdd(
                {{"blk0/a.png", "A0"}, {"blk0/b.png", "B0"}}, true);
            size_t nb_pos = kV2HeaderLen + 4;
            bytes[nb_pos + 3] = 2;  // be32(1) → be32(2)
            write_file(base / "mb_trunc.mdd", bytes);
            assert(p.load((base / "mb_trunc.mdd").string()));
            assert(p.resource_count() == 2);
        }
        // RBLK 签名后无 clen 域 → break，资源空
        {
            auto bytes = v2_header();
            const unsigned char rbct[4] = {'R', 'B', 'C', 'T'};
            const unsigned char rblk[4] = {'R', 'B', 'L', 'K'};
            bytes.insert(bytes.end(), rbct, rbct + 4);
            be32w(bytes, 1);
            bytes.insert(bytes.end(), rblk, rblk + 4);
            write_file(base / "mb_clen_trunc.mdd", bytes);
            assert(!p.load((base / "mb_clen_trunc.mdd").string()));
        }
        // clen 声称 100 但数据只有 3 字节 → break
        {
            auto bytes = v2_header();
            const unsigned char rbct[4] = {'R', 'B', 'C', 'T'};
            const unsigned char rblk[4] = {'R', 'B', 'L', 'K'};
            bytes.insert(bytes.end(), rbct, rbct + 4);
            be32w(bytes, 1);
            bytes.insert(bytes.end(), rblk, rblk + 4);
            be32w(bytes, 100);
            bytes.insert(bytes.end(), {1, 2, 3});
            write_file(base / "mb_data_trunc.mdd", bytes);
            assert(!p.load((base / "mb_data_trunc.mdd").string()));
        }
        // RBLK 块数据是垃圾（非 zlib 流）→ 解压失败 continue，资源空
        {
            auto bytes = v2_header();
            const unsigned char rbct[4] = {'R', 'B', 'C', 'T'};
            const unsigned char rblk[4] = {'R', 'B', 'L', 'K'};
            bytes.insert(bytes.end(), rbct, rbct + 4);
            be32w(bytes, 1);
            bytes.insert(bytes.end(), rblk, rblk + 4);
            be32w(bytes, 8);
            const char garbage[8] = {'g', 'a', 'r', 'b', 'a', 'g', 'e', '!'};
            bytes.insert(bytes.end(), garbage, garbage + 8);
            write_file(base / "mb_bad_zlib.mdd", bytes);
            assert(!p.load((base / "mb_bad_zlib.mdd").string()));
        }
        // 块内最后条目缺 offset/size → 条目级 break，前面条目保留
        {
            // 资源值排在块之后，条目里的 offset 要指对；offset 又影响
            // zlib 压缩长度，所以同样用固定点迭代收敛。
            const unsigned char rbct[4] = {'R', 'B', 'C', 'T'};
            const unsigned char rblk[4] = {'R', 'B', 'L', 'K'};
            std::vector<unsigned char> bytes;
            uint64_t data_off = 0;
            std::vector<unsigned char> comp;
            for (int iter = 0; iter < 8; ++iter) {
                std::vector<unsigned char> entries;
                be16w(entries, 4);
                entries.insert(entries.end(), {'g', 'o', 'o', 'd'});
                be64w(entries, data_off);
                be64w(entries, 2);
                be16w(entries, 3);
                entries.insert(entries.end(), {'b', 'a', 'd'});

                uLongf bound = compressBound(static_cast<uLong>(entries.size()));
                comp.assign(bound, 0);
                uLongf clen = bound;
                assert(compress2(comp.data(), &clen, entries.data(), entries.size(),
                                 Z_DEFAULT_COMPRESSION) == Z_OK);
                comp.resize(clen);

                bytes = v2_header();
                bytes.insert(bytes.end(), rbct, rbct + 4);
                be32w(bytes, 1);
                bytes.insert(bytes.end(), rblk, rblk + 4);
                be32w(bytes, static_cast<uint32_t>(clen));
                bytes.insert(bytes.end(), comp.begin(), comp.end());
                const uint64_t next = bytes.size();
                if (next == data_off) break;
                data_off = next;
            }
            bytes.push_back('G');
            bytes.push_back('O');
            write_file(base / "mb_entry_trunc.mdd", bytes);

            assert(p.load((base / "mb_entry_trunc.mdd").string()));
            assert(p.get_resource_as_string("good") == "GO");
            assert(!p.has_resource("bad"));
        }
        // 块内条目足够多 → zlib 输出超过 2x 预估触发扩容
        {
            std::vector<std::pair<std::string, std::string>> many;
            for (int n = 0; n < 50; ++n)
                many.emplace_back("img/" + std::to_string(n) + ".gif",
                                  "payload-" + std::to_string(n));
            auto bytes = build_v2_mdd(many, true);
            write_file(base / "mb_many.mdd", bytes);
            assert(p.load((base / "mb_many.mdd").string()));
            assert(p.resource_count() == 50);
            assert(p.get_resource_as_string("img/49.gif") == "payload-49");
        }
        // 条目 offset 指向文件尾外 → get_resource 读失败返回空
        {
            auto bytes = build_v2_mdd({{"x.png", "XY"}});
            uint64_t far_off = bytes.size() + 100;
            for (int i = 7; i >= 0; --i)
                bytes[kV2HeaderLen + 2 + 5 + i] =
                    static_cast<unsigned char>((far_off >> (i * 8)) & 0xFF);
            write_file(base / "far_offset.mdd", bytes);
            assert(p.load((base / "far_offset.mdd").string()));
            assert(p.get_resource("x.png").empty());
            assert(p.get_resource_as_string("x.png").empty());
        }
    }

    // ===== MddResourceManager：坏 offset 条目走缓存前失败分支 =====
    {
        MddResourceManager mgr;
        mgr.set_cache_directory((base / "cache_mgr2").string());
        assert(mgr.load_mdd((base / "far_offset.mdd").string(), "d2"));
        assert(mgr.has_mdd("d2"));
        assert(mgr.get_resource_path("x.png", "d2").empty());
        assert(mgr.get_resource_data("x.png", "d2").empty());
        assert(mgr.unload_mdd("d2"));
    }

    // ===== 缓存默认目录（HOME 缺失时落到 /tmp）=====
    {
        const char* home0 = std::getenv("HOME");
        std::string home_backup = home0 ? home0 : "";
        unset_env("HOME");
        MddResourceCache c;
        assert(c.get_cache_directory() == "/tmp/unidict_mdd_cache");
        set_env("HOME", home_backup);
    }

    // ===== 缓存裁剪分支：免裁 / 半删 =====
    {
        fs::path cdir = base / "cache3";
        MddResourceCache cache(cdir.string());
        std::vector<unsigned char> small(4, 'a');
        std::vector<unsigned char> big(5, 'b');

        // 总量低于上限 → 直接返回，一条不裁
        assert(cache.cache_resource(small, "s1", "image/png"));
        cache.prune_by_size(1u << 20);
        assert(cache.get_cached_count() == 1);

        // 裁到阈值以内即停（break 分支）：删旧留新
        assert(cache.cache_resource(big, "s2", "image/png"));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        assert(cache.cache_resource(small, "s3", "image/png"));
        cache.prune_by_size(5);
        assert(cache.get_cached_count() == 1);
        assert(cache.is_cached("s3") && !cache.is_cached("s1") &&
               !cache.is_cached("s2"));

        // prune_by_age 只删过期项，保留项走继续遍历分支
        std::this_thread::sleep_for(std::chrono::seconds(1));
        assert(cache.cache_resource(small, "a2", "image/png"));
        cache.prune_by_age(0);
        assert(cache.get_cached_count() == 1 && cache.is_cached("a2"));

        // prune_by_access 只删低频项，保留项走继续遍历分支
        cache.increment_access_count("a2");
        cache.increment_access_count("a2");
        assert(cache.cache_resource(small, "c1", "image/png"));
        cache.prune_by_access(3);
        assert(cache.get_cached_count() == 1 && cache.is_cached("a2"));
    }

    // ===== 缓存文件名长度有界 =====
    // cache_key = "<词典id>_<资源键>"。词典 id 在 Qt 层是从绝对路径派生的，
    // 资源键又带目录层级，两者叠加很容易超过 ext4/APFS 的 255 字节单文件名
    // 上限。原实现没有护栏：超长名被 ofstream 拒掉 → cache_resource() 返回
    // false → get_resource_path() 返回空串，.mdd 里明明有图却静默加载不出来。
    {
        const fs::path longnames_dir = base / "longnames";
        MddResourceCache cache(longnames_dir);
        const std::string base =
            "/home/user/Dictionaries/My Very Long Dictionary Collection Folder "
            "Name/Oxford Advanced/OALD9.mdx_sounds/oxford/word_00001_long_english_"
            "pronunciation_file_for_this_specific_word_in_the_oxford_advanced_"
            "learners_dictionary_ninth_edition";
        // 两个超长键：前缀完全相同，只有尾部不同
        const std::string k1 = base + "name_for_alpha_variant_of_this_word_only.mp3";
        const std::string k2 = base + "name_for_beta_variant_of_this_word_only.mp3";
        // 255 = ext4/APFS/NTFS 的单文件名上限；这两个键必须真的越线，
        // 否则这段测的还是"没超长"那条路径，等于没测
        assert(k1.size() > 255 && k2.size() > 255);

        // 关键回归点：原先这里两个都会静默失败
        assert(cache.cache_resource(std::string("A"), k1, "audio/mpeg"));
        assert(cache.cache_resource(std::string("B"), k2, "audio/mpeg"));

        const std::string p1 = cache.get_cached_path(k1);
        const std::string p2 = cache.get_cached_path(k2);
        assert(!p1.empty() && !p2.empty());
        // 名字被压回界内。约束是**单个文件名**分量的长度（255），不是整条
        // 路径，所以直接量 filename() 分量。
        assert(fs::path(p1).filename().string().size() <= 200);
        assert(fs::path(p2).filename().string().size() <= 200);
        // 纯截断会让两者挤到同一个名字（后写的覆盖先写的，音频串台）
        assert(p1 != p2);
        // 落盘的文件真实存在，且各自字节正确 —— 没串
        assert(fs::exists(p1) && fs::exists(p2));
        assert(cache.get_from_cache(k1) == std::vector<uint8_t>{'A'});
        assert(cache.get_from_cache(k2) == std::vector<uint8_t>{'B'});
        // 扩展名要保住：QML 的 Image/Audio 靠它嗅格式
        assert(p1.size() > 4 && p1.compare(p1.size() - 4, 4, ".mp3") == 0);
        assert(p2.size() > 4 && p2.compare(p2.size() - 4, 4, ".mp3") == 0);

        // 短键不该被截断/改名：名字就是斜杠换横杠
        assert(cache.cache_resource(std::string("C"), "pic/a.png", "image/png"));
        assert(cache.get_cached_path("pic/a.png") ==
               (longnames_dir / "pic-a.png").string());

        // 扩展名畸形时（压根没有点，或"扩展名"长到不像扩展名）不能硬拼——
        // 否则会把键的最后十几个字符当扩展名留在名字尾巴上。这里两种都验：
        //   k3 完全无点（真实 .mdd 里有 sound_00001 这种无扩展名资源键）
        //   k4 的点后缀 13 字节，超过 12 的判定阈值
        const std::string k3 = base + "no_extension_at_all_in_this_key";
        const std::string k4 = base + "dot.then_a_ridiculously_long_thing";
        assert(cache.cache_resource(std::string("D"), k3, "application/octet-stream"));
        assert(cache.cache_resource(std::string("E"), k4, "application/octet-stream"));
        const fs::path n3 = fs::path(cache.get_cached_path(k3));
        const fs::path n4 = fs::path(cache.get_cached_path(k4));
        assert(n3.filename().string().size() <= 200);
        assert(n4.filename().string().size() <= 200);
        // 各自独立、字节正确
        assert(cache.get_cached_path(k3) != cache.get_cached_path(k4));
        assert(cache.get_from_cache(k3) == std::vector<uint8_t>{'D'});
        assert(cache.get_from_cache(k4) == std::vector<uint8_t>{'E'});
    }

    std::cout << "OK\n";
    return 0;
}
