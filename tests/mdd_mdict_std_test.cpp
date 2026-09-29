// 真实 MDict .mdd（引擎 2.0，writemdict fileformat.md 规格）兼容测试。
//
// 夹具按规格逐字节构造：头 = u32 BE 头长 + UTF-16LE XML 属性串 +
// u32 LE adler32；key 节 = 5×u64 + adler + 压缩索引块 + 压缩 key 块；
// record 节 = 4×u64 + n×(comp,decomp) 对 + 压缩块。键文本 UTF-16LE、
// 0x0000 终止；record offset 指向全部 record 块解压后的拼接流。
// 畸形形态用 Layout 记录的字节偏移做手术（覆写字段/截断），不另造夹具。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

#include "std/mdd_resource_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

// ---------- 字节序助手 ----------
void put_u16le(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>(v >> 8));
}
void put_u16be(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v & 0xff));
}
void put_u32le(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
}
void put_u32be(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 3; i >= 0; --i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
}
void put_u64be(std::vector<uint8_t>& out, uint64_t v) {
    for (int i = 7; i >= 0; --i) out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
}
void set_u32be(std::vector<uint8_t>& b, size_t off, uint32_t v) {
    for (int i = 0; i < 4; ++i) b[off + i] = static_cast<uint8_t>((v >> (8 * (3 - i))) & 0xff);
}
void set_u64be(std::vector<uint8_t>& b, size_t off, uint64_t v) {
    for (int i = 0; i < 8; ++i) b[off + i] = static_cast<uint8_t>((v >> (8 * (7 - i))) & 0xff);
}

// ---------- UTF-16LE 编码（测试侧：UTF-8 → 单元 → LE 字节） ----------
std::vector<uint16_t> utf8_to_units(const std::string& s) {
    std::vector<uint16_t> u;
    size_t i = 0;
    while (i < s.size()) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        uint32_t cp = 0;
        int extra = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F; extra = 1;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F; extra = 2;
        } else {
            cp = c & 0x07; extra = 3;
        }
        for (int k = 0; k < extra; ++k) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[++i]) & 0x3F);
        }
        ++i;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            u.push_back(static_cast<uint16_t>(0xD800 + (cp >> 10)));
            u.push_back(static_cast<uint16_t>(0xDC00 + (cp & 0x3FF)));
        } else {
            u.push_back(static_cast<uint16_t>(cp));
        }
    }
    return u;
}
std::vector<uint8_t> units_to_le(const std::vector<uint16_t>& u) {
    std::vector<uint8_t> b;
    for (uint16_t x : u) put_u16le(b, x);
    return b;
}
std::vector<uint8_t> utf16le(const std::string& s) { return units_to_le(utf8_to_units(s)); }

// ---------- 通用压缩块 { u32 comp_type; u32 adler; payload } ----------
std::vector<uint8_t> wrap_block(uint32_t comp_type, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> out;
    put_u32be(out, comp_type);
    put_u32le(out, adler32(0, payload.data(), static_cast<uInt>(payload.size())));
    if (comp_type == 2) {
        uLongf clen = compressBound(payload.size());
        std::vector<uint8_t> z(clen);
        const Bytef* src = payload.empty()
            ? reinterpret_cast<const Bytef*>("") : payload.data();
        const int rc = compress2(z.data(), &clen, src,
                                 static_cast<uLong>(payload.size()), 6);
        assert(rc == Z_OK);
        z.resize(clen);
        out.insert(out.end(), z.begin(), z.end());
    } else {
        out.insert(out.end(), payload.begin(), payload.end());
    }
    return out;
}

// ---------- 夹具规格 ----------
struct RealKey {
    std::string utf8;            // UTF-8 键名（raw 非空时忽略）
    std::vector<uint16_t> raw;   // 直接给 UTF-16 单元（畸形代理用）
    uint64_t off = 0;            // 解压后 record 流中的 offset
};
RealKey K(const std::string& utf8, uint64_t off) {
    RealKey k;
    k.utf8 = utf8;
    k.off = off;
    return k;
}
RealKey KR(std::vector<uint16_t> raw, uint64_t off) {
    RealKey k;
    k.raw = std::move(raw);
    k.off = off;
    return k;
}
std::vector<uint8_t> vec(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

struct Build {
    std::vector<RealKey> keys;
    std::vector<std::vector<uint8_t>> rec_payloads;  // 每块解压后字节
    std::string header_xml;       // 非空则整体覆盖头 XML
    const char* engine = "2.0";   // nullptr → 省略属性
    const char* encrypted = "0";  // nullptr → 省略属性
    bool key_stored = false;      // key 块 comp_type 0
    bool rec_stored = false;      // record 块 comp_type 0
    bool drop_last_key_term = false;  // 末键去 0x0000 终止符
    size_t info_cut = 0;          // 解压后索引尾裁 N 字节（截断畸形）
};

struct Layout {
    size_t key_start = 0;        // num_key_blocks 字段
    size_t info_start = 0;       // 压缩索引块起点（comp_type 字段）
    size_t key_blocks_start = 0; // 首个 key 块起点（comp_type 字段）
    size_t rec_start = 0;        // num_record_blocks 字段
    size_t rec_info_start = 0;   // (comp,decomp) 对区起点
    size_t rec_blocks_start = 0; // 首个 record 块起点（comp_type 字段）
    size_t rec_pair0 = 0;        // 首对 comp 字段
    size_t rec_pair0_comp = 0;   // 首块原始压缩长（改写 comp 时对账用）
    size_t info_len = 0;         // 解压后索引长度
    size_t total = 0;
};

std::string make_header_xml(const char* engine, const char* encrypted) {
    std::string xml = "<Dictionary ";
    if (engine) xml += std::string("GeneratedByEngineVersion=\"") + engine + "\" ";
    if (encrypted) xml += std::string("Encrypted=\"") + encrypted + "\" ";
    xml += "Title=\"T\" Encoding=\"UTF-16\" />";
    return xml;
}

// 索引块载荷：单 key 块一条 { u64 条目数; u16 首词长; 首词+NUL;
// u16 末词长; 末词+NUL; u64 压缩长; u64 解压长 }
std::vector<uint8_t> build_info(const std::vector<RealKey>& keys,
                                size_t comp_size, size_t decomp_size) {
    const std::vector<uint16_t> first =
        keys.empty() ? std::vector<uint16_t>{}
                     : (keys.front().raw.empty() ? utf8_to_units(keys.front().utf8)
                                                 : keys.front().raw);
    const std::vector<uint16_t> last =
        keys.empty() ? std::vector<uint16_t>{}
                     : (keys.back().raw.empty() ? utf8_to_units(keys.back().utf8)
                                                : keys.back().raw);
    std::vector<uint8_t> p;
    put_u64be(p, keys.size());
    put_u16be(p, static_cast<uint16_t>(first.size()));
    const std::vector<uint8_t> fb = units_to_le(first);
    p.insert(p.end(), fb.begin(), fb.end());
    put_u16le(p, 0);
    put_u16be(p, static_cast<uint16_t>(last.size()));
    const std::vector<uint8_t> lb = units_to_le(last);
    p.insert(p.end(), lb.begin(), lb.end());
    put_u16le(p, 0);
    put_u64be(p, comp_size);
    put_u64be(p, decomp_size);
    return p;
}

std::vector<uint8_t> build_real_mdd(const Build& b, Layout* Lp = nullptr) {
    Layout L;
    const std::string xml = !b.header_xml.empty() ? b.header_xml
                                                  : make_header_xml(b.engine, b.encrypted);
    const std::vector<uint8_t> hb = utf16le(xml);

    std::vector<uint8_t> out;
    put_u32be(out, static_cast<uint32_t>(hb.size()));
    out.insert(out.end(), hb.begin(), hb.end());
    put_u32le(out, adler32(0, hb.data(), static_cast<uInt>(hb.size())));

    // key 块载荷：{ u64 record_offset; UTF-16LE 键; 0x0000 }*
    std::vector<uint8_t> kp;
    for (size_t i = 0; i < b.keys.size(); ++i) {
        put_u64be(kp, b.keys[i].off);
        const std::vector<uint8_t> t = b.keys[i].raw.empty()
            ? utf16le(b.keys[i].utf8) : units_to_le(b.keys[i].raw);
        kp.insert(kp.end(), t.begin(), t.end());
        if (!(b.drop_last_key_term && i + 1 == b.keys.size())) {
            put_u16le(kp, 0);
        }
    }
    const std::vector<uint8_t> key_block = wrap_block(b.key_stored ? 0 : 2, kp);

    std::vector<uint8_t> info = build_info(b.keys, key_block.size(), kp.size());
    if (b.info_cut > 0) {
        info.resize(info.size() > b.info_cut ? info.size() - b.info_cut : 0);
    }
    L.info_len = info.size();
    const std::vector<uint8_t> info_block = wrap_block(2, info);

    L.key_start = out.size();
    put_u64be(out, 1);                  // num_key_blocks（单块）
    put_u64be(out, b.keys.size());      // num_entries
    put_u64be(out, info.size());        // key_block_info_decomp_size
    put_u64be(out, info_block.size());  // key_block_info_size
    put_u64be(out, key_block.size());   // key_block_size
    put_u32be(out, adler32(0, out.data() + L.key_start, 40));  // 节 adler
    L.info_start = out.size();
    out.insert(out.end(), info_block.begin(), info_block.end());
    L.key_blocks_start = out.size();
    out.insert(out.end(), key_block.begin(), key_block.end());

    L.rec_start = out.size();
    std::vector<uint8_t> pairs;
    std::vector<uint8_t> blocks;
    for (const auto& payload : b.rec_payloads) {
        const std::vector<uint8_t> blk = wrap_block(b.rec_stored ? 0 : 2, payload);
        if (L.rec_pair0_comp == 0) L.rec_pair0_comp = blk.size();
        put_u64be(pairs, blk.size());
        put_u64be(pairs, payload.size());
        blocks.insert(blocks.end(), blk.begin(), blk.end());
    }
    put_u64be(out, b.rec_payloads.size());  // num_record_blocks
    put_u64be(out, b.keys.size());          // num_entries
    put_u64be(out, pairs.size());           // record_block_info_size
    put_u64be(out, blocks.size());          // record_block_size
    L.rec_info_start = out.size();
    out.insert(out.end(), pairs.begin(), pairs.end());
    L.rec_pair0 = L.rec_info_start;
    L.rec_blocks_start = out.size();
    out.insert(out.end(), blocks.begin(), blocks.end());

    L.total = out.size();
    if (Lp) *Lp = L;
    return out;
}

fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "mddmdict";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

fs::path write_mdd(const fs::path& dir, const std::string& name,
                   const std::vector<uint8_t>& bytes) {
    const fs::path p = dir / (name + ".mdd");
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    assert(out.good());
    return p;
}

std::string str_of(const std::vector<uint8_t>& v) {
    return std::string(v.begin(), v.end());
}

bool load_fails(const fs::path& p) {
    MddResourceParser parser;
    return !parser.load(p.string());
}

} // namespace

// T1 快乐路径：zlib record 块、反斜杠键、归一化查询、头部元信息
static void test_happy_path() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\data\\a.txt", 0), K("\\data\\b.bin", 8), K("\\data\\c.txt", 17)};
    b.rec_payloads = {vec("HELLO-A-"), vec("B-PAYLOAD"), vec("C!")};
    Layout L;
    const fs::path p = write_mdd(dir, "happy", build_real_mdd(b, &L));

    MddResourceParser parser;
    assert(parser.load(p.string()));
    assert(parser.is_loaded());
    assert(parser.header_info().magic == "MDICT");
    assert(parser.header_info().version == 2);
    assert(parser.header_info().num_blocks == 3);
    assert(parser.header_info().header_len == L.key_start);
    assert(parser.header_info().total_size == L.total);
    assert(!parser.header_info().encrypted);
    assert(parser.resource_count() == 3);
    assert(parser.has_resource("\\data\\a.txt"));   // 原样键
    assert(parser.has_resource("data/a.txt"));      // 正斜杠
    assert(parser.has_resource("\\DATA\\A.TXT"));   // 大小写归一
    assert(str_of(parser.get_resource("\\data\\a.txt")) == "HELLO-A-");
    assert(str_of(parser.get_resource("data/b.bin")) == "B-PAYLOAD");
    assert(parser.get_resource_as_string("\\data\\c.txt") == "C!");
    const MddResourceEntry e = parser.get_resource_info("data/b.bin");
    assert(e.offset == 8 && e.size == 9 && e.is_compressed);
    const auto keys = parser.list_resources("data/");
    assert(keys.size() == 3 && keys[0] == "data/a.txt");
    assert(parser.get_resource("nope").empty());
}

// T2 资源跨块：key 块声明 offset 落进下一 record 块，逐块拼装
static void test_spanning_resource() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\x\\long.bin", 0), K("\\x\\tail.bin", 13)};
    b.rec_payloads = {vec("0123456789"), vec("ABCDEF")};  // 总 16 字节
    const fs::path p = write_mdd(dir, "span", build_real_mdd(b));

    MddResourceParser parser;
    assert(parser.load(p.string()));
    // long.bin = 块1 全部 10 字节 + 块2 前 3 字节
    assert(str_of(parser.get_resource("\\x\\long.bin")) == "0123456789ABC");
    assert(str_of(parser.get_resource("\\x\\tail.bin")) == "DEF");
}

// T3 存储块（comp_type 0）：key 块与 record 块都不压缩
static void test_stored_blocks() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\s\\a.txt", 0)};
    b.rec_payloads = {vec("RAW")};
    b.key_stored = true;
    b.rec_stored = true;
    const fs::path p = write_mdd(dir, "stored", build_real_mdd(b));

    MddResourceParser parser;
    assert(parser.load(p.string()));
    assert(str_of(parser.get_resource("\\s\\a.txt")) == "RAW");
}

// T4 单槽块缓存：同块重复查询不重复解压、换块后重建缓存
static void test_block_cache() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\c\\a", 0), K("\\c\\b", 3), K("\\c\\d", 6)};
    b.rec_payloads = {vec("aaa"), vec("bbb"), vec("ddd")};
    const fs::path p = write_mdd(dir, "cache", build_real_mdd(b));

    MddResourceParser parser;
    assert(parser.load(p.string()));
    assert(str_of(parser.get_resource("\\c\\a")) == "aaa");
    assert(str_of(parser.get_resource("\\c\\b")) == "bbb");  // 换块（缓存重建）
    assert(str_of(parser.get_resource("\\c\\b")) == "bbb");  // 同块命中
    assert(str_of(parser.get_resource("\\c\\a")) == "aaa");  // 换回（缓存重建）
}

// T5 UTF-16LE 解码矩阵：1/2/3/4 字节 UTF-8、合法代理对、孤立低代理、
// 尾部截断高代理、高代理后随非低代理（后三者替换为 U+FFFD）
static void test_utf16_matrix() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\img\\\xC3\xA9.png", 0),          // \xC3\xA9 = é（2 字节 UTF-8）
              K("\\img\\\xE4\xB8\xAD.png", 1),          // \xE4\xB8\xAD = 中（3 字节）
              K("\\img\\\xF0\x9F\x98\x80.png", 2),      // U+1F600 表情（代理对 → 4 字节）
              KR({0xDC00, 0x0042}, 3),        // 孤立低代理 → U+FFFD + 'b'
              KR({0x0043, 0xD800}, 4),        // 尾部截断高代理 → 'c' + U+FFFD
              KR({0xD800, 0x0041}, 5)};       // 高代理+非低代理 → U+FFFD + 'a'
    b.rec_payloads = {vec("QWERTY")};
    const fs::path p = write_mdd(dir, "utf16", build_real_mdd(b));

    MddResourceParser parser;
    assert(parser.load(p.string()));
    assert(parser.resource_count() == 6);
    assert(parser.has_resource("\\img/\xC3\xA9.png"));     // img/é.png
    assert(parser.has_resource("\\img/\xE4\xB8\xAD.png")); // img/中.png
    assert(parser.has_resource("\\img/\xF0\x9F\x98\x80.png"));
    assert(parser.has_resource("\xEF\xBF\xBD" "b"));       // U+FFFD b
    assert(parser.has_resource("c\xEF\xBF\xBD"));
    assert(parser.has_resource("\xEF\xBF\xBD" "a"));
    // 每条各占 1 字节，钉住 offset→资源映射
    assert(str_of(parser.get_resource("\xEF\xBF\xBD" "b")) == "R");
}

// T6 头矩阵：引擎版本、加密位、头长 sanity、截断、未闭合引号
static void test_header_matrix() {
    fs::path dir = base_dir();
    // 引擎 1.2（LZO 时代）→ 拒收
    {
        Build b;
        b.engine = "1.2";
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        assert(load_fails(write_mdd(dir, "e12", build_real_mdd(b))));
    }
    // 省略 GeneratedByEngineVersion → 拒收
    {
        Build b;
        b.engine = nullptr;
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        assert(load_fails(write_mdd(dir, "emiss", build_real_mdd(b))));
    }
    // 加密位 1（record 块加密）/ 2（key 信息块加密）→ 拒收
    for (const char* enc : {"1", "2"}) {
        Build b;
        b.encrypted = enc;
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        assert(load_fails(write_mdd(dir, std::string("enc") + enc,
                                    build_real_mdd(b))));
    }
    // 省略 Encrypted（等价未加密）→ 放行
    {
        Build b;
        b.encrypted = nullptr;
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "encmiss",
                                     build_real_mdd(b)).string()));
    }
    // 属性引号未闭合 → engine 取值失败 → 拒收
    {
        Build b;
        b.header_xml = "<Dictionary GeneratedByEngineVersion=\"2.0 />";
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        assert(load_fails(write_mdd(dir, "noquote", build_real_mdd(b))));
    }
    // 头长 sanity：0 / 奇数 / 1MB 上限
    {
        Build b;
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        Layout L;
        std::vector<uint8_t> bytes = build_real_mdd(b, &L);
        set_u32be(bytes, 0, 0);
        assert(load_fails(write_mdd(dir, "hlen0", bytes)));
        set_u32be(bytes, 0, 3);  // 奇数（不是 UTF-16）
        assert(load_fails(write_mdd(dir, "hlenodd", bytes)));
        set_u32be(bytes, 0, 0x00200000);  // 2MB > 1MB 上限
        assert(load_fails(write_mdd(dir, "hlenbig", bytes)));
    }
    // 头文本短读 / 校验和短读
    {
        Build b;
        b.keys = {K("\\a", 0)};
        b.rec_payloads = {vec("x")};
        Layout L;
        std::vector<uint8_t> bytes = build_real_mdd(b, &L);
        size_t ht = L.total - 2;  // 比可用字节（total-4）大：文本必读不完
        if (ht % 2 != 0) ht -= 1;
        set_u32be(bytes, 0, static_cast<uint32_t>(ht));
        assert(load_fails(write_mdd(dir, "textshort", bytes)));
        size_t h2 = L.total - 6;  // 文本读完剩 1~2 字节，校验和凑不齐 4
        if (h2 % 2 != 0) h2 += 1;
        set_u32be(bytes, 0, static_cast<uint32_t>(h2));
        assert(load_fails(write_mdd(dir, "sumshort", bytes)));
    }
}

// T7 key/record 节畸形矩阵：全部经 load 全链驱动（节解析为 private）
static void test_section_matrix() {
    fs::path dir = base_dir();
    // 标准单键文件：键 "\d\a.txt" = 8 单元 → 索引载荷 64 字节
    //（8 + 2+18 + 2+18 + 16），截断算术依赖这个长度
    const auto make_std = [](Layout& L) {
        Build b;
        b.keys = {K("\\d\\a.txt", 0)};
        b.rec_payloads = {vec("v")};
        return build_real_mdd(b, &L);
    };

    // 索引解压尾裁矩阵（四条截断臂各命中一条）
    for (const auto& [cut, name] : std::vector<std::pair<size_t, const char*>>{
             {64 - 4, "info_head"},    // 剩 4：条目数读到一半
             {64 - 9, "info_first"},   // 剩 9：首词长度截断
             {64 - 28, "info_last"},   // 剩 28：末词长度截断
             {1, "info_tail"}}) {      // 剩 63：压缩/解压长截断
        Build b;
        b.keys = {K("\\d\\a.txt", 0)};
        b.rec_payloads = {vec("v")};
        b.info_cut = cut;
        assert(load_fails(write_mdd(dir, name, build_real_mdd(b))));
    }

    // key 节头 44 字节读到一半（文件截在节头中间）
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        bytes.resize(L.key_start + 10);
        assert(load_fails(write_mdd(dir, "nshort", bytes)));
    }
    // 索引解压尺寸与节头声明不符（info_decomp_size 谎报）
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.key_start + 16, L.info_len + 1);
        assert(load_fails(write_mdd(dir, "idcmp", bytes)));
    }
    // key 节头字段手术
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.key_start, 0);  // num_key_blocks 0
        assert(load_fails(write_mdd(dir, "nkb0", bytes)));
        set_u64be(bytes, L.key_start, 2);  // 索引只有 1 条 → 第二条截断
        assert(load_fails(write_mdd(dir, "nkb2", bytes)));
        set_u64be(bytes, L.key_start, 2000000);  // 超块数上限
        assert(load_fails(write_mdd(dir, "nkbbig", bytes)));
    }
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.key_start + 32, 1);  // key_block_size 谎报
        assert(load_fails(write_mdd(dir, "kbsz", bytes)));
        set_u64be(bytes, L.key_start + 24, 4);  // info_size < 8
        assert(load_fails(write_mdd(dir, "isz4", bytes)));
        set_u64be(bytes, L.key_start + 24, 33u << 20);  // 超 32MB 上限
        assert(load_fails(write_mdd(dir, "iszbig", bytes)));
        set_u64be(bytes, L.key_start + 24, 4096);  // 越过 EOF（真文件更小）
        assert(load_fails(write_mdd(dir, "iszeof", bytes)));
    }
    // 索引块本体：comp_type 未知 / zlib 载荷损坏
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u32be(bytes, L.info_start, 3);
        assert(load_fails(write_mdd(dir, "ict3", bytes)));
        for (size_t i = 8; i < 24; ++i) bytes[L.info_start + i] ^= 0xFF;
        assert(load_fails(write_mdd(dir, "igarbage", bytes)));
    }
    // key 块：comp_type LZO / 载荷损坏 / 越过 EOF
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u32be(bytes, L.key_blocks_start, 1);
        assert(load_fails(write_mdd(dir, "kctlzo", bytes)));
        for (size_t i = 8; i < 24; ++i) bytes[L.key_blocks_start + i] ^= 0xFF;
        assert(load_fails(write_mdd(dir, "kgarbage", bytes)));
        bytes.resize(L.key_blocks_start + 3);
        assert(load_fails(write_mdd(dir, "keof", bytes)));
    }
    // 末键无 0x0000 终止符
    {
        Build b;
        b.keys = {K("\\d\\a.txt", 0)};
        b.rec_payloads = {vec("v")};
        b.drop_last_key_term = true;
        assert(load_fails(write_mdd(dir, "koterm", build_real_mdd(b))));
    }
    // 空 key 列表（合法块、零条目）
    {
        Build b;
        b.keys = {};
        b.rec_payloads = {vec("v")};
        assert(load_fails(write_mdd(dir, "kempty", build_real_mdd(b))));
    }
    // record 节：头截断 / 块数 0 / 块数上限 / info_size 谎报 /
    // block_size 谎报 / 对区截断
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        bytes.resize(L.rec_start + 10);
        assert(load_fails(write_mdd(dir, "rshort", bytes)));
    }
    {
        Build b;
        b.keys = {K("\\d\\a.txt", 0)};
        b.rec_payloads = {};  // num_record_blocks 0
        assert(load_fails(write_mdd(dir, "rzero", build_real_mdd(b))));
    }
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.rec_start, 2000000);
        assert(load_fails(write_mdd(dir, "rbig", bytes)));
        set_u64be(bytes, L.rec_start, 1);
        set_u64be(bytes, L.rec_start + 16, 24);  // != 16*1
        assert(load_fails(write_mdd(dir, "risz", bytes)));
        set_u64be(bytes, L.rec_start + 16, 16);
        set_u64be(bytes, L.rec_start + 24, 1);   // block_size 谎报
        assert(load_fails(write_mdd(dir, "rbsz", bytes)));
        set_u64be(bytes, L.rec_start + 24, L.rec_pair0_comp);
        bytes.resize(L.rec_info_start + 8);      // 对区读到一半
        assert(load_fails(write_mdd(dir, "rieof", bytes)));
    }
}

// T8 取资源防线（加载成功、取时惰性失败）：LZO/未知 comp_type、
// comp_size<8、块越过 EOF、声明解压长谎报、offset 越界、乱序钳 0
static void test_get_resource_guards() {
    fs::path dir = base_dir();
    const auto make_std = [](Layout& L) {
        Build b;
        b.keys = {K("\\g\\a", 0)};
        b.rec_payloads = {vec("ok")};
        return build_real_mdd(b, &L);
    };

    // record 块 comp_type 1（LZO）→ 取资源失败返回空
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u32be(bytes, L.rec_blocks_start, 1);
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "glzo", bytes).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
    // comp_type 3（未知）→ 同上
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u32be(bytes, L.rec_blocks_start, 3);
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "g3", bytes).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
    // 首对 comp_size 改 4（<8 字节块头），block_size 同步对账 → 加载过、取失败
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.rec_pair0, 4);
        set_u64be(bytes, L.rec_start + 24, 4);  // block_size 对账到唯一的块
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "gsmall", bytes).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
    // 块数据越过 EOF（截断文件，惰性读失败）
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        bytes.resize(L.rec_blocks_start + 5);
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "geof", bytes).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
    // 声明解压长 99、实际解出 2 → 尺寸不符防线
    {
        Layout L;
        std::vector<uint8_t> bytes = make_std(L);
        set_u64be(bytes, L.rec_pair0 + 8, 99);
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "glie", bytes).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
    // offset 越过拼接流（999 > 总长 2）→ 找不到覆盖块
    {
        Build b;
        b.keys = {K("\\g\\a", 0), K("\\g\\z", 999)};
        b.rec_payloads = {vec("ok")};
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "goob", build_real_mdd(b)).string()));
        assert(parser.get_resource("\\g\\a").empty());  // 扫到 999 越界
        // 末键 next_off=total(2) < 999 → 钳 0 长度
        assert(parser.get_resource_info("\\g\\z").size == 0);
        assert(parser.get_resource("\\g\\z").empty());
    }
    // 乱序 offset（10 → 5）→ 首键钳 0
    {
        Build b;
        b.keys = {K("\\g\\a", 10), K("\\g\\b", 5)};
        b.rec_payloads = {vec("ok")};
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "grev", build_real_mdd(b)).string()));
        assert(parser.get_resource_info("\\g\\a").size == 0);
        assert(parser.get_resource("\\g\\a").empty());
    }
    // 资源长度超 10MB 上限（声明 offset 差 11MB，块根本不用碰）
    {
        Build b;
        b.keys = {K("\\g\\a", 0), K("\\g\\b", 11u << 20)};
        b.rec_payloads = {vec("ok")};
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "gmax", build_real_mdd(b)).string()));
        assert(parser.get_resource("\\g\\a").empty());
    }
}

// T9 管理器闭环：load_mdd → 数据直取 / 缓存落盘 / 缓存命中
static void test_manager_roundtrip() {
    fs::path dir = base_dir();
    Build b;
    b.keys = {K("\\data\\a.txt", 0), K("\\data\\b.bin", 5)};
    b.rec_payloads = {vec("HELLO"), vec("BBBBBBBBB")};
    const fs::path p = write_mdd(dir, "mgr", build_real_mdd(b));

    MddResourceManager mgr;
    mgr.set_cache_directory((dir / "mc").string());
    assert(mgr.load_mdd(p.string(), "d1"));
    assert(mgr.has_resource("\\DATA\\A.TXT", "d1"));
    assert(str_of(mgr.get_resource_data("data/a.txt", "d1")) == "HELLO");
    const auto names = mgr.list_resources("d1", "data/");
    assert(names.size() == 2 && names[1] == "data/b.bin");

    const std::string cpath = mgr.get_resource_path("data/b.bin", "d1");
    assert(!cpath.empty());
    std::ifstream in(cpath, std::ios::binary);
    std::string got((std::istreambuf_iterator<char>(in)),
                    std::istreambuf_iterator<char>());
    assert(got == "BBBBBBBBB");
    assert(mgr.get_resource_path("data/b.bin", "d1") == cpath);  // 二次命中
    assert(!mgr.has_resource("data/a.txt", "no-such"));
}

// T10 格式判别回归：自定义 V1/V2 魔数与 SimpleKV 兜底不受真实格式影响
static void test_format_discrimination() {
    fs::path dir = base_dir();
    // 自定义 V1：magic 1b2345 + BE32 头长(=条目区起点) + BE32 版本 + 垫到
    // 头长，条目 {be16 klen, key, be64 off, be64 sz}，blob 紧随条目区
    {
        std::vector<uint8_t> b = {0x1b, 0x23, 0x45};
        put_u32be(b, 12);  // header_len ≥ 12 的最小合法值
        put_u32be(b, 1);   // version
        b.push_back('\0'); // 3+4+4+1 = 12，垫到条目区起点
        const std::string key = "kv/a.png";
        put_u16be(b, static_cast<uint16_t>(key.size()));
        b.insert(b.end(), key.begin(), key.end());
        put_u64be(b, 12 + 2 + key.size() + 16);  // blob 绝对偏移
        put_u64be(b, 3);
        b.push_back('X'); b.push_back('Y'); b.push_back('Z');
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "customv1", b).string()));
        assert(parser.header_info().magic == "\x1b\x23\x45");
        assert(str_of(parser.get_resource("kv/a.png")) == "XYZ");
    }
    // SimpleKV：杂前缀 + SIMPLEKV 魔数 + {be32 数量, be16 klen, k, be32 vlen, v}
    {
        std::vector<uint8_t> b('x', 16);  // 前缀 16 字节
        const std::string magic = "SIMPLEKV";
        b.insert(b.end(), magic.begin(), magic.end());
        put_u32be(b, 1);
        const std::string key = "sk/a.css";
        put_u16be(b, static_cast<uint16_t>(key.size()));
        b.insert(b.end(), key.begin(), key.end());
        put_u32be(b, 2);
        b.push_back(';');
        b.push_back('}');
        MddResourceParser parser;
        assert(parser.load(write_mdd(dir, "simplekv", b).string()));
        assert(str_of(parser.get_resource("sk/a.css")) == ";}");
    }
}

int main() {
    test_happy_path();
    test_spanning_resource();
    test_stored_blocks();
    test_block_cache();
    test_utf16_matrix();
    test_header_matrix();
    test_section_matrix();
    test_get_resource_guards();
    test_manager_roundtrip();
    test_format_discrimination();
    std::cout << "OK\n";
    return 0;
}
