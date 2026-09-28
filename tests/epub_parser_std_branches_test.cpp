// epub_parser_std 分支缺口补测（真实缺边 16 条，第七大簇/最后一簇）。
//
// 缺边构成：decode_entities 的空实体（&;）、十六进制非码字（&#xg;）、
// 十进制非数字（&#1a;）原样保留臂；extract_attribute 的闭引号缺失臂；
// extract_element_text 的截断开标签（EOF 三目 \0 臂）、属性形态
// （<dc:title xml:lang=..> 空格臂）、无 '>' 与无闭标签两臂；heading_level
// 的 <hN 前缀误配（<h12 ...>）两臂；状态机的 BeforeHeading 态遇闭词头
// 标签臂。全部经 load_dictionary 的 zip 全链真实驱动（parse_container 等
// 为 private，畸形 XML 只能走容器输入面）。

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/epub_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

void put_u16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xff));
    out.push_back(static_cast<uint8_t>(value >> 8));
}

void put_u32(std::vector<uint8_t>& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
    }
}

uint32_t crc_of(const std::string& data) {
    return crc32(0, reinterpret_cast<const Bytef*>(data.data()),
                 static_cast<uInt>(data.size()));
}

struct ZipEntryInput {
    std::string name;
    std::string data;
};

// 手写 stored zip（本批畸形输入都无需压缩形态）
std::vector<uint8_t> build_zip(const std::vector<ZipEntryInput>& entries) {
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central;
    uint16_t count = 0;
    for (const auto& entry : entries) {
        const uint32_t crc = crc_of(entry.data);
        const uint32_t size = static_cast<uint32_t>(entry.data.size());
        const uint32_t offset = static_cast<uint32_t>(archive.size());
        put_u32(archive, 0x04034b50u);
        put_u16(archive, 20);
        put_u16(archive, 0);
        put_u16(archive, 0);  // stored
        put_u16(archive, 0);
        put_u16(archive, 0);
        put_u32(archive, crc);
        put_u32(archive, size);
        put_u32(archive, size);
        put_u16(archive, static_cast<uint16_t>(entry.name.size()));
        put_u16(archive, 0);
        archive.insert(archive.end(), entry.name.begin(), entry.name.end());
        archive.insert(archive.end(), entry.data.begin(), entry.data.end());

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
        put_u16(central, static_cast<uint16_t>(entry.name.size()));
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u16(central, 0);
        put_u32(central, 0);
        put_u32(central, offset);
        central.insert(central.end(), entry.name.begin(), entry.name.end());
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

fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "epubbr";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

// 组一个最小 epub：container + 可选 OPF + 任意附加条目
fs::path write_epub(const fs::path& dir, const std::string& name,
                    const std::string& container, const std::string& opf,
                    const std::vector<ZipEntryInput>& extras = {}) {
    std::vector<ZipEntryInput> entries = {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", container},
    };
    if (!opf.empty()) {
        entries.push_back({"OEBPS/content.opf", opf});
    }
    for (const auto& extra : extras) {
        entries.push_back(extra);
    }
    const fs::path path = dir / (name + ".epub");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    const std::vector<uint8_t> bytes = build_zip(entries);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    assert(out.good());
    return path;
}

const char* kContainer =
    "<rootfiles><rootfile full-path=\"OEBPS/content.opf\"/></rootfiles>";

// 可加载的最小 OPF + 章节（词头 real / 释义 def）
const char* kOpf =
    "<package><metadata><dc:title>T</dc:title></metadata>"
    "<manifest><item href=\"c.xhtml\" "
    "media-type=\"application/xhtml+xml\"/></manifest></package>";
const char* kChapter = "<html><body><h2>real</h2><p>def</p></body></html>";

} // namespace

// T1 decode_entities 畸形实体矩阵：空实体、十六进制非码字、十进制非数字
static void test_decode_entities_matrix() {
    fs::path dir = base_dir();
    const std::string chapter =
        "<html><body><h2>ent</h2>"
        "<p>a &; b &#xg; c &#x!; d &#1a2; e</p></body></html>";
    const fs::path path = write_epub(dir, "ent", kContainer, kOpf,
                                     {{"OEBPS/c.xhtml", chapter}});
    EpubParserStd parser;
    assert(parser.load_dictionary(path.string()));
    // 四种畸形实体都原样保留（空实体/非码字/小于 'A' 的非十六进制
    // 字符/非数字截停 → ok=false 回落）
    assert(parser.lookup("ent") ==
           "a &; b &#xg; c &#x!; d &#1a2; e");
}

// T2 extract_attribute 闭引号缺失：full-path 无闭引号 → 取值失败
static void test_attribute_unterminated_quote() {
    fs::path dir = base_dir();
    const std::string container =
        "<rootfiles><rootfile full-path=\"OEBPS/content.opf/></rootfiles>";
    const fs::path path = write_epub(dir, "noquote", container, "");
    EpubParserStd parser;
    // opf_path 取不到 → parse_container 假 → 加载拒绝
    assert(!parser.load_dictionary(path.string()));
}

// T3 extract_element_text 形态矩阵：属性空格臂、EOF 截断、无 '>'、无闭标签
static void test_element_text_matrix() {
    fs::path dir = base_dir();
    // 属性形态（next 为空格）：标题带 xml:lang
    {
        const std::string opf =
            "<package><metadata>"
            "<dc:title xml:lang=\"zh\">Zed</dc:title></metadata>"
            "<manifest><item href=\"c.xhtml\" "
            "media-type=\"application/xhtml+xml\"/></manifest></package>";
        const fs::path path =
            write_epub(dir, "lang", kContainer, opf, {{"OEBPS/c.xhtml", kChapter}});
        EpubParserStd parser;
        assert(parser.load_dictionary(path.string()));
        assert(parser.dictionary_name() == "Zed");
    }
    // 开标签恰好 EOF：三目的 '\0' 臂（越界读防护）
    {
        const std::string opf =
            "<package><manifest><item href=\"c.xhtml\" "
            "media-type=\"application/xhtml+xml\"/></manifest><dc:title";
        const fs::path path =
            write_epub(dir, "eof", kContainer, opf, {{"OEBPS/c.xhtml", kChapter}});
        EpubParserStd parser;
        assert(parser.load_dictionary(path.string()));
        assert(parser.dictionary_name() == "EPUB Dictionary"); // 回落名
    }
    // 标签后无 '>'：content_begin npos 臂
    {
        const std::string opf = "<package><metadata><dc:title x";
        const fs::path path = write_epub(dir, "nogt", kContainer, opf);
        EpubParserStd parser;
        assert(!parser.load_dictionary(path.string()));
        assert(parser.dictionary_name() == "EPUB Dictionary");
    }
    // 无闭标签：content_end npos 臂
    {
        const std::string opf = "<package><metadata><dc:title>T</dc:titl";
        const fs::path path = write_epub(dir, "noclose", kContainer, opf);
        EpubParserStd parser;
        assert(!parser.load_dictionary(path.string()));
    }
}

// T4 heading_level 前缀误配 + 状态机 BeforeHeading 态遇闭词头标签
static void test_heading_level_matrix() {
    fs::path dir = base_dir();
    // <h12 class="x"> 前缀误配 <h1：尺寸臂与非空白臂；</h1> 在
    // BeforeHeading 态（stray 闭标签）
    const std::string chapter =
        "<html><body><h12 class=\"x\">junk</h12>"
        "<p>intro</p></h1><h2>real</h2><p>def</p></body></html>";
    const fs::path path =
        write_epub(dir, "hd", kContainer, kOpf, {{"OEBPS/c.xhtml", chapter}});
    EpubParserStd parser;
    assert(parser.load_dictionary(path.string()));
    // h12 不是词头，junk/intro 未入库；stray </h1> 无副作用
    assert(parser.word_count() == 1);
    assert(parser.lookup("real") == "def");
    assert(parser.lookup("junk").empty());
}

// T5 正常链路回归（畸形矩阵不伤主路径）
static void test_baseline_still_loads() {
    fs::path dir = base_dir();
    const fs::path path =
        write_epub(dir, "base", kContainer, kOpf, {{"OEBPS/c.xhtml", kChapter}});
    EpubParserStd parser;
    assert(parser.load_dictionary(path.string()));
    assert(parser.dictionary_name() == "T");
    assert(parser.lookup("real") == "def");
}

int main() {
    test_decode_entities_matrix();
    test_attribute_unterminated_quote();
    test_element_text_matrix();
    test_heading_level_matrix();
    test_baseline_still_loads();
    std::cout << "OK\n";
    return 0;
}
