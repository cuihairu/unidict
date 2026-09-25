// EpubParserStd 边界补测：整条 decode_entities、容器/OPF 解析的异常形态、
// heading 判定、词条状态机的畸形输入。
//
// 这个文件之前只有 test_epub_zip_std 一个 target，epub_parser_std.cpp 的
// 行覆盖只有 80%——HTML 实体解码（`&lt;`/`&#65;`/`&#xZZ;` 一整族）完全
// 没测，容器解析的失败形态也基本没测。词典类 epub 的排版千奇百怪，这些
// 恰恰是最容易在真实文件上炸的地方。
//
// zip 全部用 stored(未压缩) 手写，顺带覆盖 ZipReaderStd 的 stored 分支。

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/epub_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------- zip 写入

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

struct Entry {
    std::string name;
    std::string data;
};

// 手写 stored zip（method 0），无压缩
std::vector<uint8_t> build_zip(const std::vector<Entry>& entries) {
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central;
    uint16_t count = 0;
    for (const auto& e : entries) {
        const uint32_t crc = crc32(0, reinterpret_cast<const Bytef*>(e.data.data()),
                                   static_cast<uInt>(e.data.size()));
        const uint32_t size = static_cast<uint32_t>(e.data.size());
        const uint32_t local_offset = static_cast<uint32_t>(archive.size());

        put_u32(archive, 0x04034b50u);
        put_u16(archive, 20);
        put_u16(archive, 0);
        put_u16(archive, 0);  // method = stored
        put_u16(archive, 0);
        put_u16(archive, 0);
        put_u32(archive, crc);
        put_u32(archive, size);
        put_u32(archive, size);
        put_u16(archive, static_cast<uint16_t>(e.name.size()));
        put_u16(archive, 0);
        put_str(archive, e.name);
        put_str(archive, e.data);

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
        put_u16(central, static_cast<uint16_t>(e.name.size()));
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u32(central, 0);
        put_u32(central, local_offset);
        put_str(central, e.name);
        ++count;
    }
    const uint32_t cd_offset = static_cast<uint32_t>(archive.size());
    const uint32_t cd_size = static_cast<uint32_t>(central.size());
    archive.insert(archive.end(), central.begin(), central.end());
    put_u32(archive, 0x06054b50u);
    put_u16(archive, 0);
    put_u16(archive, 0);
    put_u16(archive, count);
    put_u16(archive, count);
    put_u32(archive, cd_size);
    put_u32(archive, cd_offset);
    put_u16(archive, 0);
    return archive;
}

fs::path tmp_root() {
    static const fs::path dir = [] {
        const char* t = std::getenv("TMPDIR");
        fs::path d = fs::path(t && *t ? t : "/tmp") / "unidict_epub_cover";
        fs::create_directories(d);
        return d;
    }();
    return dir;
}

void write_epub(const std::string& file, const std::vector<Entry>& entries) {
    const auto bytes = build_zip(entries);
    std::ofstream out(tmp_root() / file, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
}

// 常规容器 + OPF（opf_dir 可指定；空串表示 OPF 放在 zip 根）
std::string container_xml(const std::string& full_path) {
    return "<container><rootfiles><rootfile full-path=\"" + full_path +
           "\" media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
}

std::string opf_xml(const std::string& title, const std::string& items) {
    return std::string("<?xml version=\"1.0\"?><package><metadata>") +
           "<dc:title>" + title + "</dc:title></metadata><manifest>" + items +
           "</manifest></package>";
}

// ---------------------------------------------------------------- 实体解码

// decode_entities 是匿名命名空间里的自由函数，只能经 <hN> 词头 / <dc:title>
// 两条调用路径观察。词头最省事：<h1>ENTITY_a&lt;b</h1> → lookup 拿释义，
// all_words() 拿原词形。
std::string headword_via_epub(const std::string& file, const std::string& raw_headword) {
    write_epub(file, {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>" + raw_headword + "</h1><p>x</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / file).string()));
    const auto words = p.all_words();
    assert(words.size() == 1);
    return words[0];
}

void test_named_entities() {
    assert(headword_via_epub("ent_named.epub", "A&amp;B") == "A&B");
    assert(headword_via_epub("ent_lt.epub", "&lt;tag&gt;") == "<tag>");
    assert(headword_via_epub("ent_quot.epub", "&quot;quoted&quot;") == "\"quoted\"");
    assert(headword_via_epub("ent_apos.epub", "&apos;apos&apos;") == "'apos'");
    // &nbsp; 解成普通空格，随后被 append_folded 折叠 → 首尾空白被 trim 掉
    assert(headword_via_epub("ent_nbsp.epub", "&nbsp;spaced&nbsp;") == "spaced");
}

void test_numeric_entities() {
    // 十进制 ASCII
    assert(headword_via_epub("ent_dec.epub", "&#65;&#66;&#67;") == "ABC");
    // 十六进制：小写 x + 小写 hex
    assert(headword_via_epub("ent_hex_lower.epub", "&#x41;&#x42;") == "AB");
    // 十六进制：大写 X + 大写 hex
    assert(headword_via_epub("ent_hex_upper.epub", "&#X43;&#X44;") == "CD");
    // 混合大小写 hex 数字（b/B/a-f/A-F 都要认），且码位 < 128
    assert(headword_via_epub("ent_hex_mixed.epub", "&#x4b;&#X4B;&#x3e;") == "KK>");
    // 十进制里的非数字 → 整个实体原样保留
    assert(headword_via_epub("ent_dec_bad.epub", "&#1a;") == "&#1a;");
    // 十六进制里的非 hex → 原样保留
    assert(headword_via_epub("ent_hex_bad.epub", "&#xZZ;") == "&#xZZ;");
}

void test_numeric_entities_out_of_ascii_range_preserved() {
    // code >= 128 是多字节码位，实现只解 ASCII，其余原样保留（不瞎造字节）
    assert(headword_via_epub("ent_emoji.epub", "&#x1F600;") == "&#x1F600;");
    assert(headword_via_epub("ent_cjk.epub", "&#28450;") == "&#28450;");
    // 十六进制写法的非 ASCII 码位同样保留（0xAF = 175）
    assert(headword_via_epub("ent_hex_nonascii.epub", "&#xaF;") == "&#xaF;");
    // code == 0 也不解
    assert(headword_via_epub("ent_zero.epub", "&#0;") == "&#0;");
    // 边界：127 解、128 不解
    assert(headword_via_epub("ent_127.epub", "&#127;") == "\x7F");
    assert(headword_via_epub("ent_128.epub", "&#128;") == "&#128;");
}

void test_unknown_and_malformed_entities_preserved() {
    // 未收录的命名实体原样保留
    assert(headword_via_epub("ent_unknown.epub", "&eacute;x") == "&eacute;x");
    // 只有 '&' 没有 ';' → 当普通字符
    assert(headword_via_epub("ent_bare_amp.epub", "a&amp") == "a&amp");
    // '&' 到 ';' 之间超过 10 字节 → 放弃解析（防止超长扫描）
    std::string long_entity = "&" + std::string(20, 'a') + ";";
    assert(headword_via_epub("ent_long.epub", long_entity) == long_entity);
    // 空实体 "&#;" → entity 是 "#"，走 else 原样保留
    assert(headword_via_epub("ent_empty_num.epub", "&#;") == "&#;");
}

void test_text_without_ampersand_fast_path() {
    // 没有 '&' 的文本走快路径直接返回（不进入逐字符循环）
    assert(headword_via_epub("ent_plain.epub", "plainword") == "plainword");
}

void test_entities_in_definition_too() {
    // 释义侧的 decode_entities 路径（与词头侧是同一个函数、不同调用点）
    write_epub("ent_def.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>word</h1><p>a &amp; b &lt;tag&gt; &#65;</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "ent_def.epub").string()));
    assert(p.lookup("word") == "a & b <tag> A");
}

// ---------------------------------------------------------------- 容器解析

void test_opf_at_zip_root() {
    // OPF 路径里没有 '/' → opf_dir 为空，manifest 的相对 href 原样使用
    write_epub("root_opf.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("RootTitle", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>alpha</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "root_opf.epub").string()));
    assert(p.dictionary_name() == "RootTitle");
    assert(p.lookup("alpha") == "def");
}

void test_href_with_dot_slash_prefix() {
    // href="./a.xhtml" 要剥掉 "./" 前缀，且按 opf_dir 补全
    write_epub("dot_slash.epub", {
        {"META-INF/container.xml", container_xml("OEBPS/content.opf")},
        {"OEBPS/content.opf", opf_xml("DS", "<item href=\"./a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"OEBPS/a.xhtml", "<h1>beta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "dot_slash.epub").string()));
    assert(p.lookup("beta") == "def");
}

void test_absolute_href_not_double_prefixed() {
    // href 以 '/' 开头 → 不补 opf_dir（实现里注释写的意图：这种 href 已是
    // 绝对路径，再补目录就成了 "OEBPS//a.xhtml"）。
    // 注意：EPUB 规范里 manifest 的 href 总是相对 OPF 目录的，带前导 '/'
    // 不是合法形态；这里用一个条目名真的带前导 '/' 的 zip 来验证该分支
    // 本身不崩、不重复补目录。
    write_epub("abs_href.epub", {
        {"META-INF/container.xml", container_xml("OEBPS/content.opf")},
        {"OEBPS/content.opf", opf_xml("Abs", "<item href=\"/a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"/a.xhtml", "<h1>gamma</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "abs_href.epub").string()));
    assert(p.lookup("gamma") == "def");
}

void test_absolute_href_against_normal_entry_does_not_resolve() {
    // 同一形态但 zip 条目名不带前导 '/' → 读不到，静默跳过；
    // 整本提不出词条 → 加载失败。钉住"前导斜杠不做归一化"这一现状，
    // 免得以后有人以为它会自动去掉斜杠。
    write_epub("abs_href_miss.epub", {
        {"META-INF/container.xml", container_xml("OEBPS/content.opf")},
        {"OEBPS/content.opf", opf_xml("Abs", "<item href=\"/a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>gamma</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "abs_href_miss.epub").string()));
}

void test_missing_title_falls_back() {
    // dc:title 为空 → 回退到 "EPUB Dictionary"
    write_epub("no_title.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>delta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "no_title.epub").string()));
    assert(p.dictionary_name() == "EPUB Dictionary");
}

void test_container_without_rootfile_fails() {
    write_epub("no_rootfile.epub", {
        {"META-INF/container.xml", "<container><rootfiles></rootfiles></container>"},
        {"content.opf", opf_xml("T", "")},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "no_rootfile.epub").string()));
    assert(!p.is_loaded());
}

void test_container_rootfile_without_full_path_fails() {
    // 有 <rootfile> 标签但没有 full-path 属性 → parse_container 继续找下一个，
    // 找不到就返回 false
    write_epub("rootfile_no_attr.epub", {
        {"META-INF/container.xml",
         "<container><rootfiles><rootfile media-type=\"x\"/></rootfiles></container>"},
        {"content.opf", opf_xml("T", "")},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "rootfile_no_attr.epub").string()));
}

void test_container_unterminated_rootfile_tag_fails() {
    // <rootfile 后面没有 '>' → element_end == npos → 提前返回 false
    write_epub("rootfile_unterminated.epub", {
        {"META-INF/container.xml", "<container><rootfiles><rootfile"},
        {"content.opf", opf_xml("T", "")},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "rootfile_unterminated.epub").string()));
}

void test_container_second_rootfile_used() {
    // 第一个 <rootfile> 没有 full-path，第二个才有 → 循环必须走到第二个
    write_epub("rootfile_second.epub", {
        {"META-INF/container.xml",
         "<container><rootfiles>"
         "<rootfile media-type=\"application/oebps-package+xml\"/>"
         "<rootfile full-path=\"content.opf\"/>"
         "</rootfiles></container>"},
        {"content.opf", opf_xml("Second", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>epsilon</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "rootfile_second.epub").string()));
    assert(p.lookup("epsilon") == "def");
}

void test_opf_item_unterminated_tag() {
    // <item 后面整篇都没有 '>' → collect_opf_documents break，收集到 0 个
    // 文档 → 一条词条都提不出 → 加载失败
    write_epub("opf_item_unterminated.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata><dc:title>T</dc:title></metadata>"
                        "<manifest><item href=\"a.xhtml\""},
        {"a.xhtml", "<h1>zeta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "opf_item_unterminated.epub").string()));
}

void test_opf_missing_href_attribute() {
    // manifest item 没有 href → 跳过（不产生空路径条目）
    write_epub("opf_no_href.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item media-type=\"application/xhtml+xml\"/>"
                                "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>eta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "opf_no_href.epub").string()));
    assert(p.lookup("eta") == "def");
}

void test_manifest_listed_document_absent_from_zip() {
    // manifest 里有 b.xhtml，zip 里没有 → read_entry 失败，静默跳过
    write_epub("missing_doc.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"missing.xhtml\" media-type=\"application/xhtml+xml\"/>"
                                "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>theta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "missing_doc.epub").string()));
    assert(p.lookup("theta") == "def");
    assert(p.lookup("missing").empty());
}

void test_non_xhtml_manifest_items_skipped() {
    // 只认带 "xhtml" 的 item；css/png 之类忽略
    write_epub("non_xhtml.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"style.css\" media-type=\"text/css\"/>"
                                "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"style.css", "body{}"},
        {"a.xhtml", "<h1>iota</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "non_xhtml.epub").string()));
    assert(p.word_count() == 1);
}

void test_opf_path_missing_from_zip() {
    // container 指到一个不存在的 OPF → read_entry 失败
    write_epub("missing_opf.epub", {
        {"META-INF/container.xml", container_xml("nope.opf")},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "missing_opf.epub").string()));
}

void test_load_missing_file() {
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "definitely_absent.epub").string()));
    assert(!p.is_loaded());
    assert(p.dictionary_name().empty());
    assert(p.lookup("anything").empty());
    assert(p.all_words().empty());
    assert(p.word_count() == 0);
}

// ---------------------------------------------------------------- 元数据

void test_dc_title_with_nested_tags_and_entities() {
    // extract_element_text 要跳过标签内嵌的标签，并解码实体
    write_epub("title_nested.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata>"
                        "<dc:title>  My <em>Great</em> &amp; Bold Book  </dc:title>"
                        "<dc:description>A &lt;short&gt; description</dc:description>"
                        "</metadata><manifest>"
                        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                        "</manifest></package>"},
        {"a.xhtml", "<h1>kappa</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "title_nested.epub").string()));
    // 标签内容被剥掉，首尾空白被 trim
    assert(p.dictionary_name() == "My Great & Bold Book");
    assert(p.dictionary_description() == "A <short> description");
}

void test_missing_dc_elements_yield_empty_strings() {
    write_epub("no_dc.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata></metadata><manifest>"
                        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                        "</manifest></package>"},
        {"a.xhtml", "<h1>lambda</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "no_dc.epub").string()));
    assert(p.dictionary_name() == "EPUB Dictionary");
    assert(p.dictionary_description().empty());
}

// ---------------------------------------------------------------- heading 判定

void test_heading_with_attributes() {
    // <h2 id="x" class="y"> 也算 heading 开标签
    write_epub("heading_attr.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h2 id=\"one\">mu</h2><p>first</p>"
                    "<h3 class=\"x\">nu</h3><p>second</p>"
                    "<h6>xi</h6><p>third</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "heading_attr.epub").string()));
    assert(p.word_count() == 3);
    assert(p.lookup("mu") == "first");
    assert(p.lookup("nu") == "second");
    assert(p.lookup("xi") == "third");
}

void test_heading_case_insensitive_and_non_headings_ignored() {
    // <H1> 大写也要认；<hr>/<header> 不是 heading（h 开头但不是 h1-h6）
    write_epub("heading_case.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<H1>omicron</H1><hr/><header>not a heading</header><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "heading_case.epub").string()));
    assert(p.word_count() == 1);
    assert(p.lookup("omicron").find("not a heading") != std::string::npos);
}

// ---------------------------------------------------------------- 词条状态机

void test_head_stripped_when_present() {
    // <head> 段（含 title）不参与切词
    write_epub("head_strip.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<html><head><title>PageTitle</title><style>p{}</style></head>"
                    "<body><h1>pi</h1><p>def</p></body></html>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "head_strip.epub").string()));
    assert(p.word_count() == 1);
    assert(p.lookup("pi") == "def");
    assert(p.lookup("PageTitle").empty());
}

void test_missing_head_end_keeps_head_content() {
    // 有 <head> 但没有 </head> → 不裁剪，head 里的内容照样参与切词。
    // 可观测差异：head 里的 <h1> 也会被当成词头（若 head 被正常裁剪，
    // inhead 就不存在）。
    write_epub("head_unterminated.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<head><title>T</title><h1>inhead</h1><p>head def</p>"
                    "<h1>rho</h1><p>body def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "head_unterminated.epub").string()));
    assert(p.lookup("inhead") == "head def");
    assert(p.lookup("rho") == "body def");
    // <title> 不是 heading，且在首个词头之前，不产生词条
    assert(p.lookup("T").empty());
    assert(p.word_count() == 2);
}

void test_unterminated_tag_at_document_end() {
    // 文档以 '<' 结尾且没有 '>' → 状态机 break，不崩
    write_epub("unterminated_tag.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>sigma</h1><p>def</p><"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "unterminated_tag.epub").string()));
    assert(p.lookup("sigma") == "def");
}

void test_duplicate_headword_last_wins() {
    // 同词出现两次：释义后写覆盖，all_words() 只保留首次原词形
    write_epub("dup_head.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>Tau</h1><p>first</p><h1>tau</h1><p>second</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "dup_head.epub").string()));
    assert(p.word_count() == 1);
    assert(p.lookup("TAU") == "second");  // 大小写不敏感查询
    assert(p.all_words().size() == 1);
    assert(p.all_words()[0] == "Tau");  // 首次出现的原词形
}

void test_document_with_no_heading_yields_nothing() {
    // 整篇没有 h1-h6 → 提不出词条 → 加载失败
    write_epub("no_heading.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<p>just a paragraph</p><div>and another</div>"},
    });
    EpubParserStd p;
    assert(!p.load_dictionary((tmp_root() / "no_heading.epub").string()));
}

void test_empty_heading_not_indexed() {
    // <h1></h1> 词头为空 → 跳过，不产生空词条
    write_epub("empty_heading.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>   </h1><p>orphan definition</p><h1>upsilon</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "empty_heading.epub").string()));
    assert(p.word_count() == 1);
    assert(p.lookup("upsilon") == "def");
    assert(p.all_words().size() == 1);
}

void test_definition_block_boundaries_folded() {
    // <p>/<div>/<br> 的块级边界要折成单个空格，不能出双空格
    write_epub("block_fold.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>phi</h1><p>one</p><p>two</p><div>three</div><br/>four"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "block_fold.epub").string()));
    const std::string def = p.lookup("phi");
    assert(def == "one two three four");
    assert(def.find("  ") == std::string::npos);
}

void test_second_dc_title_used_when_first_malformed() {
    // extract_element_text 的循环推进：第一个 <dc:title/> 自闭合，
    // 开标签后紧跟 '/'（既不是 '>' 也不是空白）→ 该次匹配作废，
    // 循环必须 pos = find(open, ...) 往后走，取到第二个。
    write_epub("second_title.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata>"
                        "<dc:title/>"
                        "<dc:title>GoodTitle</dc:title>"
                        "</metadata><manifest>"
                        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                        "</manifest></package>"},
        {"a.xhtml", "<h1>omega</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "second_title.epub").string()));
    assert(p.dictionary_name() == "GoodTitle");
    assert(p.lookup("omega") == "def");
}

void test_first_dc_title_wins_when_both_usable() {
    // 对照：两个形态都正常的 <dc:title> → 取第一个，循环不推进
    write_epub("first_title.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata>"
                        "<dc:title>FirstTitle</dc:title>"
                        "<dc:title>SecondTitle</dc:title>"
                        "</metadata><manifest>"
                        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                        "</manifest></package>"},
        {"a.xhtml", "<h1>omega</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "first_title.epub").string()));
    assert(p.dictionary_name() == "FirstTitle");
}

void test_dc_title_with_self_closing_only_falls_back() {
    // 只有一个自闭合的 <dc:title/> → 循环走完返回空 → 触发 "EPUB Dictionary"
    write_epub("selfclosing_title.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", "<package><metadata><dc:title/></metadata><manifest>"
                        "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                        "</manifest></package>"},
        {"a.xhtml", "<h1>omega</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "selfclosing_title.epub").string()));
    assert(p.dictionary_name() == "EPUB Dictionary");
}

void test_multiple_documents_merged() {
    // 多个 XHTML 文档的词条合并进同一索引
    write_epub("multi_doc.epub", {
        {"META-INF/container.xml", container_xml("OEBPS/content.opf")},
        {"OEBPS/content.opf", opf_xml("Multi",
                                      "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
                                      "<item href=\"b.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"OEBPS/a.xhtml", "<h1>chi</h1><p>from a</p>"},
        {"OEBPS/b.xhtml", "<h1>psi</h1><p>from b</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "multi_doc.epub").string()));
    assert(p.word_count() == 2);
    assert(p.lookup("chi") == "from a");
    assert(p.lookup("psi") == "from b");
}

// ---------------------------------------------------------------- 查询

void test_find_similar_truncation_and_zero_limit() {
    write_epub("similar.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>omega</h1><p>a</p><h1>omicron</h1><p>b</p>"
                    "<h1>omicron2</h1><p>c</p><h1>omicron3</h1><p>d</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "similar.epub").string()));

    // 前缀匹配
    assert(p.find_similar("omi", 10).size() == 3);
    // max_results 截断
    assert(p.find_similar("omi", 2).size() == 2);
    // max_results <= 0 直接返回空
    assert(p.find_similar("omi", 0).empty());
    assert(p.find_similar("omi", -5).empty());
    // 大小写不敏感 + 首尾空白
    assert(p.find_similar("  OMI  ", 10).size() == 3);
    // 无匹配前缀
    assert(p.find_similar("zzz", 10).empty());
    // 空前缀匹配全部
    assert(p.find_similar("", 10).size() == 4);
}

void test_lookup_trims_and_lowercases() {
    write_epub("lookup_norm.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("T", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>Alpha</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "lookup_norm.epub").string()));
    assert(p.lookup("alpha") == "def");
    assert(p.lookup("ALPHA") == "def");
    assert(p.lookup("  Alpha  ") == "def");
    assert(p.lookup("nope").empty());
    // 释义为空也是合法词条（返回空串而非 miss，两者无法区分——钉住现状）
    assert(p.lookup("alpha").empty() == false);
}

void test_reload_clears_previous_entries() {
    const fs::path a = tmp_root() / "reload_a.epub";
    write_epub("reload_a.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("A", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>wordA</h1><p>defA</p>"},
    });
    write_epub("reload_b.epub", {
        {"META-INF/container.xml", container_xml("content.opf")},
        {"content.opf", opf_xml("B", "<item href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>wordB</h1><p>defB</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary(a.string()));
    assert(p.lookup("wordA") == "defA");
    assert(p.dictionary_name() == "A");

    assert(p.load_dictionary((tmp_root() / "reload_b.epub").string()));
    assert(p.word_count() == 1);
    assert(p.lookup("wordA").empty());  // 旧词条被清掉
    assert(p.lookup("wordB") == "defB");
    assert(p.dictionary_name() == "B");
    assert(p.dictionary_description().empty());
}

// ---------------------------------------------------------------- 属性提取

void test_extract_attribute_does_not_match_suffixed_names() {
    // data-href="x" 不应被当成 href="x" 命中（前一个字符不是空白/边界）
    write_epub("attr_suffix.epub", {
        {"META-INF/container.xml", "<container><rootfiles>"
                                    "<rootfile data-full-path=\"wrong.opf\" full-path=\"content.opf\"/>"
                                    "</rootfiles></container>"},
        {"content.opf", opf_xml("Attr", "<item data-href=\"nope.xhtml\" href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>")},
        {"a.xhtml", "<h1>alpha</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "attr_suffix.epub").string()));
    // 命中的是真正的 full-path / href
    assert(p.dictionary_name() == "Attr");
    assert(p.lookup("alpha") == "def");
}

void test_single_quoted_attribute() {
    // attr='value' 单引号形态也要认
    write_epub("attr_single_quote.epub", {
        {"META-INF/container.xml", "<container><rootfiles>"
                                    "<rootfile full-path='content.opf'/>"
                                    "</rootfiles></container>"},
        {"content.opf", opf_xml("SQ", "<item href='a.xhtml' media-type='application/xhtml+xml'/>")},
        {"a.xhtml", "<h1>beta</h1><p>def</p>"},
    });
    EpubParserStd p;
    assert(p.load_dictionary((tmp_root() / "attr_single_quote.epub").string()));
    assert(p.lookup("beta") == "def");
}

}  // namespace

int main() {
    test_named_entities();
    test_numeric_entities();
    test_numeric_entities_out_of_ascii_range_preserved();
    test_unknown_and_malformed_entities_preserved();
    test_text_without_ampersand_fast_path();
    test_entities_in_definition_too();

    test_opf_at_zip_root();
    test_href_with_dot_slash_prefix();
    test_absolute_href_not_double_prefixed();
    test_absolute_href_against_normal_entry_does_not_resolve();
    test_missing_title_falls_back();
    test_container_without_rootfile_fails();
    test_container_rootfile_without_full_path_fails();
    test_container_unterminated_rootfile_tag_fails();
    test_container_second_rootfile_used();
    test_opf_item_unterminated_tag();
    test_opf_missing_href_attribute();
    test_manifest_listed_document_absent_from_zip();
    test_non_xhtml_manifest_items_skipped();
    test_opf_path_missing_from_zip();
    test_load_missing_file();

    test_dc_title_with_nested_tags_and_entities();
    test_missing_dc_elements_yield_empty_strings();

    test_heading_with_attributes();
    test_heading_case_insensitive_and_non_headings_ignored();

    test_head_stripped_when_present();
    test_missing_head_end_keeps_head_content();
    test_unterminated_tag_at_document_end();
    test_duplicate_headword_last_wins();
    test_document_with_no_heading_yields_nothing();
    test_empty_heading_not_indexed();
    test_definition_block_boundaries_folded();
    test_second_dc_title_used_when_first_malformed();
    test_first_dc_title_wins_when_both_usable();
    test_dc_title_with_self_closing_only_falls_back();
    test_multiple_documents_merged();

    test_find_similar_truncation_and_zero_limit();
    test_lookup_trims_and_lowercases();
    test_reload_clears_previous_entries();

    test_extract_attribute_does_not_match_suffixed_names();
    test_single_quoted_attribute();

    std::printf("OK\n");
    return 0;
}
