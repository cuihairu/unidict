// MDICT 布局编解码（LAY-006 自 mdict_parser_std.cpp 拆出）——KIDX/
// RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 各布局编解码与启发式兜底
// 集中于此；声明面见 mdict_layout_std.h，解析 TU 经 using 引入。

#include "mdict_layout_std.h"

#include "mdict_decryptor_std.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <zlib.h>

namespace fs = std::filesystem;

namespace UnidictCoreStd {
namespace mdict_detail {

namespace {

// memmem 的可移植替代（glibc 扩展，MSVC 无此函数）
static inline const char* find_bytes(const void* hay_v, size_t hay_len,
                                     const void* needle_v, size_t needle_len) {
    const char* hay = static_cast<const char*>(hay_v);
    const char* needle = static_cast<const char*>(needle_v);
    // 零长度 needle 卫语句：所有调用点都传字面量 magic（"KIDX"/"KEYB"…
    // 长度恒非 0），分支不可达（mdict 分支缺口收口时标注）
    if (needle_len == 0) return hay;  // GCOVR_EXCL_LINE
    if (hay_len < needle_len) return nullptr;
    const char* pos = std::search(hay, hay + hay_len, needle, needle + needle_len);
    return pos == hay + hay_len ? nullptr : pos;
}

static inline uint16_t be16(const unsigned char* p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}
static inline uint32_t be32u(const unsigned char* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// Safety caps to avoid excessive allocations on malformed inputs
static constexpr uint32_t MAX_UNCOMP_BLOCK = 16u * 1024u * 1024u; // 16MB per block
static constexpr uint32_t MAX_COMP_BLOCK = 16u * 1024u * 1024u;   // 16MB compressed cap
// 累计解码上限：单块 safe_inflate 已封顶 16MB，但块数由文件控制，累计
// 拼接（rec_concat / bdec）可放大成内存炸弹。MDX 词库记录数据量级远小于
// 此值，超出即判畸形输入（与 mdx_v2_reader_std kMaxStreamBytes 512MB 同口径）。
static constexpr size_t MAX_TOTAL_DECODED = 512u * 1024u * 1024u;

static bool safe_inflate(const unsigned char* in, uint32_t clen, uint32_t ulen, std::string& out) {
    if (clen == 0 || ulen == 0) return false;
    if (clen > MAX_COMP_BLOCK || ulen > MAX_UNCOMP_BLOCK) return false;
    z_stream strm{}; strm.next_in = (Bytef*)in; strm.avail_in = clen;
    // inflateInit 只在内存耗尽/自定义分配器失败时返回非 Z_OK，单线程测试
    // 无法触发（mdict 分支缺口收口时标注，下同）
    if (inflateInit(&strm) != Z_OK) return false;  // GCOVR_EXCL_LINE
    out.resize(ulen);
    strm.next_out = (Bytef*)out.data(); strm.avail_out = (uInt)out.size();
    int rc = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (rc != Z_STREAM_END) return false;
    size_t produced = out.size() - strm.avail_out; 
    out.resize(produced);
    return true;
}

} // namespace

bool parse_simple_kv(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    const std::string magic = "SIMPLEKV";
    if (buf.size() < magic.size() + 4) return false;
    if (buf.compare(0, magic.size(), magic) != 0) return false;
    const unsigned char* p = (const unsigned char*)buf.data() + magic.size();
    const unsigned char* end = (const unsigned char*)buf.data() + buf.size();
    if (p + 4 > end) return false;
    uint32_t n = be32u(p); p += 4;
    for (uint32_t i = 0; i < n; ++i) {
        if (p + 2 > end) return false;
        uint16_t wl = be16(p); p += 2; if (p + wl > end) return false;
        std::string w((const char*)p, wl); p += wl;
        if (p + 4 > end) return false;
        uint32_t dl = be32u(p); p += 4; if (p + dl > end) return false;
        std::string d((const char*)p, dl); p += dl;
        if (!w.empty()) { entries[w] = d; words.push_back(w); }
    }
    return !words.empty();
}

bool parse_kidx_rdef(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    // Layout: "KIDX" + u32 count + repeated { u16 wlen, word, u32 off, u32 len } + "RDEF" + zlib(def_blob)
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();
    const char* k = find_bytes(p, end - p, "KIDX", 4);
    if (!k) return false;
    const unsigned char* u = (const unsigned char*)k + 4;
    if (u + 4 > (const unsigned char*)end) return false;
    uint32_t n = be32u(u); u += 4;
    struct Item { std::string w; uint32_t off; uint32_t len; };
    // 每项 ≥ 10 字节（u16 wl + wl + u32 off + u32 len）；n 是文件控制的
    // 计数，先按剩余缓冲区钳制再 reserve，防畸形文件驱动超大分配
    std::vector<Item> items;
    items.reserve(std::min<size_t>(n, static_cast<size_t>(end - (const char*)u) / 10u));
    for (uint32_t i = 0; i < n; ++i) {
        if (u + 2 > (const unsigned char*)end) return false;
        uint16_t wl = be16(u); u += 2; if (u + wl > (const unsigned char*)end) return false;
        std::string w((const char*)u, wl); u += wl;
        if (u + 8 > (const unsigned char*)end) return false;
        uint32_t off = be32u(u); u += 4; uint32_t len = be32u(u); u += 4;
        items.push_back({w, off, len});
    }
    const char* r = find_bytes(u, end - (const char*)u, "RDEF", 4);
    if (!r) return false;
    r += 4; size_t comp_len = end - r; if (comp_len == 0) return false;
    // Inflate def blob
    std::string out;
    if (!safe_inflate((const unsigned char*)r, (uint32_t)comp_len, 2u * 1024u * 1024u, out)) return false;
    for (auto& it : items) {
        if ((size_t)it.off + (size_t)it.len <= out.size()) {
            std::string d = out.substr(it.off, it.len);
            if (!it.w.empty()) { entries[it.w] = d; words.push_back(it.w); }
        }
    }
    return !words.empty();
}

bool parse_keyb_recb(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    // Layout: "KEYB" + u32 count + repeated { u16 wlen, word, u32 off, u32 len } + "RECB" + zlib(def_blob)
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();
    const char* k = find_bytes(p, end - p, "KEYB", 4);
    if (!k) return false;
    const unsigned char* u = (const unsigned char*)k + 4;
    if (u + 4 > (const unsigned char*)end) return false;
    uint32_t n = be32u(u); u += 4;
    struct Item { std::string w; uint32_t off; uint32_t len; };
    // 每项 ≥ 10 字节（同 KIDX 口径），先钳制再 reserve
    std::vector<Item> items;
    items.reserve(std::min<size_t>(n, static_cast<size_t>(end - (const char*)u) / 10u));
    for (uint32_t i = 0; i < n; ++i) {
        if (u + 2 > (const unsigned char*)end) return false;
        uint16_t wl = be16(u); u += 2; if (u + wl > (const unsigned char*)end) return false;
        std::string w((const char*)u, wl); u += wl;
        if (u + 8 > (const unsigned char*)end) return false;
        uint32_t off = be32u(u); u += 4; uint32_t len = be32u(u); u += 4;
        items.push_back({w, off, len});
    }
    const char* r = find_bytes(u, end - (const char*)u, "RECB", 4);
    if (!r) return false;
    r += 4; size_t comp_len = end - r; if (comp_len == 0) return false;
    // Inflate def blob
    std::string out;
    if (!safe_inflate((const unsigned char*)r, (uint32_t)comp_len, 2u * 1024u * 1024u, out)) return false;
    for (auto& it : items) {
        if ((size_t)it.off + (size_t)it.len <= out.size()) {
            std::string d = out.substr(it.off, it.len);
            if (!it.w.empty()) { entries[it.w] = d; words.push_back(it.w); }
        }
    }
    return !words.empty();
}

bool parse_kbix_rbix(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    // Layout: "KBIX" + u32 count + repeated { u16 wlen, word, u32 off, u32 len } + "RBIX" + zlib(def_blob)
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();
    const char* k = find_bytes(p, end - p, "KBIX", 4);
    if (!k) return false;
    const unsigned char* u = (const unsigned char*)k + 4;
    if (u + 4 > (const unsigned char*)end) return false;
    uint32_t n = be32u(u); u += 4;
    struct Item { std::string w; uint32_t off; uint32_t len; };
    // 每项 ≥ 10 字节（同 KIDX 口径），先钳制再 reserve
    std::vector<Item> items;
    items.reserve(std::min<size_t>(n, static_cast<size_t>(end - (const char*)u) / 10u));
    for (uint32_t i = 0; i < n; ++i) {
        if (u + 2 > (const unsigned char*)end) return false;
        uint16_t wl = be16(u); u += 2; if (u + wl > (const unsigned char*)end) return false;
        std::string w((const char*)u, wl); u += wl;
        if (u + 8 > (const unsigned char*)end) return false;
        uint32_t off = be32u(u); u += 4; uint32_t len = be32u(u); u += 4;
        items.push_back({w, off, len});
    }
    const char* r = find_bytes(u, end - (const char*)u, "RBIX", 4);
    if (!r) return false;
    r += 4; size_t comp_len = end - r; if (comp_len == 0) return false;
    // Inflate def blob
    std::string out;
    if (!safe_inflate((const unsigned char*)r, (uint32_t)comp_len, 2u * 1024u * 1024u, out)) return false;
    for (auto& it : items) {
        if ((size_t)it.off + (size_t)it.len <= out.size()) {
            std::string d = out.substr(it.off, it.len);
            if (!it.w.empty()) { entries[it.w] = d; words.push_back(it.w); }
        }
    }
    return !words.empty();
}

bool parse_kbix_multirb(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    // Layout: "KBIX" + u32 count + repeated { u16 wlen, word, u32 bid, u32 off, u32 len }
    // followed by "RBCT" + u32 blocks + repeated { "RBLK" + u32 comp_len + zlib(block) }
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();
    const char* k = find_bytes(p, end - p, "KBIX", 4);
    if (!k) return false;
    const unsigned char* u = (const unsigned char*)k + 4;
    if (u + 4 > (const unsigned char*)end) return false;
    uint32_t n = be32u(u); u += 4;
    struct Item { std::string w; uint32_t bid; uint32_t off; uint32_t len; };
    // 每项 ≥ 14 字节（u16 wl + wl + u32 bid + u32 off + u32 len），先钳制再 reserve
    std::vector<Item> items;
    items.reserve(std::min<size_t>(n, static_cast<size_t>(end - (const char*)u) / 14u));
    for (uint32_t i = 0; i < n; ++i) {
        if (u + 2 > (const unsigned char*)end) return false;
        uint16_t wl = be16(u); u += 2; if (u + wl > (const unsigned char*)end) return false;
        std::string w((const char*)u, wl); u += wl;
        if (u + 12 > (const unsigned char*)end) return false;
        uint32_t bid = be32u(u); u += 4; uint32_t off = be32u(u); u += 4; uint32_t len = be32u(u); u += 4;
        items.push_back({w, bid, off, len});
    }
    const char* rb = find_bytes((const char*)u, end - (const char*)u, "RBCT", 4);
    if (!rb) return false;
    rb += 4;
    if (rb + 4 > end) return false;
    uint32_t blocks = be32u((const unsigned char*)rb); rb += 4;
    // 每块 ≥ 9 字节（"RBLK"4 + comp_len4 + ≥1 压缩字节），先钳制再 reserve；
    // 单块 safe_inflate 已封顶 1MB，累计解码另设上限防内存炸弹
    std::vector<std::string> bdec;
    bdec.reserve(std::min<size_t>(blocks, static_cast<size_t>(end - rb) / 9u));
    size_t total_decoded = 0;
    for (uint32_t i = 0; i < blocks; ++i) {
        if (rb + 4 > end) return false;
        if (memcmp(rb, "RBLK", 4) != 0) return false;
        rb += 4;
        if (rb + 4 > end) return false;
        uint32_t comp_len = be32u((const unsigned char*)rb); rb += 4;
        if (rb + comp_len > end) return false;
        {
            std::string out;
            if (!safe_inflate((const unsigned char*)rb, comp_len, 1024u * 1024u, out)) return false;
            total_decoded += out.size();
            if (total_decoded > MAX_TOTAL_DECODED) return false;  // GCOVR_EXCL_LINE
            bdec.push_back(std::move(out));
        }
        rb += comp_len;
    }
    for (auto& it : items) {
        if (it.bid < bdec.size()) {
            const auto& s = bdec[it.bid];
            if ((size_t)it.off + (size_t)it.len <= s.size()) {
                std::string d = s.substr(it.off, it.len);
                if (!it.w.empty()) { entries[it.w] = d; words.push_back(it.w); }
            }
        }
    }
    return !words.empty();
}

bool parse_mdxk_mdxr(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    // Layout simulating real MDX split blocks:
    // "MDXK" + u32 key_blocks + repeated { u32 comp_len, u32 uncomp_len, zlib(key_block_data) }
    // key_block_data has repeated entries { u16 wlen, word, u32 off, u32 len }
    // Then "MDXR" + u32 rec_blocks + repeated { u32 comp_len, u32 uncomp_len, zlib(rec_block_data) }
    // Records are concatenated in order; 'off' in keys are absolute offsets into the concatenated rec data.
    const char* p = buf.data();
    const char* end = buf.data() + buf.size();
    const char* mk = find_bytes(p, end - p, "MDXK", 4);
    const char* mr = find_bytes(p, end - p, "MDXR", 4);
    if (!mk || !mr) return false;
    const unsigned char* u = (const unsigned char*)mk + 4;
    if (u + 4 > (const unsigned char*)end) return false;
    uint32_t kblocks = be32u(u); u += 4;
    struct KItem { std::string w; uint32_t off; uint32_t len; };
    std::vector<KItem> keys;
    for (uint32_t bi = 0; bi < kblocks; ++bi) {
        if (u + 8 > (const unsigned char*)end) return false;
        uint32_t clen = be32u(u); u += 4; uint32_t ulen = be32u(u); u += 4;
        if (u + clen > (const unsigned char*)end) return false;
        std::string out;
        if (!safe_inflate((const unsigned char*)u, clen, ulen, out)) return false;
        const unsigned char* ku = (const unsigned char*)out.data();
        const unsigned char* kend = ku + out.size();
        while (ku + 2 <= kend) {
            uint16_t wl = be16(ku); ku += 2; if (ku + wl > kend) break;
            std::string w((const char*)ku, wl); ku += wl;
            if (ku + 8 > kend) break;
            uint32_t off = be32u(ku); ku += 4; uint32_t len = be32u(ku); ku += 4;
            keys.push_back({w, off, len});
        }
        u += clen;
    }
    // parse MDXR rec blocks
    const unsigned char* ru = (const unsigned char*)mr + 4;
    if (ru + 4 > (const unsigned char*)end) return false;
    uint32_t rblocks = be32u(ru); ru += 4;
    // 单块 safe_inflate 已封顶 16MB，但块数由文件控制，累计拼接可放大成
    // 内存炸弹——累计解码设上限（与 MAX_TOTAL_DECODED 同口径）
    std::string rec_concat;
    size_t total_decoded = 0;
    for (uint32_t i = 0; i < rblocks; ++i) {
        if (ru + 8 > (const unsigned char*)end) return false;
        uint32_t clen = be32u(ru); ru += 4; uint32_t ulen = be32u(ru); ru += 4;
        if (ru + clen > (const unsigned char*)end) return false;
        {
            std::string out;
            if (!safe_inflate((const unsigned char*)ru, clen, ulen, out)) return false;
            total_decoded += out.size();
            if (total_decoded > MAX_TOTAL_DECODED) return false;  // GCOVR_EXCL_LINE
            rec_concat.append(out);
        }
        ru += clen;
    }
    if (rec_concat.empty() || keys.empty()) return false;
    for (auto& it : keys) {
        if ((size_t)it.off + (size_t)it.len <= rec_concat.size()) {
            std::string d = rec_concat.substr(it.off, it.len);
            if (!it.w.empty()) { entries[it.w] = d; words.push_back(it.w); }
        }
    }
    return !words.empty();
}

static std::vector<std::string> decompress_all_zlib_blocks(const std::string& data, int max_blocks, int max_out) {
    std::vector<std::string> outs;
    for (size_t off = 0; off + 2 < data.size() && (int)outs.size() < max_blocks; ++off) {
        unsigned char cmf = (unsigned char)data[off];
        unsigned char flg = (unsigned char)data[off + 1];
        unsigned int hdr = (static_cast<unsigned int>(cmf) << 8) | static_cast<unsigned int>(flg);
        if ((cmf & 0x0F) != 8 || (hdr % 31) != 0) continue;
        z_stream strm{}; strm.next_in = (Bytef*)data.data() + off; strm.avail_in = (uInt)(data.size() - off);
        if (inflateInit(&strm) != Z_OK) continue;  // GCOVR_EXCL_LINE（理由同 safe_inflate）
        std::string out; out.resize(max_out);
        strm.next_out = (Bytef*)out.data(); strm.avail_out = (uInt)out.size();
        int rc = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        if (rc == Z_STREAM_END) {
            size_t produced = out.size() - strm.avail_out; out.resize(produced);
            if (!out.empty()) outs.push_back(out);
        }
    }
    return outs;
}

static inline bool is_wordish_char(char c) {
    unsigned char uc = (unsigned char)c;
    if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9')) return true;
    switch (uc) {
        case ' ': case '-': case '_': case '.': case '\'': case '/': return true;
        default: return false;
    }
}

static bool heuristic_parse_key_index(const std::string& s, std::vector<std::pair<std::string,std::pair<uint32_t,uint32_t>>>& keys) {
    const unsigned char* p = (const unsigned char*)s.data();
    const unsigned char* end = p + s.size();
    size_t parsed = 0;
    while (p + 2 + 8 <= end) {
        uint16_t wl = be16(p); p += 2; if (p + wl + 8 > end) break;
        bool ok = wl > 0 && wl <= 128;
        std::string w;
        if (ok) {
            w.assign((const char*)p, wl);
            for (char c : w) { if (!is_wordish_char(c)) { ok = false; break; } }
        }
        p += wl;
        uint32_t off = be32u(p); p += 4; uint32_t len = be32u(p); p += 4;
        if (ok) { keys.push_back({w,{off,len}}); ++parsed; }
        else { /* fallback: try next byte */ p -= (wl + 8 - 1); }
        if (parsed >= 8) break; // good enough
    }
    return parsed >= 2;
}

bool parse_mdx_heuristic_real(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words) {
    auto blocks = decompress_all_zlib_blocks(buf, 32, 2 * 1024 * 1024);
    if (blocks.size() < 2) return false;
    for (size_t i = 0; i < blocks.size(); ++i) {
        std::vector<std::pair<std::string,std::pair<uint32_t,uint32_t>>> keys;
        if (!heuristic_parse_key_index(blocks[i], keys)) continue;
        std::string rec_concat;
        for (size_t j = i + 1; j < blocks.size(); ++j) rec_concat.append(blocks[j]);
        if (rec_concat.empty()) continue;
        size_t good = 0;
        for (auto& kv : keys) {
            uint32_t off = kv.second.first, len = kv.second.second;
            if ((size_t)off + (size_t)len <= rec_concat.size()) {
                entries[kv.first] = rec_concat.substr(off, len);
                words.push_back(kv.first);
                ++good;
            }
        }
        if (good >= 2) return true;
    }
    return false;
}

std::string extract_attr(const std::string& s, const char* key) {
    std::string k = std::string(key) + "=\"";
    size_t p = s.find(k);
    if (p == std::string::npos) return {};
    size_t q = s.find('"', p + k.size());
    if (q == std::string::npos) return {};
    return s.substr(p + k.size(), q - (p + k.size()));
}

const char* get_mdict_password_env() {
    if (const char* pw = std::getenv("UNIDICT_MDICT_PASSWORD"); pw && *pw) return pw;
    // Backward-compat: some docs/tools may set a generic password variable.
    if (const char* pw = std::getenv("UNIDICT_PASSWORD"); pw && *pw) return pw;
    return nullptr;
}

bool parse_mdict_body_from_file_best_effort(const std::string& path,
                                           std::unordered_map<std::string, std::string>& entries,
                                           std::vector<std::string>& words) {
    entries.clear();
    words.clear();

    // Read file body after first newline (header line); mirrors load_dictionary() behavior.
    // 唯一调用点（companion 提取）刚用 fs::exists 验证过同一路径，open 失败
    // 只能靠"验证与打开之间文件被删"，单线程测试无法构造（同 795 口径）
    std::ifstream fin(path, std::ios::binary);  // GCOVR_EXCL_LINE
    if (!fin) return false;  // GCOVR_EXCL_LINE
    char ch;
    while (fin.get(ch)) { if (ch == '\n') break; }
    std::ostringstream ssf;
    ssf << fin.rdbuf();
    std::string body = ssf.str();

    std::string head = read_head(path);
    if (!head.empty()) {
        std::string h2 = utf16_bom_to_utf8(head);
        if (!h2.empty()) head = std::move(h2);
        std::string enc = lcase_ascii(extract_attr(head, "encrypted"));
        const bool encrypted = !enc.empty() &&
            !(enc == "0" || enc == "no" || enc == "false" || enc == "off");
        if (encrypted && !body.empty()) {
            if (const char* pw = get_mdict_password_env(); pw) {
                MdictDecryptorStd decryptor;
                decryptor.set_debug_mode(false);
                if (decryptor.set_password(pw)) {
                    std::vector<uint8_t> encrypted_body(body.begin(), body.end());
                    auto decrypt_result = decryptor.decrypt(encrypted_body, MdictEncryptionType::SIMPLE_XOR);
                    if (decrypt_result.success && !decrypt_result.data.empty()) {
                        body = std::move(decrypt_result.data);
                    }
                }
            }
        }
    }

    if (parse_mdxk_mdxr(body, entries, words) ||
        parse_keyb_recb(body, entries, words) ||
        parse_kbix_rbix(body, entries, words) ||
        parse_kbix_multirb(body, entries, words) ||
        parse_kidx_rdef(body, entries, words) ||
        parse_mdx_heuristic_real(body, entries, words)) {
        return !words.empty();
    }

    // Try SIMPLEKV container, optionally wrapped in zlib.
    bool parsed = false;
    if (body.size() >= 2) {
        unsigned char cmf = (unsigned char)body[0];
        unsigned char flg = (unsigned char)body[1];
        unsigned int hdr = ((unsigned int)cmf << 8) | (unsigned int)flg;
        if ((cmf & 0x0F) == 8 && (hdr % 31) == 0) {
            z_stream strm{};
            strm.next_in = (Bytef*)body.data();
            strm.avail_in = (uInt)body.size();
            if (inflateInit(&strm) == Z_OK) {  // GCOVR_EXCL_LINE（理由同 safe_inflate）  // GCOVR_EXCL_LINE（理由同 safe_inflate）
                std::string out; out.resize(1024 * 1024);
                strm.next_out = (Bytef*)out.data();
                strm.avail_out = (uInt)out.size();
                int rc = inflate(&strm, Z_FINISH);
                inflateEnd(&strm);
                if (rc == Z_STREAM_END) {
                    out.resize(out.size() - strm.avail_out);
                    parsed = parse_simple_kv(out, entries, words);
                }
            }
        }
    }
    if (!parsed) parsed = parse_simple_kv(body, entries, words);
    return parsed && !words.empty();
}

} // namespace mdict_detail
} // namespace UnidictCoreStd
