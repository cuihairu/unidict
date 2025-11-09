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

static std::vector<unsigned char> make_simplekv(const std::vector<std::pair<std::string,std::string>>& kv){
    std::vector<unsigned char> v; std::string magic = "SIMPLEKV"; v.insert(v.end(), magic.begin(), magic.end());
    be32w(v, (uint32_t)kv.size());
    for (auto& p : kv){ be16w(v, (uint16_t)p.first.size()); v.insert(v.end(), p.first.begin(), p.first.end()); be32w(v,(uint32_t)p.second.size()); v.insert(v.end(), p.second.begin(), p.second.end()); }
    return v;
}

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
    fs::path dir = fs::current_path() / "build-local" / "mdict_simplekv_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    std::string header = "<Dictionary title=\"DemoSimpleKV\" description=\"simplekv\"/>\n";
    auto blob = make_simplekv({{"alpha","A"},{"beta","B"}});
    auto comp = zlib_compress(blob);

    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)comp.data(), (std::streamsize)comp.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp; bool ok = mp.load_dictionary(mdx.string()); assert(ok);
    auto all = mp.all_words();
    bool a=false,b=false; for (auto& w : all){ if(w=="alpha") a=true; if(w=="beta") b=true; }
    assert(a&&b);
    assert(mp.lookup("alpha") == std::string("A"));
    return 0;
}

