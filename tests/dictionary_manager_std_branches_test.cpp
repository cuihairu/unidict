// dictionary_manager_std 分支缺口补测（真实缺边 75 条，全库第二大簇）。
//
// 缺边构成：五格式 Holder 链各臂、四扩展名的解析失败臂、ifo 伴生文件
// 矩阵（dict 在场 / 仅 dz / 无 dict 无 dz）、mdx 同目录伴生扫描四象限
// （同 stem .mdd / 异 stem .mdd / 非 .mdd / 子目录）、enabled 同态重设、
// search_all 的禁用跳过与全 miss、full_text_search 的空 query 与
// max_results<=0、结果上限 break、ensure 的空释义跳过、relaxed 索引的
// 越界 docId（crafted UDFT 指向不存在词典/不存在词/负索引/空释义词）、
// relaxed 的 null out_error/out_version 两态、accept_version=1 拒绝
// v3、签名的源文件缺失与"文件变目录"形态。
// 全部真实输入驱动（手改持久化索引与文件系统状态是解析器的真实输入面）。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "dmbr";
    fs::create_directories(d);
    return d;
}

static fs::path write_text_at(const fs::path& p, const std::string& body) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
    assert(out.good());
    return p;
}

static fs::path write_text(const std::string& name, const std::string& body) {
    return write_text_at(base_dir() / name, body);
}

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) { v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }

// MDict 词典体：SIMPLEKV 容器
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

static void write_mdict_like_file(const fs::path& path, const std::vector<unsigned char>& body) {
    write_text_at(path, "<Dictionary title=\"MDXBR\" description=\"dmbr mdd\"/>\n");
    std::ofstream out(path, std::ios::binary | std::ios::app);
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

static void be32s(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

// stardict：按开关组装 .idx/.dict/.dict.dz（ifo 恒在）
static fs::path write_stardict(const fs::path& base, bool with_idx, bool with_dict, bool with_dz) {
    const std::string w1 = "hello", d1 = "sd hello def";
    if (with_dict) {
        std::ofstream dict(base.string() + ".dict", std::ios::binary | std::ios::trunc);
        dict.write(d1.data(), (std::streamsize)d1.size());
    }
    if (with_dz) {
        gzFile gz = gzopen((base.string() + ".dict.dz").c_str(), "wb");
        assert(gz != nullptr);
        gzwrite(gz, d1.data(), (unsigned)d1.size());
        gzclose(gz);
    }
    if (with_idx) {
        std::ofstream idx(base.string() + ".idx", std::ios::binary | std::ios::trunc);
        idx.write(w1.c_str(), (std::streamsize)w1.size()); idx.put('\0');
        be32s(idx, 0); be32s(idx, (uint32_t)d1.size());
    }
    return write_text_at(base.string() + ".ifo",
                         "bookname=SDBR\nwordcount=1\nidxfilesize=" +
                         std::to_string(w1.size() + 1 + 8) + "\nidxoffsetbits=32\n");
}

// UDFT3 手写索引：docs 个 (dict,word) 对 + 1 个词 "gw"（n=1，posting
// delta=0 → docId=0，tf=1）。relaxed 加载（accept_version=0）不校验签名，
// siglen=0 即可。
static std::string craft_udft3(const std::vector<std::pair<uint32_t, uint32_t>>& docmap) {
    std::string b;
    b.append("UDFT3", 5);
    auto u32 = [&b](uint32_t v) {
        b.push_back((char)(v & 0xFF)); b.push_back((char)((v >> 8) & 0xFF));
        b.push_back((char)((v >> 16) & 0xFF)); b.push_back((char)((v >> 24) & 0xFF));
    };
    u32(0);                       // siglen=0
    u32((uint32_t)docmap.size()); // docs
    for (auto& pr : docmap) { u32(pr.first); u32(pr.second); }
    u32(1);                       // terms
    u32(2); b.append("gw");       // term "gw"
    u32(1);                       // n
    u32(2); b.append(std::string("\x00\x01", 2));  // delta=0, tf=1
    return b;
}

// T1 Holder 链各臂 + 四扩展名解析失败臂 + enabled 同态重设/翻转 +
// search_all 禁用跳过/全 miss + meta 对禁用词典照列
static void test_holder_chain_and_failures() {
    fs::path dir = base_dir();
    // 解析失败臂：不存在的 ifo/mdx/dsl（解析器开局读文件必败）与
    // 空/纯注释 csv（无有效词条 → loaded_ 假）。未知扩展走分发 else 臂。
    write_text("empty.csv", "");
    write_text("comment.csv", "# only a comment\n; another\n");
    {
        DictionaryManagerStd m;
        assert(!m.add_dictionary((dir / "nope.ifo").string()));
        assert(!m.add_dictionary((dir / "nope.mdx").string()));
        assert(!m.add_dictionary((dir / "nope.dsl").string()));
        assert(!m.add_dictionary((dir / "empty.csv").string()));
        assert(!m.add_dictionary((dir / "comment.csv").string()));
        assert(!m.add_dictionary((dir / "unknown.xyz").string()));
    }
    // 三格式混载 + Holder::lookup 链路：json 命中短路（16 真）、
    // dsl 独有词连穿 16-19 假臂、csv 独有词再穿 16-19 落 20 真臂；
    // .tsv 与 .txt 走同一 csv 分发的另外两个扩展名臂
    fs::path csv_p = write_text("dmbr.csv", "solecsv,only in csv\n");
    fs::path tsv_p = write_text("dmbr.tsv", "soletsv\tonly in tsv\n");
    fs::path txt_p = write_text("dmbr.txt", "soletxt|only in txt\n");
    fs::path dsl_p = write_text("dmbr.dsl", "#NAME DSLBR\n\nsoledsl\nonly in dsl\n");
    fs::path json_p = write_text("dmbr.json",
        "{\"name\":\"JSONBR\",\"description\":\"dmbr json\","
        "\"entries\":[{\"word\":\"hello\",\"definition\":\"json hello\"}]}");
    DictionaryManagerStd m;
    assert(m.add_dictionary(json_p.string()));
    assert(m.add_dictionary(dsl_p.string()));
    assert(m.add_dictionary(csv_p.string()));
    assert(m.add_dictionary(tsv_p.string()));
    assert(m.add_dictionary(txt_p.string()));
    assert(m.search_word("hello", true) == "json hello");     // json 短路
    assert(m.search_word("soledsl", true) == "only in dsl");  // 穿链到 dsl
    assert(m.search_word("solecsv", true) == "only in csv");  // 穿链到 csv
    assert(m.search_word("soletsv", true) == "only in tsv");  // .tsv 臂
    assert(m.search_word("soletxt", true) == "only in txt");  // .txt 臂
    // enabled 同态重设：已开再开 → 同态短路真。csv/tsv/txt 三本词典
    // 同名 "dmbr"（取自文件 stem），按名查找命中第一本（csv）
    assert(m.loaded_dictionaries().size() == 5);
    assert(m.set_dictionary_enabled("dmbr", true));
    assert(m.is_dictionary_enabled("dmbr"));
    // 翻转禁用后：默认查词跳过（continue 臂），include_disabled 收回
    assert(m.set_dictionary_enabled("dmbr", false));
    assert(!m.is_dictionary_enabled("dmbr"));
    assert(m.search_word("solecsv", false).empty());
    assert(m.search_all("solecsv", false).empty());
    assert(m.search_all("solecsv", true).size() == 1);
    // 全 miss：每个 holder 的空释义假臂
    assert(m.search_all("zzz_none", true).empty());
    // 禁用词典仍进 meta
    assert(m.dictionaries_meta().size() == 5);
}

// T2 ifo 伴生文件矩阵：dict 在场（48 真）/ 仅 dz（48 假 + 49 真）/
// 无 dict 无 dz（解析器直接拒绝）。47 假臂与 49 假臂为死臂（见源码 EXCL）。
static void test_stardict_companions() {
    fs::path dir = base_dir();
    // a) ifo+idx+dict：查词命中明文词体，伴生收 .dict
    {
        fs::path ifo = write_stardict(dir / "sd_dict", true, true, false);
        DictionaryManagerStd m;
        assert(m.add_dictionary(ifo.string()));
        assert(m.search_word("hello", true) == "sd hello def");
        assert(m.fulltext_signature().find("sd_dict.dict|") != std::string::npos);
    }
    // b) ifo+idx+dict.dz（无 .dict）：伴生收 .dict.dz
    {
        fs::path ifo = write_stardict(dir / "sd_dz", true, false, true);
        DictionaryManagerStd m;
        assert(m.add_dictionary(ifo.string()));
        assert(m.search_word("hello", true) == "sd hello def");
        assert(m.fulltext_signature().find("sd_dz.dict.dz|") != std::string::npos);
    }
    // c) ifo+idx 但既无 .dict 也无 .dict.dz：stardict 解析器拒绝加载
    {
        fs::path ifo = write_stardict(dir / "sd_none", true, false, false);
        DictionaryManagerStd m;
        assert(!m.add_dictionary(ifo.string()));
    }
}

// T3 mdx 同目录伴生扫描四象限 + mdict holder 经 manager 查词/meta
static void test_mdx_companion_scan() {
    fs::path dir = base_dir() / "mdxdir";
    fs::create_directories(dir);
    fs::path mdx = dir / "scan.mdx";
    write_mdict_like_file(mdx, make_simplekv({{"mdxword", "mdx def text"}}));
    // 同 stem .mdd：扩展名与 stem 双真 → 收进 src_paths
    write_mdict_like_file(dir / "scan.mdd", make_simplekv({{"a.png", "x"}}));
    // 异 stem .mdd：扩展真、stem 假 → 不收
    write_mdict_like_file(dir / "other.mdd", make_simplekv({{"b.png", "y"}}));
    // 同 stem 非 .mdd：扩展名假
    write_text_at(dir / "scan.png", "not an mdd");
    // 异 stem 非 .mdd：双假
    write_text_at(dir / "elsewhere.txt", "noise");
    // 子目录：is_regular_file 假臂 → continue
    fs::create_directories(dir / "subdir");

    DictionaryManagerStd m;
    assert(m.add_dictionary(mdx.string()));
    assert(m.search_word("mdxword", true) == "mdx def text");
    // 伴生只收同 stem .mdd：签名含 scan.mdd，不含 other.mdd
    std::string sig = m.fulltext_signature();
    assert(sig.find("scan.mdd|") != std::string::npos);
    assert(sig.find("other.mdd") == std::string::npos);
    auto metas = m.dictionaries_meta();
    assert(metas.size() == 1);
    assert(metas[0].name == "MDXBR");
}

// T4 full_text_search 守卫、结果上限 break、ensure 空释义跳过、
// relaxed 索引的越界 docId 四态与空释义词命中、严格加载失败臂
static void test_fulltext_guards() {
    fs::path dir = base_dir();
    // ghostword 空释义：ensure 建索引时跳过（空释义 continue 臂）
    fs::path json_p = write_text("ftbr.json",
        "{\"name\":\"FTBR\",\"description\":\"ft br\","
        "\"entries\":[{\"word\":\"ghostword\",\"definition\":\"\"},"
        "{\"word\":\"realword\",\"definition\":\"searchable body text\"},"
        "{\"word\":\"bodyword\",\"definition\":\"another body note\"}]}");
    DictionaryManagerStd m;
    assert(m.add_dictionary(json_p.string()));

    // 空 query / max_results<=0 的三个短路臂
    assert(m.full_text_search("", 10).empty());
    assert(m.full_text_search("realword", 0).empty());
    assert(m.full_text_search("realword", -2).empty());
    // 上限 break：两词命中但 max_results=1
    assert(m.full_text_search("body", 1).size() == 1);
    assert(m.full_text_search("body", 10).size() == 2);
    assert(m.full_text_search("searchable", 10).size() == 1);

    // relaxed：文件不存在，out_error null 与非 null 两态
    {
        const std::string missing = (dir / "no_such_ft.bin").string();
        assert(!m.load_fulltext_index_relaxed(missing, nullptr, nullptr));
        int ver = -1; std::string err;
        assert(!m.load_fulltext_index_relaxed(missing, &ver, &err));
        assert(!err.empty());
        (void)ver;
    }
    // 真索引往返：版本回填 + out_version null 两态
    fs::path idxp = dir / "dmbr_ft.index";
    assert(m.save_fulltext_index(idxp.string()));
    int ver = 0;
    assert(m.load_fulltext_index_relaxed(idxp.string(), &ver, nullptr));
    assert(ver == 3);
    assert(m.load_fulltext_index_relaxed(idxp.string(), nullptr, nullptr));

    // crafted UDFT：dictId 越界（> dicts_.size()）
    {
        write_text("ft_oob.index", craft_udft3({{5, 0}}));
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(json_p.string()));
        assert(m2.load_fulltext_index_relaxed((dir / "ft_oob.index").string(),
                                              nullptr, nullptr));
        assert(m2.full_text_search("gw", 10).empty());
    }
    // crafted UDFT：负 dictId（0xFFFFFFFF → int -1）
    {
        write_text("ft_neg.index", craft_udft3({{0xFFFFFFFFu, 0}}));
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(json_p.string()));
        assert(m2.load_fulltext_index_relaxed((dir / "ft_neg.index").string(),
                                              nullptr, nullptr));
        assert(m2.full_text_search("gw", 10).empty());
    }
    // crafted UDFT：wordId 越界（words.size()=3，word=99）
    {
        write_text("ft_woob.index", craft_udft3({{0, 99}}));
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(json_p.string()));
        assert(m2.load_fulltext_index_relaxed((dir / "ft_woob.index").string(),
                                              nullptr, nullptr));
        assert(m2.full_text_search("gw", 10).empty());
    }
    // crafted UDFT：负 wordId
    {
        write_text("ft_wneg.index", craft_udft3({{0, 0xFFFFFFFFu}}));
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(json_p.string()));
        assert(m2.load_fulltext_index_relaxed((dir / "ft_wneg.index").string(),
                                              nullptr, nullptr));
        assert(m2.full_text_search("gw", 10).empty());
    }
    // crafted UDFT：指向空释义词（ghostword）→ 命中后不产出结果
    {
        write_text("ft_ghost.index", craft_udft3({{0, 0}}));
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(json_p.string()));
        assert(m2.load_fulltext_index_relaxed((dir / "ft_ghost.index").string(),
                                              nullptr, nullptr));
        assert(m2.full_text_search("gw", 10).empty());
    }

    // 严格加载：坏文件失败臂
    assert(!m.load_fulltext_index((dir / "no_such_ft.bin").string()));
}

// T5 签名边界：源文件缺失 "(missing)" 臂、文件变目录（is_regular_file
// 假臂）、accept_version=1 拒绝 v3（out_error null 与非 null 两态）
static void test_signature_edges() {
    fs::path dir = base_dir();
    // 源文件删除后签名仍可算（缺源 → "(missing)"）
    fs::path csv2 = write_text("sigbr.csv", "alpha,alpha body\n");
    fs::path idxp = dir / "sigbr_ft.index";
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(csv2.string()));
        assert(m.save_fulltext_index(idxp.string()));
        fs::remove(csv2);
        assert(m.save_fulltext_index(idxp.string()));
    }
    // 文件变目录：exists 真、is_regular_file 假 → size 记 0 的三目臂
    {
        fs::path csv3 = write_text("dirmorph.csv", "beta,beta body\n");
        DictionaryManagerStd m;
        assert(m.add_dictionary(csv3.string()));
        fs::remove(csv3);
        fs::create_directories(csv3);
        assert(!m.fulltext_signature().empty());
        fs::remove(csv3);
    }
    // accept_version=1 只放行 legacy v1：对 v3 索引拒绝（版本闸门臂），
    // out_error null 与非 null 两态都要走
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(write_text("av.csv", "gamma,gamma body\n").string()));
        int ver = 0; std::string err;
        assert(!m.load_fulltext_index_relaxed(idxp.string(), &ver, &err, 1));
        assert(ver == 3);   // 版本已回填
        assert(err.find("signature mismatch") != std::string::npos);
        assert(!m.load_fulltext_index_relaxed(idxp.string(), nullptr, nullptr, 1));
    }
    fs::remove(idxp);
}

int main() {
    test_holder_chain_and_failures();
    test_stardict_companions();
    test_mdx_companion_scan();
    test_fulltext_guards();
    test_signature_edges();
    std::cout << "OK\n";
    return 0;
}
