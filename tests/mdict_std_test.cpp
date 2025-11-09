#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include "std/mdict_parser_std.h"

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "mdict_sample";
    fs::create_directories(dir);
    fs::path mdx = dir / "demo.mdx";

    // Minimal fake MDX with an XML-like header at the beginning
    std::ofstream out(mdx.string().c_str(), std::ios::binary | std::ios::trunc);
    out << "<Dictionary title=\"DemoMDict\" description=\"For test\"/>\n";
    out << "DATA...";
    out.close();

    UnidictCoreStd::MdictParserStd mp;
    bool ok = mp.load_dictionary(mdx.string());
    assert(ok);
    assert(mp.is_loaded());
    assert(mp.dictionary_name() == std::string("DemoMDict"));
    auto all = mp.all_words();
    // Skeleton seeds two words
    assert(all.size() >= 2);
    return 0;
}

