// mdict_parser_std 分支缺口第二批（round-1 已 669→569，本批继续压
// load_dictionary/五链/启发式/解密/渲染的剩余 Missing）：
//   * parse_simple_kv 与 KIDX/KEYB/KBIX 三对魔数的截断阶梯（计数器、
//     wl、词、off/len、def、尾部魔数、坏 zlib、越界、空词条目）
//   * KBIX/RBCT 多块：块数截断/块头截断/魔数错/comp_len 越界/坏块/
//     双真实块跨块偏移/目录占名导致文件写失败
//   * MDXK/MDXR：kblocks/rblocks 各阶梯、单块半条目、ulen=0、ulen>16MB
//     （上限侧提前 return，不触发大分配）、双 key 块 + 双 rec 块跨块绝对偏移
//   * 启发式：>128 词长回退、特殊 wordish 字符全 case、good<2 换块、
//     坏 zlib 头两 continue、33 块封顶、空 zlib 块跳过（两处消费点）
//   * UTF-16 LE/BE BOM 头（含 null 码位与 ≥0x80 丢弃）、头属性缺
//     description 的三元假侧、未闭合引号回落 stem、空文件头回落
//   * 加密成功路径：mt19937(hash(pw)) 密钥流在测试内逐字节复刻，XOR
//     出合法 KIDX 容器明文 → 解密链命中；flagged-but-plaintext → 明文
//     直读链；空 body + 有密码 → success=false 侧；UNIDICT_PASSWORD
//     兼容环境变量第二臂
//   * 渲染：src="" 空直通、http:// 直通、dict_dir_ 为空回落
//   * 提取：只读缓存目录 → manifest 打开失败、目录占名 → 文件写失败、
//     manifest 坏行变体重读、全坏 → 重提取
//   * 查询：fresh 对象 name 默认、find_similar break 侧/假侧/耗尽
// std-only assert 风格，一个文件一个 ctest target（同分支第一批惯例）。

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <zlib.h>

#include "std/mdict_parser_std.h"

namespace fs = std::filesystem;

static void set_env(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    ::setenv(key, value, 1);
#endif
}
static void unset_env(const char* key) {
#if defined(_WIN32)
    _putenv_s(key, "");
#else
    ::unsetenv(key);
#endif
}

static void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
}
static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
}

static std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& src) {
    std::vector<unsigned char> packed(src.size() + 1024);
    uLongf packed_len = packed.size();
    assert(compress2(packed.data(), &packed_len,
                     src.empty() ? nullptr : src.data(), src.size(), 9) == Z_OK);
    packed.resize(packed_len);
    return packed;
}
static std::vector<unsigned char> zlib_compress_str(const std::string& s) {
    return zlib_compress({s.begin(), s.end()});
}

// XML 头一行 + 容器体；attrs 追加任意头属性（如 encrypted="1"）
static void write_mdx_file(const fs::path& path, const std::string& title,
                           const std::vector<unsigned char>& body,
                           const std::string& attrs = "") {
    std::string header = "<Dictionary title=\"" + title + "\" description=\"branch desc\"" +
                         attrs + "/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}
// 自定义头（不写默认 description 等，供头属性缺省/未闭合/UTF-16 场景）
static void write_mdx_head(const fs::path& path, const std::string& header,
                           const std::vector<unsigned char>& body) {
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

static std::vector<unsigned char> make_simplekv(
    const std::vector<std::pair<std::string, std::string>>& kv) {
    std::vector<unsigned char> v;
    const std::string magic = "SIMPLEKV";
    v.insert(v.end(), magic.begin(), magic.end());
    be32w(v, (uint32_t)kv.size());
    for (const auto& p : kv) {
        be16w(v, (uint16_t)p.first.size());
        v.insert(v.end(), p.first.begin(), p.first.end());
        be32w(v, (uint32_t)p.second.size());
        v.insert(v.end(), p.second.begin(), p.second.end());
    }
    return v;
}
// SIMPLEKV 魔数后接任意残缺字节（截断阶梯用）
static std::vector<unsigned char> simplekv_raw(const std::vector<unsigned char>& after_magic) {
    std::vector<unsigned char> v{'S', 'I', 'M', 'P', 'L', 'E', 'K', 'V'};
    v.insert(v.end(), after_magic.begin(), after_magic.end());
    return v;
}

// { u16 wlen, word, u32 off, u32 len } 词条（KIDX/KEYB/KBIX 布局）
static void append_item(std::vector<unsigned char>& v, const std::string& w,
                        uint32_t off, uint32_t len) {
    be16w(v, (uint16_t)w.size());
    v.insert(v.end(), w.begin(), w.end());
    be32w(v, off);
    be32w(v, len);
}
// { u16 wlen, word, u32 bid, u32 off, u32 len } 词条（KBIX/RBCT 布局）
static void append_multi_item(std::vector<unsigned char>& v, const std::string& w,
                              uint32_t bid, uint32_t off, uint32_t len) {
    be16w(v, (uint16_t)w.size());
    v.insert(v.end(), w.begin(), w.end());
    be32w(v, bid);
    be32w(v, off);
    be32w(v, len);
}

// KIDX/KEYB/KBIX 原始构造：head 魔数 + 任意词条区字节 + 可选尾魔数 + 尾字节
static std::vector<unsigned char> make_kb_raw(const char* head_magic,
                                              const char* tail_magic,
                                              const std::vector<unsigned char>& mid,
                                              const std::vector<unsigned char>& tail) {
    std::vector<unsigned char> v;
    v.insert(v.end(), head_magic, head_magic + 4);
    v.insert(v.end(), mid.begin(), mid.end());
    if (tail_magic) v.insert(v.end(), tail_magic, tail_magic + 4);
    v.insert(v.end(), tail.begin(), tail.end());
    return v;
}

// KBIX + 可选 RBCT：items 为不带计数的词条区，blocks 为未压缩块内容
static std::vector<unsigned char> make_multirb_raw(
    uint32_t count, const std::vector<unsigned char>& items,
    const std::vector<std::string>& blocks) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'B', 'I', 'X'});
    be32w(v, count);
    v.insert(v.end(), items.begin(), items.end());
    if (!blocks.empty()) {
        v.insert(v.end(), {'R', 'B', 'C', 'T'});
        be32w(v, (uint32_t)blocks.size());
        for (const auto& b : blocks) {
            v.insert(v.end(), {'R', 'B', 'L', 'K'});
            auto packed = zlib_compress_str(b);
            be32w(v, (uint32_t)packed.size());
            v.insert(v.end(), packed.begin(), packed.end());
        }
    }
    return v;
}

// MDXK + 可选 MDXR 原始构造：key/rec 未压缩块序列；rec 空表则不写 MDXR
static std::vector<unsigned char> make_mdxk_raw(
    const std::vector<std::string>& key_blocks,
    const std::vector<std::string>& rec_blocks,
    const std::vector<unsigned char>& tail = {}) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'M', 'D', 'X', 'K'});
    be32w(v, (uint32_t)key_blocks.size());
    for (const auto& kb : key_blocks) {
        auto packed = zlib_compress_str(kb);
        be32w(v, (uint32_t)packed.size());
        be32w(v, (uint32_t)kb.size());
        v.insert(v.end(), packed.begin(), packed.end());
    }
    if (!rec_blocks.empty()) {
        v.insert(v.end(), {'M', 'D', 'X', 'R'});
        be32w(v, (uint32_t)rec_blocks.size());
        for (const auto& rb : rec_blocks) {
            auto packed = zlib_compress_str(rb);
            be32w(v, (uint32_t)packed.size());
            be32w(v, (uint32_t)rb.size());
            v.insert(v.end(), packed.begin(), packed.end());
        }
    }
    v.insert(v.end(), tail.begin(), tail.end());
    return v;
}

// 合法 KIDX 容器（加密路径的明文原型）
static std::vector<unsigned char> make_kidx_like() {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'I', 'D', 'X'});
    be32w(v, 2);
    append_item(v, "sec", 0, 4);
    append_item(v, "flg", 4, 4);
    v.insert(v.end(), {'R', 'D', 'E', 'F'});
    auto packed = zlib_compress_str("SECRFLAG");
    v.insert(v.end(), packed.begin(), packed.end());
    return v;
}

// UTF-16 头行：BOM + 全 ASCII 文本逐字符成对；文本不含 '\n'（UTF-16 下
// '\n' 的低字节就是 0x0A，会让 load_dictionary 的 body 在此截断），调用方
// 再补一个裸 '\n' 分隔符 + 原始 UTF-8 body
static std::string utf16_bytes(const std::string& ascii, bool le) {
    std::string out;
    out.push_back(le ? (char)0xFF : (char)0xFE);
    out.push_back(le ? (char)0xFE : (char)0xFF);
    for (unsigned char c : ascii) {
        if (le) { out.push_back((char)c); out.push_back('\0'); }
        else { out.push_back('\0'); out.push_back((char)c); }
    }
    return out;
}

// 与 mdict_decryptor_std.cpp 的 generate_key_stream 完全一致：
// mt19937(std::hash<std::string>{}(password))，取前 min(256, len) 字节
static std::vector<unsigned char> mdict_key_stream(const std::string& pw, size_t len) {
    std::vector<unsigned char> key;
    key.reserve(len);
    std::mt19937 rng(std::hash<std::string>{}(pw));
    for (size_t i = 0; i < len; ++i) key.push_back(static_cast<unsigned char>(rng() % 256));
    return key;
}
static std::vector<unsigned char> xor_encrypt(const std::string& plain, const std::string& pw) {
    auto key = mdict_key_stream(pw, std::min<size_t>(256, plain.size()));
    std::vector<unsigned char> out(plain.size());
    for (size_t i = 0; i < plain.size(); ++i)
        out[i] = (unsigned char)plain[i] ^ key[i % key.size()];
    return out;
}
// 挑一个密文既不含链式魔数（防 chain-1 把密文当明文解析成功）也不含
// 0x0A（防 body 读入在中途截断）的密码
static std::string pick_pw(const std::string& plain) {
    const char* magics[] = {"MDXK", "MDXR", "KEYB", "RECB", "KBIX",
                            "RBIX", "RBCT", "RBLK", "KIDX", "RDEF"};
    for (int i = 0; i < 1000; ++i) {
        std::string pw = "pw" + std::to_string(i);
        auto enc = xor_encrypt(plain, pw);
        bool bad = false;
        for (unsigned char b : enc) if (b == 0x0A) { bad = true; break; }
        if (bad) continue;
        for (const char* m : magics) {
            if (std::search(enc.begin(), enc.end(), m, m + 4) != enc.end()) { bad = true; break; }
        }
        if (!bad) return pw;
    }
    assert(false && "cannot find clean password");
    return "pw-fallback";
}

// 在缓存根目录下找 manifest.tsv 含指定 needle 的那个 mdd 缓存根
static fs::path find_root_containing(const fs::path& cache_base, const std::string& needle) {
    fs::path root;
    if (!fs::exists(cache_base)) return root;
    for (const auto& e : fs::directory_iterator(cache_base)) {
        std::ifstream in(e.path() / "manifest.tsv", std::ios::binary);
        if (!in) continue;
        std::stringstream ss;
        ss << in.rdbuf();
        if (ss.str().find(needle) != std::string::npos) { root = e.path(); break; }
    }
    return root;
}

int main() {
    fs::path base = fs::current_path() / "build-local" / "mdict_branches2";
    fs::create_directories(base);
    unset_env("UNIDICT_MDICT_PASSWORD");
    unset_env("UNIDICT_PASSWORD");

    // ===== C1) parse_simple_kv 截断阶梯与空词条目 =====
    {
        // C1a: magic+3B（count 截断）→ 尺寸卫语句假返
        {
            fs::path mdx = base / "c1a.mdx";
            write_mdx_file(mdx, "C1a", {'S', 'I', 'M', 'P', 'L', 'E', 'K', 'V', 0, 0, 1});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2 && !mp.lookup("mdict").empty());
        }
        // C1b: count=1 + 1B（wl 截断）
        {
            fs::path mdx = base / "c1b.mdx";
            write_mdx_file(mdx, "C1b", simplekv_raw({0, 0, 0, 1, 0, 4}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // C1c: wl=4 + "ab"（词截断）
        {
            fs::path mdx = base / "c1c.mdx";
            write_mdx_file(mdx, "C1c", simplekv_raw({0, 0, 0, 1, 0, 4, 'a', 'b'}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // C1d: 词后 off/len 只给 7B
        {
            fs::path mdx = base / "c1d.mdx";
            write_mdx_file(mdx, "C1d",
                           simplekv_raw({0, 0, 0, 1, 0, 3, 'a', 'b', 'c', 0, 0, 0}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // C1e: dl=10 但 def 只给 1B
        {
            fs::path mdx = base / "c1e.mdx";
            write_mdx_file(mdx, "C1e",
                           simplekv_raw({0, 0, 0, 1, 0, 3, 'a', 'b', 'c',
                                         0, 0, 0, 10, 'x'}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // C1f: 空词条目跳过 + 正常条目收录（收录条件两侧）
        {
            fs::path mdx = base / "c1f.mdx";
            write_mdx_file(mdx, "C1f", make_simplekv({{"", "D1"}, {"ok", "D2"}}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 1 && mp.lookup("ok") == "D2");
        }
        // C1g: 魔数不匹配 → 首比较假返
        {
            fs::path mdx = base / "c1g.mdx";
            auto body = make_simplekv({{"alpha", "a"}, {"alphabet", "ab"}});
            body[0] = 'H';
            write_mdx_file(mdx, "C1g", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2 && mp.lookup("alpha").empty());
        }
        // C1h: count=0 且无条目 → 尾判 !words.empty() 假返
        {
            fs::path mdx = base / "c1h.mdx";
            write_mdx_file(mdx, "C1h", simplekv_raw({0, 0, 0, 0}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // C1i: 只有魔数（count 截断在 p+4>end 卫语句）
        {
            fs::path mdx = base / "c1i.mdx";
            write_mdx_file(mdx, "C1i", {'S', 'I', 'M', 'P', 'L', 'E', 'K', 'V'});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
    }

    // ===== C2) KIDX/KEYB/KBIX 三对魔数的截断阶梯（布局共用）=====
    {
        struct Pair { const char* head; const char* tail; const char* tag; };
        const Pair pairs[] = {{"KIDX", "RDEF", "kidx"}, {"KEYB", "RECB", "keyb"},
                              {"KBIX", "RBIX", "kbix"}};
        for (const auto& pr : pairs) {
            const std::string fn = std::string("c2_") + pr.tag + "_";
            // t1: head+3B → count 截断
            {
                fs::path mdx = base / (fn + "t1.mdx");
                write_mdx_file(mdx, "C2t1", make_kb_raw(pr.head, nullptr, {0, 0, 1}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t2: count=1 + 1B → wl 截断
            {
                fs::path mdx = base / (fn + "t2.mdx");
                write_mdx_file(mdx, "C2t2", make_kb_raw(pr.head, nullptr, {0, 0, 0, 1, 0, 4}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t3: wl=4 + "ab" → 词截断
            {
                fs::path mdx = base / (fn + "t3.mdx");
                write_mdx_file(mdx, "C2t3",
                               make_kb_raw(pr.head, nullptr, {0, 0, 0, 1, 0, 4, 'a', 'b'}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t4: off/len 域只给 7B
            {
                fs::path mdx = base / (fn + "t4.mdx");
                write_mdx_file(mdx, "C2t4",
                               make_kb_raw(pr.head, nullptr,
                                           {0, 0, 0, 1, 0, 3, 'a', 'b', 'c', 0, 0, 0, 0, 0, 0}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t5: 完整词条、无尾魔数 → find 落空
            {
                fs::path mdx = base / (fn + "t5.mdx");
                write_mdx_file(mdx, "C2t5",
                               make_kb_raw(pr.head, nullptr,
                                           {0, 0, 0, 1, 0, 3, 'a', 'b', 'c',
                                            0, 0, 0, 0, 0, 0, 0, 4}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t6: 空词完整词条 + 尾魔数后零字节 → comp_len==0
            {
                fs::path mdx = base / (fn + "t6.mdx");
                write_mdx_file(mdx, "C2t6",
                               make_kb_raw(pr.head, pr.tail,
                                           {0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t7: 尾魔数后 4 字节垃圾 → safe_inflate 失败
            {
                fs::path mdx = base / (fn + "t7.mdx");
                write_mdx_file(mdx, "C2t7",
                               make_kb_raw(pr.head, pr.tail, {0, 0, 0, 0},
                                           {'N', 'O', 'P', 'E'}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
            // t8: 越界词条跳过 + 正常词条收录（guard 两侧同跑）
            {
                fs::path mdx = base / (fn + "t8.mdx");
                std::vector<unsigned char> items;
                be32w(items, 2);
                append_item(items, "abc", 9999, 4);
                append_item(items, "ok", 0, 2);
                write_mdx_file(mdx, "C2t8",
                               make_kb_raw(pr.head, pr.tail, items, zlib_compress_str("ABCD")));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.lookup("ok") == "AB" && mp.lookup("abc").empty());
            }
            // t9: 空词条目跳过 + 正常条目
            {
                fs::path mdx = base / (fn + "t9.mdx");
                std::vector<unsigned char> items;
                be32w(items, 2);
                append_item(items, "", 0, 2);
                append_item(items, "ok", 0, 2);
                write_mdx_file(mdx, "C2t9",
                               make_kb_raw(pr.head, pr.tail, items, zlib_compress_str("XY")));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 1 && mp.lookup("ok") == "XY");
            }
        }
        // C2t10: wl=0xFFFF 越界词长（三魔数各一）
        {
            const char* heads[] = {"KIDX", "KEYB", "KBIX"};
            const char* tails[] = {"RDEF", "RECB", "RBIX"};
            for (int i = 0; i < 3; ++i) {
                fs::path mdx = base / (std::string("c2_wl_") + heads[i] + ".mdx");
                write_mdx_file(mdx, "C2t10",
                               make_kb_raw(heads[i], tails[i],
                                           {0, 0, 0, 1, 0xFF, 0xFF, 'a', 'b'}, {}));
                UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
                assert(mp.word_count() == 2);
            }
        }
    }

    // ===== C3) KBIX/RBCT 多块阶梯 =====
    {
        // m1: "KBIX"+3B → count 截断
        {
            fs::path mdx = base / "c3_m1.mdx";
            write_mdx_file(mdx, "C3m1", make_kb_raw("KBIX", nullptr, {0, 0, 1}, {}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m2: count=1 + 1B → wl 截断
        {
            fs::path mdx = base / "c3_m2.mdx";
            write_mdx_file(mdx, "C3m2", make_kb_raw("KBIX", nullptr, {0, 0, 0, 1, 0, 4}, {}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m3: 12B 域只给 7B → 域截断
        {
            fs::path mdx = base / "c3_m3.mdx";
            write_mdx_file(mdx, "C3m3",
                           make_kb_raw("KBIX", nullptr,
                                       {0, 0, 0, 1, 0, 3, 'a', 'b', 'c',
                                        0, 0, 0, 0, 0, 0, 0, 0, 0}, {}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m4: 完整词条 + "RBCT" 后仅 2B → 块数截断
        {
            fs::path mdx = base / "c3_m4.mdx";
            std::vector<unsigned char> items;
            append_multi_item(items, "a", 0, 0, 4);
            auto body = make_multirb_raw(1, items, {});
            body.insert(body.end(), {'R', 'B', 'C', 'T', 0, 0});
            write_mdx_file(mdx, "C3m4", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m5: "RBCT"+count 后仅 2B → RBLK 头截断
        {
            fs::path mdx = base / "c3_m5.mdx";
            std::vector<unsigned char> items;
            append_multi_item(items, "a", 0, 0, 4);
            auto body = make_multirb_raw(1, items, {});
            body.insert(body.end(), {'R', 'B', 'C', 'T', 0, 0, 0, 1, 'X', 'X'});
            write_mdx_file(mdx, "C3m5", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m6: 块魔数错 "XXXX"
        {
            fs::path mdx = base / "c3_m6.mdx";
            std::vector<unsigned char> items;
            append_multi_item(items, "a", 0, 0, 4);
            auto body = make_multirb_raw(1, items, {});
            body.insert(body.end(), {'R', 'B', 'C', 'T', 0, 0, 0, 1,
                                     'X', 'X', 'X', 'X', 0, 0, 0, 9});
            write_mdx_file(mdx, "C3m6", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m7: "RBLK" 后仅 2B → comp_len 截断
        {
            fs::path mdx = base / "c3_m7.mdx";
            std::vector<unsigned char> items;
            append_multi_item(items, "a", 0, 0, 4);
            auto body = make_multirb_raw(1, items, {});
            body.insert(body.end(), {'R', 'B', 'C', 'T', 0, 0, 0, 1,
                                     'R', 'B', 'L', 'K', 0, 0});
            write_mdx_file(mdx, "C3m7", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m8: comp_len=100 但数据只 3B → rb+comp_len>end
        {
            fs::path mdx = base / "c3_m8.mdx";
            std::vector<unsigned char> body = {'K', 'B', 'I', 'X', 0, 0, 0, 0,
                                               'R', 'B', 'C', 'T', 0, 0, 0, 1,
                                               'R', 'B', 'L', 'K', 0, 0, 0, 100,
                                               1, 2, 3};
            write_mdx_file(mdx, "C3m8", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m9: 合法块头但坏 zlib 数据 → safe_inflate 失败
        {
            fs::path mdx = base / "c3_m9.mdx";
            write_mdx_file(mdx, "C3m9", make_multirb_raw(0, {}, {"NOPE"}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // m10: 双真实块 + 跨块词条 + 空词条目（bid 双块、guard 两侧、空词假侧）
        {
            fs::path mdx = base / "c3_m10.mdx";
            std::vector<unsigned char> items;
            append_multi_item(items, "a", 0, 0, 4);
            append_multi_item(items, "b", 1, 0, 4);
            append_multi_item(items, "", 0, 0, 4);
            write_mdx_file(mdx, "C3m10", make_multirb_raw(3, items, {"AAAA", "BBBB"}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("a") == "AAAA" && mp.lookup("b") == "BBBB");
            assert(mp.word_count() == 2);
        }
    }

    // ===== C4) MDXK/MDXR 阶梯 =====
    {
        // k1: 只有 MDXK（MDXR 缺）→ 365 假返
        {
            fs::path mdx = base / "c4_k1.mdx";
            write_mdx_file(mdx, "C4k1", make_mdxk_raw({"KEYS"}, {}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k2: 只有 "MDXR" 头（MDXK 缺）
        {
            fs::path mdx = base / "c4_k2.mdx";
            write_mdx_file(mdx, "C4k2", {{'M', 'D', 'X', 'R', 0, 0, 0, 1, 'X'}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k3: MDXK + 2B → kblocks 截断
        {
            fs::path mdx = base / "c4_k3.mdx";
            write_mdx_file(mdx, "C4k3", {{'M', 'D', 'X', 'K', 0, 0}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k4: kblocks=1 + 4B → clen/ulen 截断
        {
            fs::path mdx = base / "c4_k4.mdx";
            write_mdx_file(mdx, "C4k4", {{'M', 'D', 'X', 'K', 0, 0, 0, 1, 0, 0, 0, 9}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k5: clen=100 数据不足 → u+clen>end
        {
            fs::path mdx = base / "c4_k5.mdx";
            write_mdx_file(mdx, "C4k5",
                           {{'M', 'D', 'X', 'K', 0, 0, 0, 1,
                             0, 0, 0, 100, 0, 0, 0, 10, 'a', 'b', 'c'}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k6: ulen=0 → safe_inflate 的 ulen==0 侧
        {
            fs::path mdx = base / "c4_k6.mdx";
            write_mdx_file(mdx, "C4k6",
                           {{'M', 'D', 'X', 'K', 0, 0, 0, 1,
                             0, 0, 0, 4, 0, 0, 0, 0, 'N', 'O', 'P', 'E'}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k7: ulen>16MB → safe_inflate 上限侧（早期返回，不触发大分配）
        {
            fs::path mdx = base / "c4_k7.mdx";
            write_mdx_file(mdx, "C4k7",
                           {{'M', 'D', 'X', 'K', 0, 0, 0, 1,
                             0, 0, 0, 4, 0x80, 0, 0, 0, 'N', 'O', 'P', 'E'}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k8: key 块内半条目（wl=2 + 3B → ku+8>kend break）+ 正常 MDXR
        {
            fs::path mdx = base / "c4_k8.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            be16w(keybytes, 2);
            keybytes.push_back('x'); keybytes.push_back('y'); keybytes.push_back('z');
            write_mdx_file(mdx, "C4k8",
                           make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())},
                                         {"RE"}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("ok") == "RE");
        }
        // k9: kblocks=0 + 合法 MDXR → keys 空 → 假返
        {
            fs::path mdx = base / "c4_k9.mdx";
            write_mdx_file(mdx, "C4k9", make_mdxk_raw({}, {"REC"}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k10: 合法 key + rblocks=0（无 MDXR 块）→ rec_concat 空 → 假返
        {
            fs::path mdx = base / "c4_k10.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            write_mdx_file(mdx, "C4k10",
                           make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k11: "MDXR"+2B → rblocks 截断
        {
            fs::path mdx = base / "c4_k11.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            auto body = make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {});
            std::vector<unsigned char> extra = {'M', 'D', 'X', 'R', 0, 0};
            body.insert(body.end(), extra.begin(), extra.end());
            write_mdx_file(mdx, "C4k11", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k12: rec 块 clen/ulen 只 4B → ru+8>end
        {
            fs::path mdx = base / "c4_k12.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            auto body = make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {});
            std::vector<unsigned char> extra = {'M', 'D', 'X', 'R', 0, 0, 0, 1, 0, 0, 0, 2};
            body.insert(body.end(), extra.begin(), extra.end());
            write_mdx_file(mdx, "C4k12", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k13: rec 块 ru+clen>end
        {
            fs::path mdx = base / "c4_k13.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            auto body = make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {});
            std::vector<unsigned char> extra = {'M', 'D', 'X', 'R', 0, 0, 0, 1,
                                                0, 0, 0, 50, 0, 0, 0, 50, 'x'};
            body.insert(body.end(), extra.begin(), extra.end());
            write_mdx_file(mdx, "C4k13", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k14: 坏 zlib rec 块 → safe_inflate 失败
        {
            fs::path mdx = base / "c4_k14.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            auto body = make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {});
            std::vector<unsigned char> extra = {'M', 'D', 'X', 'R', 0, 0, 0, 1,
                                                0, 0, 0, 4, 0, 0, 0, 4,
                                                'N', 'O', 'P', 'E'};
            body.insert(body.end(), extra.begin(), extra.end());
            write_mdx_file(mdx, "C4k14", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k15: 双 key 块 + 双 rec 块 → 跨块绝对偏移 + off 越界
        {
            fs::path mdx = base / "c4_k15.mdx";
            std::vector<unsigned char> ka;
            append_item(ka, "ka", 0, 5);
            append_item(ka, "oob", 999, 3);
            std::vector<unsigned char> kb;
            append_item(kb, "kb", 8, 6);
            write_mdx_file(mdx, "C4k15",
                           make_mdxk_raw({std::string(ka.begin(), ka.end()),
                                          std::string(kb.begin(), kb.end())},
                                         {"AAAAA", "BBBBBBBBB"}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("ka") == "AAAAA" && mp.lookup("kb") == "BBBBBB");
            assert(mp.lookup("oob").empty());
        }
        // k16: rec 块 clen=0（ru+0>end 卫语句放行 → safe_inflate 的 clen==0 侧）
        {
            fs::path mdx = base / "c4_k16.mdx";
            std::vector<unsigned char> keybytes;
            append_item(keybytes, "ok", 0, 2);
            auto body = make_mdxk_raw({std::string(keybytes.begin(), keybytes.end())}, {});
            std::vector<unsigned char> extra = {'M', 'D', 'X', 'R', 0, 0, 0, 1,
                                                0, 0, 0, 0, 0, 0, 0, 10};
            body.insert(body.end(), extra.begin(), extra.end());
            write_mdx_file(mdx, "C4k16", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // k17: clen 超过 16MB 上限（safe_inflate 的 clen>MAX 侧；level-0 存储
        // 不压缩，16MB 输入产出 >16MB 密文，卫语句 u+clen>end 可放行）
        {
            fs::path mdx = base / "c4_k17.mdx";
            constexpr uint32_t kMax = 16u * 1024u * 1024u;
            std::vector<unsigned char> raw(kMax);
            std::mt19937 rng(7);
            for (size_t i = 0; i < raw.size(); i += 4) {  // 64KB 循环模式即可
                uint32_t v = rng();
                std::memcpy(&raw[i], &v, 4);
            }
            std::vector<unsigned char> packed(kMax + 4096);
            uLongf packed_len = packed.size();
            assert(compress2(packed.data(), &packed_len, raw.data(), raw.size(), 0) == Z_OK);
            packed.resize(packed_len);
            assert(packed.size() > kMax);  // 存储级压缩必然略大于输入
            std::vector<unsigned char> body = {'M', 'D', 'X', 'K'};
            be32w(body, 1);
            be32w(body, (uint32_t)packed.size());  // clen > MAX_COMP_BLOCK
            be32w(body, 16);                       // ulen 小值，短路只走 clen 臂
            body.insert(body.end(), packed.begin(), packed.end());
            write_mdx_file(mdx, "C4k17", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
    }

    // ===== C5) 启发式：特殊 wordish 字符全 case + wl>128 回退 + good<2 =====
    {
        // 特殊字符：空格 - _ . ' / 六个 switch case 逐一命中
        {
            fs::path mdx = base / "c5a.mdx";
            std::vector<unsigned char> keyblock;
            const char* words[] = {"a b", "a-b", "a_b", "a.b", "a'b", "a/b"};
            for (int i = 0; i < 6; ++i)
                append_item(keyblock, words[i], (uint32_t)i, 1);
            auto body = zlib_compress(keyblock);
            auto p2 = zlib_compress_str("0123456789");
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "C5a", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("a b") == "0" && mp.lookup("a-b") == "1");
            assert(mp.lookup("a_b") == "2" && mp.lookup("a.b") == "3");
            assert(mp.lookup("a'b") == "4" && mp.lookup("a/b") == "5");
        }
        // wl>128：条目字节齐备 → ok 假、回退 +3 落进 'x' 串、0x7878 越界
        // break；parsed 保持 2 → 命中
        {
            fs::path mdx = base / "c5b.mdx";
            std::vector<unsigned char> keyblock;
            append_item(keyblock, "it0", 0, 2);
            append_item(keyblock, "it1", 2, 2);
            be16w(keyblock, 200);
            keyblock.insert(keyblock.end(), 200, 'x');
            keyblock.insert(keyblock.end(), 8, 0x00);
            auto body = zlib_compress(keyblock);
            auto p2 = zlib_compress_str("ABCDEFGH");
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "C5b", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("it0") == "AB" && mp.lookup("it1") == "CD");
        }
        // good<2：2 条 key 但 1 越界 → 收 1 条、good>=2 假、后续块解析失败
        // → 整链假返；partial 词条仍在 words_（种子跳过）
        {
            fs::path mdx = base / "c5c.mdx";
            std::vector<unsigned char> keyblock;
            append_item(keyblock, "hk0", 0, 2);
            append_item(keyblock, "hk1", 9999, 2);
            auto body = zlib_compress(keyblock);
            auto p2 = zlib_compress_str("SECOND-BLOCK");
            body.insert(body.end(), p2.begin(), p2.end());
            auto p3 = zlib_compress_str("THIRD-BLOCK");
            body.insert(body.end(), p3.begin(), p3.end());
            write_mdx_file(mdx, "C5c", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 1 && mp.lookup("hk0") == "SE");
        }
    }

    // ===== C6) decompress_all/scan：坏头两 continue、33 块封顶、空 zlib 块 =====
    {
        // 坏 zlib 头两臂：CM!=8 / hdr%31!=0 → continue
        {
            fs::path mdx = base / "c6a.mdx";
            std::vector<unsigned char> body = {0x79, 0x9C, 0x00,   // CM=9 → 第一臂
                                               0x78, 0x00, 0x00};  // hdr%31≠0 → 第二臂
            auto p1 = zlib_compress_str("Q1");
            auto p2 = zlib_compress_str("Q2");
            body.insert(body.end(), p1.begin(), p1.end());
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "C6a", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // 空 zlib 流块（decompress 产出 0 → 不入列表）+ 33 个短块 →
        // heuristic 容量封顶 32 → 整链假返 → scan 消费同批块（空块再遇
        // !out.empty() 假侧）→ 种子
        {
            fs::path mdx = base / "c6b.mdx";
            auto packed = zlib_compress_str("SHORT");
            std::vector<unsigned char> body = zlib_compress({});
            for (int i = 0; i < 33; ++i) body.insert(body.end(), packed.begin(), packed.end());
            write_mdx_file(mdx, "C6b", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // 空 zlib 流块 + key 块 + rec 块：heuristic 整链命中（空块被跳过）
        {
            fs::path mdx = base / "c6c.mdx";
            std::vector<unsigned char> keyblock;
            append_item(keyblock, "cz", 0, 6);
            append_item(keyblock, "cz2", 6, 6);
            auto body = zlib_compress({});
            auto p1 = zlib_compress(keyblock);
            auto p2 = zlib_compress_str("RECRECRECREC");
            body.insert(body.end(), p1.begin(), p1.end());
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "C6c", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("cz") == "RECREC" && mp.lookup("cz2") == "RECREC");
        }
    }

    // ===== C7) UTF-16 头、头属性缺省、未闭合引号、空文件/1B body =====
    {
        // LE BOM + 属性文本（含 ≥0x80 码位与 U+0000，converter 丢弃/跳过）
        {
            fs::path mdx = base / "c7le.mdx";
            std::string txt = " title=\"Utf16Le\" description=\"utf16 desc\""
                              " version=\"1.0\"\xC3\xA9-\x00-\xFF";
            write_mdx_head(mdx, utf16_bytes(txt, true) + "\n",
                           make_simplekv({{"u16", "V16"}}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.dictionary_name() == "Utf16Le");
            assert(mp.dictionary_description().find("utf16 desc") != std::string::npos);
            assert(mp.lookup("u16") == "V16");
        }
        // BE BOM
        {
            fs::path mdx = base / "c7be.mdx";
            std::string txt = " title=\"Utf16Be\" description=\"utf16 be\"";
            write_mdx_head(mdx, utf16_bytes(txt, false) + "\n",
                           make_simplekv({{"u16", "VB"}}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.dictionary_name() == "Utf16Be");
            assert(mp.lookup("u16") == "VB");
        }
        // 零字节文件：head 空 → name 回落 stem；非加密空 body 全链
        {
            fs::path mdx = base / "c7empty.mdx";
            write_mdx_head(mdx, "", {});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.dictionary_name() == "c7empty");
            assert(mp.word_count() == 2);
        }
        // 1 字节 body：chain-2 的 body.size()>=2 假侧
        {
            fs::path mdx = base / "c7one.mdx";
            write_mdx_head(mdx, "<Dictionary title=\"OneB\"/>\n", {'G'});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // 头属性缺 description：desc 空时 version 前无空格的三元 "" 臂；
        // enc/comp 缺省的假侧
        {
            fs::path mdx = base / "c7desc.mdx";
            write_mdx_head(mdx, "<Dictionary title=\"T10\" version=\"3.1\"/>\n",
                           make_simplekv({{"w", "<div>d</div>"}}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.dictionary_description() == "(v=3.1)");
            assert(mp.lookup("w") == "<div>d</div>");
        }
        // 未闭合引号：extract_attr 的 q==npos 假返 → name 回落 stem
        {
            fs::path mdx = base / "c7unclose.mdx";
            write_mdx_head(mdx, "<Dictionary title=\"unclosed\n",
                           make_simplekv({{"w", "V"}}));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.dictionary_name() == "c7unclose");
            assert(mp.lookup("w") == "V");
        }
    }

    // ===== C8) chain-2 zlib 包裹：成功 / 截断流 / 非 zlib 头 =====
    {
        // zlib 包裹 SIMPLEKV 成功
        {
            fs::path mdx = base / "c8a.mdx";
            write_mdx_file(mdx, "C8a", zlib_compress(make_simplekv({{"z1", "VAL1"}})));
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("z1") == "VAL1" && mp.word_count() == 1);
        }
        // zlib 头合法但流截断 → Z_STREAM_END 假侧 → raw 兜底 → 种子
        {
            fs::path mdx = base / "c8b.mdx";
            auto packed = zlib_compress(make_simplekv({{"z2", "V"}}));
            packed.resize(packed.size() * 3 / 5);
            write_mdx_file(mdx, "C8b", packed);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // zlib 头非法（CM!=8）→ 头检查假侧 → raw 解析
        {
            fs::path mdx = base / "c8c.mdx";
            write_mdx_file(mdx, "C8c", {{0x79, 0x9C, 0x01, 0x02}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);
        }
        // scan 模式残段：word: 无 definition（pd 落空）、tab 行空词（989 假
        // 侧）、EOF 无换行的 tab 行（parsed_upto=text.size() 臂）、无 \n\n
        // 的 word:/definition: 尾段（dbl 落空臂）
        {
            fs::path mdx = base / "c8d.mdx";
            auto b1 = zlib_compress_str("word:lonely\n");
            auto b2 = zlib_compress_str("\tdata\n");
            auto b3 = zlib_compress_str("tail\tend");
            auto b4 = zlib_compress_str("word:t9\ndefinition:v9\n");
            std::vector<unsigned char> body;
            for (auto* b : {&b1, &b2, &b3, &b4}) body.insert(body.end(), b->begin(), b->end());
            write_mdx_file(mdx, "C8d", body);
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("lonely").empty());  // 无 definition → 不收录
            assert(mp.lookup("t9") == "v9");      // 无 \n\n → dbl 落空臂
            assert(mp.lookup("tail") == "end");   // EOF 无换行的 tab 行
            assert(mp.lookup("data").empty());    // tab 行空词被拒
        }
    }

    // ===== C9) 加密路径：明文直读链 / XOR 成功 / success=false 侧 / 环境回退 =====
    {
        // flagged-but-plaintext：encrypted="1" + 明文 KIDX → chain-1 先达
        {
            fs::path mdx = base / "c9flag.mdx";
            write_mdx_file(mdx, "C9Flag", make_kidx_like(), " encrypted=\"1\"");
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("flg") == "FLAG" && mp.lookup("sec") == "SECR");
            assert(mp.dictionary_description().find("[encrypted]") != std::string::npos);
        }
        // XOR 成功：密钥流在测试内复刻，密文解回合法 KIDX → 解密链命中
        {
            fs::path mdx = base / "c9xor.mdx";
            auto plain = make_kidx_like();
            const std::string plain_str(plain.begin(), plain.end());
            const std::string pw = pick_pw(plain_str);
            write_mdx_file(mdx, "C9Xor", xor_encrypt(plain_str, pw), " encrypted=\"1\"");
            set_env("UNIDICT_MDICT_PASSWORD", pw.c_str());
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("sec") == "SECR" && mp.lookup("flg") == "FLAG");
            assert(mp.word_count() == 2);
            assert(mp.dictionary_description().find("[encrypted]") != std::string::npos);
            unset_env("UNIDICT_MDICT_PASSWORD");
        }
        // 空 body + 密码环境：解密输入空 → 密钥空 → success=false 侧
        {
            fs::path mdx = base / "c9ebod.mdx";
            write_mdx_head(mdx, "<Dictionary title=\"Ebod\" encrypted=\"1\"/>\n", {});
            set_env("UNIDICT_MDICT_PASSWORD", "pw-x");
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.is_loaded() && mp.all_words().empty());
            unset_env("UNIDICT_MDICT_PASSWORD");
        }
        // UNIDICT_PASSWORD 兼容环境变量（get_mdict_password_env 第二臂）
        {
            fs::path mdx = base / "c9fall.mdx";
            write_mdx_file(mdx, "C9Fall", {{'G', 'A', 'R', 'B'}}, " encrypted=\"1\"");
            set_env("UNIDICT_PASSWORD", "fallback-pw");
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.is_loaded() && mp.all_words().empty());
            unset_env("UNIDICT_PASSWORD");
        }
        // 头行无换行符：body 读循环走 EOF 边
        {
            fs::path mdx = base / "c9nonl.mdx";
            write_mdx_head(mdx, "<Dictionary title=\"Nonl\" encrypted=\"1\"/>",
                           {{'G', 'A', 'R', 'B'}});
            UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
            assert(mp.is_loaded() && mp.all_words().empty());
        }
    }

    // ===== C10) 渲染 map_url：空 src 直通 / http:// 直通 / dict_dir_ 空回落 =====
    {
        fs::path mdx = base / "c10a.mdx";
        write_mdx_file(mdx, "C10a",
                       make_simplekv({
                           {"emptysrc", "<div><img src=\"\"/></div>"},
                           {"http", "<div><img src=\"http://e.io/i.png\"/></div>"},
                           {"missimg", "<div><img src=\"nohit.png\"/></div>"},
                           {"lq", "<i>@@@LINK=wo\"rd</i>"},
                           {"ls", "<i>@@@LINK=wo'rd</i>"},
                       }));
        UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("emptysrc").find("src=\"\"") != std::string::npos);
        assert(mp.lookup("http").find("http://e.io/i.png") != std::string::npos);
        assert(mp.lookup("missimg").find("nohit.png") != std::string::npos);
        // @@@LINK 词以引号截断（两臂），引号本体保留
        assert(mp.lookup("lq").find("</a>\"rd</i>") != std::string::npos);
        assert(mp.lookup("ls").find("</a>'rd</i>") != std::string::npos);

        // 相对文件名加载 → parent_path 为空 → dict_dir_ 空 → 目录回落假侧
        const fs::path rel = fs::current_path() / "c10_rel.mdx";
        write_mdx_file(rel, "C10Rel",
                       make_simplekv({{"deep", "<div><img src=\"g.png\"/></div>"}}));
        UnidictCoreStd::MdictParserStd mp2;
        assert(mp2.load_dictionary("c10_rel.mdx"));
        assert(mp2.lookup("deep").find("g.png") != std::string::npos);
        assert(mp2.lookup("deep").find("file://") == std::string::npos);
        fs::remove(rel);

        // dict_dir 命中路径带空格 → make_file_url 的 %20 臂
        fs::path spdir = base / "with space";
        fs::create_directories(spdir);
        std::ofstream spside(spdir / "sp.png", std::ios::binary);
        spside << "SPDATA";
        spside.close();
        write_mdx_file(spdir / "sp.mdx", "C10Sp",
                       make_simplekv({{"sp", "<div><img src=\"sp.png\"/></div>"}}));
        UnidictCoreStd::MdictParserStd mp3;
        assert(mp3.load_dictionary((spdir / "sp.mdx").string()));
        const std::string spurl = mp3.lookup("sp");
        assert(spurl.find("file://") != std::string::npos);
        assert(spurl.find("with%20space") != std::string::npos);
    }

    // ===== C11) 查询边界：fresh name 默认、find_similar break/假侧/耗尽 =====
    {
        UnidictCoreStd::MdictParserStd fresh;
        assert(fresh.dictionary_name() == "MDict");

        fs::path mdx = base / "c11.mdx";
        write_mdx_file(mdx, "C11", make_simplekv({{"wd0", "0"}, {"wd1", "1"},
                                                  {"wd2", "2"}, {"wd3", "3"},
                                                  {"wd4", "4"}, {"beta", "b"}}));
        UnidictCoreStd::MdictParserStd mp; assert(mp.load_dictionary(mdx.string()));
        auto two = mp.find_similar("wd", 2);
        assert(two.size() == 2 && two[0] == "wd0" && two[1] == "wd1");  // break 侧
        auto one = mp.find_similar("b", 9);
        assert(one.size() == 1 && one[0] == "beta");                    // 前缀假侧 + 耗尽
        assert(mp.find_similar("zz", 9).empty());
        assert(mp.find_similar("wd0", 0).empty());
    }

    // ===== C12) 提取：目录占名写失败、只读目录 manifest 打开失败、
    //           manifest 坏行变体重读、全坏 → 重提取 =====
    {
        fs::path mdx = base / "c12.mdx";
        fs::path mdd = base / "c12.mdd";
        write_mdx_file(mdx, "C12",
                       make_simplekv({{"look", "<div><img src=\"keep.png\"/></div>"}}));
        write_mdx_file(mdd, "C12Res", make_simplekv({{"keep.png", "KEEPPNG"}}));

        const fs::path cache_base = fs::current_path() / "data" / "cache" / "mdict";

        // 缓存根按 path|size|mtime 签名命名，历史运行会遗留旧根干扰
        // find_root_containing 的唯一性——先清空再开始本组场景
        if (fs::exists(cache_base)) {
            std::error_code ec;
            for (const auto& e : fs::directory_iterator(cache_base))
                fs::remove_all(e.path(), ec);
        }

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("look").find("file://") != std::string::npos);
        fs::path root = find_root_containing(cache_base, "keep.png");
        assert(!root.empty());

        // C12a: 目录占名 rel → ofstream 打不开 → 写文件 continue 侧；
        // 另带双斜杠与中部 ./ 键（sanitize 的空段/点段跳过臂）
        {
            fs::path mdx2 = base / "c12a.mdx";
            write_mdx_file(mdx2, "C12a",
                           make_simplekv({
                               {"ok", "<div><img src=\"ok.png\"/></div>"},
                               {"deep", "<div><img src=\"d.png\"/></div>"},
                               {"slash", "<div><img src=\"u//v.png\"/></div>"},
                               {"dotseg", "<div><img src=\"q/./r.png\"/></div>"},
                           }));
            write_mdx_file(base / "c12a.mdd", "C12aRes",
                           make_simplekv({{"d.png", "DEEPDATA"}, {"ok.png", "OKDATA"},
                                          {"u//v.png", "SLASHDATA"},
                                          {"q/./r.png", "DOTSEGDATA"}}));
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdx2.string()));  // 首次提取，全部落地
            assert(mp2.lookup("ok").find("file://") != std::string::npos);
            assert(mp2.lookup("slash").find("file://") != std::string::npos);
            assert(mp2.lookup("dotseg").find("file://") != std::string::npos);

            fs::path root2 = find_root_containing(cache_base, "ok.png");
            assert(!root2.empty());
            // d.png 换成目录占名，删 manifest 强制重提取
            fs::remove(root2 / "d.png");
            fs::create_directories(root2 / "d.png");
            fs::remove(root2 / "manifest.tsv");
            UnidictCoreStd::MdictParserStd mp3;
            assert(mp3.load_dictionary(mdx2.string()));
            assert(mp3.lookup("ok").find("file://") != std::string::npos);   // ok.png 照常
            assert(mp3.lookup("deep").find("d.png") != std::string::npos);   // 占名失败 → 原样
            assert(mp3.lookup("deep").find("file://") == std::string::npos);
        }

        // C12b: 只读父目录 → ensure_dir 失败 → manifest ofstream 打开失败
        {
            fs::path rootb = find_root_containing(cache_base, "keep.png");
            assert(!rootb.empty());
            fs::remove_all(rootb);
            const auto orig = fs::status(cache_base).permissions();
            std::error_code ec;
            fs::permissions(cache_base,
                            orig & ~(fs::perms::owner_write | fs::perms::group_write |
                                     fs::perms::others_write), ec);
            assert(!ec);
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdx.string()));
            fs::permissions(cache_base, orig, ec);
            assert(!ec);
            // 提取失败 → 无资源映射 → img src 原样保留
            assert(mp2.lookup("look").find("keep.png") != std::string::npos);
            assert(mp2.lookup("look").find("file://") == std::string::npos);
            // 权限恢复后再加载，缓存重建，供后续场景使用
            UnidictCoreStd::MdictParserStd mp3;
            assert(mp3.load_dictionary(mdx.string()));
            assert(mp3.lookup("look").find("file://") != std::string::npos);
        }

        // C12c: manifest 坏行变体：key 空 / rel 文件缺失 / 无 tab / 合法行
        {
            fs::path rootc = find_root_containing(cache_base, "keep.png");
            assert(!rootc.empty());
            {
                std::ofstream man(rootc / "manifest.tsv",
                                  std::ios::binary | std::ios::trunc);
                man << "\tpng\n"                    // key 空 → 跳过
                    << "keep.png\tmissing.png\n"    // rel 文件不存在 → 跳过
                    << "#no-tab-line\n"             // 无 tab → 跳过
                    << "keep.png\tkeep.png\n";      // 合法 → 收录
            }
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdx.string()));
            assert(mp2.lookup("look").find("file://") != std::string::npos);
        }

        // C12d: manifest 全坏 → 资源映射空 → 重新提取自愈
        {
            fs::path rootd = find_root_containing(cache_base, "keep.png");
            assert(!rootd.empty());
            {
                std::ofstream man(rootd / "manifest.tsv",
                                  std::ios::binary | std::ios::trunc);
                man << "\tnope\n#nocontent\n";
            }
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdx.string()));
            assert(mp2.lookup("look").find("file://") != std::string::npos);
        }

        // C12e: companion mdd 为 zlib 包裹的 SIMPLEKV → best_effort 的
        // zlib 解包链命中并完成提取
        {
            fs::path mdxe = base / "c12e.mdx";
            write_mdx_file(mdxe, "C12e",
                           make_simplekv({{"wlook", "<div><img src=\"w.png\"/></div>"}}));
            write_mdx_file(base / "c12e.mdd", "C12eRes",
                           zlib_compress(make_simplekv({{"w.png", "WRAPDATA"}})));
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdxe.string()));
            assert(mp2.lookup("wlook").find("file://") != std::string::npos);
        }

        // C12f: companion mdd 为 encrypted="1" + XOR 密文 → best_effort 的
        // 解密链命中（密钥流测试内复刻）→ 解出的 KIDX 词条当资源键提取
        {
            fs::path mdxf = base / "c12f.mdx";
            write_mdx_file(mdxf, "C12f",
                           make_simplekv({{"flook", "<div><img src=\"sec\"/></div>"}}));
            auto plain = make_kidx_like();
            const std::string plain_str(plain.begin(), plain.end());
            const std::string pw = pick_pw(plain_str);
            write_mdx_file(base / "c12f.mdd", "C12fRes",
                           xor_encrypt(plain_str, pw), " encrypted=\"1\"");
            set_env("UNIDICT_MDICT_PASSWORD", pw.c_str());
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdxf.string()));
            assert(mp2.lookup("flook").find("file://") != std::string::npos);
            unset_env("UNIDICT_MDICT_PASSWORD");
        }
    }

    unset_env("UNIDICT_MDICT_PASSWORD");
    unset_env("UNIDICT_PASSWORD");
    return 0;
}