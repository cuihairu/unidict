#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

static void be32w(std::vector<unsigned char>& v, uint32_t x){ v.push_back((x>>24)&0xFF); v.push_back((x>>16)&0xFF); v.push_back((x>>8)&0xFF); v.push_back(x&0xFF);}

static std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& in) {
    uLongf out_len = compressBound((uLongf)in.size());
    std::vector<unsigned char> out(out_len);
    int rc = compress2(out.data(), &out_len, in.data(), (uLongf)in.size(), Z_BEST_SPEED);
    assert(rc == Z_OK);
    out.resize(out_len);
    return out;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_mdxk_garbage_tail";
    fs::create_directories(dir);
    fs::path mdx = dir / "garbage.mdx";

    std::string header = "<Dictionary title=\"MDXK-Garbage\"/>\n";
    std::vector<unsigned char> body;
    // MDXK: 0 key blocks (edge)
    body.insert(body.end(), {'M','D','X','K'});
    be32w(body, 0);
    // MDXR: 1 record block with empty uncompressed (ulen=0), compressed empty stream
    body.insert(body.end(), {'M','D','X','R'});
    be32w(body, 1);
    {
        std::vector<unsigned char> empty;
        auto c = zlib_compress(empty);
        be32w(body, (uint32_t)c.size()); be32w(body, 0u);
        body.insert(body.end(), c.begin(), c.end());
    }
    // Append garbage tail
    const char* gar = "GARBAGE_TAIL#####";
    body.insert(body.end(), gar, gar + std::char_traits<char>::length(gar));

    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    // Should not crash; loader returns true and falls back to seeds
    assert(ok);
    auto words = mp.all_words();
    bool hasSeed = false;
    for (auto& w : words) if (w == "mdict") { hasSeed = true; break; }
    assert(hasSeed);
    return 0;
}

