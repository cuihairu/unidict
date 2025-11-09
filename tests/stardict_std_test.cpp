#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include "std/stardict_parser_std.h"

static void be32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "stardict_sample";
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    // Write .dict
    std::string def1 = "Definition of hello.";
    std::string def2 = "Definition of world.";
    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    uint32_t off1 = 0; dict.write(def1.data(), (std::streamsize)def1.size());
    uint32_t off2 = (uint32_t)def1.size(); dict.write(def2.data(), (std::streamsize)def2.size());
    dict.close();

    // Write .idx (word + \0 + offset + size)
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w1 = "hello"; idx.write(w1.c_str(), (std::streamsize)w1.size()); idx.put('\0'); be32(idx, off1); be32(idx, (uint32_t)def1.size());
    std::string w2 = "world"; idx.write(w2.c_str(), (std::streamsize)w2.size()); idx.put('\0'); be32(idx, off2); be32(idx, (uint32_t)def2.size());
    idx.close();

    // Write .ifo (minimal keys used by parser)
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=Sample\n";
    ifo << "wordcount=2\n";
    ifo << "idxfilesize=" << (w1.size() + 1 + 8 + w2.size() + 1 + 8) << "\n";
    ifo << "idxoffsetbits=32\n";
    ifo.close();

    UnidictCoreStd::StarDictParserStd sp;
    bool ok = sp.load_dictionary((base.string() + ".ifo"));
    assert(ok);
    assert(sp.is_loaded());
    auto all = sp.all_words();
    assert(all.size() == 2);
    auto d1 = sp.lookup("hello");
    auto d2 = sp.lookup("world");
    assert(d1 == def1 && d2 == def2);
    auto sim = sp.find_similar("he", 10);
    bool has_hello = false; for (auto& s : sim) if (s == "hello") has_hello = true; assert(has_hello);
    return 0;
}

