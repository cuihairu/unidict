#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

static std::vector<unsigned char> zlib_compress(const std::string& s) {
    uLongf out_len = compressBound((uLongf)s.size());
    std::vector<unsigned char> out(out_len);
    int rc = compress2(out.data(), &out_len, (const Bytef*)s.data(), (uLongf)s.size(), Z_BEST_COMPRESSION);
    if (rc != Z_OK) return {};
    out.resize(out_len);
    return out;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_tsv_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    std::string header = "<Dictionary title=\"DemoTSV\"/>\n";
    std::string payload = "hello\tHELLO DEF\nworld\tWORLD DEF\n";
    auto comp = zlib_compress(payload);
    assert(!comp.empty());

    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)comp.data(), (std::streamsize)comp.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    auto all = mp.all_words();
    bool has_hello = false, has_world = false;
    for (auto& w : all) { if (w == "hello") has_hello = true; if (w == "world") has_world = true; }
    assert(has_hello && has_world);
    return 0;
}

