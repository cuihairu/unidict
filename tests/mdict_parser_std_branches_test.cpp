// mdict_parser_std 分支缺口补测（lines 已 100%，本轮收 branches Missing
// 最大的模块：load_dictionary 的降级/防御路径、encrypted 头各取值、非加密
// 主路五链逐一命中、scan_and_decompress 的坏 zlib 头/截断流、word:/definition:
// 与 tab 行解析的退化片段、render_entry_for_ui 的 URL 归一与协议直通、
// companion .mdd 提取的容错分支）。std-only assert 风格，照仓库惯例
// 自包含 helpers，一个文件一个 ctest target。

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
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

// { u16 wlen, word, u32 off, u32 len } 词条表（KEYB/KBIX/KIDX 共用布局）
static void append_item(std::vector<unsigned char>& v, const std::string& w,
                        uint32_t off, uint32_t len) {
    be16w(v, (uint16_t)w.size());
    v.insert(v.end(), w.begin(), w.end());
    be32w(v, off);
    be32w(v, len);
}

// KIDX + RDEF：词条表 + zlib(def_blob)
static std::vector<unsigned char> make_kidx(
    const std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>>& items,
    const std::string& def_blob) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'I', 'D', 'X'});
    be32w(v, (uint32_t)items.size());
    for (const auto& it : items) append_item(v, it.first, it.second.first, it.second.second);
    v.insert(v.end(), {'R', 'D', 'E', 'F'});
    auto packed = zlib_compress({def_blob.begin(), def_blob.end()});
    v.insert(v.end(), packed.begin(), packed.end());
    return v;
}

// KEYB + RECB：词条表 + zlib(def_blob)
static std::vector<unsigned char> make_keyb(
    const std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>>& items,
    const std::string& def_blob) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'E', 'Y', 'B'});
    be32w(v, (uint32_t)items.size());
    for (const auto& it : items) append_item(v, it.first, it.second.first, it.second.second);
    v.insert(v.end(), {'R', 'E', 'C', 'B'});
    auto packed = zlib_compress({def_blob.begin(), def_blob.end()});
    v.insert(v.end(), packed.begin(), packed.end());
    return v;
}

// KBIX + RBIX：同 KIDX 布局的另一魔数对
static std::vector<unsigned char> make_kbix_rbix(
    const std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>>& items,
    const std::string& def_blob) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'B', 'I', 'X'});
    be32w(v, (uint32_t)items.size());
    for (const auto& it : items) append_item(v, it.first, it.second.first, it.second.second);
    v.insert(v.end(), {'R', 'B', 'I', 'X'});
    auto packed = zlib_compress({def_blob.begin(), def_blob.end()});
    v.insert(v.end(), packed.begin(), packed.end());
    return v;
}

// KBIX + RBCT：词条表带 block id + "RBCT"+nblocks+{ "RBLK"+clen+zlib(block) }
struct MultiItem { std::string w; uint32_t bid; uint32_t off; uint32_t len; };
static std::vector<unsigned char> make_kbix_multirb(
    const std::vector<MultiItem>& items,
    const std::vector<std::string>& blocks) {
    std::vector<unsigned char> v;
    v.insert(v.end(), {'K', 'B', 'I', 'X'});
    be32w(v, (uint32_t)items.size());
    for (const auto& it : items) {
        be16w(v, (uint16_t)it.w.size());
        v.insert(v.end(), it.w.begin(), it.w.end());
        be32w(v, it.bid);
        be32w(v, it.off);
        be32w(v, it.len);
    }
    v.insert(v.end(), {'R', 'B', 'C', 'T'});
    be32w(v, (uint32_t)blocks.size());
    for (const auto& b : blocks) {
        v.insert(v.end(), {'R', 'B', 'L', 'K'});
        auto packed = zlib_compress({b.begin(), b.end()});
        be32w(v, (uint32_t)packed.size());
        v.insert(v.end(), packed.begin(), packed.end());
    }
    return v;
}

// MDXK + MDXR：模拟真实 MDX 分块；keys 的 off/len 指向 rec 拼接体
static std::vector<unsigned char> make_mdxk_mdxr(
    const std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>>& items,
    const std::string& rec_concat) {
    std::vector<unsigned char> keydata;
    for (const auto& it : items) append_item(keydata, it.first, it.second.first, it.second.second);
    // 尾放半个条目：u16 wl=10 但数据只剩 3 字节 → key 解析循环 break 侧
    be16w(keydata, 10);
    keydata.push_back('x'); keydata.push_back('y'); keydata.push_back('z');

    std::vector<unsigned char> v;
    v.insert(v.end(), {'M', 'D', 'X', 'K'});
    be32w(v, 1);
    auto kpacked = zlib_compress(keydata);
    be32w(v, (uint32_t)kpacked.size());
    be32w(v, (uint32_t)keydata.size());
    v.insert(v.end(), kpacked.begin(), kpacked.end());

    v.insert(v.end(), {'M', 'D', 'X', 'R'});
    be32w(v, 1);
    auto rpacked = zlib_compress({rec_concat.begin(), rec_concat.end()});
    be32w(v, (uint32_t)rpacked.size());
    be32w(v, (uint32_t)rec_concat.size());
    v.insert(v.end(), rpacked.begin(), rpacked.end());
    return v;
}

int main() {
    fs::path base = fs::current_path() / "build-local" / "mdict_branches";
    fs::create_directories(base);
    unset_env("UNIDICT_MDICT_PASSWORD");
    unset_env("UNIDICT_PASSWORD");

    // ===== B1) 路径不存在：load 直接 false =====
    {
        UnidictCoreStd::MdictParserStd mp;
        assert(!mp.load_dictionary((base / "no_such.mdx").string()));
        assert(!mp.is_loaded());
        assert(mp.word_count() == 0);
    }

    // ===== B2) 传 .mdd 但同名 .mdx 不存在：保持 .mdd 继续解析 =====
    {
        fs::path mdd = base / "onlymdd.mdd";
        write_mdx_file(mdd, "OnlyMdd", make_simplekv({{"k", "<div>v</div>"}}));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdd.string()));
        assert(mp.lookup("k") == "<div>v</div>");
        assert(mp.dictionary_name() == "OnlyMdd");
    }

    // ===== B3) 全属性头：description 拼接 (v=) enc= comp= 各真侧 =====
    {
        fs::path mdx = base / "fullattrs.mdx";
        std::string header =
            "<Dictionary title=\"FullAttrs\" description=\"the desc\" version=\"2.0\" "
            "encoding=\"UTF-8\" compression=\"zlib\" encrypted=\"0\"/>\n";
        auto body = make_simplekv({{"w", "<div>d</div>"}});
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out << header;
        out.write((const char*)body.data(), (std::streamsize)body.size());
        out.close();
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        const std::string desc = mp.dictionary_description();
        assert(desc.find("the desc") != std::string::npos);
        assert(desc.find("(v=2.0)") != std::string::npos);
        assert(desc.find("enc=UTF-8") != std::string::npos);
        assert(desc.find("comp=zlib") != std::string::npos);
        assert(desc.find("[encrypted]") == std::string::npos);
    }

    // ===== B4) encrypted 各假值取值：0/off/no/false/空 → 都走非加密路径 =====
    {
        const char* values[] = {"0", "off", "no", "false"};
        for (const char* val : values) {
            fs::path mdx = base / (std::string("encneg_") + val + ".mdx");
            write_mdx_file(mdx, "EncNeg", make_simplekv({{"w", "<div>d</div>"}}),
                           std::string(" encrypted=\"") + val + "\"");
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("w") == "<div>d</div>");
            assert(mp.dictionary_description().find("[encrypted]") == std::string::npos);
        }
        // encrypted=""（空值）：!enc.empty() 假侧
        fs::path mdx = base / "encempty.mdx";
        write_mdx_file(mdx, "EncEmpty", make_simplekv({{"w", "<div>d</div>"}}),
                       " encrypted=\"\"");
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("w") == "<div>d</div>");
    }

    // ===== B5) encrypted="1" + 空 body：as-is 解析跳过、无密码环境 → 仍加载成功 =====
    {
        fs::path mdx = base / "encemptybody.mdx";
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out << "<Dictionary title=\"EncEmptyBody\" encrypted=\"1\"/>\n";
        out.close();
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        assert(mp.all_words().empty());
        assert(mp.dictionary_description().find("[encrypted]") != std::string::npos);
    }

    // ===== B6) encrypted="1" + 垃圾 body + 密码环境：解密后仍不可解析 → 成功返回空 =====
    {
        fs::path mdx = base / "encgarbage.mdx";
        write_mdx_file(mdx, "EncGarbage",
                       {{'G', 'A', 'R', 'B', 'A', 'G', 'E'}}, " encrypted=\"1\"");
        set_env("UNIDICT_MDICT_PASSWORD", "wrong-pw");
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.is_loaded());
        assert(mp.all_words().empty());
        unset_env("UNIDICT_MDICT_PASSWORD");
    }

    // ===== B7) 非加密主路五链 + 启发式逐一命中 =====
    {
        const std::string blob = "AAAABBBBCCCC";
        // KIDX/RDEF 主路
        {
            fs::path mdx = base / "kidxmain.mdx";
            write_mdx_file(mdx, "KidxMain",
                           make_kidx({{"ka", {0, 4}}, {"kb", {4, 4}}}, blob));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("ka") == "AAAA");
            assert(mp.lookup("kb") == "BBBB");
        }
        // KEYB/RECB 主路
        {
            fs::path mdx = base / "keybmain.mdx";
            write_mdx_file(mdx, "KeybMain",
                           make_keyb({{"kc", {0, 4}}, {"kd", {4, 4}}}, blob));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("kc") == "AAAA");
        }
        // KBIX/RBIX 主路
        {
            fs::path mdx = base / "kbixmain.mdx";
            write_mdx_file(mdx, "KbixMain",
                           make_kbix_rbix({{"ke", {0, 4}}, {"kf", {4, 4}}}, blob));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("ke") == "AAAA");
        }
        // KBIX/RBCT 主路：bid 越界与 off 越界的词条被跳过，正常词条进表
        {
            fs::path mdx = base / "kbixmultimain.mdx";
            write_mdx_file(mdx, "KbixMultiMain",
                           make_kbix_multirb({{"kg", 0, 0, 4},      // 块0 正常
                                              {"kh", 9, 0, 4},      // bid 越界（只有 1 块）
                                              {"ki", 0, 999, 4}},   // off+len 越界
                                             {"AAAABBBB"}));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("kg") == "AAAA");
            assert(mp.lookup("kh").empty());
            assert(mp.lookup("ki").empty());
        }
        // MDXK/MDXR 主路：off 越界词条跳过（畸形尾条目在 make_mdxk_mdxr 内）
        {
            fs::path mdx = base / "mdxkmain.mdx";
            write_mdx_file(mdx, "MdxkMain",
                           make_mdxk_mdxr({{"kj", {0, 5}}, {"kk", {5, 6}},
                                           {"kl", {999, 4}}},
                                          "APPLEBANANA"));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.lookup("kj") == "APPLE");
            assert(mp.lookup("kk") == "BANANA");
            assert(mp.lookup("kl").empty());
        }
        // 启发式主路：块1 = 合法 key index（含 9 词条触发 parsed>=8 break），
        // 块2 = 记录体；另含一个 off 越界词条（good 计数仍 ≥2）
        {
            fs::path mdx = base / "heurmain.mdx";
            std::vector<unsigned char> keyblock;
            for (int i = 0; i < 9; ++i) {
                std::string w = "wd" + std::to_string(i);
                append_item(keyblock, w, (uint32_t)(i * 3), 3);
            }
            append_item(keyblock, "wbad", 9999, 4);  // off 越界 → 不计数
            std::vector<unsigned char> recblock{'X', 'Y', 'Z', '0', '1', '2',
                                                '3', '4', '5', '6', '7', '8'};
            std::vector<unsigned char> body = zlib_compress(keyblock);
            auto p2 = zlib_compress(recblock);
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "HeurMain", body);
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            // parsed>=8 提前 break：keys 只收 wd0..wd7；rec_concat 仅 12 字节，
            // off+len 落界的 wd0..wd3 收录，wd4..wd7 越界剔除
            assert(mp.lookup("wd0") == "XYZ");
            assert(mp.lookup("wd3") == "678");
            assert(mp.lookup("wd7").empty());
            assert(mp.lookup("wbad").empty());
        }
        // 启发式：块内词含非 wordish 字符 → ok=false 回退重扫（p -= wl+8-1）。
        // 回退后每次失败重试恰好前进 3 字节（读 2+wl+8 再回退 wl+7），此处
        // 重扫点 30→33→36→39：off/len 逐字节摆成小 wl，避免 512 越界 break，
        // 最终在 39 处完整解出 ok2（off=1,len=1 → rec "QR" 的 "R"）
        {
            fs::path mdx = base / "heurback.mdx";
            std::vector<unsigned char> keyblock;
            append_item(keyblock, "skip", 0, 1);
            append_item(keyblock, "ok1", 0, 1);
            keyblock.push_back(0x00); keyblock.push_back(0x02);  // wl=2
            keyblock.push_back(0x01); keyblock.push_back(0x00);  // 0x01 非 wordish
            keyblock.push_back(0x03); keyblock.push_back(0xFF);  // off 高位：@30 wl=3、词含 0xFF 再败
            keyblock.push_back(0x00); keyblock.push_back(0x00);
            keyblock.insert(keyblock.end(), 4, 0x00);            // len=0：@33/@36 wl=0 逐 +3
            append_item(keyblock, "ok2", 1, 1);
            std::vector<unsigned char> recblock{'Q', 'R'};
            std::vector<unsigned char> body = zlib_compress(keyblock);
            auto p2 = zlib_compress(recblock);
            body.insert(body.end(), p2.begin(), p2.end());
            write_mdx_file(mdx, "HeurBack", body);
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            // good>=2 才收：解析到 ok1/ok2 → 命中
            assert(mp.lookup("ok2") == "R");
        }
        // 启发式失败：唯一块是 key index 但记录体缺（无后续块）→ rec_concat 空
        {
            fs::path mdx = base / "heurnorec.mdx";
            std::vector<unsigned char> keyblock;
            append_item(keyblock, "aa", 0, 1);
            append_item(keyblock, "bb", 1, 1);
            write_mdx_file(mdx, "HeurNoRec", zlib_compress(keyblock));
            UnidictCoreStd::MdictParserStd mp;
            assert(mp.load_dictionary(mdx.string()));
            assert(mp.word_count() == 2);  // fallback seeds
        }
    }

    // ===== B8) 主路五链失败 → scan_and_decompress：坏 zlib 头/截断流/完整块 =====
    {
        fs::path mdx = base / "scanmix.mdx";
        std::vector<unsigned char> body;
        // CM != 8：zlib 头魔数不合法 → 跳过
        body.push_back(0x79); body.push_back(0x9C); body.push_back(0x00);
        // hdr % 31 != 0 → 跳过
        body.push_back(0x78); body.push_back(0x00); body.push_back(0x00);
        // 头合法但流截断 → rc != Z_STREAM_END
        body.push_back(0x78); body.push_back(0x9C);
        body.push_back('w'); body.push_back('o');
        // 完整 zlib 块 1：word:/definition: 模式——pattern A 命中后
        // find("word:",i) 会跳过中间行，tab 行放同块永远不可达，故拆两块
        std::string text1 = "word:apple\ndefinition:fruit\n\n"
                            "word:\ndefinition:nogen\n\n"   // 空 w → 不进表
                            "justplainline\n"               // 无 word:/tab → 逐行推进
                            "word:nonl";                    // 无换行 → 退化
        // 完整 zlib 块 2：块内无 "word:"，tab 模式（pattern B）才可达
        std::string text2 = "cat\tfeline\n";
        auto packed = zlib_compress({text1.begin(), text1.end()});
        body.insert(body.end(), packed.begin(), packed.end());
        packed = zlib_compress({text2.begin(), text2.end()});
        body.insert(body.end(), packed.begin(), packed.end());
        // 尾部不足 3 字节：off + 2 < size 边界
        body.push_back('t'); body.push_back('a');
        write_mdx_file(mdx, "ScanMix", body);
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("apple") == "fruit");
        assert(mp.lookup("cat") == "feline");
        assert(mp.lookup("word").empty());   // 空 w 被拒
        assert(mp.word_count() >= 2);
    }

    // ===== B9) render/URL 归一大杂烩：协议直通、entry/bword、缓存命中、
    //          dict_dir 回落命中/miss、未闭合引号、@@@LINK 各形态 =====
    {
        fs::path mdx = base / "render.mdx";
        fs::path mdd = base / "render.mdd";
        write_mdx_file(mdx, "Render",
                       make_simplekv({
                           {"direct", "<div><a href=\"https://x.io/a\">x</a>"
                                      "<img src=\"data:image/png;base64,AA\"/>"
                                      "<img src=\"qrc:/icons/i.png\"/>"
                                      "<a href=\"unidict://lookup?word=a\">a</a></div>"},
                           {"entry", "<div><a href=\"entry://foo\">f</a></div>"},
                           {"bword", "<div><a href=\"bword://bar baz\">b</a></div>"},
                           {"cache", "<div><img src=\"pic.png\"/></div>"},
                           {"dirhit", "<div><img src=\"side.png\"/></div>"},
                           {"dirmiss", "<div><img src=\"ghost.png\"/></div>"},
                           {"unclosed", "<div><img src=\"oops.png></div>"},
                           {"links", "@@@LINK=first<@@@LINK=second tail"},
                           {"linktrail", "<i>see @@@LINK=lastword"},
                           {"linkspace", "@@@LINK= spaced"},
                           {"sound", "sound://audio.wav?v=1#x"},
                           {"normcase", "<div><img src=\"FILE://Pic.PNG\"/></div>"},
                           {"normlead", "<div><img src=\"//lead.png\"/></div>"},
                           {"normdot", "<div><img src=\"./dot.png\"/></div>"},
                           {"normback", "<div><img src=\"sub\\back.png\"/></div>"},
                           {"normquery", "<div><img src=\"pic.png?w=2#f\"/></div>"},
                           {"normup", "<div><img src=\"a/../../up.png\"/></div>"},
                           {"normcolon", "<div><img src=\"C:colon.png\"/></div>"},
                           {"normenc", "<div><a href=\"bword://a_b.c~d-é\">e</a></div>"},
                       }));
        const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "IMG";
        write_mdx_file(mdd, "RenderMdd", make_simplekv({{"pic.png", png}}));
        std::ofstream side(base / "side.png", std::ios::binary);
        side << "SIDEDATA";
        side.close();
        // C:colon.png 归一后变 C_colon.png → 建同名文件使 dict_dir 分支命中
        std::ofstream colon(base / "C_colon.png", std::ios::binary);
        colon << "COLONDATA";
        colon.close();

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));

        const std::string direct = mp.lookup("direct");
        assert(direct.find("https://x.io/a") != std::string::npos);
        assert(direct.find("data:image/png") != std::string::npos);
        assert(direct.find("qrc:/icons/i.png") != std::string::npos);
        assert(direct.find("unidict://lookup?word=a") != std::string::npos);

        const std::string entry = mp.lookup("entry");
        assert(entry.find("unidict://lookup?word=foo") != std::string::npos);
        const std::string bword = mp.lookup("bword");
        assert(bword.find("word=bar%20baz") != std::string::npos);

        // companion 缓存命中 → file:// 落到缓存路径
        assert(mp.lookup("cache").find("file://") != std::string::npos);
        // dict_dir 同目录回落命中
        const std::string hit = mp.lookup("dirhit");
        assert(hit.find("file://") != std::string::npos);
        assert(hit.find("side.png") != std::string::npos);
        // 两层全 miss → src 原样保留
        assert(mp.lookup("dirmiss").find("ghost.png") != std::string::npos);
        // 引号未闭合：attr 循环 append 剩余并退出
        assert(mp.lookup("unclosed").find("oops.png") != std::string::npos);
        // @@@LINK：多个（'<' 断词触发二次找标记）+ 空格截断词 + 尾随到串尾
        assert(mp.lookup("links").find("word=first") != std::string::npos);
        assert(mp.lookup("links").find("word=second") != std::string::npos);
        assert(mp.lookup("links").find("second</a> tail") != std::string::npos);
        assert(mp.lookup("linktrail").find("word=lastword") != std::string::npos);
        // 词后紧跟空格 → 空词 href（url_encode_component 空串侧）
        assert(mp.lookup("linkspace").find("word=") != std::string::npos);
        // sound:// 只标记 has_markers，key 归一后两层 miss → 原样返回
        assert(mp.lookup("sound").find("sound://audio.wav") != std::string::npos);
        // 归一：大小写前缀剥离后进缓存（file:// 小写命中） / 前导 // 与 ./
        // 剥离、../ 弹栈后两层 miss → src 原样保留 / 反斜杠 miss 原样保留 /
        // ?# 截断后命中 / : 换 _ 后 dict_dir 命中 / 大写十六进制 URL 编码
        const std::string ncase = mp.lookup("normcase");
        assert(ncase.find("file://") != std::string::npos);
        assert(ncase.find("FILE://") == std::string::npos);  // 前缀已剥离重映射
        assert(ncase.find("pic.png") != std::string::npos);  // 缓存文件名小写
        assert(mp.lookup("normlead").find("lead.png") != std::string::npos);
        assert(mp.lookup("normdot").find("dot.png") != std::string::npos);
        assert(mp.lookup("normback").find("sub\\back.png") != std::string::npos);
        assert(mp.lookup("normquery").find("file://") != std::string::npos);
        assert(mp.lookup("normup").find("up.png") != std::string::npos);
        assert(mp.lookup("normcolon").find("C_colon.png") != std::string::npos);
        assert(mp.lookup("normenc").find("%C3") != std::string::npos);
    }

    // ===== B10) companion .mdd 提取容错：rel 归一为空的 key 跳过、垃圾 mdd
    //           提取失败、manifest 删除后重提取、坏 manifest 行容错 =====
    {
        fs::path mdx = base / "mddtol.mdx";
        fs::path mdd = base / "mddtol.mdd";
        write_mdx_file(mdx, "MddTol",
                       make_simplekv({{"look", "<div><img src=\"pic.png\"/></div>"}}));
        // "/.." 归一后 rel 为空 → 写盘跳过；"pic.png" 正常落地
        write_mdx_file(mdd, "MddTolRes",
                       make_simplekv({{"/..", "JUNK"}, {"pic.png", "PNGDATA"}}));

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("look").find("file://") != std::string::npos);

        // 找到该 mdd 的缓存目录，删 manifest + 追加坏行，重加载走重新提取
        fs::path cache_root;
        // cache_dir() 未设 UNIDICT_CACHE_DIR 时默认 <cwd>/data/cache
        fs::path cache_base = fs::current_path() / "data" / "cache" / "mdict";
        if (fs::exists(cache_base)) {
            for (const auto& e : fs::directory_iterator(cache_base)) {
                if (fs::exists(e.path() / "manifest.tsv")) cache_root = e.path();
            }
        }
        assert(!cache_root.empty());
        {
            std::ofstream bad(cache_root / "manifest.tsv", std::ios::binary | std::ios::app);
            bad << "\n"            // 空行
                << "no_tab\n"      // 无 tab
                << "onlykey\t\n"   // rel 空
                << "\t只有rel\n";  // key 空
        }
        fs::remove(cache_root / "manifest.tsv");
        UnidictCoreStd::MdictParserStd mp2;
        assert(mp2.load_dictionary(mdx.string()));
        assert(mp2.lookup("look").find("file://") != std::string::npos);

        // 垃圾 .mdd：companion 提取失败 → 词典照常加载、无资源映射
        fs::path mdx2 = base / "badmdd.mdx";
        fs::path mdd2 = base / "badmdd.mdd";
        write_mdx_file(mdx2, "BadMdd", make_simplekv({{"w", "<div>d</div>"}}));
        write_mdx_file(mdd2, "BadMddRes", {{'n', 'o', 't', 'a', 'd', 'i', 'c', 't'}});
        UnidictCoreStd::MdictParserStd mp3;
        assert(mp3.load_dictionary(mdx2.string()));
        assert(mp3.lookup("w") == "<div>d</div>");
    }

    // ===== B11) 查询边界：lookup miss、find_similar max_results=0 =====
    {
        fs::path mdx = base / "query.mdx";
        write_mdx_file(mdx, "Query", make_simplekv({{"alpha", "a"}, {"alphabet", "ab"}}));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("nope").empty());
        assert(mp.find_similar("alpha", 0).empty());
        auto one = mp.find_similar("alpha", 1);
        assert(one.size() == 1 && one.front() == "alpha");
    }

    unset_env("UNIDICT_MDICT_PASSWORD");
    unset_env("UNIDICT_PASSWORD");
    return 0;
}
