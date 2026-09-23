// MdictParserStd 补覆盖：zlib 包裹的 SIMPLEKV 容器（mdx 主体与 .mdd
// companion 两条路径）、manifest 缓存命中与坏行容错、dict_dir 资源回落
// （含 "../" 归一与全 miss 原样返回）、.mdd→.mdx 路径转换、无扩展名
// 文件、空文件回落 stem 名、词条 URL 编码、UTF-16LE/BE 头非 ASCII
// 丢弃、find_similar 前缀查询。

#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/mdict_parser_std.h"

static void set_env(const char* key, const char* value) {
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    ::setenv(key, value, 1);
#endif
}

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
}

static std::vector<unsigned char> make_simplekv(const std::vector<std::pair<std::string, std::string>>& kv) {
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

// KIDX/RDEF 索引容器：词条表 + zlib 压缩的定义串接体
static std::vector<unsigned char> make_kidx(const std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>>& items,
                                            const std::string& def_blob) {
    std::vector<unsigned char> v;
    const std::string kidx = "KIDX";
    v.insert(v.end(), kidx.begin(), kidx.end());
    be32w(v, (uint32_t)items.size());
    for (const auto& it : items) {
        be16w(v, (uint16_t)it.first.size());
        v.insert(v.end(), it.first.begin(), it.first.end());
        be32w(v, it.second.first);
        be32w(v, it.second.second);
    }
    const std::string rdef = "RDEF";
    v.insert(v.end(), rdef.begin(), rdef.end());
    std::vector<unsigned char> src(def_blob.begin(), def_blob.end());
    std::vector<unsigned char> packed(src.size() + 1024);
    uLongf packed_len = packed.size();
    assert(compress2(packed.data(), &packed_len, src.data(), src.size(), 9) == Z_OK);
    packed.resize(packed_len);
    v.insert(v.end(), packed.begin(), packed.end());
    return v;
}

static std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& src) {
    std::vector<unsigned char> packed(src.size() + 1024);
    uLongf packed_len = packed.size();
    assert(compress2(packed.data(), &packed_len, src.data(), src.size(), 9) == Z_OK);
    packed.resize(packed_len);
    return packed;
}

// XML 头一行 + 容器体；compression_attr 可为 "" 或 ` compression="zlib"`
static void write_mdx_file(const std::filesystem::path& path,
                           const std::string& title,
                           const std::vector<unsigned char>& body,
                           const char* compression_attr = "") {
    std::string header = "<Dictionary title=\"" + title + "\" description=\"cov desc\"" +
                         compression_attr + "/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

static void append_env_line(const std::filesystem::path& cache_root) {
    // 往已生成的 manifest.tsv 追加坏行：空行 / 无 tab / 指向不存在文件
    std::ofstream m(cache_root / "manifest.tsv", std::ios::binary | std::ios::app);
    m << "\n";
    m << "no_tab_line\n";
    m << "ghost.bin\tghost_rel.bin\n";
}

static std::string make_utf16le(const std::u16string& s) {
    std::string out;
    out.push_back((char)0xFF); out.push_back((char)0xFE); // BOM LE
    for (char16_t ch : s) {
        out.push_back((char)(ch & 0xFF));
        out.push_back((char)((ch >> 8) & 0xFF));
    }
    return out;
}

static std::string make_utf16be(const std::u16string& s) {
    std::string out;
    out.push_back((char)0xFE); out.push_back((char)0xFF); // BOM BE
    for (char16_t ch : s) {
        out.push_back((char)((ch >> 8) & 0xFF));
        out.push_back((char)(ch & 0xFF));
    }
    return out;
}

int main() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdict_cover";
    fs::path cache = base / "cache";
    fs::create_directories(base);
    set_env("UNIDICT_CACHE_DIR", cache.string().c_str());

    // ===== 1) zlib 包裹的 SIMPLEKV：zlib 头探测 + inflate 解析 =====
    {
        std::vector<unsigned char> raw = make_simplekv({{"zed", "<div>z def</div>"}});
        std::vector<unsigned char> packed(raw.size() + 1024);
        uLongf packed_len = packed.size();
        assert(compress2(packed.data(), &packed_len, raw.data(), raw.size(), 9) == Z_OK);
        packed.resize(packed_len);

        fs::path mdx = base / "zlibwrap.mdx";
        write_mdx_file(mdx, "CovZlib", packed, " compression=\"zlib\"");
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("zed") == "<div>z def</div>");
        assert(mp.word_count() == 1);
        // description 拼接分支：comp= 与 enc=、(v=)
        assert(mp.dictionary_description().find("comp=zlib") != std::string::npos);
    }

    // ===== 2) 链接目标带空格 → URL 编码（%20）；find_similar 前缀 =====
    {
        fs::path mdx = base / "enc.mdx";
        write_mdx_file(mdx, "CovEnc",
                       make_simplekv({{"hi there",
                                       "<div><a href=\"bword://hi there\">go</a></div>"},
                                      {"hello", "<div>plain</div>"}}));
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        auto rendered = mp.lookup("hi there");
        assert(rendered.find("word=hi%20there") != std::string::npos);
        auto sim = mp.find_similar("hi", 10);
        assert(!sim.empty() && sim.front() == "hi there");
        assert(mp.find_similar("zzz", 10).empty());
    }

    // ===== 3) manifest 缓存命中 + 坏行容错 + dict_dir 回落 =====
    {
        fs::path mdx = base / "man.mdx";
        fs::path mdd = base / "man.mdd";
        const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "IMGDATA";
        write_mdx_file(mdx, "CovMan",
                       make_simplekv({{"look", "<div><img src=\"pic.png\"/></div>"}}));
        // mdd 体的 SIMPLEKV 用 zlib 包裹：companion 解析走 best_effort 的
        // zlib 头探测 + inflate 分支
        {
            std::vector<unsigned char> raw = make_simplekv({{"pic.png", png}});
            std::vector<unsigned char> packed(raw.size() + 1024);
            uLongf packed_len = packed.size();
            assert(compress2(packed.data(), &packed_len, raw.data(), raw.size(), 9) == Z_OK);
            packed.resize(packed_len);
            write_mdx_file(mdd, "CovManMdd", packed);
        }

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));   // 首次：解压 mdd 资源并写 manifest
        assert(mp.lookup("look").find("file://") != std::string::npos);

        // 二次加载：manifest.tsv 已在缓存 → 走 manifest 命中分支
        {
            UnidictCoreStd::MdictParserStd mp2;
            assert(mp2.load_dictionary(mdx.string()));
            assert(mp2.lookup("look").find("file://") != std::string::npos);
        }

        // 往 manifest 追加坏行（空行 / 无 tab / 不存在的目标文件），再加载
        for (auto& p : fs::recursive_directory_iterator(cache))
            if (p.path().filename() == "manifest.tsv")
                append_env_line(p.path().parent_path());
        {
            UnidictCoreStd::MdictParserStd mp3;
            assert(mp3.load_dictionary(mdx.string()));
            assert(mp3.lookup("look").find("file://") != std::string::npos);
        }
    }

    // ===== 4) 无 companion 资源：dict_dir 同目录回落 =====
    {
        fs::path mdx = base / "fallback.mdx";
        write_mdx_file(mdx, "CovFall",
                       make_simplekv({{"show", "<div><img src=\"side.png\"/></div>"},
                                      {"dot", "<div><img src=\"sub/../side.png\"/></div>"},
                                      {"ghost", "<div><img src=\"nothere.png\"/></div>"}}));
        // mdx 同目录放资源文件；key 归一带 "../" 也应命中同一文件
        std::ofstream img(base / "side.png", std::ios::binary);
        img << "SIDEDATA";
        img.close();

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        auto rendered = mp.lookup("show");
        assert(rendered.find("file://") != std::string::npos);
        assert(rendered.find("side.png") != std::string::npos);
        // "sub/../" 归一后命中同一文件
        auto dotted = mp.lookup("dot");
        assert(dotted.find("file://") != std::string::npos);
        assert(dotted.find("side.png") != std::string::npos);
        // 资源两层全 miss（缓存表 + dict_dir）：src 原样保留
        assert(mp.lookup("ghost").find("nothere.png") != std::string::npos);
    }

    // ===== 5) .mdd 路径加载 → 同 stem .mdx 存在时转换 =====
    {
        fs::path mdx = base / "asmdd.mdx";
        fs::path mdd = base / "asmdd.mdd";
        write_mdx_file(mdx, "CovAsMdd", make_simplekv({{"k1", "<div>v1</div>"}}));
        write_mdx_file(mdd, "CovAsMddMdd", make_simplekv({{"res.bin", "BIN"}}));

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdd.string()));   // 传 .mdd → 自动换 .mdx
        assert(mp.lookup("k1") == "<div>v1</div>");
    }

    // ===== 6) 无扩展名文件加载 =====
    {
        fs::path mdx = base / "noext.mdx";
        write_mdx_file(mdx, "CovNoExt", make_simplekv({{"nk", "<div>nv</div>"}}));
        fs::path bare = base / "noextbin";
        fs::copy_file(mdx, bare, fs::copy_options::overwrite_existing);

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(bare.string()));
        assert(mp.dictionary_name() == "CovNoExt");
        assert(mp.lookup("nk") == "<div>nv</div>");
    }

    // ===== 7) UTF-16LE 头含非 ASCII：属性扫描丢非 ASCII 继续 =====
    {
        fs::path mdx = base / "u16acc.mdx";
        std::u16string header = u"<Dictionary title=\"D";
        // 手工拼 UTF-16LE：é (U+00E9) 夹在 ASCII 之间
        header += u"émo";
        header += u"\" description=\"u16\"/>\n";
        std::string bin = make_utf16le(header);
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out.write(bin.data(), (std::streamsize)bin.size());
        out << "DATA";
        out.close();

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        // é 被丢弃：标题按 ASCII 段拼装（D + mo）
        assert(mp.dictionary_name().find("mo") != std::string::npos);
    }

    // ===== 8) UTF-16BE 头：BE BOM 识别与 ASCII 保留 =====
    {
        fs::path mdx = base / "u16be.mdx";
        std::u16string header = u"<Dictionary title=\"B";
        header += u"émo";
        header += u"\" description=\"u16be\"/>\n";
        std::string bin = make_utf16be(header);
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out.write(bin.data(), (std::streamsize)bin.size());
        out << "DATA";
        out.close();

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.dictionary_name().find("mo") != std::string::npos);
    }

    // ===== 9) 空文件：head 为空回落 stem 名，load 走兜底仍成功 =====
    {
        fs::path empty = base / "emptyfile.mdx";
        { std::ofstream out(empty.string().c_str(), std::ios::binary | std::ios::trunc); }
        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(empty.string()));
        assert(mp.dictionary_name() == "emptyfile");
        assert(mp.dictionary_description().empty());
    }

    // ===== 10) companion .mdd 的体是 KIDX/RDEF 索引：best_effort 五链命中 =====
    {
        fs::path mdx = base / "kidxmdd.mdx";
        fs::path mdd = base / "kidxmdd.mdd";
        write_mdx_file(mdx, "CovKidxMdd",
                       make_simplekv({{"main", "<div>m</div>"}}));
        write_mdx_file(mdd, "CovKidxMddRes",
                       make_kidx({{"ka", {0, 4}}, {"kb", {4, 4}}}, "AAAABBBB"));

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("main") == "<div>m</div>");
    }

    // ===== 11) encrypted="1" 头 + 明文 KIDX 体：as-is 五链先行命中 =====
    {
        fs::path mdx = base / "enckidx.mdx";
        write_mdx_file(mdx, "CovEncKidx",
                       make_kidx({{"ka", {0, 4}}, {"kb", {4, 4}}}, "AAAABBBB"),
                       " encrypted=\"1\"");

        UnidictCoreStd::MdictParserStd mp;
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.lookup("ka") == "AAAA");
        assert(mp.dictionary_description().find("[encrypted]") != std::string::npos);
    }

    // ===== 12) 两个非词典 zlib 块：启发式逐词扫描走非法词回退，全链失败 =====
    {
        fs::path mdx = base / "heurbad.mdx";
        // 块1：轮1 合法词 "a c"（含空格），轮2 词含 0x01 触发回退，
        // 回退后 wl 越界 break → parsed=1 <2 → 该块失败
        std::vector<unsigned char> b1 = {
            0x00, 0x03, 0x61, 0x20, 0x63,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x03,
            0x00, 0x02, 0x01, 0xFF,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0xFF, 0xFF,
        };
        std::vector<unsigned char> b2(8, 0xFF);   // wl=0xFFFF 立即 break
        std::vector<unsigned char> body = zlib_compress(b1);
        std::vector<unsigned char> p2 = zlib_compress(b2);
        body.insert(body.end(), p2.begin(), p2.end());
        write_mdx_file(mdx, "CovHeurBad", body);

        UnidictCoreStd::MdictParserStd mp;
        // 全链失败后走兜底 seed（"mdict"/"unidict"），load 仍为 true
        assert(mp.load_dictionary(mdx.string()));
        assert(mp.word_count() == 2);
        assert(mp.lookup("mdict").find("skeleton") != std::string::npos);
    }

    return 0;
}
