#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

// Create a minimal MDX-like file with zlib-compressed block containing
// pairs like: "word:alpha\ndefinition:First\n\nword:beta\ndefinition:Second\n\n"
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
    fs::path dir = fs::current_path() / "build-local" / "mdict_zlib_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    std::string header = "<Dictionary title=\"DemoMDictZ\" description=\"Zlib block\"/>\n";
    std::string payload = "word:alpha\ndefinition:First\n\nword:beta\ndefinition:Second\n\n";
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
    bool has_alpha = false, has_beta = false;
    for (auto& w : all) { if (w == "alpha") has_alpha = true; if (w == "beta") has_beta = true; }
    assert(has_alpha && has_beta);
    return 0;
}

