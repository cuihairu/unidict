// MDD resource parser unit tests (std-only).
// Tests for .mdd file parsing, caching, and resource retrieval.

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>

#include "std/mdd_resource_std.h"
#include "std/path_utils_std.h"

using namespace UnidictCoreStd;

// Helper: write big-endian 16-bit
static void be16w(std::vector<unsigned char>& v, uint16_t x) {
    v.push_back((x >> 8) & 0xFF);
    v.push_back(x & 0xFF);
}

// Helper: write big-endian 32-bit
static void be32w(std::vector<unsigned char>& v, uint32_t x) {
    v.push_back((x >> 24) & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 8) & 0xFF);
    v.push_back(x & 0xFF);
}

// Helper: create SimpleKV encoded data
static std::vector<unsigned char> make_simplekv(const std::vector<std::pair<std::string, std::string>>& kv) {
    std::vector<unsigned char> v;
    std::string magic = "SIMPLEKV";
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

// Helper: create minimal MDD v2 header with multi-block layout
static std::vector<unsigned char> make_mdd_v2_header(uint64_t num_blocks) {
    std::vector<unsigned char> header;

    // Header tag (adler32 checksum, placeholder)
    be32w(header, 0x12345678);

    // Header version (2 = v2, with bit 5 set for multi-block)
    uint32_t header_version = 0x00000002 | 0x00000020;
    be32w(header, header_version);

    // Number of blocks
    be32w(header, (uint32_t)num_blocks);

    // Block info entries (8 bytes each: 4-byte offset, 4-byte size)
    for (uint64_t i = 0; i < num_blocks; ++i) {
        be32w(header, 0);      // offset (placeholder)
        be32w(header, 0);      // size (placeholder)
    }

    return header;
}

// Helper: write MDict-like file with header and body
static void write_mdd_file(const std::filesystem::path& path,
                           const std::vector<unsigned char>& header,
                           const std::vector<unsigned char>& body) {
    std::string xml_header = "<Dictionary title=\"MDDTest\" description=\"Test MDD file\"/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(xml_header.data(), (std::streamsize)xml_header.size());
    out.write((const char*)header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    out.close();
    assert(out.good());
}

void test_mdd_parser_basic() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_parser_basic";
    fs::create_directories(base);

    fs::path mdd_path = base / "test.mdd";

    // Create test resources
    std::string png_data = "\x89PNG\r\n\x1a\n" + std::string("DUMMY_PNG_DATA", 15);
    std::string jpg_data = "DUMMY_JPG_DATA";

    std::vector<std::pair<std::string, std::string>> resources = {
        {"images/icon.png", png_data},
        {"images/logo.jpg", jpg_data}
    };

    std::vector<unsigned char> body = make_simplekv(resources);
    std::vector<unsigned char> header = make_mdd_v2_header(1);
    write_mdd_file(mdd_path, header, body);

    // Parse using MddResourceParser
    MddResourceParser parser;
    bool ok = parser.load(mdd_path.string());
    assert(ok);
    assert(parser.is_loaded());

    // Test resource count
    assert(parser.resource_count() == 2);

    // Test resource retrieval
    std::vector<uint8_t> png = parser.get_resource("images/icon.png");
    assert(!png.empty());
    assert(png.size() == png_data.size());

    // Test non-existent resource
    std::vector<uint8_t> missing = parser.get_resource("not_found.png");
    assert(missing.empty());

    parser.unload();
    assert(!parser.is_loaded());
}

void test_mdd_manager() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_manager";
    fs::create_directories(base);

    fs::path cache_dir = base / "cache";
    fs::create_directories(cache_dir);

    MddResourceManager manager;
    manager.set_cache_directory(cache_dir.string());

    // Create test MDD file
    fs::path mdd1 = base / "dict1.mdd";
    std::vector<std::pair<std::string, std::string>> resources1 = {
        {"pic.png", "PNG_DATA_DICT1"}
    };
    std::vector<unsigned char> body1 = make_simplekv(resources1);
    std::vector<unsigned char> header1 = make_mdd_v2_header(1);
    write_mdd_file(mdd1, header1, body1);

    // Load and get resource
    bool ok = manager.load_mdd(mdd1.string(), "dict1");
    assert(ok);
    assert(manager.has_mdd("dict1"));

    std::vector<uint8_t> data = manager.get_resource_data("pic.png", "dict1");
    assert(!data.empty());
    assert(std::string(data.begin(), data.end()) == "PNG_DATA_DICT1");

    // Get cached path
    std::string cached_path = manager.get_resource_path("pic.png", "dict1");
    assert(!cached_path.empty());

    // Unload
    ok = manager.unload_mdd("dict1");
    assert(ok);
    assert(!manager.has_mdd("dict1"));
}

void test_mime_type_detection() {
    // Image types
    assert(MddResourceParser::detect_mime_type("test.png") == "image/png");
    assert(MddResourceParser::detect_mime_type("test.PNG") == "image/png");
    assert(MddResourceParser::detect_mime_type("test.jpg") == "image/jpeg");
    assert(MddResourceParser::detect_mime_type("test.jpeg") == "image/jpeg");
    assert(MddResourceParser::detect_mime_type("test.gif") == "image/gif");
    assert(MddResourceParser::detect_mime_type("test.webp") == "image/webp");
    assert(MddResourceParser::detect_mime_type("test.svg") == "image/svg+xml");
    assert(MddResourceParser::detect_mime_type("test.ico") == "image/x-icon");
    assert(MddResourceParser::detect_mime_type("test.bmp") == "image/bmp");

    // Audio types
    assert(MddResourceParser::detect_mime_type("test.mp3") == "audio/mpeg");
    assert(MddResourceParser::detect_mime_type("test.ogg") == "audio/ogg");
    assert(MddResourceParser::detect_mime_type("test.wav") == "audio/wav");
    assert(MddResourceParser::detect_mime_type("test.m4a") == "audio/mp4");
    assert(MddResourceParser::detect_mime_type("test.flac") == "audio/flac");

    // Video types
    assert(MddResourceParser::detect_mime_type("test.mp4") == "video/mp4");
    assert(MddResourceParser::detect_mime_type("test.webm") == "video/webm");
    assert(MddResourceParser::detect_mime_type("test.avi") == "video/x-msvideo");

    // Default
    assert(MddResourceParser::detect_mime_type("test.unknown") == "application/octet-stream");
    assert(MddResourceParser::detect_mime_type("test") == "application/octet-stream");
}

void test_empty_mdd() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_empty";
    fs::create_directories(base);

    fs::path mdd_path = base / "empty.mdd";

    // Create empty MDD (no resources)
    std::vector<std::pair<std::string, std::string>> resources;
    std::vector<unsigned char> body = make_simplekv(resources);
    std::vector<unsigned char> header = make_mdd_v2_header(1);
    write_mdd_file(mdd_path, header, body);

    MddResourceParser parser;
    bool ok = parser.load(mdd_path.string());
    assert(ok);
    assert(parser.is_loaded());

    // Resource count should be 0
    assert(parser.resource_count() == 0);

    parser.unload();
}

void test_cache_operations() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_cache";
    fs::create_directories(base);

    fs::path cache_dir = base / "cache";
    fs::create_directories(cache_dir);

    MddResourceCache cache(cache_dir.string());

    // Cache a resource
    std::vector<uint8_t> data = {'H', 'E', 'L', 'L', 'O'};
    bool ok = cache.cache_resource(data, "test_key", "text/plain");
    assert(ok);

    // Check if cached
    assert(cache.is_cached("test_key"));

    // Get from cache
    std::vector<uint8_t> retrieved = cache.get_from_cache("test_key");
    assert(retrieved.size() == 5);
    assert(retrieved[0] == 'H');

    // Get cache stats
    assert(cache.get_cached_count() > 0);

    // Clear cache
    cache.clear_cache();
    assert(!cache.is_cached("test_key"));
}

void test_manager_with_cache() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_manager_cache";
    fs::create_directories(base);

    fs::path cache_dir = base / "cache";
    fs::create_directories(cache_dir);

    MddResourceManager manager;
    manager.set_cache_directory(cache_dir.string());

    // Create test MDD file
    fs::path mdd_path = base / "test.mdd";
    std::vector<std::pair<std::string, std::string>> resources = {
        {"a.bin", "AAAA"},
        {"b.bin", "BBBB"},
        {"c.bin", "CCCC"}
    };

    std::vector<unsigned char> body = make_simplekv(resources);
    std::vector<unsigned char> header = make_mdd_v2_header(1);
    write_mdd_file(mdd_path, header, body);

    manager.load_mdd(mdd_path.string(), "test_dict");

    // First access - loads from file
    std::vector<uint8_t> data1 = manager.get_resource_data("a.bin", "test_dict");
    assert(!data1.empty());

    // Second access - from cache
    std::vector<uint8_t> data2 = manager.get_resource_data("a.bin", "test_dict");
    assert(!data2.empty());

    // Check cache size
    size_t cache_size = manager.get_total_cache_size();
    assert(cache_size > 0);

    manager.unload_mdd("test_dict");
}

void test_list_resources() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_list";
    fs::create_directories(base);

    MddResourceManager manager;

    // Create test MDD file
    fs::path mdd_path = base / "test.mdd";
    std::vector<std::pair<std::string, std::string>> resources = {
        {"images/icon.png", "PNG1"},
        {"images/logo.jpg", "JPG1"},
        {"sound/bell.mp3", "MP3"}
    };

    std::vector<unsigned char> body = make_simplekv(resources);
    std::vector<unsigned char> header = make_mdd_v2_header(1);
    write_mdd_file(mdd_path, header, body);

    manager.load_mdd(mdd_path.string(), "test_dict");

    // List all resources
    auto all_resources = manager.list_resources("test_dict");
    assert(all_resources.size() == 3);

    // List with prefix
    auto image_resources = manager.list_resources("test_dict", "images/");
    assert(image_resources.size() == 2);

    manager.unload_mdd("test_dict");
}

void test_multiple_dictionaries() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdd_multi";
    fs::create_directories(base);

    MddResourceManager manager;

    // Create two MDD files
    fs::path mdd1 = base / "dict1.mdd";
    fs::path mdd2 = base / "dict2.mdd";

    write_mdd_file(mdd1, make_mdd_v2_header(1), make_simplekv({{"pic.png", "PNG1"}}));
    write_mdd_file(mdd2, make_mdd_v2_header(1), make_simplekv({{"pic.png", "PNG2"}}));

    manager.load_mdd(mdd1.string(), "dict1");
    manager.load_mdd(mdd2.string(), "dict2");

    assert(manager.has_mdd("dict1"));
    assert(manager.has_mdd("dict2"));

    // Resources from different dictionaries
    auto data1 = manager.get_resource_data("pic.png", "dict1");
    auto data2 = manager.get_resource_data("pic.png", "dict2");

    assert(std::string(data1.begin(), data1.end()) == "PNG1");
    assert(std::string(data2.begin(), data2.end()) == "PNG2");
}

int main() {
    test_mdd_parser_basic();
    test_mdd_manager();
    test_mime_type_detection();
    test_empty_mdd();
    test_cache_operations();
    test_manager_with_cache();
    test_list_resources();
    test_multiple_dictionaries();

    return 0;
}
