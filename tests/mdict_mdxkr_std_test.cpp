#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

static void be16w(std::vector<unsigned char>& v, uint16_t x){ v.push_back((x>>8)&0xFF); v.push_back(x&0xFF);} 
static void be32w(std::vector<unsigned char>& v, uint32_t x){ v.push_back((x>>24)&0xFF); v.push_back((x>>16)&0xFF); v.push_back((x>>8)&0xFF); v.push_back(x&0xFF);} 

static std::vector<unsigned char> zlib_compress(const std::vector<unsigned char>& in) {
    uLongf out_len = compressBound((uLongf)in.size());
    std::vector<unsigned char> out(out_len);
    int rc = compress2(out.data(), &out_len, in.data(), (uLongf)in.size(), Z_BEST_COMPRESSION);
    assert(rc == Z_OK);
    out.resize(out_len);
    return out;
}

int main(){
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_mdxkr_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // Build MDXK key blocks (one block) with entries
    std::vector<unsigned char> key_block;
    // entries: alpha->off0 len1, beta->off1 len1
    be16w(key_block, 5); key_block.insert(key_block.end(), {'a','l','p','h','a'}); be32w(key_block, 0); be32w(key_block, 1);
    be16w(key_block, 4); key_block.insert(key_block.end(), {'b','e','t','a'});     be32w(key_block, 1); be32w(key_block, 1);
    auto key_block_c = zlib_compress(key_block);

    std::vector<unsigned char> mdxk; mdxk.insert(mdxk.end(), {'M','D','X','K'});
    be32w(mdxk, 1);
    be32w(mdxk, (uint32_t)key_block_c.size()); be32w(mdxk, (uint32_t)key_block.size()); mdxk.insert(mdxk.end(), key_block_c.begin(), key_block_c.end());

    // Build MDXR record blocks (one block) with data "AB"
    std::vector<unsigned char> rec_block = {(unsigned char)'A',(unsigned char)'B'}; auto rec_block_c = zlib_compress(rec_block);
    std::vector<unsigned char> mdxr; mdxr.insert(mdxr.end(), {'M','D','X','R'});
    be32w(mdxr, 1);
    be32w(mdxr, (uint32_t)rec_block_c.size()); be32w(mdxr, (uint32_t)rec_block.size()); mdxr.insert(mdxr.end(), rec_block_c.begin(), rec_block_c.end());

    std::string header = "<Dictionary title=\"DemoMDXKR\" description=\"mdxkr\"/>\n";
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)mdxk.data(), (std::streamsize)mdxk.size());
    out.write((const char*)mdxr.data(), (std::streamsize)mdxr.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words(); bool a=false,b=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; }
    assert(a&&b);
    assert(mp.lookup("alpha")==std::string("A"));
    assert(mp.lookup("beta")==std::string("B"));
    return 0;
}

