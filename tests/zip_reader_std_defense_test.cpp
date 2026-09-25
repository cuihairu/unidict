// ZipReaderStd 防御分支补测：恶意/畸形 zip 的每一条拒收路径。
//
// zip 是外部输入（词典 epub 直接就是用户下的文件），open()/read_entry()
// 里的每一处边界检查都是安全护栏，此前大部分没被测到——89% 行覆盖里缺的
// 15 行几乎全是这些护栏。这份测试逐条构造能触发它们的字节布局。
//
// 覆盖目标：EOCD 自洽性、zip64 拒收、CDE 越界/签名不符/字段溢出、
// LFH 越界与签名不符、条目数据越界、单条目解压上限、deflate 流的
// 损坏/截断/长度不符/CRC 不符。

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/zip_reader_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

void put_u16(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xff));
    out.push_back(static_cast<uint8_t>(v >> 8));
}

void put_u32(std::vector<uint8_t>& out, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
    }
}

void put_str(std::vector<uint8_t>& out, const std::string& s) {
    out.insert(out.end(), s.begin(), s.end());
}

void patch_u16(std::vector<uint8_t>& buf, size_t off, uint16_t v) {
    buf[off] = static_cast<uint8_t>(v & 0xff);
    buf[off + 1] = static_cast<uint8_t>(v >> 8);
}

void patch_u32(std::vector<uint8_t>& buf, size_t off, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        buf[off + static_cast<size_t>(i)] = static_cast<uint8_t>((v >> (8 * i)) & 0xff);
    }
}

uint32_t crc_of(const std::string& data) {
    return crc32(0, reinterpret_cast<const Bytef*>(data.data()),
                 static_cast<uInt>(data.size()));
}

uint32_t read32(const std::vector<uint8_t>& b, size_t off) {
    return static_cast<uint32_t>(b[off]) |
           static_cast<uint32_t>(b[off + 1]) << 8 |
           static_cast<uint32_t>(b[off + 2]) << 16 |
           static_cast<uint32_t>(b[off + 3]) << 24;
}

bool deflate_raw(const std::string& input, std::string& output) {
    z_stream s{};
    if (deflateInit2(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    s.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    s.avail_in = static_cast<uInt>(input.size());
    std::vector<char> buf(deflateBound(&s, static_cast<uLong>(input.size())));
    s.next_out = reinterpret_cast<Bytef*>(buf.data());
    s.avail_out = static_cast<uInt>(buf.size());
    const int ret = deflate(&s, Z_FINISH);
    deflateEnd(&s);
    if (ret != Z_STREAM_END) return false;
    output.assign(buf.data(), buf.size() - s.avail_out);
    return true;
}

// 可捏造的 zip 构造器。kNoOverride 表示"用真值"。
// 两个坑，都踩过：
//  1) 32 位与 16 位字段要用各自的哨兵——同一个 0xFFFFFFFF 塞进 uint16_t
//     会被截成 0xFFFF，哨兵比较随之失效（曾因此把默认 name_length 写成 0xFFFF）。
//  2) 哨兵不能取 0xFFFFFFFF——它同时是 zip64 的 cd_offset/cd_size 标志值，
//     而那正是本文件要测的输入，哨兵与被测值撞车会让覆盖悄悄失效。
constexpr uint32_t kNoOverride = 0xDEADBEEFu;
constexpr uint16_t kNoOverride16 = 0xBEEFu;

struct ZipSpec {
    std::string name = "a.txt";
    std::string data = "hello";
    bool use_deflate = false;

    // CDE 字段覆盖
    uint32_t cde_crc = kNoOverride;
    uint32_t cde_compressed_size = kNoOverride;
    uint32_t cde_uncompressed_size = kNoOverride;
    uint32_t cde_local_offset = kNoOverride;
    uint16_t cde_name_length = kNoOverride16;
    uint16_t method_override = kNoOverride16;

    // LFH 字段覆盖
    uint32_t lfh_crc = kNoOverride;
    uint32_t lfh_compressed_size = kNoOverride;
    uint32_t lfh_uncompressed_size = kNoOverride;

    // EOCD 字段覆盖
    uint32_t eocd_cd_offset = kNoOverride;
    uint32_t eocd_cd_size = kNoOverride;
    uint16_t eocd_entry_count = kNoOverride16;
    uint16_t eocd_comment_length = kNoOverride16;
};

// 便捷构造：deflate 条目（成员名不能叫 deflate，会撞 zlib 的 deflate()）
ZipSpec deflated() {
    ZipSpec s;
    s.use_deflate = true;
    return s;
}

std::vector<uint8_t> build_zip(const ZipSpec& s) {
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central;

    std::string payload = s.data;
    uint16_t method = s.use_deflate ? 8 : 0;
    if (s.method_override != kNoOverride16) method = s.method_override;
    if (s.use_deflate) {
        assert(deflate_raw(s.data, payload));
    }
    const uint32_t crc = crc_of(s.data);
    const uint32_t stored = static_cast<uint32_t>(payload.size());
    const uint32_t plain = static_cast<uint32_t>(s.data.size());

    const uint32_t local_offset = static_cast<uint32_t>(archive.size());
    put_u32(archive, 0x04034b50u);
    put_u16(archive, 20);
    put_u16(archive, 0);
    put_u16(archive, s.use_deflate ? 8 : 0);
    put_u16(archive, 0);
    put_u16(archive, 0);
    put_u32(archive, s.lfh_crc == kNoOverride ? crc : s.lfh_crc);
    put_u32(archive, s.lfh_compressed_size == kNoOverride ? stored : s.lfh_compressed_size);
    put_u32(archive, s.lfh_uncompressed_size == kNoOverride ? plain : s.lfh_uncompressed_size);
    put_u16(archive, static_cast<uint16_t>(s.name.size()));
    put_u16(archive, 0);
    put_str(archive, s.name);
    put_str(archive, payload);

    put_u32(central, 0x02014b50u);
    put_u16(central, 20);
    put_u16(central, 20);
    put_u16(central, 0);
    put_u16(central, method);
    put_u16(central, 0);
    put_u16(central, 0);
    put_u32(central, s.cde_crc == kNoOverride ? crc : s.cde_crc);
    put_u32(central, s.cde_compressed_size == kNoOverride ? stored : s.cde_compressed_size);
    put_u32(central, s.cde_uncompressed_size == kNoOverride ? plain : s.cde_uncompressed_size);
    put_u16(central, s.cde_name_length == kNoOverride16
                          ? static_cast<uint16_t>(s.name.size())
                          : s.cde_name_length);
    put_u16(central, 0);  // extra
    put_u16(central, 0);  // comment
    put_u16(central, 0);
    put_u16(central, 0);
    put_u32(central, 0);
    put_u32(central, s.cde_local_offset == kNoOverride ? local_offset : s.cde_local_offset);
    put_str(central, s.name);

    const size_t cd_start = archive.size();
    archive.insert(archive.end(), central.begin(), central.end());
    const uint32_t cd_size = static_cast<uint32_t>(central.size());
    const size_t eocd = archive.size();
    put_u32(archive, 0x06054b50u);
    put_u16(archive, 0);
    put_u16(archive, 0);
    put_u16(archive, s.eocd_entry_count == kNoOverride16 ? 1 : s.eocd_entry_count);
    put_u16(archive, s.eocd_entry_count == kNoOverride16 ? 1 : s.eocd_entry_count);
    put_u32(archive, s.eocd_cd_size == kNoOverride ? cd_size : s.eocd_cd_size);
    put_u32(archive, s.eocd_cd_offset == kNoOverride
                         ? static_cast<uint32_t>(cd_start)
                         : s.eocd_cd_offset);
    put_u16(archive, s.eocd_comment_length == kNoOverride16 ? 0 : s.eocd_comment_length);
    (void)eocd;
    return archive;
}

fs::path tmp_root() {
    static const fs::path dir = [] {
        const char* t = std::getenv("TMPDIR");
        fs::path d = fs::path(t && *t ? t : "/tmp") / "unidict_zip_defense";
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

// 写盘并跑 open()，返回是否加载成功
bool try_open(const std::string& file, const std::vector<uint8_t>& bytes) {
    std::ofstream out(tmp_root() / file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    return z.open((tmp_root() / file).string());
}

// ---------------------------------------------------------------- 基线

void test_baseline_valid_zip_loads() {
    assert(try_open("ok_stored.zip", build_zip(ZipSpec{})));
    assert(try_open("ok_deflate.zip", build_zip(deflated())));
}

// ---------------------------------------------------------------- 文件层

void test_reject_tiny_file() {
    // 比空 EOCD(22B) 还小 → 直接拒
    assert(!try_open("tiny.zip", std::vector<uint8_t>(10, 0x41)));
    // 正好 22 字节但不是合法 EOCD
    assert(!try_open("empty_eocd.zip", std::vector<uint8_t>(22, 0x00)));
}

void test_reject_directory_or_unreadable() {
    // 打不开的路径
    ZipReaderStd z;
    assert(!z.open((tmp_root() / "no_such_file.zip").string()));
    // 目录：ifstream 能打开但读不出内容
    fs::create_directories(tmp_root() / "adir");
    ZipReaderStd z2;
    assert(!z2.open((tmp_root() / "adir").string()));
}

// ---------------------------------------------------------------- EOCD 层

void test_reject_absent_eocd() {
    // 合法 zip 去掉尾部 22 字节 → 扫不到 EOCD
    auto bytes = build_zip(ZipSpec{});
    bytes.resize(bytes.size() - 22);
    assert(!try_open("no_eocd.zip", bytes));
}

void test_reject_eocd_comment_length_inconsistent() {
    // EOCD 注释长度写成非 0，但文件里没有那么多注释字节 →
    // i + 22 + comment_length != size → 视为不自洽，扫不到
    ZipSpec s;
    s.eocd_comment_length = 5;
    assert(!try_open("eocd_comment_mismatch.zip", build_zip(s)));

    // 自洽的注释（补足真实字节）→ 能加载。
    // 注意 EOCD 不在文件尾了：追加注释后 EOCD 偏移不变，注释长度字段
    // 仍在 EOCD+20 处，不能按 bytes.size()-2 定位。
    std::vector<uint8_t> bytes = build_zip(ZipSpec{});
    const size_t eocd_off = bytes.size() - 22;
    put_str(bytes, "12345");
    patch_u16(bytes, eocd_off + 20, 5);
    assert(try_open("eocd_comment_ok.zip", bytes));
}

void test_reject_zip64_markers() {
    // cd_offset 写成 0xFFFFFFFF（zip64 标志）→ 明确拒收
    {
        ZipSpec s;
        s.eocd_cd_offset = 0xFFFFFFFFu;
        assert(!try_open("zip64_offset.zip", build_zip(s)));
    }
    // cd_size 写成 0xFFFFFFFF
    {
        ZipSpec s;
        s.eocd_cd_size = 0xFFFFFFFFu;
        assert(!try_open("zip64_size.zip", build_zip(s)));
    }
}

void test_reject_cd_out_of_bounds() {
    // cd_offset + cd_size 越过 EOCD → central directory 放不下
    ZipSpec s;
    s.eocd_cd_size = 0xFFFFu;
    assert(!try_open("cd_oob.zip", build_zip(s)));
}

// ---------------------------------------------------------------- CDE 层

void test_reject_cde_signature_mismatch() {
    // 让 CDE 头的签名不是 0x02014b50：把 cd_offset 指向中央目录前一个字节
    // 之外的合法位置——直接改中央目录首 4 字节更直接
    std::vector<uint8_t> bytes = build_zip(ZipSpec{});
    // 中央目录起点 = 归档总长 - 22 - cd_size；这里靠签名扫描定位：
    // 找第一个 0x02014b50 并破坏它
    bool patched = false;
    for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (read32(bytes, i) == 0x02014b50u) {
            patch_u32(bytes, i, 0xDEADBEEFu);
            patched = true;
            break;
        }
    }
    assert(patched);
    assert(!try_open("cde_sig.zip", bytes));
}

void test_reject_cde_field_overflow() {
    // CDE 的 name_length 谎报成超大值 → 46+name+extra+comment 越过 CD 末尾
    ZipSpec s;
    s.cde_name_length = 0xFFFFu;
    assert(!try_open("cde_name_overflow.zip", build_zip(s)));
}

void test_reject_unsupported_method() {
    // method=99（WinZip AES 等）→ 明确拒收
    ZipSpec s;
    s.method_override = 99;
    assert(!try_open("method99.zip", build_zip(s)));
    // method=12（bzip2）同样拒收
    ZipSpec s2;
    s2.method_override = 12;
    assert(!try_open("method12.zip", build_zip(s2)));
}

void test_reject_entry_count_mismatch() {
    // 声明 2 个条目但只有 1 个 CDE → 第二次循环签名不符
    ZipSpec s;
    s.eocd_entry_count = 2;
    assert(!try_open("count2.zip", build_zip(s)));
    // 声明 0 个条目 → 加载成功但一个条目都没有
    ZipSpec s2;
    s2.eocd_entry_count = 0;
    std::ofstream out(tmp_root() / "count0.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s2);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "count0.zip").string()));
    assert(z.entry_names().empty());
}

// ---------------------------------------------------------------- LFH/数据层

void test_reject_lfh_offset_out_of_bounds() {
    // local_header_offset 指到文件尾附近 → offset + 30 越界
    ZipSpec s;
    s.cde_local_offset = 0xFFFFu;
    std::ofstream out(tmp_root() / "lfh_oob.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "lfh_oob.zip").string()));  // CDE 层过得去
    std::string out_text;
    assert(!z.read_entry("a.txt", out_text));                // 读条目时才炸
}

void test_reject_lfh_signature_mismatch() {
    // local_header_offset 有效但那个位置的签名不对（指向中央目录开头）
    auto bytes = build_zip(ZipSpec{});
    // 中央目录起点：找 0x02014b50
    size_t cd = 0;
    for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (read32(bytes, i) == 0x02014b50u) { cd = i; break; }
    }
    assert(cd > 30);
    // 构造一个 zip：CD 里写的 local offset 指向中央目录
    ZipSpec s;
    s.cde_local_offset = static_cast<uint32_t>(cd);
    std::ofstream out(tmp_root() / "lfh_sig.zip", std::ios::binary | std::ios::trunc);
    const auto b2 = build_zip(s);
    out.write(reinterpret_cast<const char*>(b2.data()),
              static_cast<std::streamsize>(b2.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "lfh_sig.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
    (void)bytes;
}

void test_reject_entry_data_out_of_bounds() {
    // LFH 有效，但 CDE 谎报 compressed_size 远超实际数据长度
    ZipSpec s;
    s.cde_compressed_size = 0x10000u;
    std::ofstream out(tmp_root() / "data_oob.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "data_oob.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_reject_entry_over_max_uncompressed_size() {
    // 声明解压后 256MB+ → 在碰数据之前就拒（谎报 CDE 的防线）
    ZipSpec s;
    s.cde_uncompressed_size = 256ull * 1024 * 1024 + 1;
    std::ofstream out(tmp_root() / "entry_too_big.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "entry_too_big.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));

    // 正好等于上限：不触发这道护栏，会走到 stored 的 size 不符检查
    ZipSpec s2;
    s2.cde_uncompressed_size = 256ull * 1024 * 1024;
    std::ofstream out2(tmp_root() / "entry_at_limit.zip", std::ios::binary | std::ios::trunc);
    const auto bytes2 = build_zip(s2);
    out2.write(reinterpret_cast<const char*>(bytes2.data()),
               static_cast<std::streamsize>(bytes2.size()));
    out2.close();
    ZipReaderStd z2;
    assert(z2.open((tmp_root() / "entry_at_limit.zip").string()));
    std::string t2;
    assert(!z2.read_entry("a.txt", t2));
}

void test_reject_stored_size_mismatch() {
    // stored 条目 compressed_size != uncompressed_size → 拒
    ZipSpec s;
    s.data = "hello";
    s.cde_uncompressed_size = static_cast<uint32_t>(s.data.size()) + 1;
    s.lfh_uncompressed_size = s.cde_uncompressed_size;
    std::ofstream out(tmp_root() / "stored_mismatch.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "stored_mismatch.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_reject_stored_crc_mismatch() {
    // stored 且 crc 非 0 但对不上 → 拒
    ZipSpec s;
    s.cde_crc = 0x12345678u;
    s.lfh_crc = s.cde_crc;
    std::ofstream out(tmp_root() / "stored_crc.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "stored_crc.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_stored_zero_crc_skips_verification() {
    // stored 且 crc == 0 → 跳过 CRC 校验（约定：0 表示"不校验"）
    ZipSpec s;
    s.cde_crc = 0;
    s.lfh_crc = 0;
    std::ofstream out(tmp_root() / "stored_crc0.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "stored_crc0.zip").string()));
    std::string text;
    assert(z.read_entry("a.txt", text));
    assert(text == "hello");
}

// ---------------------------------------------------------------- deflate 流

void deflate_zip_roundtrip(const std::string& file, const ZipSpec& s,
                           std::string& text, bool expect_ok) {
    std::ofstream out(tmp_root() / file, std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(s);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / file).string()));
    const bool ok = z.read_entry(s.name, text);
    assert(ok == expect_ok);
}

void test_deflate_ok() {
    std::string text;
    deflate_zip_roundtrip("deflate_ok.zip", deflated(), text, true);
    assert(text == "hello");
}

void test_reject_deflate_invalid_block_type() {
    // raw deflate 第一个字节的 bit0=BFINAL、bit1-2=BTYPE，BTYPE=3 是保留值
    // （RFC 1951）→ inflate 立刻返回 Z_DATA_ERROR。这是"载荷被破坏后
    // inflate 自身报错"的确定触发方式，比乱改字节稳定。
    ZipSpec s;
    s.data = "some compressible text to deflate here";
    s.use_deflate = true;
    auto bytes = build_zip(s);
    const size_t payload_off = 30 + s.name.size();
    assert(payload_off < bytes.size());
    bytes[payload_off] = static_cast<uint8_t>(bytes[payload_off] | 0x06u);  // BTYPE = 3

    std::ofstream out(tmp_root() / "deflate_btype.zip", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "deflate_btype.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_reject_deflate_garbage_payload() {
    // method=8 但载荷被改成非法 deflate 数据 → inflate 报 Z_DATA_ERROR
    // 而不是"长度不符"或"CRC 不符"。用较长的可压缩载荷，破坏其中部的
    // 字节（会打到 Huffman 码字上，产生非法符号）。
    ZipSpec s;
    s.data = std::string(4000, 'a');  // 压得很小，但 deflate 输出仍有几十字节
    s.use_deflate = true;
    auto bytes = build_zip(s);
    // 载荷区间 = [30 + name, CDE 起点)
    size_t cd = 0;
    for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (read32(bytes, i) == 0x02014b50u) { cd = i; break; }
    }
    assert(cd > 0);
    const size_t payload_off = 30 + s.name.size();
    const size_t payload_len = cd - payload_off;
    assert(payload_len >= 8);
    for (size_t i = payload_off + payload_len / 2; i < payload_off + payload_len; ++i) {
        bytes[i] = 0x07;  // 0x07 作为 deflate 码字几乎必然非法
    }
    std::ofstream out(tmp_root() / "deflate_garbage.zip", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "deflate_garbage.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_reject_deflate_truncated_stream() {
    // 把 deflate 载荷砍掉一半 → 输入耗尽但流未结束
    ZipSpec s;
    s.data = std::string(400, 'x');  // 压得动，砍一半仍能解出部分
    s.use_deflate = true;
    auto bytes = build_zip(s);
    // 找到载荷结束位置 = 中央目录起点
    size_t cd = 0;
    for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (read32(bytes, i) == 0x02014b50u) { cd = i; break; }
    }
    assert(cd > 40);
    // 从载荷尾部削掉 3 字节：把中央目录整体前移，LFH/CDE 的 size 同步改小
    const size_t payload_off = 30 + 5;
    const size_t payload_len = cd - payload_off;
    assert(payload_len > 4);
    const uint32_t new_payload_len = static_cast<uint32_t>(payload_len - 3);
    bytes.erase(bytes.begin() + static_cast<std::ptrdiff_t>(cd),
                bytes.begin() + static_cast<std::ptrdiff_t>(cd) + 3);
    // LFH 的 compressed/uncompressed 不用改（读条目只看 CDE 的值，
    // 但 CDE 里 LFH offset 不变；这里改 CDE 的两个 size）
    // 重新定位中央目录并改 CDE
    size_t cd2 = 0;
    for (size_t i = 0; i + 4 <= bytes.size(); ++i) {
        if (read32(bytes, i) == 0x02014b50u) { cd2 = i; break; }
    }
    patch_u32(bytes, cd2 + 20, new_payload_len);  // compressed_size
    patch_u32(bytes, bytes.size() - 6, static_cast<uint32_t>(cd2));  // eocd cd_offset
    patch_u32(bytes, bytes.size() - 14, static_cast<uint32_t>(bytes.size() - 22 - cd2));  // cd_size

    std::ofstream out(tmp_root() / "deflate_trunc.zip", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "deflate_trunc.zip").string()));
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_reject_deflate_size_mismatch() {
    // CDE 声明的 uncompressed_size 与实际解压长度不符 → inflate_raw 末尾拒
    ZipSpec s;
    s.use_deflate = true;
    s.cde_uncompressed_size = 99;  // 真实是 5
    std::string text;
    deflate_zip_roundtrip("deflate_size.zip", s, text, false);
}

void test_reject_deflate_crc_mismatch() {
    // deflate 条目的 CRC 对不上
    ZipSpec s;
    s.use_deflate = true;
    s.cde_crc = 0x0BADF00Du;
    std::string text;
    deflate_zip_roundtrip("deflate_crc.zip", s, text, false);
}

void test_deflate_zero_crc_skips_verification() {
    // deflate 且 crc == 0 → 跳过 CRC 校验
    ZipSpec s;
    s.use_deflate = true;
    s.cde_crc = 0;
    s.lfh_crc = 0;
    std::string text;
    deflate_zip_roundtrip("deflate_crc0.zip", s, text, true);
    assert(text == "hello");
}

// ---------------------------------------------------------------- 杂项

void test_read_entry_unknown_name_and_unloaded() {
    ZipReaderStd z;
    std::string text;
    // 没 open 过 → loaded_ 为 false
    assert(!z.read_entry("a.txt", text));
    // open 之后再问不存在的条目
    std::ofstream out(tmp_root() / "names.zip", std::ios::binary | std::ios::trunc);
    const auto bytes = build_zip(ZipSpec{});
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    assert(z.open((tmp_root() / "names.zip").string()));
    assert(!z.read_entry("nope.txt", text));
    assert(z.read_entry("a.txt", text));
    assert(text == "hello");
    // entry_names 有序返回
    const auto names = z.entry_names();
    assert(names.size() == 1);
    assert(names[0] == "a.txt");
}

void test_reopen_resets_state() {
    // 先加载成功，再加载一个坏文件 → loaded_ 必须回到 false
    assert(try_open("reopen_good.zip", build_zip(ZipSpec{})));
    ZipReaderStd z;
    assert(z.open((tmp_root() / "reopen_good.zip").string()));
    assert(!z.entry_names().empty());
    assert(!z.open((tmp_root() / "reopen_missing.zip").string()));
    assert(z.entry_names().empty());
    std::string text;
    assert(!z.read_entry("a.txt", text));
}

void test_duplicate_entry_names_last_wins() {
    // 两个同名条目：entries_ 是 map，后写的覆盖 → 只留一个
    // 手工造：两个 LFH + 两个 CDE 同名
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central;
    const std::string name = "dup.txt";
    const std::string d1 = "first";
    const std::string d2 = "second";
    for (const std::string& d : {d1, d2}) {
        const uint32_t crc = crc_of(d);
        const uint32_t size = static_cast<uint32_t>(d.size());
        const uint32_t off = static_cast<uint32_t>(archive.size());
        put_u32(archive, 0x04034b50u);
        put_u16(archive, 20);
        put_u16(archive, 0);
        put_u16(archive, 0);
        put_u16(archive, 0);
        put_u16(archive, 0);
        put_u32(archive, crc);
        put_u32(archive, size);
        put_u32(archive, size);
        put_u16(archive, static_cast<uint16_t>(name.size()));
        put_u16(archive, 0);
        put_str(archive, name);
        put_str(archive, d);

        put_u32(central, 0x02014b50u);
        put_u16(central, 20);
        put_u16(central, 20);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u32(central, crc);
        put_u32(central, size);
        put_u32(central, size);
        put_u16(central, static_cast<uint16_t>(name.size()));
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u32(central, 0);
        put_u32(central, off);
        put_str(central, name);
    }
    const size_t cd_start = archive.size();
    archive.insert(archive.end(), central.begin(), central.end());
    const uint32_t cd_size = static_cast<uint32_t>(central.size());
    put_u32(archive, 0x06054b50u);
    put_u16(archive, 0);
    put_u16(archive, 0);
    put_u16(archive, 2);
    put_u16(archive, 2);
    put_u32(archive, cd_size);
    put_u32(archive, static_cast<uint32_t>(cd_start));
    put_u16(archive, 0);

    std::ofstream out(tmp_root() / "dup.zip", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(archive.data()),
              static_cast<std::streamsize>(archive.size()));
    out.close();
    ZipReaderStd z;
    assert(z.open((tmp_root() / "dup.zip").string()));
    assert(z.entry_names().size() == 1);
    std::string text;
    assert(z.read_entry(name, text));
    // 后写的 CDE 覆盖前一个（map 赋值语义）
    assert(text == "second");
}

}  // namespace

int main() {
    test_baseline_valid_zip_loads();

    test_reject_tiny_file();
    test_reject_directory_or_unreadable();

    test_reject_absent_eocd();
    test_reject_eocd_comment_length_inconsistent();
    test_reject_zip64_markers();
    test_reject_cd_out_of_bounds();

    test_reject_cde_signature_mismatch();
    test_reject_cde_field_overflow();
    test_reject_unsupported_method();
    test_reject_entry_count_mismatch();

    test_reject_lfh_offset_out_of_bounds();
    test_reject_lfh_signature_mismatch();
    test_reject_entry_data_out_of_bounds();
    test_reject_entry_over_max_uncompressed_size();
    test_reject_stored_size_mismatch();
    test_reject_stored_crc_mismatch();
    test_stored_zero_crc_skips_verification();

    test_deflate_ok();
    test_reject_deflate_garbage_payload();
    test_reject_deflate_invalid_block_type();
    test_reject_deflate_truncated_stream();
    test_reject_deflate_size_mismatch();
    test_reject_deflate_crc_mismatch();
    test_deflate_zero_crc_skips_verification();

    test_read_entry_unknown_name_and_unloaded();
    test_reopen_resets_state();
    test_duplicate_entry_names_last_wins();

    std::printf("OK\n");
    return 0;
}
