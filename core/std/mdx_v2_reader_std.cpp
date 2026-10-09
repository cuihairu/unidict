#include "std/mdx_v2_reader_std.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <zlib.h>

#include "std/charset_codec_std.h"
#include "std/mdict_crypto_std.h"
#include "std/zip_reader_std.h"

namespace UnidictCoreStd {
namespace {

constexpr uint64_t kMaxHeaderBytes = 1u << 20;        // 头文本 1MB 上限
constexpr uint64_t kMaxKeyBlocks = 65536;             // key 块数上限
constexpr uint64_t kMaxRecordBlocks = 65536;          // record 块数上限
constexpr uint64_t kMaxKeyInfoBytes = 16u << 20;      // 索引块 16MB 上限
constexpr uint64_t kMaxStreamBytes = 512u << 20;      // 拼接流 512MB 上限
constexpr uint32_t kZipMagic = 0x504b0304u;  // "PK\x03\x04"（LE 存储）

uint32_t be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

uint64_t be64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

uint16_t be16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

// UTF-16LE → UTF-8（完整中文/代理对；非法代理字节原样吸收为替换语义交给
// 上层——词头转换失败按空串处理，不中断整表）。
std::string utf16le_to_utf8(const uint8_t* data, size_t units) {
    std::string out;
    out.reserve(units * 3);
    for (size_t i = 0; i < units;) {
        const uint32_t u = static_cast<uint32_t>(data[2 * i]) |
                           (static_cast<uint32_t>(data[2 * i + 1]) << 8);
        ++i;
        if (u < 0x80) {
            out.push_back(static_cast<char>(u));
        } else if (u < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (u >> 6)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        } else if (u >= 0xD800 && u <= 0xDBFF && i < units) {
            const uint32_t lo = static_cast<uint32_t>(data[2 * i]) |
                                (static_cast<uint32_t>(data[2 * i + 1]) << 8);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                ++i;
                const uint32_t cp = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xE0 | (u >> 12)));
                out.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
            }
        } else {
            out.push_back(static_cast<char>(0xE0 | (u >> 12)));
            out.push_back(static_cast<char>(0x80 | ((u >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (u & 0x3F)));
        }
    }
    return out;
}

// 属性抽取（大小写不敏感）：lower 拷贝查找，原串按同偏移切片（lower 不改
// 变长度）。头属性在真实 mdx 里是 Title=/Description= 首字母大写，项目内
// fixture 亦然，但引擎历史版本有全小写产出。
std::string extract_attr(const std::string& xml, const char* name) {
    std::string low;
    low.reserve(xml.size());
    for (const char c : xml) {
        low.push_back(static_cast<char>(
            (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));
    }
    std::string needle = std::string(name) + "=\"";
    for (char& c : needle) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    const size_t pos = low.find(needle);
    if (pos == std::string::npos) {
        return {};
    }
    const size_t begin = pos + needle.size();
    const size_t endq = low.find('"', begin);
    if (endq == std::string::npos) {
        return {};
    }
    return xml.substr(begin, endq - begin);
}

std::string lcase(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        out.push_back(static_cast<char>(
            (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c));
    }
    return out;
}

// zlib 解压到期望长度：解出长度必须与声明一致（多/少都算谎报）。
bool inflate_expected(const uint8_t* data, size_t size, size_t expect,
                      std::string& out) {
    if (expect > kMaxStreamBytes) {
        return false;
    }
    out.clear();
    out.resize(expect);
    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(data);
    strm.avail_in = static_cast<uInt>(size);
    if (inflateInit(&strm) != Z_OK) {
        return false;  // GCOVR_EXCL_LINE：zlib 常量初始化，失败不可构造
    }
    strm.next_out = reinterpret_cast<Bytef*>(out.data());
    strm.avail_out = static_cast<uInt>(expect);
    const int rc = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (rc != Z_STREAM_END || strm.total_out != expect) {
        out.clear();
        return false;
    }
    return true;
}

// 头 Encoding 声明 → UTF-8。UTF-16LE 在此自转（词头/释义的原始字节是
// UTF-16），GBK/GB18030/Latin-1/CP1252 走 charset_codec；声明缺失或不认
// 识的编码按 UTF-8 透传（v2 头未写 Encoding 时实际就是 UTF-8）。
std::string decode_by_declared(const std::string& bytes,
                               const std::string& encoding) {
    const std::string low = lcase(encoding);
    if (encoding.empty() || low == "utf-8" || low == "utf8") {
        return bytes;
    }
    if (low == "utf-16le" || low == "utf16le") {
        return utf16le_to_utf8(reinterpret_cast<const uint8_t*>(bytes.data()),
                               bytes.size() / 2);
    }
    const CharsetCodec::Charset cs = CharsetCodec::from_name(encoding);
    if (!CharsetCodec::is_supported(cs)) {
        return bytes;
    }
    return CharsetCodec::to_utf8(cs, bytes);
}

// 头部解析：返回属性串并推进到 key 节起点。诊断空 = 头形态不合法（不是
// v2 文件，让上层落回既有解析链）。
bool parse_header(const uint8_t* data, size_t size, size_t& key_start,
                  std::string& xml, std::string& diag) {
    diag.clear();
    if (size < 4) {
        return false;
    }
    const uint32_t text_len = be32(data);
    if (text_len < 2 || text_len > kMaxHeaderBytes || (text_len % 2) != 0) {
        return false;
    }
    if (size < 4 + static_cast<size_t>(text_len) + 4) {
        return false;  // 头段截断：不是完整 v2（不设诊断，交上层兜底）
    }
    // UTF-16LE 首单元必须是 '<'（XML）：文本头 JSON/MDXK 魔数等在这里分流
    if (data[4] != '<' || data[5] != 0x00) {
        return false;
    }
    xml = utf16le_to_utf8(data + 4, text_len / 2);
    key_start = 4 + static_cast<size_t>(text_len) + 4;  // 4 字节 adler 读而不验
    return true;
}

// 加密壳解包（Encrypted & 2 的索引块/key 块）：mdict_crypto 口径
// { u32 LE 块信息字; u32 LE adler32(明文); 密文(前 N 字节) }。密钥由调用
// 方派生（索引块 key_info_key / key 块 block_key）。解密后过 adler32 判据
// （防误解密出垃圾——这是权威判据），再按块信息字的压缩方式解压并校验
// 声明的解压长度（索引块取节头、key 块取索引条目，与普通块同判据）。
bool decode_encrypted_block(const std::string& block, const std::string& key,
                            size_t expect_decomp, std::string& out,
                            std::string& diag) {
    out.clear();
    if (block.size() < 8) {
        diag = "decrypt failed (crypto shell)";
        return false;
    }
    std::string work = block;
    const std::string adler_bytes = work.substr(4, 4);
    if (!decrypt_block(work, key)) {
        diag = "decrypt failed (crypto shell)";
        return false;
    }
    if (adler32_of(work) !=
        be32(reinterpret_cast<const uint8_t*>(adler_bytes.data()))) {
        diag = "adler32 mismatch after decrypt (wrong key or corrupt)";
        return false;
    }
    const BlockInfo info = parse_block_info(block);
    if (info.compression == 2) {
        return inflate_expected(reinterpret_cast<const uint8_t*>(work.data()),
                                work.size(), expect_decomp, out);
    }
    if (info.compression == 0) {
        if (work.size() != expect_decomp) {
            diag = "key block decomp size mismatch";
            return false;
        }
        out = work;
        return true;
    }
    diag = "unsupported compression in encrypted block";
    return false;
}

}  // namespace

bool MdxV2ReaderStd::read_file(const std::string& path, MdxV2Dict& out,
                               std::string& diag) {
    diag.clear();
    out = MdxV2Dict{};

    // 两段式 probe：先看头 6 字节（zip 魔数 / v2 头长度与 '<' 单元），
    // 形态不合法直接返回假、不读全文——上层解析链对本路径文件都会先走
    // 一遍这里，非 v2 文件必须零成本分流
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    uint8_t head6[6] = {0};
    const size_t head_n = std::fread(head6, 1, sizeof(head6), f);
    const bool is_zip = head_n == 6 &&
        be32(head6) == kZipMagic;
    const uint32_t probe_len = head_n == 6 ? be32(head6) : 0;
    const bool is_v2_head =
        head_n == 6 && probe_len >= 2 && probe_len <= kMaxHeaderBytes &&
        (probe_len % 2) == 0 && head6[4] == '<' && head6[5] == 0x00;
    if (!is_zip && !is_v2_head) {
        std::fclose(f);
        return false;
    }
    std::rewind(f);  // probe 消费了头 6 字节，全文重读必须回卷
    std::string data;
    char buf[64 * 1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.append(buf, n);
    }
    std::fclose(f);

    if (data.size() >= 4 &&
        be32(reinterpret_cast<const uint8_t*>(data.data())) == kZipMagic) {
        // zip 容器形态：解包取 .mdx 条目后递归走同一解析
        ZipReaderStd zip;
        if (!zip.open(path)) {
            diag = "zip container but open failed";
            return false;
        }
        std::vector<std::string> names = zip.entry_names();
        const std::string stem = lcase(path);
        std::string want = stem;
        const size_t slash = want.find_last_of("/\\");
        if (slash != std::string::npos) want = want.substr(slash + 1);
        const size_t dot = want.rfind('.');
        if (dot != std::string::npos) want = want.substr(0, dot);
        want += ".mdx";

        std::string chosen;
        for (const auto& name : names) {
            if (name.empty() || name.back() == '/') continue;
            const std::string low = lcase(name);
            if (low == want) { chosen = name; break; }
            if (chosen.empty() && low.size() > 4 &&
                low.compare(low.size() - 4, 4, ".mdx") == 0) {
                chosen = name;
            }
        }
        if (chosen.empty()) {
            for (const auto& name : names) {
                if (!name.empty() && name.back() != '/') { chosen = name; break; }
            }
        }
        if (chosen.empty()) {
            diag = "zip container has no entry to parse";
            return false;
        }
        std::string inner;
        if (!zip.read_entry(chosen, inner)) {
            diag = "zip entry '" + chosen + "' read failed";
            return false;
        }
        const bool ok = read_buffer(inner, out, diag);
        if (!ok && !diag.empty()) {
            diag = "zip entry '" + chosen + "': " + diag;
        }
        return ok;
    }

    return read_buffer(data, out, diag);
}

bool MdxV2ReaderStd::read_buffer(const std::string& data, MdxV2Dict& out,
                                 std::string& diag) {
    out = MdxV2Dict{};
    diag.clear();

    const uint8_t* base = reinterpret_cast<const uint8_t*>(data.data());
    size_t key_start = 0;
    std::string xml;
    if (!parse_header(base, data.size(), key_start, xml, diag)) {
        return false;  // 诊断空 = 不是 v2 形态（交上层兜底链）
    }

    out.encoding = extract_attr(xml, "Encoding");
    out.title = extract_attr(xml, "Title");
    if (out.title.empty()) out.title = extract_attr(xml, "title");
    out.description = extract_attr(xml, "Description");
    if (out.description.empty()) out.description = extract_attr(xml, "description");

    // 引擎版本：1.x 的 key 块普遍 LZO（不支持），明确拒收
    const std::string engine = extract_attr(xml, "GeneratedByEngineVersion");
    if (engine != "2.0") {
        diag = "engine version '" + engine + "' not supported (need 2.0)";
        return false;
    }

    // 加密位判定（mdict_crypto 口径）：数值 1/3 含 &1 = record 块 Salsa20，
    // 需要用户 regcode 派生——不支持，诚实失败；2 / "Yes" = key 侧固定密
    // 钥加密，走解密壳
    const std::string enc_attr = extract_attr(xml, "Encrypted");
    const std::string enc_low = lcase(enc_attr);
    bool key_encrypted = false;
    if (!enc_low.empty() && enc_low != "0" && enc_low != "no" &&
        enc_low != "false" && enc_low != "off") {
        if (enc_low == "yes" || enc_low == "true") {
            key_encrypted = true;
        } else {
            const int v = std::atoi(enc_low.c_str());
            if (v & 1) {
                diag = "record block encryption (Encrypted=" + enc_attr +
                       ") not supported";
                return false;
            }
            key_encrypted = (v & 2) != 0;
            if (!key_encrypted && v != 0) {
                diag = "unknown Encrypted=" + enc_attr;
                return false;
            }
        }
    }

    // 文本单元宽度（索引块词长字段与 key 块 NUL 终止符扫描用）：
    // UTF-16LE 2 字节/单元，其余 1
    const std::string enc_low_attr = lcase(out.encoding);
    const size_t unit = (enc_low_attr == "utf-16le" || enc_low_attr == "utf16le")
                            ? 2 : 1;

    // ---- key 节：5×u64 BE + u32 adler（读而不验） ----
    if (data.size() < key_start + 44) {
        diag = "key section header truncated";
        return false;
    }
    const uint8_t* kh = base + key_start;
    const uint64_t num_key_blocks = be64(kh);
    const uint64_t info_decomp_size = be64(kh + 16);
    const uint64_t info_size = be64(kh + 24);
    const uint64_t key_block_size = be64(kh + 32);
    if (num_key_blocks == 0 || num_key_blocks > kMaxKeyBlocks) {
        diag = "key block count " + std::to_string(num_key_blocks) + " out of range";
        return false;
    }
    if (info_size < 8 || info_size > kMaxKeyInfoBytes) {
        diag = "key block info size " + std::to_string(info_size) + " out of range";
        return false;
    }

    // ---- 索引块 ----
    const size_t info_start = key_start + 44;
    if (data.size() < info_start + info_size) {
        diag = "key block info truncated";
        return false;
    }
    std::string info;
    const std::string info_raw = data.substr(info_start, static_cast<size_t>(info_size));
    if (key_encrypted) {
        if (!decode_encrypted_block(
                info_raw, key_info_key(info_raw.substr(4, 4)),
                static_cast<size_t>(info_decomp_size), info, diag)) {
            if (diag.empty()) diag = "key block info decode failed";
            return false;
        }
    } else {
        const uint8_t* ir = reinterpret_cast<const uint8_t*>(info_raw.data());
        // GCOVR_EXCL_START：防御护栏结构不可达——info_size ≥ 8 由上方
        // 范围检查保证（385），substr 界内保证 info_raw 长度恒等 info_size
        if (info_raw.size() < 8) {  // GCOVR_EXCL_LINE
            diag = "key block info decode failed";  // GCOVR_EXCL_LINE
            return false;  // GCOVR_EXCL_LINE
        }  // GCOVR_EXCL_LINE
        // GCOVR_EXCL_STOP
        const uint32_t comp_type = be32(ir);
        if (comp_type == 2) {
            if (!inflate_expected(ir + 8, info_raw.size() - 8,
                                  static_cast<size_t>(info_decomp_size), info)) {
                diag = "key block info decode failed";
                return false;
            }
        } else if (comp_type == 0) {
            info = info_raw.substr(8);
        } else {
            diag = "key block info decode failed";
            return false;
        }
    }
    if (info.size() != info_decomp_size) {
        diag = "key block info decomp size mismatch";
        return false;
    }

    // 索引条目：{ u64 条目数(读而不验); u16 首词单元数; 首词+NUL;
    //             u16 末词单元数; 末词+NUL; u64 压缩长; u64 解压长 }
    std::vector<std::pair<uint64_t, uint64_t>> block_sizes;  // (压缩长, 解压长)
    {
        size_t ip = 0;
        for (uint64_t i = 0; i < num_key_blocks; ++i) {
            if (ip + 10 > info.size()) {
                diag = "key block info truncated";
                return false;
            }
            ip += 8;  // 块内条目数读而不校验（坏计数在块解析中自然暴露）
            const size_t first_units = be16(reinterpret_cast<const uint8_t*>(info.data()) + ip);
            ip += 2 + (first_units + 1) * unit;
            if (ip + 2 > info.size()) {
                diag = "key block info truncated";
                return false;
            }
            const size_t last_units = be16(reinterpret_cast<const uint8_t*>(info.data()) + ip);
            ip += 2 + (last_units + 1) * unit;
            if (ip + 16 > info.size()) {
                diag = "key block info truncated";
                return false;
            }
            block_sizes.emplace_back(be64(reinterpret_cast<const uint8_t*>(info.data()) + ip),
                                     be64(reinterpret_cast<const uint8_t*>(info.data()) + ip + 8));
            ip += 16;
        }
    }

    // ---- key 块：条目 { u64 record_offset; 词头; NUL } ----
    const uint8_t* kd = base + info_start + info_size;
    if (data.size() < info_start + info_size + key_block_size) {
        diag = "key blocks truncated";
        return false;
    }
    std::vector<std::pair<std::string, uint64_t>> key_list;  // (词头, offset)
    uint64_t sum_comp = 0;
    {
        uint64_t cursor = 0;
        for (const auto& sz : block_sizes) {
            if (cursor + sz.first > key_block_size) {
                diag = "key blocks truncated";
                return false;
            }
            std::string block = data.substr(
                info_start + info_size + static_cast<size_t>(cursor),
                static_cast<size_t>(sz.first));
            std::string blob;
            if (key_encrypted) {
                const std::string adler_bytes = block.size() >= 8
                                                    ? block.substr(4, 4)
                                                    : std::string();
                if (!decode_encrypted_block(block, block_key(adler_bytes),
                                            static_cast<size_t>(sz.second),
                                            blob, diag)) {
                    if (diag.empty()) diag = "key block decode failed";
                    return false;
                }
            } else {
                if (block.size() < 8) {
                    diag = "key block decode failed";
                    return false;
                }
                const uint8_t* br = reinterpret_cast<const uint8_t*>(block.data());
                const uint32_t comp_type = be32(br);
                if (comp_type == 2) {
                    if (!inflate_expected(br + 8, block.size() - 8,
                                          static_cast<size_t>(sz.second), blob)) {
                        diag = "key block decode failed";
                        return false;
                    }
                } else if (comp_type == 0) {
                    blob = block.substr(8);
                } else {
                    diag = "key block decode failed";
                    return false;
                }
            }
            if (blob.size() != sz.second) {
                diag = "key block decomp size mismatch";
                return false;
            }
            // 词头扫描：8 字节 offset + 文本 + NUL 终止符（按单元宽度）
            const uint8_t* bp = reinterpret_cast<const uint8_t*>(blob.data());
            size_t pos = 0;
            while (pos + 8 <= blob.size()) {
                const uint64_t rec_off = be64(bp + pos);
                pos += 8;
                const size_t text_begin = pos;
                while (pos + unit <= blob.size()) {
                    bool zero = true;
                    for (size_t u = 0; u < unit; ++u) {
                        if (bp[pos + u] != 0) { zero = false; break; }
                    }
                    if (zero) break;
                    pos += unit;
                }
                if (pos + unit > blob.size()) {
                    diag = "key block word missing NUL terminator";
                    return false;
                }
                std::string raw_word(reinterpret_cast<const char*>(bp + text_begin),
                                     pos - text_begin);
                key_list.emplace_back(decode_by_declared(raw_word, out.encoding),
                                      rec_off);
                pos += unit;  // 终止符
            }
            sum_comp += sz.first;
            cursor += sz.first;
        }
    }
    if (sum_comp != key_block_size) {
        diag = "key block size mismatch";
        return false;
    }
    if (key_list.empty()) {
        diag = "no entries found in key blocks";
        return false;
    }

    // ---- record 节：4×u64 BE + 直排块表 + 压缩块 ----
    const size_t rec_start = info_start + info_size + static_cast<size_t>(key_block_size);
    if (data.size() < rec_start + 32) {
        diag = "record section header truncated";
        return false;
    }
    const uint8_t* rh = base + rec_start;
    const uint64_t num_record_blocks = be64(rh);
    const uint64_t rec_info_size = be64(rh + 16);
    const uint64_t rec_block_size = be64(rh + 24);
    if (num_record_blocks == 0 || num_record_blocks > kMaxRecordBlocks) {
        diag = "record block count " + std::to_string(num_record_blocks) + " out of range";
        return false;
    }
    if (rec_info_size != 16 * num_record_blocks) {
        diag = "record block info size mismatch";
        return false;
    }
    if (data.size() < rec_start + 32 + rec_info_size) {
        diag = "record block info truncated";
        return false;
    }
    std::vector<std::pair<uint64_t, uint64_t>> rec_sizes;  // (压缩长, 解压长)
    for (uint64_t i = 0; i < num_record_blocks; ++i) {
        const uint8_t* e = base + rec_start + 32 + i * 16;
        rec_sizes.emplace_back(be64(e), be64(e + 8));
    }

    // record 块全量解压拼接（词条切片靠相邻 offset 差）。512MB 封顶防
    // 炸弹；大词典惰性化属后续真实样本批次。
    std::string stream;
    {
        uint64_t sum = 0;
        uint64_t cursor = rec_start + 32 + rec_info_size;
        for (const auto& sz : rec_sizes) {
            if (stream.size() + sz.second > kMaxStreamBytes) {
                diag = "record stream exceeds cap";
                return false;
            }
            if (data.size() < cursor + sz.first) {
                diag = "record blocks truncated";
                return false;
            }
            std::string block = data.substr(static_cast<size_t>(cursor),
                                            static_cast<size_t>(sz.first));
            const uint8_t* br = reinterpret_cast<const uint8_t*>(block.data());
            if (block.size() < 8) {
                diag = "record block decode failed";
                return false;
            }
            const uint32_t comp_type = be32(br);
            std::string blob;
            if (comp_type == 2) {
                if (!inflate_expected(br + 8, block.size() - 8,
                                      static_cast<size_t>(sz.second), blob)) {
                    diag = "record block decode failed";
                    return false;
                }
            } else if (comp_type == 0) {
                blob = block.substr(8);
            } else {
                diag = "record block decode failed";
                return false;
            }
            if (blob.size() != sz.second) {
                diag = "record block decomp size mismatch";
                return false;
            }
            stream.append(blob);
            sum += sz.first;
            cursor += sz.first;
        }
        if (sum != rec_block_size) {
            diag = "record block size mismatch";
            return false;
        }
    }

    // ---- 切释义：offset 指拼接流，长度 = 相邻差（乱序钳 0） ----
    // 释义尾部的 NUL 终止单元剥掉（Qt fixture 与部分引擎在释义末尾写
    // NUL 分隔；无终止符变体剥零次自然通过）
    out.entries.reserve(key_list.size());
    for (size_t i = 0; i < key_list.size(); ++i) {
        const uint64_t off = key_list[i].second;
        const uint64_t next = (i + 1 < key_list.size()) ? key_list[i + 1].second
                                                        : stream.size();
        MdxV2Entry e;
        e.key = key_list[i].first;
        if (off < stream.size() && next > off) {
            size_t len = static_cast<size_t>(
                std::min<uint64_t>(next, stream.size()) - off);
            while (len >= unit) {
                bool zero_tail = true;
                for (size_t u = 0; u < unit; ++u) {
                    if (stream[static_cast<size_t>(off) + len - unit + u] != 0) {
                        zero_tail = false;
                        break;
                    }
                }
                if (!zero_tail) break;
                len -= unit;
            }
            e.definition = decode_by_declared(
                stream.substr(static_cast<size_t>(off), len), out.encoding);
        }
        out.entries.push_back(std::move(e));
    }
    return true;
}

}  // namespace UnidictCoreStd
