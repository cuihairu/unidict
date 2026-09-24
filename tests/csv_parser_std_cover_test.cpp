// CsvParserStd 补覆盖：加载目标为目录（ifstream 可打开但读不出行）
// 时 name_ 回落 "CSV Dictionary"、load 返回 false。

#include <cassert>
#include <filesystem>
#include "std/csv_parser_std.h"

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "csv_cover" / "dirnode";
    fs::create_directories(dir);

    UnidictCoreStd::CsvParserStd parser;
    // 尾分隔符使 stem() 为空；ifstream 打开目录成功但读不出任何行
    assert(!parser.load_dictionary((dir.string() + "/")));
    assert(parser.dictionary_name() == "CSV Dictionary");

    return 0;
}
