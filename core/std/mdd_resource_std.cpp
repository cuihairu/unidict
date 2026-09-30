// MDict .mdd resource file parser implementation (std-only).

#include "mdd_resource_std.h"
#include "path_utils_std.h"
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <ctime>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// ============================================================================
// MddResourceParser Implementation
// ============================================================================

namespace {
    // MDD magic bytes
    const uint8_t MDD_MAGIC_V1[] = {0x1b, 0x23, 0x45};  // Older format
    const uint8_t MDD_MAGIC_V2[] = {0x1b, 0x23, 0x01};  // Newer format

    // Block signatures
    const char* BLOCK_SIGNATURE_RBLK = "RBLK";
    const char* BLOCK_SIGNATURE_RBCT = "RBCT";

    // Maximum resource size to load into memory (10MB)
    const size_t MAX_RESOURCE_SIZE = 10 * 1024 * 1024;

    // 缓存文件名的长度上限。ext4/APFS/NTFS 的单个文件名上限都是 255 字节，
    // 留出余量取 200（路径总长另见 MAX_CACHE_PATH_LEN）。
    const size_t MAX_CACHE_NAME_LEN = 200;

    // 路径总长上限：Windows MAX_PATH=260（含终止符，可用 259）。POSIX 无
    // 此硬限（4096），统一按同一口径收——文件名本来就是摘要名，短一点
    // 无害，换回的是"目录深也绝不静默写失败"的单口径。
    const size_t MAX_CACHE_PATH_LEN = 259;

    uint64_t fnv1a64(const void* data, size_t len) {
        const unsigned char* p = static_cast<const unsigned char*>(data);
        uint64_t h = 1469598103934665603ULL;
        for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
        return h;
    }

    // 取扩展名（含点），异常长的（>12 字节，说明键本身畸形）当没有
    std::string file_extension(const std::string& name) {
        const size_t dot = name.rfind('.');
        if (dot == std::string::npos || name.size() - dot > 12) {
            return {};
        }
        return name.substr(dot);
    }

    // Big-endian reading helpers
    inline uint16_t be16(const uint8_t* p) {
        return (uint16_t)p[0] << 8 | p[1];
    }

    inline uint32_t be32(const uint8_t* p) {
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 |
               (uint32_t)p[2] << 8 | p[3];
    }

    inline uint64_t be64(const uint8_t* p) {
        return (uint64_t)p[0] << 56 | (uint64_t)p[1] << 48 |
               (uint64_t)p[2] << 40 | (uint64_t)p[3] << 32 |
               (uint64_t)p[4] << 24 | (uint64_t)p[5] << 16 |
               (uint64_t)p[6] << 8 | p[7];
    }

    // Simple decompression using zlib
    bool decompress_zlib(const uint8_t* input, size_t input_len,
                        std::vector<uint8_t>& output) {
#ifdef USE_ZLIB
        z_stream stream = {};
        // 窗口位 15+32 参数合法，inflateInit2 只在 OOM/版本不符时失败，
        // 测试无法构造。
        // GCOVR_EXCL_START
        if (inflateInit2(&stream, 15 + 32) != Z_OK) {
            return false;
        }
        // GCOVR_EXCL_STOP

        stream.avail_in = static_cast<uInt>(input_len);
        stream.next_in = const_cast<uint8_t*>(input);

        // Estimate output size (start with 2x input)
        output.resize(std::max(input_len * 2, size_t(1024)));
        size_t output_pos = 0;

        int ret;
        do {
            stream.avail_out = static_cast<uInt>(output.size() - output_pos);
            stream.next_out = output.data() + output_pos;

            ret = inflate(&stream, Z_NO_FLUSH);

            if (ret == Z_OK || ret == Z_STREAM_END) {
                output_pos = stream.total_out;
                if (ret == Z_OK && stream.avail_out == 0) {
                    // Need more output space
                    output.resize(output.size() * 2);
                }
            }
        } while (ret == Z_OK);

        inflateEnd(&stream);

        if (ret != Z_STREAM_END) {
            return false;
        }

        output.resize(stream.total_out);
        return true;
#else
        // Fallback: copy input to output (assuming uncompressed)
        output.assign(input, input + input_len);
        return true;
#endif
    }

    // ===== 真实 MDict（引擎 2.0）格式的辅助 ===============================

    // 真实 MDict 头/键/索引节的上限：防谎报尺寸把内存打爆
    const uint32_t MAX_MDICT_HEADER_BYTES = 1u << 20;         // 头文本 1MB
    const uint64_t MAX_MDICT_KEY_BLOCKS = 1000000;            // 块数上限
    const uint64_t MAX_MDICT_KEY_INFO_BYTES = 32ull << 20;    // 索引节 32MB
    const uint64_t MAX_MDICT_RECORD_BLOCKS = 1000000;

    // 码点追加为 UTF-8（1/2/3/4 字节形态）
    void append_utf8(std::string& out, uint32_t cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // UTF-16LE → UTF-8：真实 MDD 的头文本与键文本都是 UTF-16LE。代理对
    // 按规格组合；孤立/截断代理替换为 U+FFFD（与 WHATWG 同口径）而不判
    // 失败——资源名里混进坏单元不该让整个词典加载失败。
    std::string utf16le_to_utf8(const uint8_t* p, size_t len) {
        std::string out;
        for (size_t i = 0; i + 1 < len; i += 2) {
            uint32_t cp = static_cast<uint32_t>(p[i]) |
                          static_cast<uint32_t>(p[i + 1]) << 8;
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                if (i + 3 < len) {
                    const uint32_t lo = static_cast<uint32_t>(p[i + 2]) |
                                        static_cast<uint32_t>(p[i + 3]) << 8;
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i += 2;
                    } else {
                        cp = 0xFFFD;  // 高代理后随非低代理
                    }
                } else {
                    cp = 0xFFFD;      // 尾部截断的代理对
                }
            } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                cp = 0xFFFD;          // 孤立低代理
            }
            append_utf8(out, cp);
        }
        return out;
    }

    // 头 XML 是单标签属性串（writemdict 规格）；取 attr="value"，找不到
    // 或引号未闭合返回空
    std::string extract_attr(const std::string& xml, const char* name) {
        const std::string needle = std::string(name) + "=\"";
        const size_t pos = xml.find(needle);
        if (pos == std::string::npos) {
            return {};
        }
        const size_t begin = pos + needle.size();
        const size_t endq = xml.find('"', begin);
        if (endq == std::string::npos) {
            return {};
        }
        return xml.substr(begin, endq - begin);
    }

    // 真实 MDict 的通用块封装：{ u32 BE comp_type; u32 BE adler32; payload }。
    // comp_type 0=存储 / 2=zlib / 1=LZO（引擎 1.x 用，无 liblzo 不支持）。
    // adler 读而不校验（writemdict / mdict-analysis 等上游库同口径）。
    bool decode_mdict_block(const uint8_t* data, size_t len,
                            std::vector<uint8_t>& out) {
        if (len < 8) {
            return false;
        }
        const uint32_t comp_type = be32(data);
        if (comp_type == 2) {
            return decompress_zlib(data + 8, len - 8, out);
        }
        if (comp_type == 0) {
            out.assign(data + 8, data + len);
            return true;
        }
        return false;
    }
}

MddResourceParser::MddResourceParser() {
}

// 打开 .mdd：Windows 下 CRT fopen 的共享模式不带 FILE_SHARE_DELETE——
// app 挂着 .mdd 时用户删/换词典文件会被"另一个程序正在使用"挡住
// （POSIX unlink 语义没这个问题，测试 mdd_remount_after_file_swap 在
// Windows CI 挂的正是这里）。改走 CreateFileW + FILE_SHARE_DELETE 再
// 转 CRT FILE*，与 POSIX 行为对齐；顺带修掉 fopen 用 ACP 解 UTF-8
// 路径、非 ASCII 词典路径直接打不开的坑（MultiByteToWideChar 按
// UTF-8 转）。非 Windows 维持 fopen。
#ifdef _WIN32
static std::FILE* open_mdd_shared(const std::string& mdd_path) {
    std::wstring wide;
    if (!mdd_path.empty()) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, mdd_path.data(),
                                          static_cast<int>(mdd_path.size()),
                                          nullptr, 0);
        wide.resize(static_cast<size_t>(n));
        MultiByteToWideChar(CP_UTF8, 0, mdd_path.data(),
                            static_cast<int>(mdd_path.size()), wide.data(), n);
    }
    HANDLE h = CreateFileW(wide.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return nullptr;
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(h), _O_RDONLY);
    if (fd < 0) {
        CloseHandle(h);
        return nullptr;
    }
    std::FILE* f = _fdopen(fd, "rb");
    if (!f) _close(fd);
    return f;
}
#else
static std::FILE* open_mdd_shared(const std::string& mdd_path) {
    return std::fopen(mdd_path.c_str(), "rb");
}
#endif

bool MddResourceParser::load(const std::string& mdd_path) {
    unload();

    mdd_path_ = mdd_path;
    file_ = open_mdd_shared(mdd_path);
    if (!file_) {
        return false;
    }

    if (!parse_header()) {
        // Fallback: support the project's SimpleKV container (used by unit tests and lightweight dicts).
        if (!parse_simplekv_fallback()) {
            std::fclose(file_);
            file_ = nullptr;
            return false;
        }
        loaded_ = true;
        return true;
    }

    if (!parse_resource_blocks()) {
        std::fclose(file_);
        file_ = nullptr;
        return false;
    }

    loaded_ = true;
    return true;
}

void MddResourceParser::unload() {
    if (file_) {
        std::fclose(file_);
        file_ = nullptr;
    }
    resources_.clear();
    resource_keys_.clear();
    mdict_record_blocks_.clear();
    mdict_cached_block_ = static_cast<size_t>(-1);
    mdict_cached_data_.clear();
    mdict_real_ = false;
    loaded_ = false;
}

MddResourceParser::~MddResourceParser() {
    unload();
}

bool MddResourceParser::parse_header() {
    // Read first 4 bytes to detect magic
    uint8_t magic[4] = {0};
    if (std::fread(magic, 1, 4, file_) != 4) {
        return false;
    }
    std::rewind(file_);

    // Detect version
    if (std::memcmp(magic, MDD_MAGIC_V1, 3) == 0) {
        header_.magic = std::string(reinterpret_cast<char*>(magic), 3);
        return parse_v1_header();
    } else if (std::memcmp(magic, MDD_MAGIC_V2, 3) == 0) {
        header_.magic = std::string(reinterpret_cast<char*>(magic), 3);
        return parse_v2_header();
    }

    // 两种自定义魔数都不匹配：尝试真实 MDict 布局（无魔数，文件开头就是
    // u32 BE 头文本长度 + UTF-16LE XML）。前 4 字节已在 magic 里，直接传
    // 下去不重读；不是真实 MDict（长度/属性不合法）则返回假走 SimpleKV
    // 兜底链
    return parse_mdict_header(magic);
}

bool MddResourceParser::parse_v1_header() {
    // V1 format: magic (3) + header_len (4) + version (4) + ...
    // 注意 buf[0..2] 是 magic：parse_header 读过 magic 后 rewind 了，本函数
    // 从偏移 0 重新读，所以字段必须从 buf+3 起取。原先从 buf / buf+4 取，
    // 等于把 magic 当成 header_len 的高位——V1 会算出 0x1b2345 ≈ 1.7MB，
    // 后面 fseek 直接跳到 EOF 之后，真实 .mdd 一律加载失败。
    uint8_t buf[12] = {0};
    if (std::fread(buf, 1, 12, file_) != 12) {
        return false;
    }

    header_.header_len = be32(buf + 3);
    header_.version = be32(buf + 7);

    // header_len 至少要盖住已读的 12 字节，否则下面的减法下溢成一个
    // 巨大的 uint32（fseek 跳飞/后续偏移全歪）——畸形文件直接拒收
    if (header_.header_len < 12) {
        return false;
    }

    // Skip to end of header
    uint32_t remaining = header_.header_len - 12;
    if (remaining > 0) {
        std::fseek(file_, remaining, SEEK_CUR);
    }

    // Get file size
    std::fseek(file_, 0, SEEK_END);
    header_.total_size = std::ftell(file_);
    std::fseek(file_, header_.header_len, SEEK_SET);

    return true;
}

bool MddResourceParser::parse_v2_header() {
    // V2 format: magic (3) + header_len (2) + version (2) + ...
    // 同 parse_v1_header：buf[0..2] 是 magic，字段从 buf+3 起取。原先从
    // buf / buf+2 取会把 magic 当 header_len，算出 0x1b2301 ≈ 1.7MB。
    uint8_t buf[8] = {0};
    if (std::fread(buf, 1, 8, file_) != 8) {
        return false;
    }

    header_.header_len = be16(buf + 3);
    header_.version = be16(buf + 5);

    // 同 parse_v1_header：header_len 小于已读字节数即畸形，拒收
    if (header_.header_len < 8) {
        return false;
    }

    // Skip to end of header
    uint32_t remaining = header_.header_len - 8;
    if (remaining > 0) {
        std::fseek(file_, remaining, SEEK_CUR);
    }

    // Get file size
    std::fseek(file_, 0, SEEK_END);
    header_.total_size = std::ftell(file_);
    std::fseek(file_, header_.header_len, SEEK_SET);

    return true;
}

bool MddResourceParser::parse_mdict_header(const uint8_t len4[4]) {
    // 真实 MDict .mdd 头（writemdict fileformat.md，引擎 2.0）：
    //   u32 BE text_len + text_len 字节 UTF-16LE XML 属性串
    //   + u32 LE adler32（读而不验，上游写库同口径）
    const uint32_t text_len = be32(len4);
    // 长度三轮 sanity：奇数（不是 UTF-16）、过小、超上限（1MB）都直接判
    // 非真实 MDict——SimpleKV/zip/JSON 等文件的首 u32 BE 几乎都落在这里
    if (text_len < 2 || text_len > MAX_MDICT_HEADER_BYTES ||
        (text_len % 2) != 0) {
        return false;
    }
    std::vector<uint8_t> text(text_len);
    // parse_header 读过 4 字节 magic 后 rewind 了；跳过长度字段读头文本
    std::fseek(file_, 4, SEEK_SET);
    if (std::fread(text.data(), 1, text_len, file_) != text_len) {
        return false;
    }
    uint8_t checksum[4];
    if (std::fread(checksum, 1, 4, file_) != 4) {
        return false;  // adler32 占位（LE 存储），不比对
    }
    const std::string header_text = utf16le_to_utf8(text.data(), text.size());

    // 只支持引擎 2.0 + 未加密：1.2 的 key 块普遍 LZO 压缩（无 liblzo 不可
    // 解）；Encrypted=1 加密 record 块、=2 加密 key 信息块，同样超出当前
    // 能力，一律拒收（走 SimpleKV 兜底后整体 load 失败，诚实失败）
    if (extract_attr(header_text, "GeneratedByEngineVersion") != "2.0") {
        return false;
    }
    const std::string encrypted = extract_attr(header_text, "Encrypted");
    if (!encrypted.empty() && encrypted != "0") {
        return false;
    }

    mdict_real_ = true;
    header_ = {};
    header_.magic = "MDICT";
    header_.header_len = 4 + text_len + 4;  // 恰为 key 节的绝对起点
    header_.version = 2;                    // GeneratedByEngineVersion "2.0"
    return true;
}

bool MddResourceParser::parse_simplekv_fallback() {
    std::fseek(file_, 0, SEEK_END);
    long file_size_l = std::ftell(file_);
    if (file_size_l <= 0) return false;
    uint64_t file_size = static_cast<uint64_t>(file_size_l);
    std::fseek(file_, 0, SEEK_SET);

    const std::string magic = "SIMPLEKV";
    constexpr size_t kChunkSize = 64 * 1024;
    std::vector<char> chunk(kChunkSize);

    uint64_t cursor = 0;
    std::string overlap;
    overlap.reserve(magic.size() - 1);

    uint64_t magic_offset = 0;
    bool found_magic = false;

    while (true) {
        size_t nread = std::fread(chunk.data(), 1, chunk.size(), file_);
        if (nread == 0) break;

        std::string combined = overlap;
        combined.append(chunk.data(), nread);

        size_t found = combined.find(magic);
        if (found != std::string::npos) {
            uint64_t combined_start = cursor - static_cast<uint64_t>(overlap.size());
            magic_offset = combined_start + static_cast<uint64_t>(found);
            found_magic = true;
            break;
        }

        cursor += static_cast<uint64_t>(nread);
        if (combined.size() >= magic.size() - 1) {
            overlap = combined.substr(combined.size() - (magic.size() - 1));
        } else {
            overlap = combined;
        }
    }

    if (!found_magic) {
        std::fseek(file_, 0, SEEK_SET);
        return false;
    }

    std::fseek(file_, static_cast<long>(magic_offset + magic.size()), SEEK_SET);

    auto read_exact = [&](void* out, size_t n) -> bool {
        return std::fread(out, 1, n, file_) == n;
    };
    auto be16_local = [](const uint8_t* p) -> uint16_t {
        return (uint16_t)p[0] << 8 | p[1];
    };
    auto be32_local = [](const uint8_t* p) -> uint32_t {
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    };

    uint8_t count_be[4] = {0};
    if (!read_exact(count_be, sizeof(count_be))) {
        std::fseek(file_, 0, SEEK_SET);
        return false;
    }
    uint32_t count = be32_local(count_be);

    resources_.clear();
    resource_keys_.clear();
    resources_.reserve(count);
    resource_keys_.reserve(count);

    for (uint32_t n = 0; n < count; ++n) {
        uint8_t klen_be[2] = {0};
        if (!read_exact(klen_be, sizeof(klen_be))) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }
        uint16_t klen = be16_local(klen_be);
        if (klen == 0) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }

        std::string key;
        key.resize(klen);
        if (!read_exact(key.data(), klen)) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }

        uint8_t vlen_be[4] = {0};
        if (!read_exact(vlen_be, sizeof(vlen_be))) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }
        uint32_t vlen = be32_local(vlen_be);

        long value_offset_l = std::ftell(file_);
        // ftell 只在流已出错/无 seek 能力时返回 -1。前面每次 fread 都用
        // read_exact 校验过失败并提前返回，走到这里的流状态必然正常，
        // 测试无法构造 ftell 失败。
        // GCOVR_EXCL_START
        if (value_offset_l < 0) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }
        // GCOVR_EXCL_STOP
        uint64_t value_offset = static_cast<uint64_t>(value_offset_l);
        if (value_offset + static_cast<uint64_t>(vlen) > file_size) {
            std::fseek(file_, 0, SEEK_SET);
            return false;
        }

        MddResourceEntry entry;
        entry.key = normalize_key(key);
        entry.offset = value_offset;
        entry.size = vlen;
        entry.uncompressed_size = 0;
        entry.block_id = 0;
        entry.is_compressed = false;

        resources_[entry.key] = entry;
        resource_keys_.push_back(entry.key);

        std::fseek(file_, static_cast<long>(vlen), SEEK_CUR);
    }

    header_ = {};
    header_.magic = magic;
    header_.total_size = file_size;

    std::fseek(file_, 0, SEEK_SET);
    return true;
}

bool MddResourceParser::parse_resource_blocks() {
    // 真实 MDict：节布局由 parse_mdict_sections 处理，当前位置没有
    // RBCT/单块语义
    if (mdict_real_) {
        return parse_mdict_sections();
    }

    // Read initial bytes to detect format
    uint8_t sig[8] = {0};
    long pos = std::ftell(file_);
    if (std::fread(sig, 1, 4, file_) != 4) {
        return false;
    }
    std::fseek(file_, pos, SEEK_SET);

    // Check for RBCT signature (multi-block format)
    if (std::memcmp(sig, BLOCK_SIGNATURE_RBCT, 4) == 0) {
        return parse_multi_block();
    }

    // Otherwise assume single-block format
    return parse_single_block();
}

bool MddResourceParser::parse_single_block() {
    // Single block format: list of { key_len, key, offset, size }
    // This is a simplified format for smaller .mdd files

    long start_pos = std::ftell(file_);
    std::fseek(file_, 0, SEEK_END);
    long end_pos = std::ftell(file_);
    std::fseek(file_, start_pos, SEEK_SET);

    while (std::ftell(file_) < end_pos) {
        // Read key length
        uint8_t len_buf[2];
        if (std::fread(len_buf, 1, 2, file_) != 2) {
            break;
        }
        uint16_t key_len = be16(len_buf);
        if (key_len == 0 || key_len > 1024) {
            break;  // Invalid key length
        }

        // Read key
        std::string key(key_len, '\0');
        if (std::fread(&key[0], 1, key_len, file_) != key_len) {
            break;
        }

        // Read offset and size
        uint8_t entry_buf[16];
        if (std::fread(entry_buf, 1, 16, file_) != 16) {
            break;
        }
        uint64_t offset = be64(entry_buf);
        uint64_t size = be64(entry_buf + 8);

        // Normalize and store
        std::string normalized = normalize_key(key);
        MddResourceEntry entry;
        entry.key = normalized;
        entry.offset = offset;
        entry.size = size;
        entry.is_compressed = false;

        resources_[normalized] = entry;
        resource_keys_.push_back(normalized);
    }

    return !resources_.empty();
}

bool MddResourceParser::parse_multi_block() {
    // Multi-block format: RBCT + num_blocks + repeated { RBLK + comp_len + data }

    // Read RBCT signature
    char sig[4];
    // GCOVR_EXCL_START：唯一调用方 parse_resource_blocks 在调本函数前已经
    // 读过这 4 个字节并比对过 RBCT，所以这里的重复校验在同一次 load 里
    // 不可能失败——属于防御性重复检查，测试无法构造不匹配。
    if (std::fread(sig, 1, 4, file_) != 4 || std::memcmp(sig, BLOCK_SIGNATURE_RBCT, 4) != 0) {
        return false;
    }
    // GCOVR_EXCL_STOP

    // Read number of blocks
    uint8_t buf[4];
    if (std::fread(buf, 1, 4, file_) != 4) {
        return false;
    }
    uint32_t num_blocks = be32(buf);
    header_.num_blocks = num_blocks;

    // Parse each block
    for (uint32_t block_id = 0; block_id < num_blocks; ++block_id) {
        // Read RBLK signature
        if (std::fread(sig, 1, 4, file_) != 4 || std::memcmp(sig, BLOCK_SIGNATURE_RBLK, 4) != 0) {
            break;
        }

        // Read compressed length
        if (std::fread(buf, 1, 4, file_) != 4) {
            break;
        }
        uint32_t comp_len = be32(buf);

        // Read compressed data
        std::vector<uint8_t> comp_data(comp_len);
        if (std::fread(comp_data.data(), 1, comp_len, file_) != comp_len) {
            break;
        }

        // Decompress
        std::vector<uint8_t> block_data;
        if (!decompress_zlib(comp_data.data(), comp_len, block_data)) {
            continue;
        }

        // Parse resource entries from decompressed block
        const uint8_t* p = block_data.data();
        const uint8_t* end = p + block_data.size();

        while (p + 2 <= end) {
            uint16_t key_len = be16(p);
            p += 2;

            if (key_len == 0 || p + key_len > end) {
                break;
            }

            std::string key(reinterpret_cast<const char*>(p), key_len);
            p += key_len;

            if (p + 16 > end) {
                break;
            }

            uint64_t offset = be64(p);
            uint64_t size = be64(p + 8);
            p += 16;

            std::string normalized = normalize_key(key);
            MddResourceEntry entry;
            entry.key = normalized;
            entry.offset = offset;
            entry.size = size;
            entry.block_id = block_id;
            entry.is_compressed = false;

            resources_[normalized] = entry;
            resource_keys_.push_back(normalized);
        }
    }

    return !resources_.empty();
}

bool MddResourceParser::parse_mdict_sections() {
    // 真实 MDict 引擎 2.0 的两节布局（writemdict fileformat.md）：
    //   key 节：5×u64 BE（num_key_blocks / num_entries /
    //           key_block_info_decomp_size / key_block_info_size /
    //           key_block_size）+ u32 adler + 索引块 + num_key_blocks 个
    //           压缩 key 块
    //   record 节：4×u64 BE（num_record_blocks / num_entries /
    //           record_block_info_size / record_block_size）+
    //           n×(u64 comp, u64 decomp) + n 个压缩块
    // 条目里的 record offset 指向全部 record 块解压后拼接成的流。
    std::fseek(file_, 0, SEEK_END);
    header_.total_size = static_cast<uint64_t>(std::ftell(file_));

    // ---- key 节 ----
    std::vector<uint8_t> num_buf;
    if (!read_bytes(header_.header_len, 44, num_buf)) {
        return false;
    }
    const uint64_t num_key_blocks = be64(num_buf.data());
    // num_entries（+8）读而不校验：宽容口径，坏计数会在块解析中自然
    // 暴露，上游库（mdict-analysis 等）同样不校验
    const uint64_t info_decomp_size = be64(num_buf.data() + 16);
    const uint64_t info_size = be64(num_buf.data() + 24);
    const uint64_t key_block_size = be64(num_buf.data() + 32);
    // num_buf+40 是 key 节 adler32，同头部校验和口径：读而不验
    if (num_key_blocks == 0 || num_key_blocks > MAX_MDICT_KEY_BLOCKS) {
        return false;
    }
    if (info_size < 8 || info_size > MAX_MDICT_KEY_INFO_BYTES) {
        return false;
    }

    // 索引块（key_block_info）：一个通用压缩块，解出后是每 key 块一条的
    // { u64 块内条目数; u16 首词长度; 首词(+NUL); u16 末词长度; 末词(+NUL);
    //   u64 压缩长; u64 解压长 }
    std::vector<uint8_t> info_raw;
    if (!read_bytes(header_.header_len + 44, info_size, info_raw)) {
        return false;
    }
    std::vector<uint8_t> info;
    if (!decode_mdict_block(info_raw.data(), info_raw.size(), info)) {
        return false;
    }
    if (info.size() != info_decomp_size) {
        return false;
    }

    std::vector<std::pair<uint64_t, uint64_t>> block_sizes;  // (压缩长, 解压长)
    size_t ip = 0;
    for (uint64_t i = 0; i < num_key_blocks; ++i) {
        if (ip + 8 > info.size()) {
            return false;  // 块内条目数读到一半
        }
        ip += 8;  // 块内条目数只用于索引展示，不校验
        if (ip + 2 > info.size()) {
            return false;  // 首词长度截断
        }
        const size_t first_units = be16(info.data() + ip);
        ip += 2 + (first_units + 1) * 2;  // 首词文本 + 一个 NUL 单元
        if (ip + 2 > info.size()) {
            return false;  // 末词长度截断
        }
        const size_t last_units = be16(info.data() + ip);
        ip += 2 + (last_units + 1) * 2;
        if (ip + 16 > info.size()) {
            return false;  // 压缩/解压长度截断
        }
        block_sizes.emplace_back(be64(info.data() + ip), be64(info.data() + ip + 8));
        ip += 16;
    }

    // ---- key 块：条目 { u64 record_offset; UTF-16LE 键文本; 0x0000 } ----
    const uint64_t key_data_start = header_.header_len + 44 + info_size;
    std::vector<std::pair<std::string, uint64_t>> key_list;  // (归一键, offset)
    uint64_t cursor = key_data_start;
    uint64_t sum_comp = 0;
    for (const auto& sz : block_sizes) {
        std::vector<uint8_t> raw;
        if (!read_bytes(cursor, sz.first, raw)) {
            return false;
        }
        std::vector<uint8_t> data;
        if (!decode_mdict_block(raw.data(), raw.size(), data)) {
            return false;
        }
        size_t pos = 0;
        while (pos + 8 <= data.size()) {
            const uint64_t rec_off = be64(data.data() + pos);
            pos += 8;
            const size_t text_begin = pos;
            while (pos + 1 < data.size() &&
                   (data[pos] | (data[pos + 1] << 8)) != 0) {
                pos += 2;
            }
            if (pos + 1 >= data.size()) {
                return false;  // 键文本没有 0x0000 终止符
            }
            key_list.emplace_back(
                normalize_key(utf16le_to_utf8(data.data() + text_begin,
                                              pos - text_begin)),
                rec_off);
            pos += 2;  // 终止符
        }
        sum_comp += sz.first;
        cursor += sz.first;
    }
    if (sum_comp != key_block_size) {
        return false;  // 索引声明的块长合计与节头不符
    }
    if (key_list.empty()) {
        return false;
    }

    // ---- record 节：建块表（惰性解压） ----
    const uint64_t rec_start = key_data_start + key_block_size;
    std::vector<uint8_t> rec_buf;
    if (!read_bytes(rec_start, 32, rec_buf)) {
        return false;
    }
    const uint64_t num_record_blocks = be64(rec_buf.data());
    // rec_buf+8 的 num_entries 同 key 节：读而不校验
    const uint64_t rec_info_size = be64(rec_buf.data() + 16);
    const uint64_t rec_block_size = be64(rec_buf.data() + 24);
    if (num_record_blocks == 0 || num_record_blocks > MAX_MDICT_RECORD_BLOCKS) {
        return false;
    }
    if (rec_info_size != 16 * num_record_blocks) {
        return false;
    }
    std::vector<uint8_t> rec_info;
    if (!read_bytes(rec_start + 32, rec_info_size, rec_info)) {
        return false;
    }
    uint64_t total_decomp = 0;
    uint64_t rec_sum_comp = 0;
    uint64_t block_cursor = rec_start + 32 + rec_info_size;
    for (uint64_t i = 0; i < num_record_blocks; ++i) {
        MdictRecordBlock blk;
        blk.file_offset = block_cursor;
        blk.comp_size = be64(rec_info.data() + i * 16);
        blk.decomp_size = be64(rec_info.data() + i * 16 + 8);
        blk.decomp_start = total_decomp;
        mdict_record_blocks_.push_back(blk);
        total_decomp += blk.decomp_size;
        rec_sum_comp += blk.comp_size;
        block_cursor += blk.comp_size;
    }
    if (rec_sum_comp != rec_block_size) {
        return false;  // 块表合计与节头不符
    }

    // ---- 依下一键 offset 推资源长度，填条目表 ----
    for (size_t i = 0; i < key_list.size(); ++i) {
        const uint64_t next_off =
            (i + 1 < key_list.size()) ? key_list[i + 1].second : total_decomp;
        MddResourceEntry entry;
        entry.key = key_list[i].first;
        entry.offset = key_list[i].second;
        // 键序与 record 写入序一致（升序）；乱序文件钳成 0 长度条目
        entry.size = next_off > entry.offset ? next_off - entry.offset : 0;
        entry.uncompressed_size = entry.size;
        entry.is_compressed = true;
        resources_[entry.key] = entry;
        resource_keys_.push_back(entry.key);
    }
    header_.num_blocks = static_cast<uint32_t>(num_record_blocks);
    return true;  // key_list 非空 ⇒ resources_ 非空
}

bool MddResourceParser::read_mdict_record(const MddResourceEntry& entry,
                                          std::vector<uint8_t>& out) const {
    uint64_t pos = entry.offset;
    const uint64_t end = entry.offset + entry.size;
    out.clear();
    while (pos < end) {
        // 找覆盖 pos 的 record 块。块区间按 decomp_start 连续铺满
        // [0, total)，正常必然命中；offset 越过 total 的畸形文件落空
        size_t idx = mdict_record_blocks_.size();
        for (size_t i = 0; i < mdict_record_blocks_.size(); ++i) {
            const auto& b = mdict_record_blocks_[i];
            if (pos < b.decomp_start + b.decomp_size) {
                idx = i;
                break;
            }
        }
        if (idx == mdict_record_blocks_.size()) {
            return false;
        }
        const MdictRecordBlock& blk = mdict_record_blocks_[idx];

        // 单槽缓存：相邻资源常落在同一块，重复查询不必重复解压
        if (mdict_cached_block_ != idx) {
            std::vector<uint8_t> raw;
            if (!read_bytes(blk.file_offset, blk.comp_size, raw)) {
                return false;
            }
            if (!decode_mdict_block(raw.data(), raw.size(), mdict_cached_data_)) {
                return false;
            }
            if (mdict_cached_data_.size() != blk.decomp_size) {
                return false;  // 实际解出尺寸与块表声明不符（谎报文件）
            }
            mdict_cached_block_ = idx;
        }

        // 资源可能跨块：逐块拷贝到覆盖 end 为止
        const uint64_t chunk_end =
            std::min(end, blk.decomp_start + blk.decomp_size);
        const size_t from = static_cast<size_t>(pos - blk.decomp_start);
        const size_t to = static_cast<size_t>(chunk_end - blk.decomp_start);
        out.insert(out.end(), mdict_cached_data_.begin() + from,
                   mdict_cached_data_.begin() + to);
        pos = chunk_end;
    }
    return true;
}

bool MddResourceParser::has_resource(const std::string& key) const {
    std::string normalized = normalize_key(key);
    return resources_.find(normalized) != resources_.end();
}

std::vector<uint8_t> MddResourceParser::get_resource(const std::string& key) const {
    std::vector<uint8_t> result;

    std::string normalized = normalize_key(key);
    auto it = resources_.find(normalized);
    if (it == resources_.end()) {
        return result;
    }

    const auto& entry = it->second;

    // Sanity check size
    if (entry.size > MAX_RESOURCE_SIZE) {
        return result;
    }

    // 真实 MDict：资源躺在压缩 record 块里，entry.offset 是解压后拼接流
    // 中的位置，不能直接读文件字节——走块表惰性解压路径
    if (mdict_real_) {
        if (!read_mdict_record(entry, result)) {
            result.clear();
        }
        return result;
    }

    // Read raw data
    std::vector<uint8_t> data;
    if (!read_bytes(entry.offset, entry.size, data)) {
        return result;
    }
    result = data;

    return result;
}

std::string MddResourceParser::get_resource_as_string(const std::string& key) const {
    auto data = get_resource(key);
    if (data.empty()) {
        return "";
    }
    return std::string(data.begin(), data.end());
}

MddResourceEntry MddResourceParser::get_resource_info(const std::string& key) const {
    std::string normalized = normalize_key(key);
    auto it = resources_.find(normalized);
    if (it != resources_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::string> MddResourceParser::list_resources(const std::string& prefix) const {
    std::vector<std::string> result;

    for (const auto& key : resource_keys_) {
        if (prefix.empty() || key.find(prefix) == 0) {
            result.push_back(key);
        }
    }

    return result;
}

bool MddResourceParser::extract_to_cache(const std::string& key, const std::string& cache_dir) {
    auto data = get_resource(key);
    if (data.empty()) {
        return false;
    }

    // Create cache directory if needed.
    // 用 error_code 重载而不是异常版：cache_dir 来自用户配置/环境变量，
    // 指着已存在的普通文件之类畸形路径时，异常版会抛 filesystem_error
    // 直接把调用方（GUI 资源加载）打穿——这里应当优雅地返回 false。
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        return false;
    }

    // Generate cache file path：只把 key 内部的 '/' 换成 '-'，
    // 不能整串替换——否则连 cache_dir 的目录分隔符一起被换掉，
    // 文件会写到 cwd 下一个畸形名字（此前 bug）
    std::string filename = key;
    std::replace(filename.begin(), filename.end(), '/', '-');
    fs::path cache_path = fs::path(cache_dir) / filename;

    // Write to file
    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

bool MddResourceParser::extract_all_to_cache(const std::string& cache_dir, int max_count) {
    std::error_code ec;
    fs::create_directories(cache_dir, ec);
    if (ec) {
        return false;  // 畸形 cache_dir：优雅退化，见 extract_to_cache 的说明
    }

    int count = 0;
    for (const auto& key : resource_keys_) {
        if (max_count > 0 && count >= max_count) {
            break;
        }

        if (extract_to_cache(key, cache_dir)) {
            ++count;
        }
    }

    return count > 0;
}

// 原先这里有个 decompress_resource(entry, out)（读 entry 处字节后整段
// inflate）：它唯一的调用点是 get_resource 里 is_compressed 的旧分支，
// 而该分支对三种自定义格式恒不可达。真实 MDict 的压缩单位是 record 块
// （一块装多条资源），不是单条资源，旧分支语义对真实格式也不成立——
// 两者已随真实格式支持一并删除，由 read_mdict_record 接替。

std::string MddResourceParser::normalize_key(const std::string& key) {
    std::string result = key;

    // Convert backslashes to forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove leading slashes
    size_t start = result.find_first_not_of("/");
    if (start != std::string::npos) {
        result = result.substr(start);
    }

    // Remove URL protocol prefixes
    std::vector<std::string> prefixes = {
        "file://", "sound://", "entry://", "bword://", "gxres://", "mdd://"
    };

    for (const auto& prefix : prefixes) {
        if (result.size() >= prefix.size()) {
            std::string lower_prefix = prefix;
            std::transform(lower_prefix.begin(), lower_prefix.end(), lower_prefix.begin(), ::tolower);

            std::string lower_result = result.substr(0, prefix.size());
            std::transform(lower_result.begin(), lower_result.end(), lower_result.begin(), ::tolower);

            if (lower_result == lower_prefix) {
                result = result.substr(prefix.size());
                break;
            }
        }
    }

    // Remove query strings and fragments
    size_t query_pos = result.find('?');
    if (query_pos != std::string::npos) {
        result = result.substr(0, query_pos);
    }

    size_t frag_pos = result.find('#');
    if (frag_pos != std::string::npos) {
        result = result.substr(0, frag_pos);
    }

    // Convert to lowercase for case-insensitive lookup
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);

    return result;
}

std::string MddResourceParser::detect_mime_type(const std::string& key) {
    std::string lower = key;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Image types
    if (lower.find(".png") != std::string::npos) return "image/png";
    if (lower.find(".jpg") != std::string::npos || lower.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (lower.find(".gif") != std::string::npos) return "image/gif";
    if (lower.find(".svg") != std::string::npos) return "image/svg+xml";
    if (lower.find(".webp") != std::string::npos) return "image/webp";
    if (lower.find(".bmp") != std::string::npos) return "image/bmp";
    if (lower.find(".ico") != std::string::npos) return "image/x-icon";

    // Audio types
    if (lower.find(".mp3") != std::string::npos) return "audio/mpeg";
    if (lower.find(".wav") != std::string::npos) return "audio/wav";
    if (lower.find(".ogg") != std::string::npos) return "audio/ogg";
    if (lower.find(".m4a") != std::string::npos) return "audio/mp4";
    if (lower.find(".aac") != std::string::npos) return "audio/aac";
    if (lower.find(".flac") != std::string::npos) return "audio/flac";

    // Video types
    if (lower.find(".mp4") != std::string::npos) return "video/mp4";
    if (lower.find(".webm") != std::string::npos) return "video/webm";
    if (lower.find(".ogv") != std::string::npos) return "video/ogg";
    if (lower.find(".avi") != std::string::npos) return "video/x-msvideo";

    // Default
    return "application/octet-stream";
}

// 64 位 seek/tell：Windows 的 long 与 fseek 偏移是 32 位，>2GB 的 .mdd 会截断
#ifdef _WIN32
static int64_t tell64(std::FILE* f) {
    return _ftelli64(f);
}
static bool seek64(std::FILE* f, int64_t pos) {
    return _fseeki64(f, pos, SEEK_SET) == 0;
}
#else
static int64_t tell64(std::FILE* f) {
    return static_cast<int64_t>(std::ftell(f));
}
static bool seek64(std::FILE* f, int64_t pos) {
    return std::fseek(f, static_cast<long>(pos), SEEK_SET) == 0;
}
#endif

bool MddResourceParser::read_bytes(uint64_t offset, size_t size, std::vector<uint8_t>& out) const {
    // GCOVR_EXCL_LINE：file_ 为空时 resources_ 必然也是空的——load() 开头
    // 的 unload() 会清条目表，而块解析失败（parse_single_block 返回
    // !resources_.empty()）也只在没解析出任何条目时发生，于是 get_resource
    // 早在查表阶段就返回了，永远到不了这里。留作"将来新增了不经文件就能
    // 填表的新入口"时的兜底。
    if (!file_) {  // GCOVR_EXCL_LINE
        return false;  // GCOVR_EXCL_LINE
    }

    // 显式边界检查：不能依赖“fseek 越过 EOF 后 fread 必短读”的 stdio 语义——
    // 该涌现行为在 MSVC CRT 与 glibc 上不一致（Windows CI 曾因此让越界读
    // 意外返回数据）。先取真实文件长度做范围校验，越界一律返回假。
    // 同时用 64 位 seek/tell：Windows 的 long 是 32 位，>2GB 的 .mdd 会被截断。
    // 下面两个 fseek 失败分支在"普通可 seek 的常规文件"上无法构造——流
    // 只在 IO 出错时才会 seek 失败，而 parse_* 阶段的每次 fread 失败都已
    // 提前 return。保留它们是因为 .mdd 来自用户下载，文件随时可能在解析
    // 期间被替换/抽走。
    // GCOVR_EXCL_START
    if (std::fseek(file_, 0, SEEK_END) != 0) {
        return false;
    }
    // GCOVR_EXCL_STOP
    const int64_t file_size = tell64(file_);
    // GCOVR_EXCL_LINE：file_size < 0 臂与上面 364 行同一证明——tell64
    // 只在流已出错/无 seek 能力时返回 -1，而走到这里的前提是 fseek(END)
    // 刚刚成功，常规文件上不再可能；后两个条件（offset/size 越界）由
    // 分支补测用畸形条目真实驱动。
    if (file_size < 0 || offset > static_cast<uint64_t>(file_size) ||  // GCOVR_EXCL_LINE
        size > static_cast<uint64_t>(file_size) - offset) {
        return false;
    }

    // GCOVR_EXCL_START
    if (!seek64(file_, static_cast<int64_t>(offset))) {
        return false;
    }
    // GCOVR_EXCL_STOP
    out.resize(size);

    return std::fread(out.data(), 1, size, file_) == size;
}

// 原先这里有个 read_string(offset, size) 私有辅助，声明了、定义了，
// 但全仓库零调用方（get_resource_as_string 自己走 get_resource）——
// 与 read_bytes 重复的薄封装。已删除。

// ============================================================================
// MddResourceCache Implementation
// ============================================================================

MddResourceCache::MddResourceCache() {
    // Set default cache directory
    const char* home = std::getenv("HOME");
    if (home) {
        cache_dir_ = std::string(home) + "/.cache/unidict/mdd_resources";
    } else {
        cache_dir_ = "/tmp/unidict_mdd_cache";
    }
}

MddResourceCache::MddResourceCache(const std::string& cache_dir)
    : cache_dir_(cache_dir) {
}

void MddResourceCache::set_cache_directory(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
    // 同上：畸形路径不该抛异常打穿调用方，建不出来就留给后续写操作去失败
    std::error_code ec;
    fs::create_directories(cache_dir_, ec);
}

bool MddResourceCache::cache_resource(const std::vector<uint8_t>& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    if (data.empty()) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(cache_dir_, ec);
    if (ec) {
        return false;  // 畸形 cache_dir，见 extract_to_cache 的说明
    }

    std::string cache_path = get_cache_file_path(key);

    // Write to file
    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!out.good()) {
        return false;
    }

    // Update metadata
    CachedResource info;
    info.key = key;
    info.local_path = cache_path;
    info.mime_type = mime_type;
    info.size = data.size();
    info.last_used = std::time(nullptr);
    info.access_count = 1;

    cache_meta_[key] = info;

    return true;
}

bool MddResourceCache::cache_resource(const std::string& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    std::vector<uint8_t> vec(data.begin(), data.end());
    return cache_resource(vec, key, mime_type);
}

std::string MddResourceCache::get_cached_path(const std::string& key) const {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        return it->second.local_path;
    }
    return "";
}

bool MddResourceCache::is_cached(const std::string& key) const {
    return cache_meta_.find(key) != cache_meta_.end();
}

std::vector<uint8_t> MddResourceCache::get_from_cache(const std::string& key) const {
    std::string path = get_cached_path(key);
    if (path.empty()) {
        return {};
    }

    // 元数据说有，但那条路径可能已被换成目录。这类文件系统上 ifstream
    // 能"打开"目录而 tellg 报出 INT64_MAX——vector 按它分配直接
    // bad_alloc（实测：tmpfs 上打不开、ext4 上打得开，行为随文件系统
    // 类型漂移；test_cache_read_of_directory_target 曾因此炸出
    // std::bad_alloc）。所以打开之后、取 size 之前先验正身：目录
    // （tmpfs 打不开就靠 !in 短路）和权限问题都挡在同一道门里。
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    std::error_code ec;
    if (!in || !fs::is_regular_file(path, ec)) {
        return {};
    }

    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    // GCOVR_EXCL_LINE：常规文件在 tellg 报出的 size 上读不满只可能发生
    // 在"读期间文件被截短/IO 错误"，单线程测试无法稳定构造。
    if (!in.read(reinterpret_cast<char*>(data.data()), size)) {  // GCOVR_EXCL_LINE
        return {};  // GCOVR_EXCL_LINE
    }

    return data;
}

void MddResourceCache::clear_cache(const std::string& prefix) {
    if (prefix.empty()) {
        // Clear all
        for (const auto& entry : cache_meta_) {
            fs::remove(entry.second.local_path);
        }
        cache_meta_.clear();
    } else {
        // Clear matching entries
        auto it = cache_meta_.begin();
        while (it != cache_meta_.end()) {
            if (it->first.find(prefix) == 0) {
                fs::remove(it->second.local_path);
                it = cache_meta_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void MddResourceCache::prune_by_size(size_t max_bytes) {
    size_t total_size = get_cache_size();
    if (total_size <= max_bytes) {
        return;
    }

    // Sort by last used time (LRU)
    std::vector<CachedResource*> sorted;
    for (auto& entry : cache_meta_) {
        sorted.push_back(&entry.second);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const CachedResource* a, const CachedResource* b) {
            return a->last_used < b->last_used;
        });

    // Remove oldest entries until under limit
    for (auto* entry : sorted) {
        if (total_size <= max_bytes) {
            break;
        }

        fs::remove(entry->local_path);
        total_size -= entry->size;
        cache_meta_.erase(entry->key);
    }
}

void MddResourceCache::prune_by_age(uint64_t max_age_seconds) {
    uint64_t now = std::time(nullptr);

    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (now - it->second.last_used > max_age_seconds) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

void MddResourceCache::prune_by_access(uint64_t min_access_count) {
    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (it->second.access_count < min_access_count) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t MddResourceCache::get_cache_size() const {
    size_t total = 0;
    for (const auto& entry : cache_meta_) {
        total += entry.second.size;
    }
    return total;
}

int MddResourceCache::get_cached_count() const {
    return static_cast<int>(cache_meta_.size());
}

std::vector<CachedResource> MddResourceCache::get_cache_info() const {
    std::vector<CachedResource> result;
    result.reserve(cache_meta_.size());

    for (const auto& entry : cache_meta_) {
        result.push_back(entry.second);
    }

    return result;
}

std::string MddResourceCache::get_cache_file_path(const std::string& key) const {
    // 把文件系统上非法/易歧义的字符换成 '-'（斜杠同时充当了目录分隔的
    // 扁平化：缓存是单层目录，不还原层级）
    std::string filename = key;
    static const char kBad[] = "/\\:?*\"<>|";
    for (char& c : filename) {
        if (std::strchr(kBad, c) != nullptr && c != '\0') {
            c = '-';
        }
    }

    // 文件名长度必须有界。cache_key 是 "<词典id>_<资源键>"，词典 id 在 Qt 层
    // 是从**绝对路径**派生的，资源键又是 "sounds/oxford/word_00001.mp3" 这种
    // 带目录的键——两者一叠加，词典装在稍深一点的目录里就轻松超过 255 字节。
    //
    // 原先没有这道护栏：超长名被 std::ofstream 直接拒掉，cache_resource()
    // 返回 false，get_resource_path() 返回空串。也就是 .mdd 里明明有这张图，
    // QML 却什么都拿不到，而且没有任何报错——静默失败最难查。
    //
    // 超限时截断并追加 FNV-1a-64 的 16 位十六进制摘要：既把名字压回界内，
    // 又让不同的超长键仍映射到不同文件（纯截断会让所有超长键挤到同一个
    // 名字，后写的覆盖先写的，图/音频就串了）。扩展名另作保留——QML 的
    // Image/Audio 靠它嗅格式。
    // 文件名分量达标还不够——总长要一起收（CI Windows 曾挂在超长文件名
    // 块：目录 61 + 1 + 名 200 = 262 > 259，ofstream 拒开、返回 false）。
    // 按实测目录长度把文件名预算折进来，目录越深名字越短，总长钉在
    // MAX_CACHE_PATH_LEN 内。
    size_t name_budget = MAX_CACHE_NAME_LEN;
    const size_t dir_len = cache_dir_.size() + 1;
    if (dir_len < MAX_CACHE_PATH_LEN) {
        name_budget = std::min(name_budget, MAX_CACHE_PATH_LEN - dir_len);
    }

    if (filename.size() > name_budget) {
        const std::string ext = file_extension(filename);
        char digest[17];
        std::snprintf(digest, sizeof(digest), "%016llx",
                      static_cast<unsigned long long>(fnv1a64(key.data(), key.size())));
        // '_' + 16 进制摘要 + 扩展名是不可再压的骨架；目录深到连骨架都
        // 放不下时预算归 0，名字退化成 "_digest.ext"，让 ofstream 按真实
        // 边界自然失败返回 false（这种深度 <filesystem> 建目录那关在
        // Windows 上本来就已经挡掉了）
        const size_t overhead = 17 + ext.size();
        const size_t budget =
            name_budget > overhead ? name_budget - overhead : 0;
        filename = filename.substr(0, budget) + "_" + digest + ext;
    }

    return cache_dir_ + "/" + filename;
}

void MddResourceCache::update_access_time(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.last_used = std::time(nullptr);
    }
}

void MddResourceCache::increment_access_count(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.access_count++;
    }
}

// ============================================================================
// MddResourceManager Implementation
// ============================================================================

MddResourceManager::MddResourceManager() {
    cache_ = std::make_unique<MddResourceCache>();
}

bool MddResourceManager::load_mdd(const std::string& mdd_path,
                                  const std::string& dictionary_id) {
    auto parser = std::make_unique<MddResourceParser>();
    if (!parser->load(mdd_path)) {
        return false;
    }

    DictionaryResources dict_res;
    dict_res.parser = std::move(parser);
    dict_res.mdd_path = mdd_path;

    dictionaries_[dictionary_id] = std::move(dict_res);
    return true;
}

bool MddResourceManager::unload_mdd(const std::string& dictionary_id) {
    auto it = dictionaries_.find(dictionary_id);
    if (it == dictionaries_.end()) {
        return false;
    }

    dictionaries_.erase(it);
    return true;
}

bool MddResourceManager::has_mdd(const std::string& dictionary_id) const {
    return dictionaries_.find(dictionary_id) != dictionaries_.end();
}

std::string MddResourceManager::get_resource_path(const std::string& key,
                                                  const std::string& dictionary_id) {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return "";
    }

    auto& parser = dict_it->second.parser;
    auto info = parser->get_resource_info(key);
    if (info.key.empty()) return "";
    const std::string cache_key = dictionary_id + "_" + info.key;

    // Check cache first
    std::string cached = cache_->get_cached_path(cache_key);
    if (!cached.empty() && fs::exists(cached)) {
        cache_->update_access_time(cache_key);
        cache_->increment_access_count(cache_key);
        return cached;
    }

    // Get from parser and cache
    auto data = parser->get_resource(info.key);
    if (data.empty()) {
        return "";
    }

    std::string mime_type = MddResourceParser::detect_mime_type(info.key);
    if (!cache_->cache_resource(data, cache_key, mime_type)) {
        return "";
    }

    return cache_->get_cached_path(cache_key);
}

std::vector<uint8_t> MddResourceManager::get_resource_data(const std::string& key,
                                                           const std::string& dictionary_id) {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return {};
    }

    auto& parser = dict_it->second.parser;
    auto info = parser->get_resource_info(key);
    if (info.key.empty()) return {};

    // Check cache first.
    const std::string cache_key = dictionary_id + "_" + info.key;
    std::string cached = cache_->get_cached_path(cache_key);
    if (!cached.empty() && fs::exists(cached)) {
        cache_->update_access_time(cache_key);
        cache_->increment_access_count(cache_key);
        return cache_->get_from_cache(cache_key);
    }

    // Load from parser and populate cache.
    auto data = parser->get_resource(info.key);
    if (data.empty()) {
        return {};
    }

    std::string mime_type = MddResourceParser::detect_mime_type(info.key);
    cache_->cache_resource(data, cache_key, mime_type);
    return data;
}

bool MddResourceManager::has_resource(const std::string& key,
                                     const std::string& dictionary_id) const {
    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return false;
    }

    return dict_it->second.parser->has_resource(key);
}

std::vector<std::string> MddResourceManager::list_resources(
    const std::string& dictionary_id, const std::string& prefix) const {

    auto dict_it = dictionaries_.find(dictionary_id);
    if (dict_it == dictionaries_.end()) {
        return {};
    }

    return dict_it->second.parser->list_resources(prefix);
}

void MddResourceManager::set_cache_directory(const std::string& cache_dir) {
    cache_->set_cache_directory(cache_dir);
}

void MddResourceManager::clear_cache(const std::string& dictionary_id) {
    if (dictionary_id.empty()) {
        cache_->clear_cache();
    } else {
        cache_->clear_cache(dictionary_id + "_");
    }
}

void MddResourceManager::prune_cache(size_t max_bytes) {
    cache_->prune_by_size(max_bytes);
}

size_t MddResourceManager::get_total_cache_size() const {
    return cache_->get_cache_size();
}

int MddResourceManager::get_total_resource_count() const {
    int total = 0;
    for (const auto& entry : dictionaries_) {
        total += entry.second.parser->resource_count();
    }
    return total;
}

} // namespace UnidictCoreStd
