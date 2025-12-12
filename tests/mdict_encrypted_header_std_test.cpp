#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include "std/mdict_parser_std.h"

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_enc_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "enc.mdx";

    // Minimal MDX with encrypted flag in header
    {
        std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
        out << "<Dictionary title=\"EncMDict\" description=\"Encrypted\" encrypted=\"1\"/>\n";
        out << "BODY...";
        out.close();
    }

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    assert(mp.is_loaded());
    // Description should indicate encrypted
    auto desc = mp.dictionary_description();
    assert(desc.find("[encrypted]") != std::string::npos);
    // No words seeded to avoid polluting index/suggestions
    auto all = mp.all_words();
    assert(all.size() == 0);
    return 0;
}
