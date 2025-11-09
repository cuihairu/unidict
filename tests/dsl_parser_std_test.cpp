#include <cassert>
#include <iostream>
#include <fstream>
#include "std/dsl_parser_std.h"

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

    std::cout << "OK" << std::endl;
    return 0;
}