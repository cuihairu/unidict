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
    fs::path dir = fs::current_path() / "build-local" / "mdict_keyb_empty";
    fs::create_directories(dir);
    fs::path mdx = dir / "keyb_empty.mdx";

    std::string file;
    file += "<Dictionary title=\"KEYBEmpty\"/>\n";

    std::string body;
    // KEYB with zero entries
    body += "KEYB";
    u32be(body, 0);
    // RECB zlib blob with some content (not used because zero keys)
    body += "RECB";
    {
        std::string def = "zzz";
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
    // Expect fallback seeds due to zero entries
    auto words = mp.all_words();
    bool hasSeed = false;
    for (auto& w : words) if (w == "mdict") { hasSeed = true; break; }
    assert(hasSeed);
    return 0;
}

