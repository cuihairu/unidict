#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <iostream>

#include "std/json_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_json(const std::string& content, const std::string& name) {
    fs::path p = fs::current_path()/"build-local"/name;
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary|std::ios::trunc); out << content;
    return p;
}

int main() {
    // Minimal valid entries without name/description
    auto p1 = write_json("{\n  \"entries\": [ {\"word\":\"a\",\"definition\":\"x\"} ]\n}\n", "jp_edge1.json");
    JsonParserStd jp1; bool ok = jp1.load_dictionary(p1.string()); assert(ok);
    assert(jp1.word_count() == 1);
    assert(jp1.lookup("a") == std::string("x"));

    // Whitespace and extra fields should be ignored
    auto p2 = write_json("{\n  \"name\": \"N\", \n  \"description\": \"D\",\n  \"entries\": [ \n    { \n      \"word\": \"term\" , \n      \"definition\": \"def\", \n      \"extra\": 123 \n    } \n  ]\n}\n", "jp_edge2.json");
    JsonParserStd jp2; ok = jp2.load_dictionary(p2.string()); assert(ok);
    assert(jp2.word_count() == 1);
    assert(jp2.lookup("term") == std::string("def"));
    auto sim = jp2.find_similar("te", 10); assert(!sim.empty());

    // 转义一致面（与 data_store_std/dictionary_manager_std 状态文件同方言）：
    // \\ \" \n \r \t 解码；释义含裸 {/}（合法 JSON）不破坏对象边界扫描；
    // \uXXXX 序列按字面保留
    auto p3 = write_json(
        "{\n"
        "  \"name\": \"esc \\\\ \\\"name\\\"\",\n"
        "  \"description\": \"line\\nbreak\\ttab\\rcr\",\n"
        "  \"entries\": [\n"
        "    { \"word\": \"q\", \"definition\": \"say \\\"hi\\\"\" },\n"
        "    { \"word\": \"b\", \"definition\": \"path C:\\\\tmp\\\\x {a,b} end\" },\n"
        "    { \"word\": \"n\", \"definition\": \"a\\nb\\tc\\rd\" },\n"
        "    { \"word\": \"u\", \"definition\": \"\\u0041 literal\" },\n"
        "    { \"word\": \"bp\", \"definition\": \"brace } inside\" }\n"
        "  ]\n"
        "}\n", "jp_edge3.json");
    JsonParserStd jp3; ok = jp3.load_dictionary(p3.string()); assert(ok);
    assert(jp3.word_count() == 5);
    assert(jp3.lookup("q") == std::string("say \"hi\""));
    assert(jp3.lookup("b") == std::string("path C:\\tmp\\x {a,b} end"));
    assert(jp3.lookup("n") == std::string("a\nb\tc\rd"));
    // \u0041 不解码（按字面保留 6 字符序列）
    assert(jp3.lookup("u") == std::string("\\u0041 literal"));
    // 尾条目释义里的裸 } 不腰斩对象扫描
    assert(jp3.lookup("bp") == std::string("brace } inside"));
    assert(jp3.find_similar("q", 10).size() >= 1);

    // 串未闭合（文件截断在字符串中间）→ 该值取空，条目缺词头不入表
    auto p4 = write_json(
        "{\n  \"entries\": [ { \"word\": \"gone\", \"definition\": \"unterminated\n  ]\n}\n",
        "jp_edge4.json");
    JsonParserStd jp4; ok = jp4.load_dictionary(p4.string());
    assert(!ok || jp4.word_count() == 0);

    // name 值 EOF 截断（闭引号扫描到尾未闭合）→ name 空串，无 entries 拒载
    auto p5 = write_json("{\n  \"name\": \"trunc", "jp_edge5.json");
    JsonParserStd jp5; ok = jp5.load_dictionary(p5.string());
    assert(!ok);

    // 数组终点回归（旧版每条目 find(']', i) 重扫余下全缓冲的平方热路径
    // 被 arr_end 单趟定位替换后，这些形状的结果必须保持不变）：
    // ① entries 数组后还有其他顶层键（对象值）→ 全量解析，不越界
    auto p6 = write_json(
        "{\n"
        "  \"name\": \"after\",\n"
        "  \"entries\": [\n"
        "    { \"word\": \"one\", \"definition\": \"first\" },\n"
        "    { \"word\": \"two\", \"definition\": \"second\" }\n"
        "  ],\n"
        "  \"meta\": { \"count\": 2, \"note\": \"trailing key\" }\n"
        "}\n", "jp_edge6.json");
    JsonParserStd jp6; ok = jp6.load_dictionary(p6.string()); assert(ok);
    assert(jp6.word_count() == 2);
    assert(jp6.lookup("one") == std::string("first"));
    assert(jp6.lookup("two") == std::string("second"));

    // ② 数组闭合后跟垃圾尾（无下一个 {）→ 停在数组界内
    auto p7 = write_json(
        "{\n  \"entries\": [\n"
        "    { \"word\": \"g1\", \"definition\": \"gloss one\" }\n"
        "  ]\n  this is trailing garbage , ] { \n",
        "jp_edge7.json");
    JsonParserStd jp7; ok = jp7.load_dictionary(p7.string()); assert(ok);
    assert(jp7.word_count() == 1);
    assert(jp7.lookup("g1") == std::string("gloss one"));

    // ③ 释义字符串内含 ] 与 [（数组结构符出现在串内）→ 其后条目照常解析，
    // 串内括号不参与 arr_end 配对
    auto p8 = write_json(
        "{\n  \"entries\": [\n"
        "    { \"word\": \"b1\", \"definition\": \"note [1] and {brace} done\" },\n"
        "    { \"word\": \"b2\", \"definition\": \"array [0] = {a}\" },\n"
        "    { \"word\": \"b3\", \"definition\": \"plain\" }\n"
        "  ]\n}\n", "jp_edge8.json");
    JsonParserStd jp8; ok = jp8.load_dictionary(p8.string()); assert(ok);
    assert(jp8.word_count() == 3);
    assert(jp8.lookup("b1") == std::string("note [1] and {brace} done"));
    assert(jp8.lookup("b2") == std::string("array [0] = {a}"));
    assert(jp8.lookup("b3") == std::string("plain"));

    // ④ entries 数组为空 → 拒载（word_count 0）
    auto p9 = write_json("{\n  \"entries\": [ ]\n}\n", "jp_edge9.json");
    JsonParserStd jp9; ok = jp9.load_dictionary(p9.string());
    assert(!ok || jp9.word_count() == 0);

    std::cout << "OK\n";
    return 0;
}

