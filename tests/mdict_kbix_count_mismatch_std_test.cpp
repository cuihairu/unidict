#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <zlib.h>
#include "std/mdict_parser_std.h"

static void u32be(std::string& s, uint32_t v) {
    s.push_back((char)((v >> 24) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF));
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

static void u16be(std::string& s, uint16_t v) {
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

static std::string zlib_compress(const std::string& in) {
    uLongf out_len = compressBound((uLong)in.size());
    std::string out; out.resize(out_len);
    int rc = compress2((Bytef*)out.data(), &out_len, (const Bytef*)in.data(), (uLong)in.size(), Z_BEST_SPEED);
    assert(rc == Z_OK);
    out.resize(out_len);
    return out;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_kbix_count_mismatch";
    fs::create_directories(dir);
    fs::path mdx = dir / "kbix_bad.mdx";

    std::string file;
    file += "<Dictionary title=\"KBIXBad\"/>\n";

    std::string body;
    // KBIX header with declared count=2 but only 1 entry written
    body += "KBIX";
    u32be(body, 2);
    // One entry: wlen=3 "abc", off=0,len=3
    u16be(body, 3); body += "abc"; u32be(body, 0); u32be(body, 3);
    // RBIX zlib blob containing "abc" definition
    body += "RBIX";
    {
        std::string def = "abc";
        std::string z = zlib_compress(def);
        body.append(z);
    }
    file += body;

    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(file.data(), (std::streamsize)file.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    assert(mp.is_loaded());
    // Should not crash and should fallback to seeds
    auto words = mp.all_words();
    bool hasSeed = false;
    for (auto& w : words) if (w == "mdict") { hasSeed = true; break; }
    assert(hasSeed);
    return 0;
}

