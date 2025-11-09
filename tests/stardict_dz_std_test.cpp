#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include "std/stardict_parser_std.h"

static void be32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "stardict_dz_sample";
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    // Write compressed .dict.dz
    std::string def1 = "Definition of alpha.";
    std::string def2 = "Definition of beta.";
    gzFile gz = gzopen((base.string() + ".dict.dz").c_str(), "wb");
    assert(gz != nullptr);
    int wn = gzwrite(gz, def1.data(), (unsigned)def1.size()); (void)wn;
    wn = gzwrite(gz, def2.data(), (unsigned)def2.size()); (void)wn;
    gzclose(gz);

    // Write .idx (word + \0 + offset + size) — offsets refer to decompressed stream
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w1 = "alpha"; idx.write(w1.c_str(), (std::streamsize)w1.size()); idx.put('\0'); be32(idx, 0u); be32(idx, (uint32_t)def1.size());
    std::string w2 = "beta";  idx.write(w2.c_str(), (std::streamsize)w2.size()); idx.put('\0'); be32(idx, (uint32_t)def1.size()); be32(idx, (uint32_t)def2.size());
    idx.close();

    // Write .ifo minimal keys
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=SampleDZ\n";
    ifo << "wordcount=2\n";
    ifo << "idxfilesize=" << (w1.size() + 1 + 8 + w2.size() + 1 + 8) << "\n";
    ifo << "idxoffsetbits=32\n";
    ifo.close();

    UnidictCoreStd::StarDictParserStd sp;
    bool ok = sp.load_dictionary((base.string() + ".ifo"));
    assert(ok);
    auto a = sp.lookup("alpha");
    auto b = sp.lookup("beta");
    assert(a.size() == def1.size());
    assert(b.size() == def2.size());
    return 0;
}
