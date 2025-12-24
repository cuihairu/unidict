#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/mdict_parser_std.h"
#include "std/path_utils_std.h"

static void be16w(std::vector<unsigned char>& v, uint16_t x) { v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }
static void be32w(std::vector<unsigned char>& v, uint32_t x) { v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF); v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF); }

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

static void write_mdict_like_file(const std::filesystem::path& path, const std::vector<unsigned char>& body) {
    std::string header = "<Dictionary title=\"MDDTest\" description=\"mdd\"/>\n";
    std::ofstream out(path.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    out.close();
    assert(out.good());
}

int main() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "mdict_mdd_resources";
    fs::path data = base / "data";
    fs::path cache = base / "cache";
    fs::create_directories(data);
    fs::create_directories(cache);
    setenv("UNIDICT_DATA_DIR", data.string().c_str(), 1);
    setenv("UNIDICT_CACHE_DIR", cache.string().c_str(), 1);

    fs::path mdx = base / "demo.mdx";
    fs::path mdd = base / "demo.mdd";

    const std::string def =
        "<div>"
        "hello "
        "<img src=\"pic.png\"/> "
        "<a href=\"entry://world\">world</a> "
        "@@@LINK=world"
        "</div>";

    // Minimal PNG header + dummy bytes; validity is not required for this test.
    const std::string png = std::string("\x89PNG\r\n\x1a\n", 8) + "DUMMY";

    write_mdict_like_file(mdx, make_simplekv({{"hello", def}}));
    write_mdict_like_file(mdd, make_simplekv({{"pic.png", png}}));

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);

    std::string rendered = mp.lookup("hello");
    assert(rendered.find("unidict://lookup?word=world") != std::string::npos);
    assert(rendered.find("file://") != std::string::npos);

    // Ensure resource extracted into cache.
    bool found_png = false;
    for (auto& p : fs::recursive_directory_iterator(UnidictCoreStd::PathUtilsStd::cache_dir())) {
        if (!p.is_regular_file()) continue;
        if (p.path().filename().string() == "pic.png") {
            found_png = true;
            break;
        }
    }
    assert(found_png);
    return 0;
}

