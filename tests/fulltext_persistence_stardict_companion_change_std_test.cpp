#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static void be32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)((v>>24)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)(v&0xFF) };
    out.write((const char*)b, 4);
}

static void write_stardict(const fs::path& base, const std::string& def1, const std::string& def2) {
    // .dict
    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    uint32_t off1 = 0; dict.write(def1.data(), (std::streamsize)def1.size());
    uint32_t off2 = (uint32_t)def1.size(); dict.write(def2.data(), (std::streamsize)def2.size());
    dict.close();
    // .idx
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    std::string w1 = "hello"; idx.write(w1.c_str(), (std::streamsize)w1.size()); idx.put('\0'); be32(idx, off1); be32(idx, (uint32_t)def1.size());
    std::string w2 = "world"; idx.write(w2.c_str(), (std::streamsize)w2.size()); idx.put('\0'); be32(idx, off2); be32(idx, (uint32_t)def2.size());
    idx.close();
    // .ifo
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=Sample\n";
    ifo << "wordcount=2\n";
    ifo << "idxfilesize=" << (w1.size() + 1 + 8 + w2.size() + 1 + 8) << "\n";
    ifo << "idxoffsetbits=32\n";
    ifo.close();
}

int main() {
    fs::path dir = fs::current_path() / "build-local" / "sd_sig";
    fs::create_directories(dir);
    fs::path base = dir / "sample";

    write_stardict(base, "Definition of hello.", "Definition of world.");

    fs::path ifo = base; ifo += ".ifo";
    fs::path idxp = fs::current_path()/"build-local"/"ft_sd.index";

    // Save
    {
        DictionaryManagerStd m1; assert(m1.add_dictionary(ifo.string()));
        auto r = m1.full_text_search("Definition", 10); assert(!r.empty());
        assert(m1.save_fulltext_index(idxp.string()));
    }

    // Modify .dict to force signature change
    {
        std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::app);
        dict << " extra";
    }

    // Load should now fail
    {
        DictionaryManagerStd m2; assert(m2.add_dictionary(ifo.string()));
        bool ok = m2.load_fulltext_index(idxp.string());
        assert(!ok);
    }
    return 0;
}

