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
    fs::path dir = fs::current_path() / "build-local" / "mdict_kbix_multi_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // KBIX entries: three words mapping to two blocks
    std::vector<unsigned char> kbix; kbix.insert(kbix.end(), {'K','B','I','X'});
    be32w(kbix, 3);
    std::string w1 = "alpha", w2 = "beta", w3 = "gamma";
    be16w(kbix, (uint16_t)w1.size()); kbix.insert(kbix.end(), w1.begin(), w1.end()); be32w(kbix, 0); be32w(kbix, 0); be32w(kbix, 1);
    be16w(kbix, (uint16_t)w2.size()); kbix.insert(kbix.end(), w2.begin(), w2.end()); be32w(kbix, 1); be32w(kbix, 2); be32w(kbix, 1);
    be16w(kbix, (uint16_t)w3.size()); kbix.insert(kbix.end(), w3.begin(), w3.end()); be32w(kbix, 1); be32w(kbix, 0); be32w(kbix, 1);

    // RBCT with two blocks: block0="A", block1="XYZ"
    std::vector<unsigned char> rbct; rbct.insert(rbct.end(), {'R','B','C','T'});
    be32w(rbct, 2);
    // block0
    rbct.insert(rbct.end(), {'R','B','L','K'});
    { std::vector<unsigned char> b0 = {(unsigned char)'A'}; auto c0 = zlib_compress(b0); be32w(rbct, (uint32_t)c0.size()); rbct.insert(rbct.end(), c0.begin(), c0.end()); }
    // block1
    rbct.insert(rbct.end(), {'R','B','L','K'});
    { std::vector<unsigned char> b1 = {(unsigned char)'X',(unsigned char)'Y',(unsigned char)'Z'}; auto c1 = zlib_compress(b1); be32w(rbct, (uint32_t)c1.size()); rbct.insert(rbct.end(), c1.begin(), c1.end()); }

    std::string header = "<Dictionary title=\"DemoKBIXMulti\" description=\"kbix_multi\"/>\n";
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)kbix.data(), (std::streamsize)kbix.size());
    out.write((const char*)rbct.data(), (std::streamsize)rbct.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words(); bool a=false,b=false,g=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; if(w=="gamma") g=true; }
    assert(a&&b&&g);
    assert(mp.lookup("alpha")==std::string("A"));
    assert(mp.lookup("beta")==std::string("Z"));
    assert(mp.lookup("gamma")==std::string("X"));
    return 0;
}

