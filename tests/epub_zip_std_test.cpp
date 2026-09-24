// ZipReaderStd 往返/防御 + EpubParserStd 全链：一次性 zip 写入 helper 只活在测试里
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <string>
#include <vector>
#include <zlib.h>

#include "std/epub_parser_std.h"
#include "std/zip_reader_std.h"

using namespace UnidictCoreStd;

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

void put_str(std::vector<uint8_t>& out, const std::string& text) {
    out.insert(out.end(), text.begin(), text.end());
}

uint32_t crc_of(const std::string& data) {
    return crc32(0, reinterpret_cast<const Bytef*>(data.data()),
                 static_cast<uInt>(data.size()));
}

// raw deflate（zip method 8 无 zlib 头）
bool deflate_raw(const std::string& input, std::string& output) {
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8,
                     Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    std::vector<char> buffer(deflateBound(&stream, static_cast<uLong>(input.size())));
    stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
    stream.avail_out = static_cast<uInt>(buffer.size());
    const int ret = deflate(&stream, Z_FINISH);
    deflateEnd(&stream);
    if (ret != Z_STREAM_END) {
        return false;
    }
    output.assign(buffer.data(), buffer.size() - stream.avail_out);
    return true;
}

struct ZipEntryInput {
    std::string name;
    std::string data;
    bool deflate = false;
};

// 手写 zip：EOCD 前 central directory，各条目 local header + 数据
std::vector<uint8_t> build_zip(const std::vector<ZipEntryInput>& entries,
                               bool lie_about_uncompressed_size = false) {
    std::vector<uint8_t> archive;
    std::vector<uint8_t> central;
    uint16_t count = 0;

    for (const auto& entry : entries) {
        std::string payload = entry.data;
        uint16_t method = 0;
        if (entry.deflate) {
            assert(deflate_raw(entry.data, payload));
            method = 8;
        }
        const uint32_t crc = crc_of(entry.data);
        const uint32_t stored_size = static_cast<uint32_t>(payload.size());
        const uint32_t declared_size =
            lie_about_uncompressed_size ? stored_size + 1 : static_cast<uint32_t>(entry.data.size());

        const uint32_t local_offset = static_cast<uint32_t>(archive.size());
        put_u32(archive, 0x04034b50u);
        put_u16(archive, 20);          // version needed
        put_u16(archive, 0);           // flags
        put_u16(archive, method);
        put_u16(archive, 0);           // time
        put_u16(archive, 0);           // date
        put_u32(archive, crc);
        put_u32(archive, stored_size);
        put_u32(archive, declared_size);
        put_u16(archive, static_cast<uint16_t>(entry.name.size()));
        put_u16(archive, 0);           // extra length
        put_str(archive, entry.name);
        put_str(archive, payload);

        put_u32(central, 0x02014b50u);
        put_u16(central, 20);          // version made by
        put_u16(central, 20);          // version needed
        put_u16(central, 0);           // flags
        put_u16(central, method);
        put_u16(central, 0);           // time
        put_u16(central, 0);           // date
        put_u32(central, crc);
        put_u32(central, stored_size);
        put_u32(central, declared_size);
        put_u16(central, static_cast<uint16_t>(entry.name.size()));
        put_u16(central, 0);           // extra
        put_u16(central, 0);           // comment
        put_u16(central, 0);           // disk number
        put_u16(central, 0);           // internal attrs
        put_u32(central, 0);           // external attrs
        put_u32(central, local_offset);
        put_str(central, entry.name);
        ++count;
    }

    const uint32_t cd_offset = static_cast<uint32_t>(archive.size());
    const uint32_t cd_size = static_cast<uint32_t>(central.size());
    archive.insert(archive.end(), central.begin(), central.end());
    put_u32(archive, 0x06054b50u);
    put_u16(archive, 0);   // disk
    put_u16(archive, 0);   // cd disk
    put_u16(archive, count);
    put_u16(archive, count);
    put_u32(archive, cd_size);
    put_u32(archive, cd_offset);
    put_u16(archive, 0);   // comment length
    return archive;
}

bool write_bytes(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

bool write_file(const std::filesystem::path& path, const std::string& text) {
    std::vector<uint8_t> bytes(text.begin(), text.end());
    return write_bytes(path, bytes);
}

std::filesystem::path make_sample_epub(const std::filesystem::path& dir, bool without_headings) {
    const std::string container =
        "<?xml version=\"1.0\"?>"
        "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
        "<rootfiles><rootfile full-path=\"OEBPS/content.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles></container>";
    const std::string opf =
        "<?xml version=\"1.0\"?>"
        "<package xmlns=\"http://www.idpf.org/2007/opf\" version=\"3.0\">"
        "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
        "<dc:title>Mini Epub Dict</dc:title>"
        "<dc:description>test only</dc:description>"
        "</metadata>"
        "<manifest>"
        "<item id=\"ch1\" href=\"a.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "<item id=\"nav\" href=\"nav.xhtml\" media-type=\"application/xhtml+xml\"/>"
        "</manifest></package>";
    const std::string chapter_a =
        without_headings
            ? "<html><head><title>A</title></head><body><p>no headings here</p></body></html>"
            : "<html><head><title>A</title></head><body>"
              "<h2><b>Apple</b></h2><p>a fruit &amp; a company</p><p>second line</p>"
              "<h3>banana</h3>yellow curved fruit<br/>tropical"
              "</body></html>";
    const std::string nav =
        without_headings
            ? "<html><body><nav><li>a.xhtml</li></nav></body></html>"
            : "<html><body><nav><h1>Contents</h1><li>a.xhtml</li></nav></body></html>";

    const std::filesystem::path path = dir / (without_headings ? "plain.epub" : "mini.epub");
    const std::vector<uint8_t> archive = build_zip({
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", container},
        {"OEBPS/content.opf", opf},
        {"OEBPS/a.xhtml", chapter_a},
        {"OEBPS/nav.xhtml", nav},
        {"OEBPS/styles.css", "h2 { color: red }", true}, // deflate 条目混排
    });
    assert(write_bytes(path, archive));
    return path;
}

} // namespace

int main() {
    namespace fs = std::filesystem;
    const fs::path dir = fs::current_path() / "build-local" / "epub_zip_sample";
    fs::create_directories(dir);

    // ---------- zip 往返 ----------
    {
        const fs::path path = dir / "roundtrip.zip";
        assert(write_bytes(path, build_zip({
            {"a.txt", "stored content"},
            {"b.txt", "deflated content that is long enough to compress a bit", true},
        })));

        ZipReaderStd zip;
        assert(zip.open(path.string()));
        assert(zip.is_open());

        const auto names = zip.entry_names();
        assert(names.size() == 2);
        assert(std::find(names.begin(), names.end(), "a.txt") != names.end());
        assert(std::find(names.begin(), names.end(), "b.txt") != names.end());

        std::string out;
        assert(zip.read_entry("a.txt", out));
        assert(out == "stored content");
        assert(zip.read_entry("b.txt", out));
        assert(out == "deflated content that is long enough to compress a bit");
        assert(!zip.read_entry("missing.txt", out));
    }

    // ---------- 畸形归档拒绝 ----------
    {
        // 非 zip 文件
        const fs::path not_zip = dir / "not.zip";
        assert(write_file(not_zip, "plain text, not an archive"));
        ZipReaderStd zip;
        assert(!zip.open(not_zip.string()));

        const fs::path good = dir / "good.zip";
        assert(write_bytes(good, build_zip({{"x.txt", "hello"}})));
        std::vector<uint8_t> all(1024 * 1024);
        {
            std::ifstream in(good, std::ios::binary);
            in.read(reinterpret_cast<char*>(all.data()), static_cast<std::streamsize>(all.size()));
            all.resize(static_cast<size_t>(in.gcount()));
        }

        // 截断（EOCD 没了）
        {
            std::vector<uint8_t> truncated(all.begin(), all.end() - 10);
            const fs::path path = dir / "truncated.zip";
            assert(write_bytes(path, truncated));
            ZipReaderStd broken;
            assert(!broken.open(path.string()));
        }

        // central directory 起点越界（EOCD +16 的 cd_offset 置 0xff）
        {
            std::vector<uint8_t> tampered = all;
            const size_t eocd = tampered.size() - 22;
            tampered[eocd + 16] = 0xff;
            const fs::path path = dir / "bad_offset.zip";
            assert(write_bytes(path, tampered));
            ZipReaderStd broken;
            assert(!broken.open(path.string()));
        }

        // 不支持的压缩方法（12 = bzip2）——open() 扫的是 central directory
        // 的 method 字段，不是 local header
        {
            std::vector<uint8_t> tampered = all;
            // 布局：LFH 30 + name 5 + data 5 = 40 → CDE 起点 40，method @ +10
            tampered[40 + 10] = 12;
            tampered[40 + 11] = 0;
            const fs::path path = dir / "bad_method.zip";
            assert(write_bytes(path, tampered));
            ZipReaderStd broken;
            assert(!broken.open(path.string()));
        }
    }

    // ---------- 大小谎报与 CRC ----------
    {
        // CDE 谎报 uncompressed_size：stored 与 deflate 两条路径都必须拒绝
        const fs::path lied = dir / "lied.zip";
        assert(write_bytes(lied, build_zip({
            {"x.txt", "1234567890"},
            {"y.txt", "deflated payload, long enough for the deflate path", true},
        }, true)));
        ZipReaderStd zip;
        assert(zip.open(lied.string()));
        std::string out;
        assert(!zip.read_entry("x.txt", out));
        assert(!zip.read_entry("y.txt", out));

        // CRC 错误：stored 条目改一个字节
        const fs::path bad_crc_path = dir / "bad_crc.zip";
        assert(write_bytes(bad_crc_path, build_zip({{"x.txt", "hello"}})));
        std::vector<uint8_t> tampered = [&] {
            std::ifstream in(bad_crc_path, std::ios::binary);
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),
                                        std::istreambuf_iterator<char>());
        }();
        tampered[30 + 5] = 'H'; // local header 30 + name 5 → 数据起点
        assert(write_bytes(bad_crc_path, tampered));
        ZipReaderStd broken;
        assert(broken.open(bad_crc_path.string()));
        assert(!broken.read_entry("x.txt", out));
    }

    // ---------- EPUB 全链 ----------
    {
        const fs::path path = make_sample_epub(dir, false);
        EpubParserStd parser;
        assert(parser.load_dictionary(path.string()));
        assert(parser.is_loaded());
        assert(parser.dictionary_name() == "Mini Epub Dict");
        assert(parser.dictionary_description() == "test only");
        assert(parser.word_count() == 3); // Apple、banana + nav 的 Contents

        // 大小写不敏感查询，返回释义
        assert(parser.lookup("apple") == "a fruit & a company second line");
        assert(parser.lookup("APPLE") == "a fruit & a company second line");
        assert(parser.lookup("Banana") == "yellow curved fruit tropical");
        assert(parser.lookup("missing").empty());

        // 词头保留原词形
        const auto words = parser.all_words();
        assert(words.size() == 3);
        assert(std::find(words.begin(), words.end(), "Apple") != words.end());
        assert(std::find(words.begin(), words.end(), "banana") != words.end());

        // 相近词（前缀语义）
        const auto similar = parser.find_similar("app", 10);
        assert(similar.size() == 1);
        assert(similar.front() == "Apple");
    }

    // ---------- 非词典 epub 拒绝 ----------
    {
        const fs::path path = make_sample_epub(dir, true); // xhtml 无 heading
        EpubParserStd parser;
        assert(!parser.load_dictionary(path.string()));
        assert(!parser.is_loaded());
    }

    // ---------- 容器损坏拒绝 ----------
    {
        // 缺 container.xml
        const fs::path no_container = dir / "no_container.epub";
        assert(write_bytes(no_container, build_zip({{"a.xhtml", "<h1>word</h1>def"}})));
        EpubParserStd parser;
        assert(!parser.load_dictionary(no_container.string()));

        // container 声明的 OPF 不存在
        const fs::path bad_opf = dir / "bad_opf.epub";
        assert(write_bytes(bad_opf, build_zip({
            {"META-INF/container.xml",
             "<rootfiles><rootfile full-path=\"missing.opf\"/></rootfiles>"},
        })));
        assert(!parser.load_dictionary(bad_opf.string()));
    }

    return 0;
}
