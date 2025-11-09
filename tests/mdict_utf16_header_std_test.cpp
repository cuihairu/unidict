#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include "std/mdict_parser_std.h"

static std::string make_utf16le(const std::u16string& s) {
    std::string out;
    out.push_back((char)0xFF); out.push_back((char)0xFE); // BOM LE
    for (char16_t ch : s) {
        out.push_back((char)(ch & 0xFF));
        out.push_back((char)((ch >> 8) & 0xFF));
    }
    return out;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_utf16_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // UTF-16LE header with ASCII attrs
    std::u16string header = u"<Dictionary title=\"DemoUTF16\" description=\"Header\"/>\n";
    std::string bin = make_utf16le(header);

    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out.write(bin.data(), (std::streamsize)bin.size());
    out << "DATA";
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    assert(mp.dictionary_name() == std::string("DemoUTF16"));
    return 0;
}

