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
    fs::path dir = fs::current_path() / "build-local" / "mdict_kidx_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // Build KIDX entries
    std::vector<unsigned char> kidx; kidx.insert(kidx.end(), {'K','I','D','X'});
    // two entries
    be32w(kidx, 2);
    // alpha at offset 0 len 1, beta at offset 1 len 1
    std::string w1 = "alpha", w2 = "beta";
    be16w(kidx, (uint16_t)w1.size()); kidx.insert(kidx.end(), w1.begin(), w1.end()); be32w(kidx, 0); be32w(kidx, 1);
    be16w(kidx, (uint16_t)w2.size()); kidx.insert(kidx.end(), w2.begin(), w2.end()); be32w(kidx, 1); be32w(kidx, 1);

    // Build RDEF compressed payload of "AB"
    std::vector<unsigned char> rdef; rdef.insert(rdef.end(), {'R','D','E','F'});
    std::vector<unsigned char> payload = {(unsigned char)'A', (unsigned char)'B'};
    auto comp = zlib_compress(payload);
    rdef.insert(rdef.end(), comp.begin(), comp.end());

    std::string header = "<Dictionary title=\"DemoKIDX\" description=\"kidx\"/>\n";
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)kidx.data(), (std::streamsize)kidx.size());
    out.write((const char*)rdef.data(), (std::streamsize)rdef.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words(); bool a=false,b=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; }
    assert(a&&b);
    assert(mp.lookup("alpha")==std::string("A"));
    assert(mp.lookup("beta")==std::string("B"));
    return 0;
}

