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
    fs::path dir = fs::current_path() / "build-local" / "mdict_realheur_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // Create a file: header + (zlib(keys)) + (zlib(records)), no ASCII tags, for heuristic parse
    // keys: u16 wlen + word + be32 off + be32 len ...
    std::vector<unsigned char> keys;
    std::string w1 = "alpha", w2 = "beta";
    be16w(keys, (uint16_t)w1.size()); keys.insert(keys.end(), w1.begin(), w1.end()); be32w(keys, 0); be32w(keys, 1);
    be16w(keys, (uint16_t)w2.size()); keys.insert(keys.end(), w2.begin(), w2.end()); be32w(keys, 1); be32w(keys, 1);
    auto keys_c = zlib_compress(keys);

    std::vector<unsigned char> recs = {(unsigned char)'A',(unsigned char)'B'};
    auto recs_c = zlib_compress(recs);

    std::string header = "<Dictionary title=\"DemoHeur\" description=\"heur\"/>\n";
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)keys_c.data(), (std::streamsize)keys_c.size());
    out.write((const char*)recs_c.data(), (std::streamsize)recs_c.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words(); bool a=false,b=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; }
    assert(a&&b);
    assert(mp.lookup("alpha")==std::string("A"));
    assert(mp.lookup("beta")==std::string("B"));
    return 0;
}

