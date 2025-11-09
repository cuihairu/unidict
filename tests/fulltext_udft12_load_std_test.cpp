#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/fulltext_index_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static inline void w32(std::ofstream& out, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v & 0xFF), (unsigned char)((v>>8)&0xFF), (unsigned char)((v>>16)&0xFF), (unsigned char)((v>>24)&0xFF) };
    out.write((const char*)b, 4);
}

static fs::path p1() { fs::path p = fs::current_path()/"build-local"/"udft1.index"; fs::create_directories(p.parent_path()); return p; }
static fs::path p2() { fs::path p = fs::current_path()/"build-local"/"udft2.index"; fs::create_directories(p.parent_path()); return p; }

static void write_udft1(const fs::path& p) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    const char magic[5] = {'U','D','F','T','1'}; out.write(magic, 5);
    // docs = 1; doc map (dict=0, word=0)
    w32(out, 1);
    w32(out, 0); w32(out, 0);
    // terms = 1
    w32(out, 1);
    // term="hello"; postings = 1: (docId=0, tf=3)
    std::string term = "hello";
    w32(out, (uint32_t)term.size()); out.write(term.data(), (std::streamsize)term.size());
    w32(out, 1); w32(out, 0); w32(out, 3);
}

static void write_udft2(const fs::path& p, const std::string& sig) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    const char magic[5] = {'U','D','F','T','2'}; out.write(magic, 5);
    // signature block
    w32(out, (uint32_t)sig.size()); if (!sig.empty()) out.write(sig.data(), (std::streamsize)sig.size());
    // docs = 1; doc map (dict=0, word=0)
    w32(out, 1);
    w32(out, 0); w32(out, 0);
    // terms = 1
    w32(out, 1);
    // term="hello"; postings = 1: (docId=0, tf=1)
    std::string term = "hello";
    w32(out, (uint32_t)term.size()); out.write(term.data(), (std::streamsize)term.size());
    w32(out, 1); w32(out, 0); w32(out, 1);
}

int main() {
    // UDFT1
    write_udft1(p1());
    {
        FullTextIndexStd ft;
        bool ok = ft.load(p1().string());
        assert(ok);
        assert(ft.version() == 1);
        auto r = ft.search("hello", 10);
        assert(!r.empty());
        assert(r[0].dict == 0 && r[0].word == 0);
    }

    // UDFT2 with signature
    write_udft2(p2(), "SIG-ABC");
    {
        FullTextIndexStd ft;
        bool ok = ft.load(p2().string());
        assert(ok);
        assert(ft.version() == 2);
        assert(ft.signature() == std::string("SIG-ABC"));
        auto r = ft.search("hello", 10);
        assert(!r.empty());
        assert(r[0].dict == 0 && r[0].word == 0);
    }
    return 0;
}
