#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "std/csv_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_csv(const std::string& content, const std::string& name) {
    fs::path p = fs::current_path()/"build-local"/name;
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc); out << content;
    return p;
}

int main() {
    // 逗号分隔：基本加载、文件名作词典名、条目数后缀
    auto p1 = write_csv("hello,world\nfoo,bar\n", "csv_basic.csv");
    CsvParserStd csv1;
    assert(csv1.load_dictionary(p1.string()));
    assert(csv1.is_loaded());
    assert(csv1.dictionary_name() == "csv_basic");
    assert(csv1.word_count() == 2);
    assert(csv1.lookup("hello") == std::string("world"));
    assert(csv1.lookup("foo") == std::string("bar"));
    assert(csv1.all_words().size() == 2);
    assert(csv1.dictionary_description() == " (2 entries)");

    // TSV：制表符分隔优先于逗号（同一行两者并存时 tab 胜）
    auto p2 = write_csv("a\tb,c\nd\te,f\n", "csv_tsv.tsv");
    CsvParserStd csv2;
    assert(csv2.load_dictionary(p2.string()));
    assert(csv2.word_count() == 2);
    assert(csv2.lookup("a") == std::string("b,c"));
    assert(csv2.lookup("d") == std::string("e,f"));

    // 分号与竖线分隔符
    auto p3 = write_csv("x;y\n", "csv_semi.csv");
    CsvParserStd csv3;
    assert(csv3.load_dictionary(p3.string()));
    assert(csv3.lookup("x") == std::string("y"));

    auto p4 = write_csv("m|n\n", "csv_pipe.csv");
    CsvParserStd csv4;
    assert(csv4.load_dictionary(p4.string()));
    assert(csv4.lookup("m") == std::string("n"));

    // 注释（# 与 ;）、空行、首尾空白 trim；分隔符已定时含其他分隔符的行不切换、被跳过
    auto p5 = write_csv("# comment\n\n;also comment\n  k1 , v1  \n\tk2\tv2\n", "csv_comments.csv");
    CsvParserStd csv5;
    assert(csv5.load_dictionary(p5.string()));
    assert(csv5.word_count() == 1);
    assert(csv5.lookup("k1") == std::string("v1"));
    assert(csv5.lookup("k2") == std::string(""));

    // 引号字段剥离
    auto p6 = write_csv("\"quoted word\",\"quoted, def\"\n", "csv_quoted.csv");
    CsvParserStd csv6;
    assert(csv6.load_dictionary(p6.string()));
    assert(csv6.lookup("quoted word") == std::string("quoted, def"));

    // 首行无分隔符：跳过后继续自动探测
    auto p7 = write_csv("noseparator\nword,def\n", "csv_nosep.csv");
    CsvParserStd csv7;
    assert(csv7.load_dictionary(p7.string()));
    assert(csv7.word_count() == 1);

    // 分隔符已定后，无分隔符的行被跳过；空 word/def 的行不入库
    auto p8 = write_csv("a,b\nnodsep\n,c\n,d\n", "csv_skip.csv");
    CsvParserStd csv8;
    assert(csv8.load_dictionary(p8.string()));
    assert(csv8.word_count() == 1);
    assert(csv8.lookup("a") == std::string("b"));

    // lookup 大小写不敏感回退；找不到返回空串
    auto p9 = write_csv("Apple,Pie\n", "csv_case.csv");
    CsvParserStd csv9;
    assert(csv9.load_dictionary(p9.string()));
    assert(csv9.lookup("APPLE") == std::string("Pie"));
    assert(csv9.lookup("missing") == std::string(""));

    // find_similar：前缀匹配 + max_results 截断
    auto p10 = write_csv("alpha,1\nbeta,2\nalpine,3\n", "csv_similar.csv");
    CsvParserStd csv10;
    assert(csv10.load_dictionary(p10.string()));
    auto sim = csv10.find_similar("al", 10);
    assert(sim.size() == 2); // alpha, alpine
    auto sim1 = csv10.find_similar("al", 1);
    assert(sim1.size() == 1);
    assert(csv10.find_similar("zzz", 10).empty());

    // 文件不存在 → false；重载失败后旧条目已清空
    CsvParserStd csv11;
    assert(csv11.load_dictionary(p10.string()));
    assert(!csv11.load_dictionary("/nonexistent/no_such_file.csv"));
    assert(!csv11.is_loaded());
    assert(csv11.word_count() == 0);
    assert(csv11.all_words().empty());

    // 空文件 → false；dictionary_name 兜底
    auto p12 = write_csv("\n\n", "csv_empty.csv");
    CsvParserStd csv12;
    assert(!csv12.load_dictionary(p12.string()));
    // 未加载时名字与描述的兜底行为
    CsvParserStd csv13;
    assert(csv13.dictionary_name() == "CSV Dictionary");
    assert(csv13.dictionary_description() == " (0 entries)");
    assert(csv13.word_count() == 0);
    assert(!csv13.is_loaded());

    std::cout << "OK\n";
    return 0;
}
