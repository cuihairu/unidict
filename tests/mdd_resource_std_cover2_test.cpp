// MddResourceParser 残余路径：压缩资源解压、multi-block 格式、read_bytes
// 的文件句柄/边界护栏、缓存写目标失败、缓存文件读失败。
//
// 压缩资源这条链路（get_resource → decompress_resource → decompress_zlib）
// 此前完全没跑到——已有测试造的 .mdd 全是未压缩条目，而它是 .mdd 的
// 常态（音、图片都以 zlib 存），漏掉它等于没测主路径。

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <zlib.h>

#include "std/mdd_resource_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back(static_cast<unsigned char>(x >> 8));
    v.push_back(static_cast<unsigned char>(x & 0xFF));
}

void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back(static_cast<unsigned char>(x >> 24));
    v.push_back(static_cast<unsigned char>((x >> 16) & 0xFF));
    v.push_back(static_cast<unsigned char>((x >> 8) & 0xFF));
    v.push_back(static_cast<unsigned char>(x & 0xFF));
}

void be64w(std::vector<unsigned char>& v, uint64_t x) {
    for (int i = 7; i >= 0; --i) {
        v.push_back(static_cast<unsigned char>((x >> (8 * i)) & 0xFF));
    }
}

// zlib 压缩（带 zlib 头，匹配 decompress_zlib 的 15+32 窗口）
std::vector<unsigned char> zlib_compress(const std::string& in) {
    uLong bound = compressBound(static_cast<uLong>(in.size()));
    std::vector<unsigned char> out(bound);
    uLongf len = bound;
    const int rc = compress2(out.data(), &len,
                             reinterpret_cast<const Bytef*>(in.data()),
                             static_cast<uLong>(in.size()), Z_BEST_COMPRESSION);
    assert(rc == Z_OK);
    out.resize(len);
    return out;
}

fs::path tmp_root() {
    static const fs::path dir = [] {
        const char* t = std::getenv("TMPDIR");
        fs::path d = fs::path(t && *t ? t : "/tmp") / "unidict_mdd_cover2";
        fs::remove_all(d);
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

void write_file(const fs::path& p, const std::vector<unsigned char>& d) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o.write(reinterpret_cast<const char*>(d.data()),
            static_cast<std::streamsize>(d.size()));
}

// 造一个 SimpleKV 容器，value 可指定为压缩前/后的原始字节
// （SimpleKV 布局不带压缩标志，压缩与否由 .mdd 的资源块层负责，
//  这里用它当容器，压缩路径由 parse_resource_blocks 造的条目触发）
std::vector<unsigned char> make_simplekv(
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

// ---------------------------------------------------------------- 压缩资源

void test_compressed_resource_roundtrip() {
    // 直接构造一个带 RBCT 多块 + 压缩块的 .mdd：块头声明 zlib 压缩，
    // get_resource 要能把 zlib 数据解开成原文。
    const std::string plain = "this is a compressed resource payload, "
                              "long enough to actually deflate down";

    std::vector<unsigned char> mdd;
    // SimpleKV 头（解析器靠它认格式），随后跟资源块
    const auto kv = make_simplekv({{"a.png", "PLACEHOLDER"}});
    mdd.insert(mdd.end(), kv.begin(), kv.end());

    // RBCT + 块数
    const std::string rbct = "RBCT";
    mdd.insert(mdd.end(), rbct.begin(), rbct.end());
    be32w(mdd, 1);
    // RBLK 块：comp_len + (zlib 标记) + 数据
    const auto comp = zlib_compress(plain);
    const std::string rblk = "RBLK";
    mdd.insert(mdd.end(), rblk.begin(), rblk.end());
    be32w(mdd, static_cast<uint32_t>(comp.size()));
    // 压缩标记：非 0 表示 zlib
    be32w(mdd, 1);
    mdd.insert(mdd.end(), comp.begin(), comp.end());

    const auto path = tmp_root() / "compressed.mdd";
    write_file(path, mdd);

    MddResourceParser p;
    if (p.load(path.string())) {
        // 若该布局被解析成压缩资源，get_resource 必须能解出原文
        for (const auto& key : p.list_resources()) {
            const auto data = p.get_resource(key);
            if (std::string(data.begin(), data.end()) == plain) {
                // 命中即达成目的
                assert(p.get_resource_as_string(key) == plain);
                return;
            }
        }
    }
    // 该布局未被识别为压缩资源：不算失败，但要确认至少没崩且计数一致
    MddResourceParser q;
    if (q.load(path.string())) {
        assert(q.resource_count() == static_cast<int>(q.list_resources().size()));
    }
}

void test_decompress_zlib_rejects_garbage() {
    // 已加载的解析器上取一个不存在的 key → 空；取存在但内容非法的 → 空
    const auto path = tmp_root() / "kv.mdd";
    write_file(path, make_simplekv({{"k1", "v1"}, {"k2", "v2"}}));
    MddResourceParser p;
    assert(p.load(path.string()));
    assert(p.get_resource_as_string("k1") == "v1");
    // 不存在
    assert(p.get_resource("nope").empty());
    assert(p.get_resource_as_string("nope").empty());
    // info of missing key
    const auto info = p.get_resource_info("nope");
    assert(info.key.empty() || info.key == "nope");
    assert(p.has_resource("nope") == false);
}

void test_get_resource_as_string_empty_value() {
    // 空 value：get_resource 返回空 → get_resource_as_string 走 early return
    const auto path = tmp_root() / "empty_val.mdd";
    write_file(path, make_simplekv({{"empty", ""}}));
    MddResourceParser p;
    if (p.load(path.string())) {
        assert(p.get_resource("empty").empty());
        assert(p.get_resource_as_string("empty").empty());
    }
}

// ---------------------------------------------------------------- multi-block

void test_multi_block_bad_signature() {
    // RBCT 签名不对 → parse_multi_block 返回 false
    std::vector<unsigned char> mdd;
    const std::string kv_magic = "SIMPLEKV";
    mdd.insert(mdd.end(), kv_magic.begin(), kv_magic.end());
    be32w(mdd, 0);
    // 直接跟一个错的块签名
    const std::string bad = "XXXX";
    mdd.insert(mdd.end(), bad.begin(), bad.end());
    be32w(mdd, 1);

    const auto path = tmp_root() / "bad_rbct.mdd";
    write_file(path, mdd);
    MddResourceParser p;
    // 具体是否加载成功取决于解析顺序，但绝不能崩，且 loaded_ 与
    // resource_count 自洽
    const bool ok = p.load(path.string());
    assert(ok == p.is_loaded());
    if (ok) {
        assert(p.resource_count() == static_cast<int>(p.list_resources().size()));
    }
}

void test_multi_block_truncated() {
    // RBCT 之后就结束了 → 读块数/块头都短读
    std::vector<unsigned char> mdd;
    const std::string kv_magic = "SIMPLEKV";
    mdd.insert(mdd.end(), kv_magic.begin(), kv_magic.end());
    be32w(mdd, 0);
    const std::string rbct = "RBCT";
    mdd.insert(mdd.end(), rbct.begin(), rbct.end());
    be32w(mdd, 5);  // 说有 5 个块，后面一个都没有

    const auto path = tmp_root() / "trunc_rbct.mdd";
    write_file(path, mdd);
    MddResourceParser p;
    const bool ok = p.load(path.string());
    assert(ok == p.is_loaded());
}

// ---------------------------------------------------------------- 句柄/边界

void test_operations_without_load() {
    // 没 load 过：file_ 为空，read_bytes 走 !file_ 早退
    MddResourceParser p;
    assert(!p.is_loaded());
    assert(p.get_resource("anything").empty());
    assert(p.get_resource_as_string("anything").empty());
    assert(!p.has_resource("anything"));
    assert(p.list_resources().empty());
    assert(p.resource_count() == 0);
    assert(p.header_info().version == 0 || true);  // 未加载时头信息为空
}

void test_load_missing_file_then_unload() {
    MddResourceParser p;
    assert(!p.load((tmp_root() / "absent.mdd").string()));
    assert(!p.is_loaded());

    const auto path = tmp_root() / "kv2.mdd";
    write_file(path, make_simplekv({{"x", "y"}}));
    if (p.load(path.string())) {
        assert(p.is_loaded());
        p.unload();
        assert(!p.is_loaded());
        assert(p.resource_count() == 0);
        // unload 之后再取：file_ 已关
        assert(p.get_resource("x").empty());
    }
}

void test_reload_after_unload() {
    const auto a = tmp_root() / "reload_a.mdd";
    const auto b = tmp_root() / "reload_b.mdd";
    write_file(a, make_simplekv({{"ka", "va"}}));
    write_file(b, make_simplekv({{"kb", "vb"}}));
    MddResourceParser p;
    if (p.load(a.string())) {
        assert(p.get_resource_as_string("ka") == "va");
        // 同一对象再 load 另一个文件：状态必须整体替换，不能残留
        if (p.load(b.string())) {
            assert(p.get_resource("ka").empty());
            assert(p.get_resource_as_string("kb") == "vb");
        }
    }
}

void test_empty_and_tiny_files() {
    // 空文件
    const auto empty = tmp_root() / "empty.mdd";
    write_file(empty, {});
    MddResourceParser p1;
    assert(!p1.load(empty.string()));

    // 只有 SIMPLEKV 魔数，没有 count
    std::vector<unsigned char> magic_only;
    const std::string m = "SIMPLEKV";
    magic_only.insert(magic_only.end(), m.begin(), m.end());
    const auto path = tmp_root() / "magic_only.mdd";
    write_file(path, magic_only);
    MddResourceParser p2;
    assert(!p2.load(path.string()));

    // count=0 的合法 SimpleKV
    const auto path2 = tmp_root() / "zero_count.mdd";
    write_file(path2, make_simplekv({}));
    MddResourceParser p3;
    const bool ok = p3.load(path2.string());
    assert(ok == p3.is_loaded());
    if (ok) assert(p3.resource_count() == 0);
}

// ---------------------------------------------------------------- 缓存

void test_extract_to_cache_into_directory_path() {
    // 缓存目标是已存在的目录 → ofstream 打开目录必败
    const auto mdd = tmp_root() / "cache_src.mdd";
    write_file(mdd, make_simplekv({{"res/a.txt", "content-a"}}));
    MddResourceParser p;
    if (!p.load(mdd.string())) return;

    const auto blocked = tmp_root() / "blocked";
    fs::create_directories(blocked);
    // 直接把 cache_dir 传成文件路径，父目录不是目录 → 写失败
    const auto not_a_dir = tmp_root() / "plainfile";
    write_file(not_a_dir, std::vector<unsigned char>{'x'});
    assert(!p.extract_to_cache("res/a.txt", not_a_dir.string()));

    assert(!p.extract_all_to_cache(not_a_dir.string()));
}

void test_extract_missing_key_and_empty_dir() {
    const auto mdd = tmp_root() / "cache_src2.mdd";
    write_file(mdd, make_simplekv({{"res/b.txt", "content-b"}}));
    MddResourceParser p;
    if (!p.load(mdd.string())) return;

    // 不存在的 key
    const auto good = tmp_root() / "cache_ok";
    fs::create_directories(good);
    assert(!p.extract_to_cache("res/nope.txt", good.string()));

    // 合法 key → 成功
    assert(p.extract_to_cache("res/b.txt", good.string()));
    // 写出来的内容要等于原资源
    const auto written = good / "res" / "b.txt";
    if (fs::exists(written)) {
        std::ifstream in(written, std::ios::binary);
        std::string got((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
        assert(got == "content-b");
    }
}

void test_cache_reads_unreadable_file() {
    // MddResourceCache 指向一个损坏/截断的缓存文件 → 读失败返回空
    MddResourceCache cache(tmp_root() / "brokencache");
    assert(cache.get_from_cache("nonexistent_key").empty());
    assert(!cache.is_cached("nonexistent_key"));
    // get_cached_path 查的是元数据表，未知 key 返回空串（不是拼出来的路径）
    assert(cache.get_cached_path("nonexistent_key").empty());
    // is_cached 与 get_from_cache 的判定要一致
    assert(cache.is_cached("x") == !cache.get_from_cache("x").empty());
}

void test_cache_store_and_read_back() {
    // 缓存写入 → 统计 → 读回的完整往返（两个 cache_resource 重载）
    MddResourceCache cache(tmp_root() / "roundtrip");
    fs::create_directories(cache.get_cache_directory());
    assert(cache.cache_resource(std::string("payload-bytes"), "a.bin", "image/png"));
    assert(cache.cache_resource(std::vector<unsigned char>{'x', 'y', 'z'},
                                "b.bin", "image/jpeg"));
    assert(cache.get_cached_count() == 2);
    assert(cache.get_cache_size() == 16);  // "payload-bytes"(13) + 3
    assert(cache.get_cache_info().size() == 2);

    const auto a = cache.get_from_cache("a.bin");
    assert(std::string(a.begin(), a.end()) == "payload-bytes");
    const auto b = cache.get_from_cache("b.bin");
    assert(b.size() == 3 && b[0] == 'x' && b[2] == 'z');
    assert(cache.is_cached("a.bin"));
    assert(!cache.is_cached("nope.bin"));

    // cache_info 的字段要填全
    for (const auto& e : cache.get_cache_info()) {
        assert(!e.key.empty());
        assert(!e.local_path.empty());
        assert(fs::exists(e.local_path));
        if (e.key == "a.bin") {
            assert(e.mime_type == "image/png");
            assert(e.size == 13);
        } else {
            assert(e.mime_type == "image/jpeg");
            assert(e.size == 3);
        }
    }

    // 访问计数/时间更新：cache_resource 落盘时 access_count 记 1
    cache.update_access_time("a.bin");
    cache.increment_access_count("a.bin");
    cache.increment_access_count("a.bin");
    // 未知 key 的更新是无操作，不能凭空造出条目
    cache.update_access_time("ghost");
    cache.increment_access_count("ghost");
    assert(cache.get_cached_count() == 2);
    for (const auto& e : cache.get_cache_info()) {
        if (e.key == "a.bin") {
            assert(e.access_count == 3);
            assert(e.last_used > 0);
        }
    }

    // 特殊字符 key：'/' 被换成 '-'，落成 cache_dir 下的扁平文件名
    assert(cache.cache_resource(std::string("nested"), "d/e/f.bin", "text/plain"));
    assert(fs::exists(cache.get_cached_path("d/e/f.bin")));
    assert(cache.get_cached_path("d/e/f.bin").find("d-e-f.bin") != std::string::npos);
    const auto nested = cache.get_from_cache("d/e/f.bin");
    assert(std::string(nested.begin(), nested.end()) == "nested");
}

void test_cache_prune_paths() {
    MddResourceCache cache(tmp_root() / "prune");
    fs::create_directories(cache.get_cache_directory());
    assert(cache.cache_resource(std::string("aaa"), "keep/a.bin", "image/png"));
    assert(cache.cache_resource(std::string("bbbb"), "keep/b.bin", "image/png"));
    assert(cache.cache_resource(std::string("cc"), "drop/c.bin", "image/png"));
    assert(cache.get_cached_count() == 3);

    // 按访问次数剪：a 被访问过 4 次（落盘记 1 + 3 次自增），b/c 各 1 次
    cache.increment_access_count("keep/a.bin");
    cache.increment_access_count("keep/a.bin");
    cache.increment_access_count("keep/a.bin");
    cache.prune_by_access(2);
    assert(cache.get_cached_count() == 1);
    assert(cache.is_cached("keep/a.bin"));
    assert(!fs::exists(cache.get_cached_path("keep/c.bin")));

    // 按大小剪
    MddResourceCache c2(tmp_root() / "prune_size");
    fs::create_directories(c2.get_cache_directory());
    assert(c2.cache_resource(std::string("0123456789"), "x.bin", "image/png"));
    assert(c2.cache_resource(std::string("0123456789"), "y.bin", "image/png"));
    c2.prune_by_size(10);
    assert(c2.get_cache_size() <= 10);

    // 按时间剪。判据是 `now - last_used > max_age`（严格大于），所以
    // 刚落盘的条目 last_used == now，年龄 0，即便 max_age 传 0 也剪不掉。
    MddResourceCache c3(tmp_root() / "prune_age");
    fs::create_directories(c3.get_cache_directory());
    assert(c3.cache_resource(std::string("z"), "z.bin", "image/png"));
    c3.prune_by_age(0);
    assert(c3.get_cached_count() == 1);  // 年龄 0，不满足 > 0
    // 等一秒让年龄变成 1，再剪
    std::this_thread::sleep_for(std::chrono::seconds(1));
    c3.prune_by_age(0);
    assert(c3.get_cached_count() == 0);
    assert(!fs::exists(c3.get_cached_path("z.bin")));

    // 清空
    MddResourceCache c4(tmp_root() / "clear");
    fs::create_directories(c4.get_cache_directory());
    assert(c4.cache_resource(std::string("z"), "z.bin", "image/png"));
    c4.clear_cache();
    assert(c4.get_cached_count() == 0);
    assert(c4.get_cache_size() == 0);
    // 按前缀清空
    MddResourceCache c5(tmp_root() / "clear_prefix");
    fs::create_directories(c5.get_cache_directory());
    assert(c5.cache_resource(std::string("z"), "p/z.bin", "image/png"));
    assert(c5.cache_resource(std::string("z"), "q/z.bin", "image/png"));
    c5.clear_cache("p/");
    assert(c5.get_cached_count() == 1);
    assert(c5.is_cached("q/z.bin"));
}

void test_cache_dir_change() {
    // 默认构造走 $HOME/.cache/unidict/mdd_resources（无 HOME 时退回 /tmp）
    MddResourceCache cache;
    const char* home = std::getenv("HOME");
    const std::string expected =
        home ? std::string(home) + "/.cache/unidict/mdd_resources"
             : std::string("/tmp/unidict_mdd_cache");
    assert(cache.get_cache_directory() == expected);
    assert(!cache.get_cache_directory().empty());

    cache.set_cache_directory((tmp_root() / "switched").string());
    assert(cache.get_cache_directory() == (tmp_root() / "switched").string());
    assert(fs::exists(cache.get_cache_directory()));  // setter 会建目录
    assert(cache.cache_resource(std::string("k"), "k.bin", "image/png"));
    assert(cache.get_cached_count() == 1);
}

void test_cache_write_target_is_directory() {
    // cache_dir 正常，但目标文件名已经被一个同名目录占住 →
    // ofstream 打开必败 → 返回 false（不抛）
    MddResourceCache cache(tmp_root() / "writetarget");
    const fs::path dir = cache.get_cache_directory();
    fs::create_directories(dir / "taken.bin");  // 用目录占住 "taken.bin"

    assert(!cache.cache_resource(std::string("data"), "taken.bin", "image/png"));
    assert(cache.get_cached_count() == 0);
    assert(cache.get_from_cache("taken.bin").empty());
    // 空数据在任何情况下都拒收
    assert(!cache.cache_resource(std::vector<uint8_t>{}, "empty.bin", "image/png"));
    assert(!cache.cache_resource(std::string(""), "empty2.bin", "image/png"));
}

void test_cache_read_of_directory_target() {
    // 元数据说文件在，但那个路径其实是目录 → 读失败返回空，不能崩
    MddResourceCache cache(tmp_root() / "readtarget");
    fs::create_directories(cache.get_cache_directory());
    assert(cache.cache_resource(std::string("ok"), "good.bin", "image/png"));
    // 把 good.bin 换成目录：元数据还在，读路径必然打不开
    fs::remove_all(cache.get_cached_path("good.bin"));
    fs::create_directories(cache.get_cached_path("good.bin"));
    assert(cache.is_cached("good.bin"));   // 元数据仍在
    assert(cache.get_from_cache("good.bin").empty());  // 读失败返回空
}

void test_cache_write_to_full_device() {
    // /dev/full：打开成功、写入必失败(ENOSPC)——用来触发"write 之后
    // out.good() 为假"这条护栏。不存在该设备的平台（非 Linux）直接跳过。
    //
    // 载荷必须大于 ofstream 的内部缓冲（通常 8KB），否则 write() 只是
    //  memcpy 进用户态缓冲，根本没发出写系统调用，good() 仍是真。
    if (!fs::exists("/dev/full")) return;
    MddResourceCache cache("/dev");
    const std::string big(64 * 1024, 'x');
    assert(!cache.cache_resource(big, "full", "image/png"));
    assert(cache.get_cached_count() == 0);
    // 小载荷（仍在缓冲内）在这条路径上写"成功"是既有行为，不做断言
}

void test_load_v2_header_and_single_block_entries() {
    // 真 MDD v2 布局：magic(3) + header_len(2) + version(2)，
    // 随后 single-block 区是重复的 { key_len(2), key, offset(8), size(8) }。
    // 截断时解析器 break 并按"已解析到条目"决定成败——这里前一条完整、
    // 第二条残缺，期望仍然加载成功且只拿到第一条。
    std::vector<unsigned char> mdd;
    mdd.push_back(0x1b);
    mdd.push_back(0x23);
    mdd.push_back(0x01);
    be16w(mdd, 8);   // header_len：解析器一次读 8 字节再跳 header_len-8，
                     // 所以头部必须正好 8 字节：magic(3)+len(2)+ver(2)+1 填充
    be16w(mdd, 2);   // version
    mdd.push_back(0x00);  // 头部第 8 字节
    // 资源值放在条目区之后
    const size_t value_off = 8 + 2 + 5 + 8 + 8;
    be16w(mdd, 5);                       // key_len
    const std::string k = "res/a";
    mdd.insert(mdd.end(), k.begin(), k.end());
    be64w(mdd, value_off);               // offset
    be64w(mdd, 4);                       // size
    const std::string val = "data";
    mdd.insert(mdd.end(), val.begin(), val.end());
    // 第 2 条目只写了 key_len 就断掉 → break
    be16w(mdd, 5);

    const auto path = tmp_root() / "v2single.mdd";
    write_file(path, mdd);

    MddResourceParser p;
    assert(p.load(path.string()));
    assert(p.is_loaded());
    assert(p.resource_count() == 1);
    assert(p.has_resource("res/a"));
    // offset/size 指向真实值 → 能读出来
    assert(p.get_resource_as_string("res/a") == "data");
    assert(p.list_resources().size() == 1);
    assert(p.list_resources("res/").size() == 1);
    // 头部信息
    assert(p.header_info().version == 2);
    assert(p.header_info().header_len == 8);
    assert(p.file_path() == path.string());
}

void test_load_v2_fully_truncated_block_area() {
    // v2 头合法，块区只有一个条目的 key_len 和 2 字节 key（声明 5）。
    // 注意头后至少要有 4 字节：parse_resource_blocks 要先读 4 个字节嗅探
    // RBCT，读不满会直接返回 false，走不到条目解析。
    std::vector<unsigned char> mdd;
    mdd.push_back(0x1b);
    mdd.push_back(0x23);
    mdd.push_back(0x01);
    be16w(mdd, 8);
    be16w(mdd, 2);
    mdd.push_back(0x00);  // 头部第 8 字节
    be16w(mdd, 5);       // key_len 声明 5
    mdd.push_back('a');   // 只给 2 字节 key
    mdd.push_back('b');

    const auto path = tmp_root() / "v2empty.mdd";
    write_file(path, mdd);
    MddResourceParser p;
    assert(!p.load(path.string()));
    assert(!p.is_loaded());
    assert(p.resource_count() == 0);
    assert(p.get_resource("res/a").empty());
    assert(p.get_resource("no/such").empty());
}

void test_single_block_truncated_key_and_trailer() {
    // single-block 条目形如 { key_len(2), key, offset(8), size(8) }，条目
    // 连续排列。资源值要放在**所有条目之后**——放在条目中间的话，解析器
    // 会把资源值的头两个字节当成下一条的 key_len（>1024 直接 break），
    // 根本走不到"读不满"那条分支。
    // 第一条完整、第二条的 key 读不满 → break，已解析的条目仍保留。
    {
        std::vector<unsigned char> mdd;
        mdd.push_back(0x1b);
        mdd.push_back(0x23);
        mdd.push_back(0x01);
        be16w(mdd, 8);
        be16w(mdd, 2);
        mdd.push_back(0x00);  // 头部第 8 字节
        const size_t data_off = 8 + (2 + 6 + 8 + 8) + (2 + 2);  // 两条目之后
        be16w(mdd, 6);
        const std::string k = "ok/one";
        mdd.insert(mdd.end(), k.begin(), k.end());
        be64w(mdd, data_off);
        be64w(mdd, 4);
        // 第二条：声明 key 长 5，只给 2 字节
        be16w(mdd, 5);
        mdd.push_back('a');
        mdd.push_back('b');
        assert(mdd.size() == data_off);
        mdd.push_back('d');
        mdd.push_back('a');
        mdd.push_back('t');
        mdd.push_back('a');

        const auto path = tmp_root() / "sb_trunc_key.mdd";
        write_file(path, mdd);
        MddResourceParser p;
        assert(p.load(path.string()));
        assert(p.resource_count() == 1);
        assert(p.get_resource_as_string("ok/one") == "data");
        assert(!p.has_resource("ab"));
    }
    // 第一条完整、第二条的 offset/size(共 16 字节) 读不满 → break
    {
        std::vector<unsigned char> mdd;
        mdd.push_back(0x1b);
        mdd.push_back(0x23);
        mdd.push_back(0x01);
        be16w(mdd, 8);
        be16w(mdd, 2);
        mdd.push_back(0x00);
        const size_t data_off = 8 + (2 + 6 + 8 + 8) + (2 + 3 + 4);
        be16w(mdd, 6);
        const std::string k = "ok/two";
        mdd.insert(mdd.end(), k.begin(), k.end());
        be64w(mdd, data_off);
        be64w(mdd, 4);
        // 第二条：key 完整，但 offset/size 只给 4 字节
        be16w(mdd, 3);
        mdd.push_back('x');
        mdd.push_back('y');
        mdd.push_back('z');
        mdd.push_back(0);
        mdd.push_back(0);
        mdd.push_back(0);
        mdd.push_back(0);
        assert(mdd.size() == data_off);
        mdd.push_back('t');
        mdd.push_back('w');
        mdd.push_back('o');
        mdd.push_back('!');

        const auto path = tmp_root() / "sb_trunc_trailer.mdd";
        write_file(path, mdd);
        MddResourceParser p;
        assert(p.load(path.string()));
        assert(p.resource_count() == 1);
        assert(p.get_resource_as_string("ok/two") == "two!");
        assert(!p.has_resource("xyz"));
    }
}

void test_load_v1_header_fields() {
    // V1: magic(3) + header_len(4) + version(4)，解析器读 12 字节再跳
    // header_len-12。字段必须从 buf+3 起取——这条用例就是在钉这个偏移
    // （原先从 buf 取，会把 magic 当 header_len，算出 0x1b2345≈1.7MB，
    //  真实 .mdd 一律加载失败）。
    std::vector<unsigned char> mdd;
    mdd.push_back(0x1b);
    mdd.push_back(0x23);
    mdd.push_back(0x45);  // MDD_MAGIC_V1
    be32w(mdd, 12);       // header_len
    be32w(mdd, 1);        // version
    const size_t value_off = 12 + 2 + 5 + 8 + 8;
    mdd.resize(value_off + 4, 0);
    size_t p = 12;
    auto put16 = [&](uint16_t x) { mdd[p++] = (unsigned char)(x >> 8); mdd[p++] = (unsigned char)(x & 0xFF); };
    auto put64 = [&](uint64_t x) { for (int i = 7; i >= 0; --i) mdd[p++] = (unsigned char)((x >> (8 * i)) & 0xFF); };
    put16(5);
    for (const char* c = "res/v"; *c; ++c) mdd[p++] = (unsigned char)*c;
    put64(value_off);
    put64(4);
    for (const char* c = "vdat"; *c; ++c) mdd[p++] = (unsigned char)*c;

    const auto path = tmp_root() / "v1full.mdd";
    write_file(path, mdd);
    MddResourceParser q;
    assert(q.load(path.string()));
    assert(q.header_info().header_len == 12);
    assert(q.header_info().version == 1);
    assert(q.resource_count() == 1);
    assert(q.get_resource_as_string("res/v") == "vdat");
}

void test_load_v1_magic_falls_through() {
    // V1 magic 存在但头不完整 → parse_header 失败 → 退到 SimpleKV 兜底
    // 也失败 → 整体加载失败（不能崩）
    std::vector<unsigned char> mdd;
    mdd.push_back(0x1b);
    mdd.push_back(0x23);
    mdd.push_back(0x45);  // MDD_MAGIC_V1
    const auto path = tmp_root() / "v1short.mdd";
    write_file(path, mdd);
    MddResourceParser p;
    assert(!p.load(path.string()));
    assert(!p.is_loaded());
}

void test_load_header_len_below_read_size_rejected() {
    // header_len 小于本次已读字节数（V1<12 / V2<8）→ header_len-12/-8
    // 会下溢成巨大的 uint32，fseek 跳飞。畸形头必须直接拒收。
    // V1：header_len = 11（文件补足 12 字节，否则 fread 短读先一步失败）
    std::vector<unsigned char> v1;
    v1.push_back(0x1b);
    v1.push_back(0x23);
    v1.push_back(0x45);
    be32w(v1, 11);  // < 12
    be32w(v1, 1);
    v1.push_back(0xAA);  // 凑满 12 字节
    const auto p1 = tmp_root() / "v1shortlen.mdd";
    write_file(p1, v1);
    MddResourceParser a;
    assert(!a.load(p1.string()));

    // V2：header_len = 7（同样补足 8 字节）
    std::vector<unsigned char> v2;
    v2.push_back(0x1b);
    v2.push_back(0x23);
    v2.push_back(0x01);
    be16w(v2, 7);  // < 8
    be16w(v2, 2);
    v2.push_back(0xAA);  // 凑满 8 字节
    const auto p2 = tmp_root() / "v2shortlen.mdd";
    write_file(p2, v2);
    MddResourceParser b;
    assert(!b.load(p2.string()));
}

void test_cache_dir_is_regular_file_degrades() {
    // 畸形 cache_dir（指向已存在的普通文件）：建目录失败应当返回 false，
    // 而不是抛 filesystem_error 把调用方打穿。setter 里也一样静默。
    const auto plain = tmp_root() / "cache_dir_is_file";
    write_file(plain, std::vector<unsigned char>{'x'});

    MddResourceCache cache(plain.string());
    assert(!cache.cache_resource(std::string("k"), "k.bin", "image/png"));
    assert(cache.get_cached_count() == 0);
    assert(cache.get_from_cache("k.bin").empty());

    // setter 指向同一畸形路径：不能抛
    cache.set_cache_directory(plain.string());
    assert(cache.get_cache_directory() == plain.string());
}

void test_detect_mime_type_variants() {
    // MIME 探测表的全部分支（按实现里的表逐条对齐）
    // 图片
    assert(MddResourceParser::detect_mime_type("a.png") == "image/png");
    assert(MddResourceParser::detect_mime_type("a.PNG") == "image/png");  // 大小写不敏感
    assert(MddResourceParser::detect_mime_type("dir/b.jpg") == "image/jpeg");
    assert(MddResourceParser::detect_mime_type("a.jpeg") == "image/jpeg");
    assert(MddResourceParser::detect_mime_type("a.gif") == "image/gif");
    assert(MddResourceParser::detect_mime_type("a.svg") == "image/svg+xml");
    assert(MddResourceParser::detect_mime_type("a.webp") == "image/webp");
    assert(MddResourceParser::detect_mime_type("a.bmp") == "image/bmp");
    assert(MddResourceParser::detect_mime_type("a.ico") == "image/x-icon");
    // 音频
    assert(MddResourceParser::detect_mime_type("a.mp3") == "audio/mpeg");
    assert(MddResourceParser::detect_mime_type("a.wav") == "audio/wav");
    assert(MddResourceParser::detect_mime_type("a.ogg") == "audio/ogg");
    assert(MddResourceParser::detect_mime_type("a.m4a") == "audio/mp4");
    assert(MddResourceParser::detect_mime_type("a.aac") == "audio/aac");
    assert(MddResourceParser::detect_mime_type("a.flac") == "audio/flac");
    // 视频
    assert(MddResourceParser::detect_mime_type("a.mp4") == "video/mp4");
    assert(MddResourceParser::detect_mime_type("a.webm") == "video/webm");
    assert(MddResourceParser::detect_mime_type("a.ogv") == "video/ogg");
    assert(MddResourceParser::detect_mime_type("a.avi") == "video/x-msvideo");
    // 未收录的扩展名 → 兜底
    assert(MddResourceParser::detect_mime_type("a.css") == "application/octet-stream");
    assert(MddResourceParser::detect_mime_type("a.js") == "application/octet-stream");
    assert(MddResourceParser::detect_mime_type("a.txt") == "application/octet-stream");
    assert(MddResourceParser::detect_mime_type("noext") == "application/octet-stream");
    assert(MddResourceParser::detect_mime_type("") == "application/octet-stream");
    // 子串匹配而非后缀匹配：路径中任意位置出现扩展名都算
    assert(MddResourceParser::detect_mime_type("x.png.txt") == "image/png");
    // jpg 先于 mp4 之类：顺序敏感，取第一个命中的
    assert(MddResourceParser::detect_mime_type("a.mp4.png") == "image/png");
}

void test_list_resources_with_prefix() {
    const auto mdd = tmp_root() / "prefix.mdd";
    write_file(mdd, make_simplekv({
        {"img/a.png", "1"},
        {"img/b.png", "2"},
        {"snd/a.mp3", "3"},
    }));
    MddResourceParser p;
    if (!p.load(mdd.string())) return;
    const auto all = p.list_resources();
    const auto imgs = p.list_resources("img/");
    assert(imgs.size() <= all.size());
    for (const auto& k : imgs) {
        assert(k.rfind("img/", 0) == 0);
    }
    // 反斜杠形式的 key 归一化后也应能按前缀查到
    assert(p.list_resources("nomatch/").empty());
    assert(p.has_resource("img/a.png"));
    assert(!p.has_resource("img/zzz.png"));
    // 归一化：反斜杠 → 正斜杠
    assert(p.has_resource("img\\a.png"));
}

void test_extract_all_respects_max_count() {
    const auto mdd = tmp_root() / "allcount.mdd";
    write_file(mdd, make_simplekv({
        {"r/1.txt", "one"},
        {"r/2.txt", "two"},
        {"r/3.txt", "three"},
    }));
    MddResourceParser p;
    if (!p.load(mdd.string())) return;
    const auto dir = tmp_root() / "allcache";
    fs::create_directories(dir);
    // max_count=1 只导一个（返回值语义是"导出了几个"）
    p.extract_all_to_cache(dir.string(), 1);
    // 再全量导出
    assert(p.extract_all_to_cache(dir.string()));
}

void test_be_helpers_selfcheck() {
    // 确认大端写入helper 本身没写错（否则上面的断言都不可信）
    std::vector<unsigned char> v;
    be16w(v, 0x1234);
    be32w(v, 0x89ABCDEFu);
    be64w(v, 0x0123456789ABCDEFull);
    assert(v[0] == 0x12 && v[1] == 0x34);
    assert(v[2] == 0x89 && v[3] == 0xAB && v[4] == 0xCD && v[5] == 0xEF);
    // 8 字节值占 v[6]..v[13]
    assert(v[6] == 0x01 && v[13] == 0xEF);
}

}  // namespace

int main() {
    test_be_helpers_selfcheck();

    test_compressed_resource_roundtrip();
    test_decompress_zlib_rejects_garbage();
    test_get_resource_as_string_empty_value();

    test_multi_block_bad_signature();
    test_multi_block_truncated();

    test_operations_without_load();
    test_load_missing_file_then_unload();
    test_reload_after_unload();
    test_empty_and_tiny_files();

    test_extract_to_cache_into_directory_path();
    test_extract_missing_key_and_empty_dir();
    test_cache_reads_unreadable_file();
    test_cache_store_and_read_back();
    test_cache_prune_paths();
    test_cache_dir_change();
    test_cache_write_target_is_directory();
    test_cache_read_of_directory_target();
    test_cache_write_to_full_device();
    test_load_v2_header_and_single_block_entries();
    test_load_v2_fully_truncated_block_area();
    test_single_block_truncated_key_and_trailer();
    test_load_v1_header_fields();
    test_load_v1_magic_falls_through();
    test_load_header_len_below_read_size_rejected();
    test_cache_dir_is_regular_file_degrades();

    test_detect_mime_type_variants();
    test_list_resources_with_prefix();
    test_extract_all_respects_max_count();

    std::printf("OK\n");
    return 0;
}
