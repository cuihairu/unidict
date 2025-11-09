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
    fs::path dir = fs::current_path() / "build-local" / "mdict_keyb_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // Build KEYB entries
    std::vector<unsigned char> keyb; keyb.insert(keyb.end(), {'K','E','Y','B'});
    be32w(keyb, 2);
    std::string w1 = "alpha", w2 = "beta";
    be16w(keyb, (uint16_t)w1.size()); keyb.insert(keyb.end(), w1.begin(), w1.end()); be32w(keyb, 0); be32w(keyb, 1);
    be16w(keyb, (uint16_t)w2.size()); keyb.insert(keyb.end(), w2.begin(), w2.end()); be32w(keyb, 1); be32w(keyb, 1);

    // Build RECB compressed payload of "AB"
    std::vector<unsigned char> recb; recb.insert(recb.end(), {'R','E','C','B'});
    std::vector<unsigned char> payload = {(unsigned char)'A', (unsigned char)'B'};
    auto comp = zlib_compress(payload);
    recb.insert(recb.end(), comp.begin(), comp.end());

    std::string header = "<Dictionary title=\"DemoKEYB\" description=\"keyb\"/>\n";
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)keyb.data(), (std::streamsize)keyb.size());
    out.write((const char*)recb.data(), (std::streamsize)recb.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words(); bool a=false,b=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; }
    assert(a&&b);
    assert(mp.lookup("alpha")==std::string("A"));
    assert(mp.lookup("beta")==std::string("B"));
    return 0;
}

