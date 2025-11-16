#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

// Helpers to write big-endian integers
static void be32w(std::vector<unsigned char>& v, uint32_t x){ v.push_back((x>>24)&0xFF); v.push_back((x>>16)&0xFF); v.push_back((x>>8)&0xFF); v.push_back(x&0xFF);} 
static void be16w(std::vector<unsigned char>& v, uint16_t x){ v.push_back((x>>8)&0xFF); v.push_back(x&0xFF);} 

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
    fs::path dir = fs::current_path() / "build-local" / "mdict_mdxk_composite_edge";
    fs::create_directories(dir);
    fs::path mdx = dir / "mdxk_edge.mdx";

    // Header
    std::string header = "<Dictionary title=\"MDXK-Composite\" description=\"edge\"/>\n";

    // Build MDXK with two key blocks: second is effectively empty
    std::vector<unsigned char> body;
    // "MDXK"
    body.push_back('M'); body.push_back('D'); body.push_back('X'); body.push_back('K');
    // kblocks = 2
    be32w(body, 2);
    // Key block 1 with 3 entries (alpha, beta, gamma), offsets into rec_concat
    {
        std::vector<unsigned char> kb1;
        // alpha -> off=0 len=4
        std::string w1 = "alpha"; be16w(kb1, (uint16_t)w1.size()); kb1.insert(kb1.end(), w1.begin(), w1.end()); be32w(kb1, 0); be32w(kb1, 4);
        // beta -> off=4 len=4
        std::string w2 = "beta"; be16w(kb1, (uint16_t)w2.size()); kb1.insert(kb1.end(), w2.begin(), w2.end()); be32w(kb1, 4); be32w(kb1, 4);
        // gamma -> off=8 len=4 (spans record block boundary)
        std::string w3 = "gamma"; be16w(kb1, (uint16_t)w3.size()); kb1.insert(kb1.end(), w3.begin(), w3.end()); be32w(kb1, 8); be32w(kb1, 4);
        auto c = zlib_compress(kb1);
        // write clen, ulen, data
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)kb1.size()); body.insert(body.end(), c.begin(), c.end());
    }
    // Key block 2: tiny payload that decodes to 2 bytes (not enough for an entry)
    {
        std::vector<unsigned char> kb2 = { 0x00, 0x00 }; // arbitrary two bytes
        auto c = zlib_compress(kb2);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)kb2.size()); body.insert(body.end(), c.begin(), c.end());
    }
    // "MDXR" with 3 record blocks: first contains "AAAABBBBCC", second contains "CC", third tiny filler
    body.push_back('M'); body.push_back('D'); body.push_back('X'); body.push_back('R');
    be32w(body, 3);
    {
        std::vector<unsigned char> r1; // "AAAA" + "BBBB" + "CC"
        r1.insert(r1.end(), {'A','A','A','A','B','B','B','B','C','C'});
        auto c = zlib_compress(r1);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)r1.size()); body.insert(body.end(), c.begin(), c.end());
    }
    {
        std::vector<unsigned char> r2; // remaining "CC" to complete "CCCC" for gamma
        r2.insert(r2.end(), {'C','C'});
        auto c = zlib_compress(r2);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)r2.size()); body.insert(body.end(), c.begin(), c.end());
    }
    {
        std::vector<unsigned char> r3 = { 0x00, 0x00 };
        auto c = zlib_compress(r3);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)r3.size()); body.insert(body.end(), c.begin(), c.end());
    }

    // Write file
    {
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out.write(header.data(), (std::streamsize)header.size());
        out.write((const char*)body.data(), (std::streamsize)body.size());
        out.close();
    }

    // Parse and verify
    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    auto all = mp.all_words();
    bool hasA=false, hasB=false, hasG=false;
    for (auto& w : all) { if (w=="alpha") hasA=true; if (w=="beta") hasB=true; if (w=="gamma") hasG=true; }
    assert(hasA && hasB && hasG);
    assert(mp.lookup("alpha") == std::string("AAAA"));
    assert(mp.lookup("beta") == std::string("BBBB"));
    assert(mp.lookup("gamma") == std::string("CCCC"));
    return 0;
}

