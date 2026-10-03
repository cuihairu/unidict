#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include "std/dictionary_manager_std.h"

static std::filesystem::path write_bytes(const std::string& name, const std::string& content) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary);
    out << content;
    return p;
}

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) { v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }

// MDict 词典体：SIMPLEKV 容器（键值对序列）
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

// MDict 文件 = XML 头一行 + 容器体
static void write_mdict_like_file(const std::filesystem::path& path,
                                  const std::vector<unsigned char>& body) {
    std::string header = "<Dictionary title=\"MgrMDX\" description=\"mgr mdd\"/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    assert(out.good());
}

// 最小 .ifo + .idx + .dict.dz（无 .dict），单词条 word -> definition
static std::filesystem::path write_dz_dict(const std::string& stem,
                                           const std::string& word,
                                           const std::string& definition) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr";
    fs::create_directories(dir);
    fs::path base = dir / stem;

    gzFile gz = gzopen((base.string() + ".dict.dz").c_str(), "wb");
    assert(gz != nullptr);
    gzwrite(gz, definition.data(), (unsigned)definition.size());
    gzclose(gz);

    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary);
    idx.write(word.c_str(), (std::streamsize)word.size());
    idx.put('\0');
    unsigned char off[8] = {0};
    idx.write((const char*)off, 8);
    unsigned char sz[4] = { (unsigned char)((definition.size()>>24)&0xFF),
                            (unsigned char)((definition.size()>>16)&0xFF),
                            (unsigned char)((definition.size()>>8)&0xFF),
                            (unsigned char)(definition.size()&0xFF) };
    idx.write((const char*)sz, 4);
    idx.close();

    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary);
    ifo << "bookname=DZ " << stem << "\nwordcount=1\nidxfilesize="
        << (word.size() + 1 + 12) << "\nidxoffsetbits=64\n";
    ifo.close();
    return base.string() + ".ifo";
}

// --- EPUB（manager 面）：最小 stored zip（method 0），小端手拼 ---
static void le16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF); }
static void le32w(std::vector<unsigned char>& v, uint32_t x) {
    for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 0xFF);
}

static std::filesystem::path write_stored_epub(const std::string& name) {
    namespace fs = std::filesystem;
    const std::string xhtml =
        "<html><body>"
        "<h1>alpha</h1><p>first entry</p>"
        "<h2>beta</h2><p><b>second</b> entry</p>"
        "</body></html>";
    const std::string opf =
        "<?xml version=\"1.0\"?><package><metadata>"
        "<dc:title>MgrEPUB</dc:title>"
        "<dc:description>epub for manager</dc:description>"
        "</metadata><manifest>"
        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest></package>";
    const std::string container =
        "<container><rootfiles><rootfile full-path=\"content.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
    const std::pair<const char*, const std::string*> files[] = {
        {"META-INF/container.xml", &container}, {"content.opf", &opf}, {"a.xhtml", &xhtml}};

    std::vector<unsigned char> zip, central;
    uint16_t count = 0;
    for (const auto& f : files) {
        const std::string& ename = f.first;
        const std::string& data = *f.second;
        const uint32_t crc = crc32(0, reinterpret_cast<const Bytef*>(data.data()),
                                   (uInt)data.size());
        const uint32_t sz = (uint32_t)data.size();
        const uint32_t off = (uint32_t)zip.size();
        le32w(zip, 0x04034b50u); le16w(zip, 20); le16w(zip, 0); le16w(zip, 0);
        le16w(zip, 0); le16w(zip, 0);
        le32w(zip, crc); le32w(zip, sz); le32w(zip, sz);
        le16w(zip, (uint16_t)ename.size()); le16w(zip, 0);
        zip.insert(zip.end(), ename.begin(), ename.end());
        zip.insert(zip.end(), data.begin(), data.end());

        le32w(central, 0x02014b50u); le16w(central, 20); le16w(central, 20);
        le16w(central, 0); le16w(central, 0); le16w(central, 0); le16w(central, 0);
        le32w(central, crc); le32w(central, sz); le32w(central, sz);
        le16w(central, (uint16_t)ename.size()); le16w(central, 0); le16w(central, 0);
        le16w(central, 0); le16w(central, 0); le32w(central, 0); le32w(central, off);
        central.insert(central.end(), ename.begin(), ename.end());
        ++count;
    }
    const uint32_t cd_off = (uint32_t)zip.size();
    const uint32_t cd_sz = (uint32_t)central.size();
    zip.insert(zip.end(), central.begin(), central.end());
    le32w(zip, 0x06054b50u); le16w(zip, 0); le16w(zip, 0);
    le16w(zip, count); le16w(zip, count);
    le32w(zip, cd_sz); le32w(zip, cd_off); le16w(zip, 0);

    fs::path dir = fs::current_path() / "build-local" / "dict_mgr";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write((const char*)zip.data(), (std::streamsize)zip.size());
    assert(out.good());
    return p;
}

int main() {
    UnidictCoreStd::DictionaryManagerStd mgr;
    bool ok = false;
    const char* candidates[] = {"examples/dict.json","../examples/dict.json","../../examples/dict.json"};
    for (auto p : candidates) { if (mgr.add_dictionary(p)) { ok = true; break; } }
    assert(ok);
    mgr.build_index();
    auto pref = mgr.prefix_search("he", 10);
    bool has_hello = false; for (auto& s : pref) if (s == "hello") has_hello = true;
    assert(has_hello);
    auto ex = mgr.exact_search("hello");
    assert(!ex.empty() && ex.front() == std::string("hello"));
    auto def = mgr.search_word("hello");
    assert(!def.empty());

    auto enabled = mgr.enabled_dictionaries();
    assert(enabled.size() == 1);
    const std::string dict_name = enabled.front();
    assert(!dict_name.empty());

    assert(mgr.set_dictionary_enabled(dict_name, false));
    assert(!mgr.is_dictionary_enabled(dict_name));
    assert(mgr.enabled_dictionaries().empty());
    assert(mgr.search_word("hello").empty());
    assert(mgr.search_all("hello").empty());

    assert(mgr.set_dictionary_enabled(dict_name, true));
    assert(mgr.is_dictionary_enabled(dict_name));
    assert(!mgr.search_word("hello").empty());

    // --- 不存在的词典：开关查询均回落 ---
    assert(!mgr.set_dictionary_enabled("ghost", true));
    assert(!mgr.is_dictionary_enabled("ghost"));

    // --- 多格式挂载：dsl / csv / 未知名 / .dz stardict（companion 记录）---
    auto dsl_p = write_bytes("mgr.dsl", "#NAME DSLX\n\nhello\nA greeting from DSL.\n");
    assert(mgr.add_dictionary(dsl_p.string()));
    auto csv_p = write_bytes("mgr.csv", "bonjour,hello in csv\nhello,csv greeting\n");
    assert(mgr.add_dictionary(csv_p.string()));
    assert(!mgr.add_dictionary("build-local/dict_mgr/unknown.xyz"));
    auto dz_ifo = write_dz_dict("mgrdz", "hello", "hello from dz dict");
    assert(mgr.add_dictionary(dz_ifo.string()));

    assert(mgr.loaded_dictionaries().size() == 4);

    // Holder::lookup 的 dsl / csv / stardict 分支：search_all 汇总各格式命中
    auto all = mgr.search_all("hello", /*include_disabled=*/true);
    assert(all.size() == 4);
    bool saw_dsl = false, saw_csv = false, saw_dz = false;
    for (const auto& e : all) {
        if (e.definition == "A greeting from DSL.") saw_dsl = true;
        if (e.definition == "csv greeting") saw_csv = true;
        if (e.definition == "hello from dz dict") saw_dz = true;
    }
    assert(saw_dsl && saw_csv && saw_dz);

    // dictionaries_meta：stardict / dsl / csv 的 description 分支
    auto metas = mgr.dictionaries_meta();
    assert(metas.size() == 4);
    for (const auto& m : metas) assert(m.word_count >= 1);

    // --- wildcard / regex / 全词列表 ---
    auto wc = mgr.wildcard_search("hel*", 10);
    assert(!wc.empty());
    auto rx = mgr.regex_search("he..o", 10);
    assert(!rx.empty());
    assert(mgr.all_indexed_words().size() >= 4);
    assert(mgr.indexed_word_count() >= 4);
    auto dicts_of = mgr.dictionaries_for_word("hello");
    assert(dicts_of.size() == 4);

    // --- search_word 未命中回落空串 ---
    assert(mgr.search_word("zzz_not_there").empty());

    // --- 全文索引：构建、统计、搜索 ---
    assert(mgr.full_text_search("greeting", 10).size() >= 1);
    auto stats0 = mgr.fulltext_stats();
    (void)stats0;   // ft_index_ 已由 ensure 建好：stats() 分支
    auto sig = mgr.fulltext_signature();
    assert(!sig.empty());

    // 签名随词典文件删除变化：(missing) 分支
    auto before = mgr.fulltext_signature();
    std::filesystem::remove(csv_p);
    auto after = mgr.fulltext_signature();
    assert(before != after);

    // --- 全文索引持久化：save / relaxed load（好与坏）---
    auto idx_path = write_bytes("mgr_ft.idx", "");
    assert(mgr.save_fulltext_index(idx_path.string()));
    {
        UnidictCoreStd::DictionaryManagerStd fresh;   // 空管理器
        // 1) 文件不存在：load 失败且错误信息非空
        int ver = -1; std::string err;
        assert(!fresh.load_fulltext_index_relaxed("build-local/dict_mgr/no_such_ft.bin",
                                                  &ver, &err));
        assert(!err.empty());
        // 2) relaxed 忽略签名不匹配：空管理器也能接收已存索引
        assert(fresh.load_fulltext_index_relaxed(idx_path.string(), &ver, &err));
        assert(ver >= 0);
        auto fs2 = fresh.fulltext_stats();
        assert(fs2.docs >= 4 && fs2.terms >= 1);   // 与保存时 4 本词典的文档数一致
    }

    // --- mdict 挂载：同 stem .mdd 被记入 companion（60 行）+ meta 走 mdict 描述分支 ---
    {
        namespace fs = std::filesystem;
        fs::path mdir = fs::current_path() / "build-local" / "dict_mgr";
        fs::create_directories(mdir);
        fs::path mdx = mdir / "mgrmdd.mdx";
        fs::path mdd = mdir / "mgrmdd.mdd";
        write_mdict_like_file(mdx, make_simplekv({{"hola", "<div>mdx hi</div>"}}));
        write_mdict_like_file(mdd, make_simplekv({{"pic.png", "DUMMY"}}));
        assert(mgr.add_dictionary(mdx.string()));
        // .mdd 与 .mdx 同 stem → add 时记入 src_paths；签名应体现两个源文件
        std::string sig_mdx = mgr.fulltext_signature();
        assert(!sig_mdx.empty());
        auto metas2 = mgr.dictionaries_meta();
        bool saw_mdict_desc = false;
        for (const auto& m : metas2)
            if (m.name == "MgrMDX" && m.description == "mgr mdd") saw_mdict_desc = true;
        assert(saw_mdict_desc);
        assert(mgr.search_word("hola", true) == "<div>mdx hi</div>");
        assert(mgr.remove_dictionary("MgrMDX"));
    }

    // --- epub 挂载：add_dictionary .epub 分支 → lookup/index/meta 全链 ---
    {
        auto epub_p = write_stored_epub("mgrepub.epub");
        assert(mgr.add_dictionary(epub_p.string()));
        mgr.build_index(); // prefix 走 trie，装载后重建（与 CLI 同用法）
        // Holder::lookup 的 epub 链 + epub 键表大小写折叠
        assert(mgr.search_word("alpha") == "first entry");
        auto all_epub = mgr.search_all("BETA");
        assert(all_epub.size() == 1 && all_epub.front().dict_name == "MgrEPUB" &&
               all_epub.front().definition == "second entry");
        // 索引面：exact / prefix / 词典归属
        assert(!mgr.exact_search("alpha").empty() && mgr.exact_search("alpha").front() == "alpha");
        assert(!mgr.prefix_search("bet", 10).empty());
        assert(mgr.dictionaries_for_word("alpha").size() == 1 &&
               mgr.dictionaries_for_word("alpha").front() == "MgrEPUB");
        // meta：dc:title 名称 + dc:description 描述 + 词条数
        bool saw_epub_meta = false;
        for (const auto& m : mgr.dictionaries_meta())
            if (m.name == "MgrEPUB")
                saw_epub_meta = m.description == "epub for manager" && m.word_count == 2;
        assert(saw_epub_meta);
        // 释义全文（ensure_fulltext_index_built 经 d.lookup 统一分发）
        assert(mgr.full_text_search("second", 10).size() >= 1);
        assert(mgr.remove_dictionary("MgrEPUB"));
    }

    // --- remove_dictionary：移除并重建索引 ---
    assert(mgr.remove_dictionary("DSLX"));
    assert(mgr.loaded_dictionaries().size() == 3);
    assert(!mgr.remove_dictionary("ghost"));
    mgr.clear_dictionaries();
    assert(mgr.loaded_dictionaries().empty());
    return 0;
}
