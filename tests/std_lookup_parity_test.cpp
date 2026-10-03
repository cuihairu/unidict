// std 面查词口径回归（BUGS.md BUG-005 的 std 侧）。
//
// BUG-005 首版修复只落在 Qt 面（core/json_parser.cpp + core/unidict_core.cpp），
// 而 unidict_cli_std 与 Android JNI 走的是 core/std/ 这一套（DictionaryManagerStd
// + JsonParserStd）：JSON 解析器精确匹配、大小写敏感，查询链不接释义全文索引
// ——同一个「简单词查不出来」在 std 面原样存在（实测 `unidict_cli_std Hello` 报
// Word not found、`the`/`good` 连输出都没有，只 exit 7）。
//
// 这里钉住三件事：折叠键回退、释义全文兜底（显式开启才生效）、计数与可查一致。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace {

std::filesystem::path write_dict(const std::string& name, const std::string& content) {
    namespace fs = std::filesystem;
    fs::path dir = fs::current_path() / "build-local" / "std_lookup";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
    return p;
}

// 词头小写英文（demo 词典同构）
const char* kEnglish = R"({
  "name": "ParityEN",
  "description": "lookup parity fixture",
  "entries": [
    {"word": "hello", "definition": "a greeting"},
    {"word": "qt",    "definition": "c++ gui framework"},
    {"word": "café",  "definition": "coffee shop"}
  ]
})";

// 汉英词典形态：词头全是汉字，英文只在释义里（CC-CEDICT 同构）
const char* kChinese = R"({
  "name": "ParityZH",
  "description": "zh-en fixture",
  "entries": [
    {"word": "不知好歹", "definition": "unable to differentiate good and bad; the good"},
    {"word": "计算机",   "definition": "computer; computing machine"}
  ]
})";

void test_parser_case_fold() {
    auto p = write_dict("fold.json", kEnglish);
    JsonParserStd jp;
    assert(jp.load_dictionary(p.string()));

    const std::string hello = jp.lookup("hello");
    assert(!hello.empty());
    // 大小写：精确 miss 时回退折叠键，返回的是 canonical 词形的释义
    assert(jp.lookup("Hello") == hello);
    assert(jp.lookup("HELLO") == hello);
    assert(jp.lookup("HeLLo") == hello);
    // 全半角：键用 TextNorm::fold_key，与 IndexEngineStd::exact_match 同口径
    assert(jp.lookup("ｈｅｌｌｏ") == hello);
    // 重音折叠（café → cafe）
    assert(jp.lookup("cafe") == jp.lookup("café"));
    // 真的没有的词仍然空
    assert(jp.lookup("zzz_not_here").empty());
    // 折叠不改变词头索引本身
    assert(jp.word_count() == 3);
    assert(jp.lookup("hello") == hello);
}

void test_manager_lookup_paths() {
    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(write_dict("en.json", kEnglish).string()));
    assert(mgr.add_dictionary(write_dict("zh.json", kChinese).string()));
    mgr.build_index();

    // 1) 词头命中走原路径，且不掺兜底结果（默认 false 与显式 true 同结果）
    auto head = mgr.search_all("hello");
    assert(head.size() == 1);
    auto head_opt_in = mgr.search_all("hello", false, true);
    assert(head_opt_in.size() == head.size());
    for (const auto& e : head_opt_in) assert(e.word == "hello");

    // 2) 大小写在管理器层同样互通（此前只有索引侧互通、释义查不到）
    assert(mgr.search_word("Hello") == mgr.search_word("hello"));
    assert(!mgr.search_word("QT").empty());

    // 3) 英文只在释义里：默认（纯词头语义）查不到，显式开启兜底才命中
    assert(mgr.search_all("the").empty());
    auto ft = mgr.search_all("the", false, /*allow_fulltext_fallback=*/true);
    assert(!ft.empty());
    assert(ft.size() <= 12);
    for (const auto& e : ft) {
        assert(e.word != "the");   // 兜底命中的是释义所在词条，不是查询串
        assert(e.definition.find("good") != std::string::npos);
    }
    // search_word 的兜底与 search_all 同源（首个命中即可）
    assert(!mgr.search_word("computer").empty());

    // 4) 真的没有的词：兜底也不该凭空造条目
    assert(mgr.search_all("zzz_not_here", false, true).empty());
    assert(mgr.search_word("zzz_not_here").empty());

    // 5) 计数与可查一致性：索引里数出来的词条都应能查出具义
    //    （「顶栏 125170 词条但查不出来」正是这两者脱节的表现）
    mgr.build_index();
    int checked = 0;
    for (const auto& w : mgr.all_indexed_words()) {
        if (checked++ >= 100) break;
        assert(!mgr.search_all(w).empty());
    }
    assert(checked > 0);
}

} // namespace

int main() {
    test_parser_case_fold();
    test_manager_lookup_paths();
    std::cout << "OK\n";
    return 0;
}
