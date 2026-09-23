#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "std/dsl_parser_std.h"

// 把 DSL 正文写到临时目录，返回文件路径
static std::string write_dsl_file(const std::string& tag, const std::string& body) {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / ("dsl_std_test_" + tag);
    fs::create_directories(dir);
    fs::path p = dir / "d.dsl";
    std::ofstream out(p, std::ios::binary);
    out << body;
    out.close();
    return p.string();
}

// 全表面：BOM 剥离、注释头忽略、紧凑词条 flush、缩进续行拼接、
// 标记清理（加粗/音频/引用）、逗号别名索引、大小写回落、语言头描述
static void test_dsl_full_surface() {
    namespace fs = std::filesystem;
    UnidictCoreStd::DslParserStd dp;

    // 文件不存在 → false
    assert(!dp.load_dictionary("/nonexistent_dir_xyz/no.dsl"));

    std::string body;
    body += "\xEF\xBB\xBF#NAME \"Full DSL\"\n";   // UTF-8 BOM 开头
    body += "#INDEX_LANGUAGE English\n";
    body += "#CONTENTS_LANGUAGE Chinese\n";
    body += "#NOTE ignored header\n";             // 未知头：忽略但算头
    body += "\n";
    body += "hello\nA greeting.\n";
    body += "world\nThe earth.\n";                // 紧凑词条：world 行触发 hello 落库
    body += "\n";
    body += "[b]cat[/b], kitten\n{{c.wav}} a feline animal <<see lion>>\n";
    body += "\n";
    body += "plain, alt1, alt2\nplain def\n";
    std::string path = write_dsl_file("full", body);
    assert(dp.load_dictionary(path));
    assert(dp.is_loaded());
    assert(dp.dictionary_name() == "Full DSL");
    assert(dp.dictionary_description().find("(English -> Chinese)") != std::string::npos);
    assert(dp.lookup("hello") == "A greeting.");
    assert(dp.lookup("world") == "The earth.");
    assert(dp.lookup("cat") == "a feline animal");
    assert(dp.lookup("cat, kitten") == "a feline animal");   // 清理后的完整词条
    assert(dp.lookup("plain") == "plain def");               // 逗号前缀别名
    assert(dp.lookup("HELLO") == "A greeting.");        // 大小写回落
    assert(dp.lookup("Missing") == "");
    auto sim = dp.find_similar("cat", 10);
    assert(!sim.empty());
    fs::remove_all(fs::path(path).parent_path());

    // 无任何头的词典：名字回落默认、描述为空
    std::string bare_path = write_dsl_file("bare", "solo\nonly word\n");
    UnidictCoreStd::DslParserStd dp2;
    assert(dp2.load_dictionary(bare_path));
    assert(dp2.dictionary_name() == "DSL Dictionary");
    assert(dp2.dictionary_description() == "");
    assert(dp2.word_count() >= 1);
    fs::remove_all(fs::path(bare_path).parent_path());
}

int main() {
    UnidictCoreStd::DslParserStd dp;
    bool ok = false;
    const char* candidates[] = {"examples/test.dsl", "../examples/test.dsl", "../../examples/test.dsl"};
    for (const char* p : candidates) {
        std::cout << "Trying to load: " << p << std::endl;

        // Check if file exists first
        std::ifstream test_file(p);
        if (!test_file) {
            std::cout << "File does not exist: " << p << std::endl;
            continue;
        }
        test_file.close();
        std::cout << "File exists: " << p << std::endl;

        if (dp.load_dictionary(p)) {
            ok = true;
            std::cout << "Successfully loaded: " << p << std::endl;
            break;
        } else {
            std::cout << "Failed to load: " << p << std::endl;
        }
    }

    if (!ok) {
        std::cout << "No DSL file found, creating a minimal test..." << std::endl;

        // Create a test file
        std::ofstream out("test_simple.dsl");
        out << "#NAME \"Simple Test\"\n";
        out << "\nhello\nA greeting.\n\nworld\nThe earth.\n";
        out.close();

        if (dp.load_dictionary("test_simple.dsl")) {
            std::cout << "Successfully loaded created file" << std::endl;
            ok = true;
        } else {
            std::cout << "Failed to load created file" << std::endl;
            return 1;
        }
    }

    assert(dp.is_loaded());
    std::cout << "Dictionary name: " << dp.dictionary_name() << std::endl;
    std::cout << "Word count: " << dp.word_count() << std::endl;

    if (dp.word_count() >= 1) {
        auto d = dp.lookup("hello");
        std::cout << "Lookup 'hello': " << d << std::endl;

        auto all = dp.all_words();
        std::cout << "All words: ";
        for (const auto& w : all) {
            std::cout << w << " ";
        }
        std::cout << std::endl;

        auto sim = dp.find_similar("h", 10);
        std::cout << "Similar to 'h':" << std::endl;
        for (const auto& s : sim) {
            std::cout << "  " << s << std::endl;
        }
    }

    test_dsl_full_surface();

    std::cout << "OK" << std::endl;
    return 0;
}