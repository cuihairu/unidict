// 真实 MDict v2 .mdx 忠实读取测试（std core，无 Qt）。
//
// 按 mdx_v2_reader_std.h 的布局口径手工构造最小 v2 文件：引擎 2.0 头
// （UTF-16LE XML）+ key 节（5×u64+u32 / 索引块 / key 块）+ record 节
// （4×u64 + 直排块表 + 压缩块），覆盖：
//   - 编码：UTF-8 / UTF-16LE / GBK（charset_codec 转换）
//   - 块形态：zlib / 存储块；单/多 key 块；多 record 块
//   - 加密：Encrypted=2（索引块 key_info_key / key 块 block_key 加密壳，
//     mdict_crypto 口径），及坏 adler 的诚实失败
//   - 容器：裸 mdx / zip 容器（stored zip 手摆）
//   - parser 集成：解析成功不再种 skeleton 占位词，lookup 忠实往返
//   - 负例：Encrypted=1（Salsa20）、截断、块长谎报——诊断非空、兜底不误吞

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <fstream>
#include <string>
#include <vector>

#include <zlib.h>

#include "std/mdict_crypto_std.h"
#include "std/mdict_parser_std.h"
#include "std/mdx_v2_reader_std.h"

namespace {

namespace fs = std::filesystem;

std::string be32(uint32_t v) {
    return std::string{static_cast<char>(v >> 24), static_cast<char>(v >> 16),
                       static_cast<char>(v >> 8), static_cast<char>(v)};
}

std::string be64(uint64_t v) {
    std::string s;
    for (int i = 7; i >= 0; --i) {
        s.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    }
    return s;
}

std::string be16(uint16_t v) {
    return std::string{static_cast<char>(v >> 8), static_cast<char>(v)};
}

std::string le32(uint32_t v) {
    std::string s;
    for (int i = 0; i < 4; ++i) {
        s.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
    }
    return s;
}

// 字节手术：按文件内偏移覆写定长字段（负例变体用）
void patch_be64(std::string& bytes, size_t off, uint64_t v) {
    const std::string be = be64(v);
    std::memcpy(bytes.data() + off, be.data(), 8);
}

void patch_be32(std::string& bytes, size_t off, uint32_t v) {
    const std::string be = be32(v);
    std::memcpy(bytes.data() + off, be.data(), 4);
}

void patch_be16(std::string& bytes, size_t off, uint16_t v) {
    const std::string be = be16(v);
    std::memcpy(bytes.data() + off, be.data(), 2);
}

std::string zlib_compress(const std::string& raw) {
    uLongf bound = compressBound(static_cast<uLong>(raw.size()));
    std::string out(bound, '\0');
    uLongf out_len = bound;
    assert(compress2(reinterpret_cast<Bytef*>(out.data()), &out_len,
                     reinterpret_cast<const Bytef*>(raw.data()),
                     raw.size(), Z_BEST_COMPRESSION) == Z_OK);
    out.resize(out_len);
    return out;
}

std::string adler_bytes_of(const std::string& data) {
    return be32(UnidictCoreStd::adler32_of(data));
}

// 普通块封装：{ u32 BE 压缩类型; u32 BE adler32; 载荷 }（.mdd 侧同口径，
// adler 为载荷的 adler32——reader 读而不验）
std::string wrap_plain(const std::string& payload, bool compress) {
    const std::string body = compress ? zlib_compress(payload) : payload;
    return be32(compress ? 2u : 0u) + adler_bytes_of(body) + body;
}

// 加密壳：{ u32 LE 块信息字(压缩位|加密1<<4|加密数<<8); u32 BE adler(明文
// 数据); 密文 }。密钥按 mdict_crypto 既有派生由调用方传入。加密数
// 8 位上限 255（crypto 模块口径），测试夹具压缩数据控制在范围内。
std::string wrap_encrypted(const std::string& plain_payload,
                           const std::string& key, bool compress) {
    assert(plain_payload.size() <= 255);
    const uint32_t info_word = (compress ? 2u : 0u) | (1u << 4) |
                               (static_cast<uint32_t>(plain_payload.size()) << 8);
    return le32(info_word) + adler_bytes_of(plain_payload) +
           UnidictCoreStd::fast_encrypt(plain_payload, key);
}

std::string utf8_to_utf16le(const std::string& utf8) {
    std::string out;
    for (size_t i = 0; i < utf8.size();) {
        const auto b0 = static_cast<unsigned char>(utf8[i]);
        uint32_t cp = 0;
        int extra = 0;
        if (b0 < 0x80) {
            cp = b0;
            extra = 0;
        } else if ((b0 & 0xE0) == 0xC0) {
            cp = b0 & 0x1F;
            extra = 1;
        } else if ((b0 & 0xF0) == 0xE0) {
            cp = b0 & 0x0F;
            extra = 2;
        } else {
            cp = b0 & 0x07;
            extra = 3;
        }
        ++i;
        for (int k = 0; k < extra && i < utf8.size(); ++k, ++i) {
            cp = (cp << 6) | (static_cast<unsigned char>(utf8[i]) & 0x3F);
        }
        if (cp < 0x10000) {
            out.push_back(static_cast<char>(cp & 0xff));
            out.push_back(static_cast<char>(cp >> 8));
        } else {
            cp -= 0x10000;
            const uint32_t hi = 0xD800 + (cp >> 10);
            const uint32_t lo = 0xDC00 + (cp & 0x3FF);
            out.push_back(static_cast<char>(hi & 0xff));
            out.push_back(static_cast<char>(hi >> 8));
            out.push_back(static_cast<char>(lo & 0xff));
            out.push_back(static_cast<char>(lo >> 8));
        }
    }
    return out;
}

// UTF-8 词头/释义 → 头 Encoding 声明的字节。GBK 只覆盖测试用到的已知
// 码位：「指」= D6 B8（charset_codec_std.h 权威注释背书）；其余非 ASCII
// 断言拦下（不静默产出错误码位）。
std::string encode_text(const std::string& utf8, const std::string& encoding) {
    if (encoding == "UTF-16LE") {
        return utf8_to_utf16le(utf8);
    }
    if (encoding == "GBK") {
        std::string out;
        for (size_t i = 0; i < utf8.size();) {
            const auto b = static_cast<unsigned char>(utf8[i]);
            if (b < 0x80) {
                out.push_back(utf8[i]);
                ++i;
                continue;
            }
            assert(i + 2 < utf8.size() && b == 0xE6 &&
                   static_cast<unsigned char>(utf8[i + 1]) == 0x8C &&
                   static_cast<unsigned char>(utf8[i + 2]) == 0x87);
            out += std::string("\xD6\xB8", 2);
            i += 3;
        }
        return out;
    }
    return utf8;
}

size_t unit_width(const std::string& encoding) {
    return encoding == "UTF-16LE" ? 2 : 1;
}

std::string terminator(const std::string& encoding) {
    return std::string(unit_width(encoding), '\0');
}

struct V2Spec {
    std::vector<std::pair<std::string, std::string>> entries;  // (utf8 词, utf8 释义)
    std::string encoding = "UTF-8";
    std::string encrypted = "0";  // "0" / "2" / "1" / "3" / "Yes" / "4"
    std::string engine = "2.0";
    bool compress = true;    // false → 存储块（压缩类型 0）
    int key_blocks = 1;
    bool encrypted_flag_lie = false;  // 头 Encrypted 与实际壳不符（负例）
    uint64_t lie_key_block_size = 0;  // 声明的 key 块总长 ≠ 实际（负例）
    bool truncate_key_header = false; // key 节 44 字节截断（负例）
    bool def_terminator = true;       // 释义尾 NUL 终止单元（部分引擎口径）
    bool key_missing_nul = false;     // 最后一词头无 NUL 终止（负例）
    uint64_t lie_key_decomp = 0;      // 索引条目谎报 key 块解压长（负例）
    bool no_record_section = false;   // 文件在 key 节末截断（负例）
    std::string raw_xml16;            // 非空 → 头 XML 直接用该 UTF-16LE 字节
    // 头声明 Encrypted=2 但块壳实际未加密（info word enc=0）：decrypt_block
    // 对 enc=0 只剥 8 字节壳即放行，adler 对上就是合法数据——真实世界确有
    // 这种写作器不一致的文件；测试借它让索引保持明文，可对索引条目做手术
    bool fake_enc_shell = false;
};

// build_v2 的关键偏移（字节手术定位用；索引/key 块内容起点仅 plain 形态
// 有意义——加密形态内容是密文）
struct V2Layout {
    size_t key_start = 0;         // key 节头（44 字节）
    size_t info_start = 0;        // 索引块壳（info word + adler + 载荷）
    size_t key_blocks_start = 0;  // key 块区起点
    size_t index_content = 0;     // 索引块内容起点（plain：壳后 8 字节）
    size_t rec_start = 0;         // record 节起点
    size_t record_block_start = 0;// 首个 record 块壳起点
};

std::string build_v2(const V2Spec& spec, V2Layout* layout = nullptr) {
    assert(spec.key_blocks >= 1 && !spec.entries.empty());
    const size_t unit = unit_width(spec.encoding);
    const std::string term = terminator(spec.encoding);

    // record 流：释义拼接（可带 NUL 终止单元）
    std::string record_data;
    std::vector<uint64_t> offsets;
    for (const auto& [word, def] : spec.entries) {
        offsets.push_back(record_data.size());
        record_data += encode_text(def, spec.encoding);
        if (spec.def_terminator) record_data += term;
    }

    // key 块（分块）+ 索引条目
    const bool enc_blocks = spec.encrypted == "2" || spec.encrypted == "3" ||
                            spec.encrypted == "Yes";
    const size_t n = spec.entries.size();
    const size_t per_block =
        spec.key_blocks > 0 ? (n + spec.key_blocks - 1) / spec.key_blocks : n;
    std::string key_block_section;   // 压缩块拼接（含壳）
    std::string info_raw;
    uint64_t key_block_size_sum = 0;
    for (size_t b = 0; b * per_block < n; ++b) {
        const size_t begin = b * per_block;
        const size_t end = std::min(begin + per_block, n);
        std::string block_raw;
        for (size_t i = begin; i < end; ++i) {
            block_raw += be64(offsets[i]);
            block_raw += encode_text(spec.entries[i].first, spec.encoding);
            if (!(spec.key_missing_nul && i == n - 1)) {
                block_raw += term;
            }
        }
        const std::string comp = spec.compress ? zlib_compress(block_raw) : block_raw;
        const std::string enc =
            enc_blocks && !spec.fake_enc_shell
                ? wrap_encrypted(comp, UnidictCoreStd::block_key(adler_bytes_of(comp)), spec.compress)
                : wrap_plain(block_raw, spec.compress);
        key_block_section += enc;
        key_block_size_sum += enc.size();

        info_raw += be64(end - begin);  // 块内条目数
        const std::string first = encode_text(spec.entries[begin].first, spec.encoding);
        info_raw += be16(static_cast<uint16_t>(first.size() / unit));
        info_raw += first + term;
        const std::string last = encode_text(spec.entries[end - 1].first, spec.encoding);
        info_raw += be16(static_cast<uint16_t>(last.size() / unit));
        info_raw += last + term;
        info_raw += be64(enc.size());
        info_raw += be64(spec.lie_key_decomp ? spec.lie_key_decomp
                                             : block_raw.size());
    }

    // 索引块（同样可加密）
    const std::string info_comp = spec.compress ? zlib_compress(info_raw) : info_raw;
    const std::string info_block =
        enc_blocks && !spec.fake_enc_shell
            ? wrap_encrypted(info_comp, UnidictCoreStd::key_info_key(adler_bytes_of(info_comp)), spec.compress)
            : wrap_plain(info_raw, spec.compress);

    // key 节头：5×u64 + u32 adler（读而不验）
    std::string key_header;
    const uint64_t num_key_blocks = (n + per_block - 1) / per_block;
    key_header += be64(num_key_blocks);
    key_header += be64(n);
    key_header += be64(info_raw.size());
    key_header += be64(info_block.size());
    key_header += be64(spec.lie_key_block_size ? spec.lie_key_block_size
                                               : key_block_size_sum);
    key_header += be32(0x12345678);
    std::string key_section = spec.truncate_key_header
                                  ? key_header.substr(0, 20)  // 截断即文件尾
                                  : key_header + info_block + key_block_section;

    // record 节：4×u64 + 直排块表 + 块
    std::string record_section;
    record_section += be64(1);
    record_section += be64(n);
    record_section += be64(16);
    record_section += be64(wrap_plain(record_data, spec.compress).size());
    record_section += be64(wrap_plain(record_data, spec.compress).size());
    record_section += be64(record_data.size());
    record_section += wrap_plain(record_data, spec.compress);

    // 头：u32 BE 文本长 + UTF-16LE XML + u32 LE adler（读而不验）
    const std::string enc_attr = spec.encrypted_flag_lie
                                     ? (spec.encrypted == "2" ? "0" : "2")
                                     : spec.encrypted;
    const std::string xml = "<Dictionary GeneratedByEngineVersion=\"" +
                            spec.engine + "\" RequiredEngineVersion=\"2.0\" "
                            "Encrypted=\"" + enc_attr + "\" Encoding=\"" +
                            spec.encoding + "\" Format=\"Html\" />";
    std::string xml16 = spec.raw_xml16.empty() ? utf8_to_utf16le(xml)
                                               : spec.raw_xml16;
    std::string header = be32(static_cast<uint32_t>(xml16.size())) + xml16 + le32(0);

    if (layout) {
        layout->key_start = header.size();
        layout->info_start = header.size() + 44;
        layout->key_blocks_start = layout->info_start + info_block.size();
        layout->index_content = layout->info_start + 8;
        layout->rec_start = layout->key_blocks_start + key_block_size_sum;
        layout->record_block_start = layout->rec_start + 48;
    }
    if (spec.truncate_key_header) {
        return header + key_section;  // 截断变体：key 节读不满 44 字节
    }
    if (spec.no_record_section) {
        return header + key_section;  // 截断变体：record 节整个缺席
    }
    return header + key_section + record_section;
}

// stored zip（手摆 local file header + central directory + EOCD，
// ZipReaderStd 支持 stored(0)/deflate(8)）。zip 字段全 LE 存储——魔数
// "PK\x03\x04" 即 LE32(0x04034b50)
std::string make_stored_zip(
    const std::vector<std::pair<std::string, std::string>>& items) {
    auto le16 = [](uint16_t v) {
        return std::string{static_cast<char>(v & 0xff),
                           static_cast<char>(v >> 8)};
    };
    auto le32l = [](uint32_t v) { return le32(v); };
    std::string local, central;
    for (const auto& [name, data] : items) {
        const uint32_t crc =
            crc32(0L, reinterpret_cast<const Bytef*>(data.data()),
                  static_cast<uInt>(data.size()));
        const uint32_t lho = static_cast<uint32_t>(local.size());
        local += le32l(0x04034b50u);
        local += le16(0x14);     // version needed
        local += le16(0);        // flags
        local += le16(0);        // method: stored
        local += le16(0);        // time
        local += le16(0x21);     // date 1980-01-01
        local += le32l(crc);
        local += le32l(static_cast<uint32_t>(data.size()));
        local += le32l(static_cast<uint32_t>(data.size()));
        local += le16(static_cast<uint16_t>(name.size()));
        local += le16(0);
        local += name + data;

        central += le32l(0x02014b50u);
        central += le16(0x0214);  // version made by
        central += le16(0x14);
        central += le16(0);
        central += le16(0);
        central += le16(0);
        central += le16(0x21);
        central += le32l(crc);
        central += le32l(static_cast<uint32_t>(data.size()));
        central += le32l(static_cast<uint32_t>(data.size()));
        central += le16(static_cast<uint16_t>(name.size()));
        central += le16(0);  // extra
        central += le16(0);  // comment
        central += le16(0);  // disk
        central += le16(0);  // internal attr
        central += le32l(0);  // external attr
        central += le32l(lho);
        central += name;
    }
    const std::string cd = central;
    const uint32_t cd_offset = static_cast<uint32_t>(local.size());
    std::string eocd;
    eocd += le32l(0x06054b50u);
    eocd += le16(0);  // disk
    eocd += le16(0);
    eocd += le16(static_cast<uint16_t>(items.size()));
    eocd += le16(static_cast<uint16_t>(items.size()));
    eocd += le32l(static_cast<uint32_t>(cd.size()));
    eocd += le32l(cd_offset);
    eocd += le16(0);
    return local + cd + eocd;
}

std::string write_file(const fs::path& dir, const std::string& name,
                       const std::string& bytes) {
    fs::create_directories(dir);
    const fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << bytes;
    return p.string();
}

fs::path tmp_dir(const char* leaf) {
    return fs::current_path() / "build-local" / "mdx_v2" / leaf;
}

// 断言不含 skeleton 占位词（解析成功的词典不许再混入占位词条）
void assert_no_skeleton(const UnidictCoreStd::MdictParserStd& mp) {
    const auto words = mp.all_words();
    for (const auto& w : words) {
        assert(w != "mdict" && w != "unidict");
    }
}

}  // namespace

int main() {
    using UnidictCoreStd::MdictParserStd;
    using UnidictCoreStd::MdxV2Dict;
    using UnidictCoreStd::MdxV2ReaderStd;

    // ---- T1 UTF-8 + zlib：reader 层忠实往返 ----
    {
        V2Spec spec;
        spec.entries = {{"alpha", "first entry"},
                        {"beta", "second entry"},
                        {"gamma", "third entry"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.empty());
        assert(dict.title.empty() && dict.encoding == "UTF-8");
        assert(dict.entries.size() == 3);
        assert(dict.entries[0].key == "alpha" && dict.entries[0].definition == "first entry");
        assert(dict.entries[1].key == "beta" && dict.entries[1].definition == "second entry");
        assert(dict.entries[2].key == "gamma" && dict.entries[2].definition == "third entry");
    }

    // ---- T2 UTF-16LE：中文词头/释义按头编码转换 ----
    {
        V2Spec spec;
        spec.encoding = "UTF-16LE";
        spec.entries = {{"\xE4\xBD\xA0\xE5\xA5\xBD", "\xE4\xBD\xA0"}};  // 你好 → 你
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].key == "\xE4\xBD\xA0\xE5\xA5\xBD");
        assert(dict.entries[0].definition == "\xE4\xBD\xA0");
    }

    // ---- T3 GBK：词头/释义经 charset_codec 转 UTF-8 ----
    {
        V2Spec spec;
        spec.encoding = "GBK";
        spec.entries = {{"\xE6\x8C\x87", "see \xE6\x8C\x87 point"}};  // 指
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].key == "\xE6\x8C\x87");
        assert(dict.entries[0].definition == "see \xE6\x8C\x87 point");
    }

    // ---- T4 存储块（压缩类型 0）----
    {
        V2Spec spec;
        spec.compress = false;
        spec.entries = {{"k1", "v1"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].definition == "v1");
    }

    // ---- T5 多 key 块 + 释义尾部无 NUL 变体 ----
    {
        V2Spec spec;
        spec.key_blocks = 2;
        spec.def_terminator = false;
        for (int i = 0; i < 4; ++i) {
            spec.entries.emplace_back("word" + std::to_string(i),
                                      "def" + std::to_string(i));
        }
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 4);
        for (int i = 0; i < 4; ++i) {
            assert(dict.entries[i].definition == "def" + std::to_string(i));
        }
    }

    // ---- T6 Encrypted=2：加密索引块 + 加密 key 块 ----
    {
        V2Spec spec;
        spec.encrypted = "2";
        spec.entries = {{"secret", "locked value"}, {"token", "second"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 2);
        assert(dict.entries[0].key == "secret");
        assert(dict.entries[0].definition == "locked value");
        assert(dict.entries[1].definition == "second");
    }

    // ---- T7 parser 集成：v2 成功 → 无 skeleton 占位词、lookup 忠实 ----
    {
        const V2Spec spec = [] {
            V2Spec s;
            s.entries = {{"hello", "greeting \xE4\xBD\xA0\xE5\xA5\xBD"},
                         {"world", "second word"}};
            return s;
        }();
        const std::string path =
            write_file(tmp_dir("t7"), "demo.mdx", build_v2(spec));
        MdictParserStd mp;
        assert(mp.load_dictionary(path));
        assert(mp.is_loaded());
        assert(mp.word_count() == 2);
        assert_no_skeleton(mp);
        assert(mp.lookup("hello") == "greeting \xE4\xBD\xA0\xE5\xA5\xBD");
        assert(mp.lookup("world") == "second word");
        // 头 Title 缺省 → 文件名兜底
        assert(mp.dictionary_name() == "demo");
    }

    // ---- T8 zip 容器：.mdx 条目解包后忠实读取 ----
    {
        V2Spec spec;
        spec.entries = {{"zipword", "from zip"}};
        const std::string zip = make_stored_zip(
            {{"readme.txt", "not a dictionary"},
             {"test.mdx", build_v2(spec)}});
        const std::string path = write_file(tmp_dir("t8"), "test.mdx", zip);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_file(path, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].definition == "from zip");

        MdictParserStd mp;
        assert(mp.load_dictionary(path));
        assert(mp.lookup("zipword") == "from zip");
        assert_no_skeleton(mp);
    }

    // ---- T9 Encrypted=1（record 块 Salsa20）诚实拒绝 ----
    {
        V2Spec spec;
        spec.encrypted = "1";
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("not supported") != std::string::npos);
    }

    // ---- T10 Encrypted=2 但壳 adler 翻坏 → 解密判据失败 ----
    {
        V2Spec spec;
        spec.encrypted = "2";
        spec.entries = {{"a", "b"}};
        std::string bytes = build_v2(spec);
        // 按结构定位索引块壳的 adler 字节：头长 = be32(bytes[0..4])，头段
        // = 4 + header_len + 4（尾 adler 读而不验），索引块起点在其后 44
        // 字节处，壳 adler 在块内偏移 4
        const uint32_t header_len =
            (static_cast<uint32_t>(static_cast<unsigned char>(bytes[0])) << 24) |
            (static_cast<uint32_t>(static_cast<unsigned char>(bytes[1])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(bytes[2])) << 8) |
            static_cast<uint32_t>(static_cast<unsigned char>(bytes[3]));
        const size_t shell_adler = 4 + header_len + 4 + 44 + 4;
        bytes[shell_adler] = static_cast<char>(bytes[shell_adler] ^ 0x55);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        // adler 字节也是 key_info_key 的派生输入——翻坏后密钥变、解密出
        // 垃圾，权威判据（adler32 不符）拦下
        assert(diag.find("adler32 mismatch") != std::string::npos);
    }

    // ---- T11 截断：key 节头不完整 ----
    {
        V2Spec spec;
        spec.truncate_key_header = true;
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("truncated") != std::string::npos);
    }

    // ---- T12 key 块总长谎报 ----
    {
        V2Spec spec;
        spec.lie_key_block_size = 99999;
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(!diag.empty());
    }

    // ---- T13 引擎 1.2 拒收（LZO 不支持）----
    {
        V2Spec spec;
        spec.engine = "1.2";
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("not supported") != std::string::npos);
    }

    // ---- T14 非 v2 文本头：诊断空（交上层兜底链，不误报）----
    {
        MdxV2Dict dict;
        std::string diag = "sentinel";
        assert(!MdxV2ReaderStd::read_buffer(
            "<Dictionary title=\"x\"/>\nDATA...", dict, diag));
        assert(diag.empty());
    }

    // ---- T15 Encrypted=2 + 存储块 + 无释义 NUL：加密×非压缩正交 ----
    {
        V2Spec spec;
        spec.encrypted = "2";
        spec.compress = false;
        spec.def_terminator = false;
        spec.entries = {{"plain", "stored and locked"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].definition == "stored and locked");
    }

    // ---- T16 头 XML 转换全矩阵：é（2 字节）/中（3 字节）/😀（代理对）/
    //      孤立高代理（无效低代理 → 按 3 字节直推）/末尾孤立高代理 ----
    {
        V2Spec spec;
        spec.entries = {{"k", "v"}};
        std::string xml16 = utf8_to_utf16le(
            "<Dictionary GeneratedByEngineVersion=\"2.0\" Encoding=\"UTF-8\" "
            "Title=\"");
        xml16 += std::string("\xE9\x00", 2);          // U+00E9 é
        xml16 += std::string("\x2D\x4E", 2);          // U+4E2D 中
        xml16 += std::string("\x3D\xD8\x00\xDE", 4);  // U+1F600 😀 代理对
        xml16 += std::string("\x00\xD8\x41\x00", 4);  // 高代理 + 'A'（非低代理）
        xml16 += std::string("\x00\xD8", 2);          // 末尾孤立高代理
        xml16 += utf8_to_utf16le("\" />");
        spec.raw_xml16 = xml16;
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        // 孤立代理按 3 字节 UTF-8 直推（ED A0 80 = U+D800 的直接编码）
        assert(dict.title ==
               "\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80"
               "\xED\xA0\x80" "A\xED\xA0\x80");
        assert(dict.entries.size() == 1 && dict.entries[0].key == "k");
    }

    // ---- T17 Title 引号未闭合 → 属性抽取空值，解析照常 ----
    {
        V2Spec spec;
        spec.entries = {{"k", "v"}};
        spec.raw_xml16 = utf8_to_utf16le(
            "<Dictionary GeneratedByEngineVersion=\"2.0\" Encoding=\"UTF-8\" "
            "Title=\"unclosed");
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.title.empty());
        assert(dict.entries.size() == 1);
    }

    // ---- T18 未知编码（Big5）：声明不认识 → 字节透传 ----
    {
        V2Spec spec;
        spec.encoding = "Big5";
        spec.entries = {{"\xE6\x8C\x87", "see \xE6\x8C\x87"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries[0].key == "\xE6\x8C\x87");
        assert(dict.entries[0].definition == "see \xE6\x8C\x87");
    }

    // ---- T19/T20/T21 头形态三负例：诊断空（不是 v2，交上层兜底） ----
    {
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer("abc", dict, diag));
        assert(diag.empty());
        diag = "sentinel";
        assert(!MdxV2ReaderStd::read_buffer(be32(8) + "<\0", dict, diag));
        assert(diag.empty());  // 头段截断
        diag = "sentinel";
        assert(!MdxV2ReaderStd::read_buffer(
            be32(4) + std::string("A\0B\0", 4) + le32(0), dict, diag));
        assert(diag.empty());  // 首单元不是 '<'
    }

    // ---- T22 read_file：路径不存在零成本失败 ----
    {
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_file(
            "/nonexistent/unidict_t22_missing.mdx", dict, diag));
        assert(diag.empty());
    }

    // ---- T23/T24/T25/T26/T27 zip 容器五负正例 ----
    {
        MdxV2Dict dict;
        std::string diag;
        // T23 zip 魔数但结构坏：open 失败
        std::string zipish = std::string("PK\x03\x04", 4);
        zipish += std::string(40, '\x01');
        std::string path = write_file(tmp_dir("t23"), "broken.mdx", zipish);
        assert(!MdxV2ReaderStd::read_file(path, dict, diag));
        assert(diag.find("open failed") != std::string::npos);

        // T24 条目名与 stem 不符 → 落到「首个 .mdx」分支
        V2Spec inner;
        inner.entries = {{"zipword", "inner pick"}};
        const std::string zip = make_stored_zip(
            {{"readme.txt", "note"}, {"inner.mdx", build_v2(inner)}});
        path = write_file(tmp_dir("t24"), "zzz.mdx", zip);
        assert(MdxV2ReaderStd::read_file(path, dict, diag));
        assert(dict.entries.size() == 1);
        assert(dict.entries[0].definition == "inner pick");

        // T25 无 .mdx 条目 → 首个非目录条目；内层失败诊断带条目前缀
        V2Spec bad;
        bad.truncate_key_header = true;
        bad.entries = {{"a", "b"}};
        const std::string zip2 = make_stored_zip(
            {{"blob.bin", build_v2(bad)}});
        path = write_file(tmp_dir("t25"), "zzz2.mdx", zip2);
        assert(!MdxV2ReaderStd::read_file(path, dict, diag));
        assert(diag.find("zip entry 'blob.bin'") != std::string::npos);

        // T26 仅目录条目 → 无条目可解析
        const std::string zip3 = make_stored_zip({{"dir/", ""}});
        path = write_file(tmp_dir("t26"), "onlydir.mdx", zip3);
        assert(!MdxV2ReaderStd::read_file(path, dict, diag));
        assert(diag.find("no entry") != std::string::npos);

        // T27 条目数据 CRC 翻坏 → read_entry 失败（stored 偏移手工推演：
        // 条目1 头 30+10 名 + 1 数据 → 条目2 头 30+5 名，数据起点 76）
        std::string zip4 = make_stored_zip(
            {{"readme.txt", "n"}, {"x.mdx", build_v2(inner)}});
        zip4[76 + 3] = static_cast<char>(zip4[76 + 3] ^ 0xFF);
        path = write_file(tmp_dir("t27"), "y.mdx", zip4);
        assert(!MdxV2ReaderStd::read_file(path, dict, diag));
        assert(diag.find("read failed") != std::string::npos);
    }

    // ---- T28 Encrypted="Yes"：加密块的第三种头写法 ----
    {
        V2Spec spec;
        spec.encrypted = "Yes";
        spec.entries = {{"a", "yes value"}, {"b", "second"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(dict.entries.size() == 2);
        assert(dict.entries[0].definition == "yes value");
    }

    // ---- T29 Encrypted="4"：未知值诚实拒收 ----
    {
        V2Spec spec;
        spec.encrypted = "4";
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("unknown Encrypted") != std::string::npos);
    }

    // ---- T30/T31/T32 key 节头计数与界内负例（字节手术） ----
    {
        V2Spec spec;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        MdxV2Dict dict;
        std::string diag;
        patch_be64(bytes, L.key_start, 0);  // num_key_blocks = 0
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("out of range") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.key_start + 24, 4);  // info_size < 8
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("info size 4 out of range") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.key_start + 24, 4096);  // 界内值但文件不够长
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block info truncated") != std::string::npos);
    }

    // ---- T33/T34/T36 索引块解码三负例：坏流 / 非法压缩类型 / 期望超限 ----
    {
        V2Spec spec;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        MdxV2Dict dict;
        std::string diag;
        bytes[L.info_start + 8 + 6] ^= 0xFF;  // zlib 流深处翻坏
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block info decode failed") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be32(bytes, L.info_start, 5);  // 压缩类型 5
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block info decode failed") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.key_start + 16, 600000000);  // > 512MB 上限
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block info decode failed") != std::string::npos);
    }

    // ---- T35 存储索引块：声明解压长与实际不符 ----
    {
        V2Spec spec;
        spec.compress = false;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        // 索引内容 32 字节（count8+fu2+词2+lu2+词2+comp8+decomp8）
        patch_be64(bytes, L.key_start + 16, 33);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("decomp size mismatch") != std::string::npos);
    }

    // ---- T37/T38/T39 索引条目三负例（存储形态手术明文索引） ----
    {
        V2Spec spec;
        spec.compress = false;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        MdxV2Dict dict;
        std::string diag;
        patch_be64(bytes, L.key_start, 2);  // 块数谎报 → 第二条目越界
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("truncated") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be16(bytes, L.index_content + 8, 0xFFFF);  // 首词单元数爆表
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("truncated") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be16(bytes, L.index_content + 12, 0xFFFF);  // 末词单元数爆表
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("truncated") != std::string::npos);
    }

    // ---- T40 索引块压缩长超 key 块总长 ----
    {
        V2Spec spec;
        spec.compress = false;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        patch_be64(bytes, L.index_content + 16,
                   L.rec_start - L.key_blocks_start + 1);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key blocks truncated") != std::string::npos);
    }

    // ---- T41/T42/T63 加密路径（fake 壳保明文索引可手术）----
    {
        MdxV2Dict dict;
        std::string diag;
        // T41 加密路径下 key 块不足 8 字节
        V2Spec tiny;
        tiny.encrypted = "2";
        tiny.fake_enc_shell = true;
        tiny.compress = false;
        tiny.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(tiny, &L);
        patch_be64(bytes, L.index_content + 16, 4);  // 压缩长 4
        patch_be64(bytes, L.key_start + 32, 4);      // key 块总长同步 4
        // 手术后重敲索引壳 adler（剥壳路径验的是壳内声明 vs 明文实际）
        patch_be32(bytes, L.info_start + 4,
                   static_cast<uint32_t>(UnidictCoreStd::adler32_of(bytes.substr(
                       L.info_start + 8, L.key_blocks_start - L.info_start - 8))));
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("crypto shell") != std::string::npos);

        // T42 真·加密 zlib key 块解压长谎报（索引加密前写入坏值）：
        // inflate_expected 解出长度不符静默返回假 → 上层兜底诊断
        V2Spec lie;
        lie.encrypted = "2";
        lie.lie_key_decomp = 11;  // 块内容 = offset8 + "a" + NUL = 实际 10
        lie.entries = {{"a", "b"}};
        bytes = build_v2(lie, &L);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decode failed") != std::string::npos);

        // T63 加密存储块长度谎报 → 解密成功但尺寸不符
        V2Spec store;
        store.encrypted = "2";
        store.fake_enc_shell = true;
        store.compress = false;
        store.entries = {{"a", "b"}};
        bytes = build_v2(store, &L);
        patch_be64(bytes, L.index_content + 24, 11);
        patch_be32(bytes, L.info_start + 4,
                   static_cast<uint32_t>(UnidictCoreStd::adler32_of(bytes.substr(
                       L.info_start + 8, L.key_blocks_start - L.info_start - 8))));
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decomp size mismatch") != std::string::npos);
    }

    // ---- T43/T44/T45/T46/T47/T48/T49 key 块七负例 ----
    {
        MdxV2Dict dict;
        std::string diag;
        // T43 普通 key 块不足 8 字节
        V2Spec spec;
        spec.compress = false;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        patch_be64(bytes, L.index_content + 16, 4);
        patch_be64(bytes, L.key_start + 32, 4);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decode failed") != std::string::npos);

        // T44 普通 zlib key 块坏流
        bytes = build_v2(V2Spec{{{"a", "b"}}}, &L);
        bytes[L.key_blocks_start + 8 + 6] ^= 0xFF;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decode failed") != std::string::npos);

        // T45 key 块压缩类型非法
        bytes = build_v2(spec, &L);
        patch_be32(bytes, L.key_blocks_start, 7);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decode failed") != std::string::npos);

        // T46 存储块声明解压长与实际不符
        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.index_content + 24, 11);  // 实际 10
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block decomp size mismatch") != std::string::npos);

        // T47 末词头缺 NUL 终止符
        V2Spec nonul;
        nonul.key_missing_nul = true;
        nonul.entries = {{"a", "b"}, {"c", "d"}};
        bytes = build_v2(nonul, &L);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("NUL terminator") != std::string::npos);

        // T48 key 块总长声明偏大 → 各块和不上账
        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.key_start + 32,
                   L.rec_start - L.key_blocks_start + 8);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("key block size mismatch") != std::string::npos);

        // T49 空 key 块（壳无内容）→ 词表空
        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.index_content + 16, 8);   // 压缩长 = 壳
        patch_be64(bytes, L.index_content + 24, 0);   // 解压长 0
        patch_be64(bytes, L.key_start + 32, 8);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("no entries") != std::string::npos);
    }

    // ---- T50 record 节整个缺席 ----
    {
        V2Spec spec;
        spec.no_record_section = true;
        spec.entries = {{"a", "b"}};
        const std::string bytes = build_v2(spec);
        MdxV2Dict dict;
        std::string diag;
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record section header truncated") != std::string::npos);
    }

    // ---- T51-T60 record 节十负例（字节手术）----
    {
        MdxV2Dict dict;
        std::string diag;
        V2Spec spec;
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        patch_be64(bytes, L.rec_start, 0);  // 块数 0
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block count 0") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.rec_start + 16, 32);  // 信息区长度与块数不符
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block info size mismatch") != std::string::npos);

        bytes = build_v2(spec, &L);
        bytes.resize(L.rec_start + 32);  // 块表截断
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block info truncated") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.rec_start + 40, 600000000);  // 拼接流超 512MB 上限
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("exceeds cap") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.rec_start + 32, bytes.size() + 100);  // 压缩长越界
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record blocks truncated") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be64(bytes, L.rec_start + 32, 4);  // 块不足 8 字节
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block decode failed") != std::string::npos);

        bytes = build_v2(spec, &L);
        bytes[L.record_block_start + 8 + 6] ^= 0xFF;  // zlib 流深处翻坏
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block decode failed") != std::string::npos);

        bytes = build_v2(spec, &L);
        patch_be32(bytes, L.record_block_start, 5);  // 压缩类型非法
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block decode failed") != std::string::npos);

        V2Spec stored;
        stored.compress = false;
        stored.entries = {{"a", "b"}};
        bytes = build_v2(stored, &L);
        patch_be64(bytes, L.rec_start + 40, 3);  // 存储块声明解压长 3（实际 2）
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block decomp size mismatch") != std::string::npos);

        bytes = build_v2(stored, &L);
        patch_be64(bytes, L.rec_start + 24,
                   bytes.size() - L.rec_start - 48 + 8);  // 总长偏大
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("record block size mismatch") != std::string::npos);
    }

    // ---- T61/T62 加密壳两负例：压缩类型非法 / 加密长度越界 ----
    {
        V2Spec spec;
        spec.encrypted = "2";
        spec.entries = {{"a", "b"}};
        V2Layout L;
        std::string bytes = build_v2(spec, &L);
        MdxV2Dict dict;
        std::string diag;
        // 信息字低字节 0x12 → 0x11：压缩 2→1（LZO 位），加密位不变 →
        // 解密过、adler 过，卡在压缩类型
        bytes[L.info_start] = static_cast<char>(bytes[L.info_start] ^ 0x03);
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("unsupported compression") != std::string::npos);

        bytes = build_v2(spec, &L);
        // 信息字 bit8-15（LE 字节 1）= 加密字节数 → 255 越界拒收
        bytes[L.info_start + 1] = '\xFF';
        assert(!MdxV2ReaderStd::read_buffer(bytes, dict, diag));
        assert(diag.find("crypto shell") != std::string::npos);
    }

    // ---- T64 parser 集成：v2 失败诊断留痕（fprintf 面执行即达；随后落
    //      回既有兜底链，返回值视链而定，不在此断言）----
    {
        V2Spec spec;
        spec.encrypted = "2";
        spec.truncate_key_header = true;
        spec.entries = {{"a", "b"}};
        const std::string path = write_file(tmp_dir("t64"), "broken_v2.mdx",
                                            build_v2(spec));
        const fs::path err_log = tmp_dir("t64") / "stderr.log";
        assert(std::freopen(err_log.string().c_str(), "w", stderr) != nullptr);
        MdictParserStd mp;
        (void)mp.load_dictionary(path);
        std::fflush(stderr);
        std::ifstream in(err_log, std::ios::binary);
        const std::string log((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>());
        assert(log.find("[unidict] mdx v2 parse failed") != std::string::npos);
        assert(log.find("broken_v2.mdx") != std::string::npos);
    }

    std::printf("mdx_v2_reader_std_test: all assertions passed\n");
    return 0;
}
