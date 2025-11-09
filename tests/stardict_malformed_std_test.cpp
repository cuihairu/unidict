#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/stardict_parser_std.h"

static void be32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "stardict_bad";
    fs::create_directories(dir);
    fs::path base = dir / "bad";

    // .dict with minimal bytes
    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    dict << "abc"; dict.close();

    // .idx truncated (word + NUL but missing offset/size)
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w = "hello"; idx.write(w.c_str(), (std::streamsize)w.size()); idx.put('\0'); idx.close();

    // .ifo minimal
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=Bad\n";
    ifo << "wordcount=1\n";
    ifo << "idxfilesize=4\n"; // bogus
    ifo << "idxoffsetbits=32\n";
    ifo.close();

    UnidictCoreStd::StarDictParserStd sp;
    bool ok = sp.load_dictionary((base.string() + ".ifo"));
    assert(!ok); // should fail due to truncated idx
    return 0;
}

