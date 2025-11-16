#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <zlib.h>
#include "std/mdict_parser_std.h"

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
    fs::path dir = fs::current_path() / "build-local" / "mdict_mdxk_oob_offsets";
    fs::create_directories(dir);
    fs::path mdx = dir / "oob.mdx";

    std::string header = "<Dictionary title=\"MDXK-OOB\"/>\n";
    std::vector<unsigned char> body;
    // MDXK with 1 key block
    body.insert(body.end(), {'M','D','X','K'});
    be32w(body, 1);
    {
        std::vector<unsigned char> kb;
        // alpha -> off=0,len=4
        std::string w1 = "alpha";
        be16w(kb, (uint16_t)w1.size()); kb.insert(kb.end(), w1.begin(), w1.end()); be32w(kb, 0); be32w(kb, 4);
        // badword -> off way beyond rec_concat
        std::string w2 = "badword";
        be16w(kb, (uint16_t)w2.size()); kb.insert(kb.end(), w2.begin(), w2.end()); be32w(kb, 999999); be32w(kb, 4);
        auto c = zlib_compress(kb);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)kb.size()); body.insert(body.end(), c.begin(), c.end());
    }
    // MDXR with 1 record block "AAAA"
    body.insert(body.end(), {'M','D','X','R'});
    be32w(body, 1);
    {
        std::vector<unsigned char> r = {'A','A','A','A'};
        auto c = zlib_compress(r);
        be32w(body, (uint32_t)c.size()); be32w(body, (uint32_t)r.size()); body.insert(body.end(), c.begin(), c.end());
    }

    // Write file
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(header.data(), (std::streamsize)header.size());
    out.write((const char*)body.data(), (std::streamsize)body.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    auto all = mp.all_words();
    bool hasAlpha=false, hasBad=false;
    for (auto& w : all) { if (w=="alpha") hasAlpha=true; if (w=="badword") hasBad=true; }
    assert(hasAlpha);
    assert(mp.lookup("alpha") == std::string("AAAA"));
    // OOB entry should be ignored
    assert(!hasBad);
    assert(mp.lookup("badword").empty());
    return 0;
}

