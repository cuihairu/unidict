#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/mdict_parser_std.h"

static void u32be(std::string& s, uint32_t v) {
    s.push_back((char)((v >> 24) & 0xFF));
    s.push_back((char)((v >> 16) & 0xFF));
    s.push_back((char)((v >> 8) & 0xFF));
    s.push_back((char)(v & 0xFF));
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_mdxk_ulen_cap";
    fs::create_directories(dir);
    fs::path mdx = dir / "ulen_cap.mdx";

    // Header
    std::string file;
    file += "<Dictionary title=\"ULenCapTest\"/>\n";

    // Body: MDXK with one key block whose ulen exceeds safety cap, so parse should fail
    std::string body;
    body += "MDXK";
    // key_blocks = 1
    u32be(body, 1);
    // comp_len = 4 (bogus), ulen = 16MB+1 (exceeds cap)
    u32be(body, 4);
    u32be(body, 16u * 1024u * 1024u + 1u);
    body.append("\x78\x9C\x03\x00", 4); // trivial zlib header + end (still bogus for our purposes)
    // also include MDXR to satisfy presence check, though MDXK already fails
    body += "MDXR";
    u32be(body, 0); // zero blocks

    file += body;
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(file.data(), (std::streamsize)file.size());
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    assert(mp.is_loaded());
    // Should fallback to seed entries due to ulen cap rejection
    auto words = mp.all_words();
    bool hasSeed = false;
    for (auto& w : words) if (w == "mdict") { hasSeed = true; break; }
    assert(hasSeed);
    return 0;
}
