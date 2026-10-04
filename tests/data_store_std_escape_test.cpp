// DataStoreStd 的 JSON 转义/解析对称性补测。
//
// 背景：容错解析器曾把转义解错，且三处各写一份——
//   1) 历史数组的状态机把 \n \r \t 解成字母 n r t → 同一个搜索词
//      每次载入都变成不同串，去重永不命中，每搜一次多一条历史；
//   2) obj_val 用 find('"') 找闭引号，含 \" 的值被截断（quo"te → quo\），
//      再存盘被 json_escape 加倍 → 每轮载入存盘体积翻倍（实测 30 轮
//      把 store.json 撑到 2GB）；
//   3) find_section / for_each_object 的深度计数不认识字符串，释义里的
//      '{' '}' ']' 当成结构 → 区段被静默截断，后面的生词读不回来。
// 三条都以"多轮载入存盘后文件逐字节不变"收口——回归即体积再次发散。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;

namespace {

namespace fs = std::filesystem;

std::string slurp(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void spit(const fs::path& p, const std::string& s) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << s;
}

// 1) 控制字符与引号/反斜杠的往返对称 + 多轮幂等（体积不再翻倍）
void test_escape_roundtrip_idempotent() {
    const fs::path dir = fs::current_path() / "build-local" / "ds_escape";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path f = dir / "store.json";

    // 搜索词含 \" \n \r \t；生词含引号（printf("x") 一类）与花括号
    // 释义（int main() { }）——三者各自踩中一条旧缺陷
    const std::string tricky = "say \"hi\"\nnext\rrow\tend";
    const std::string word = "printf(\"x\")";
    const std::string def = "int main() { return 0; } // \"{}\" [x]";
    const std::string note = "反斜杠 \\ 与 \" 引号混排\t制表";

    {
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        ds.add_search_history(tricky);
        VocabItemStd v;
        v.word = word;
        v.definition = def;
        ds.add_vocabulary_item(v);
        ds.set_note(word, note);
        assert(ds.save());
    }

    // 多轮：每轮重新载入并原样写回，文件必须逐字节不变
    std::string first;
    for (int round = 0; round < 5; ++round) {
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        assert(ds.load());
        // 读回来的值与写进去的完全一致（逐字节）
        const auto hist = ds.get_search_history();
        assert(hist.size() == 1);
        assert(hist[0] == tricky);
        const auto vocab = ds.get_vocabulary();
        assert(vocab.size() == 1);
        assert(vocab[0].word == word);
        assert(vocab[0].definition == def);
        assert(ds.get_note(word) == note);
        // 同样的搜索词/生词再写一次：去重命中，条数不变
        ds.add_search_history(tricky);
        ds.add_vocabulary_item(vocab[0]);
        ds.set_note(word, note);
        const auto hist2 = ds.get_search_history();
        assert(hist2.size() == 1);
        assert(ds.get_vocabulary().size() == 1);
        assert(ds.get_notes().size() == 1);
        assert(ds.save());
        if (round == 0) {
            first = slurp(f);
            assert(!first.empty());
        } else {
            assert(slurp(f) == first);
        }
    }
    // 收口断言：反斜杠数量有限（旧缺陷下这里会是 2^n 级的天文数字）
    size_t backslashes = 0;
    for (const char c : slurp(f)) {
        if (c == '\\') ++backslashes;
    }
    assert(backslashes < 200);
}

// 2) 释义里的结构字符不截断区段：{ } ] " 混排时后面的生词仍读得回来
void test_brackets_inside_strings() {
    const fs::path dir = fs::current_path() / "build-local" / "ds_escape2";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path f = dir / "store.json";
    {
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        for (int i = 0; i < 4; ++i) {
            VocabItemStd v;
            v.word = "w" + std::to_string(i);
            // 每个释义都带 } 与 ]，且越靠后越"像"截断点
            v.definition = "close } and ] and \" quote " +
                           std::string(static_cast<size_t>(i), '{');
            ds.add_vocabulary_item(v);
        }
    }
    DataStoreStd ds;
    ds.set_storage_path(f.string());
    assert(ds.load());
    const auto vocab = ds.get_vocabulary();
    assert(vocab.size() == 4);
    for (int i = 0; i < 4; ++i) {
        assert(vocab[static_cast<size_t>(i)].word == "w" + std::to_string(i));
        assert(vocab[static_cast<size_t>(i)].definition ==
               "close } and ] and \" quote " +
                   std::string(static_cast<size_t>(i), '{'));
    }
}

// 3) 外来/畸形转义的宽容路径：\b \f 正常解码，未知转义按字面，
//    \uXXXX 不臆造（按字面保留）；数组里的非字符串元素被跳过
void test_unescape_tolerant_paths() {
    const fs::path dir = fs::current_path() / "build-local" / "ds_escape3";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path f = dir / "store.json";
    spit(f,
         "{\n"
         "  \"history\": [\"a\\bb\", \"c\\fd\", \"e\\qgf\", 42, \"\\u0041x\"],\n"
         "  \"vocab\": [\n"
         "    {\"word\":\"k\\u0041\",\"definition\":\"d\\q\","
         "\"tags\":[\"z\\qtag\",\"s\\u0041\"],\"added_at\":7}\n"
         "  ],\n"
         "  \"notes\": []\n"
         "}\n");

    DataStoreStd ds;
    ds.set_storage_path(f.string());
    assert(ds.load());

    const auto hist = ds.get_search_history();
    assert(hist.size() == 4);  // 数字元素 42 被跳过
    assert(hist[0] == std::string("a\bb"));
    assert(hist[1] == std::string("c\fd"));
    assert(hist[2] == "eqgf");
    assert(hist[3] == "u0041x");

    const auto vocab = ds.get_vocabulary();
    assert(vocab.size() == 1);
    assert(vocab[0].word == "ku0041");
    assert(vocab[0].definition == "dq");
    assert(vocab[0].added_at == 7);
    assert(vocab[0].tags.size() == 2);
    assert(vocab[0].tags[0] == "zqtag");   // 未知转义 \q 取字面 q
    assert(vocab[0].tags[1] == "su0041");
}

// 4) 截断的 JSON（写到一半没了）不崩不挂：区段提取不到就整体回落空
void test_truncated_json_tolerated() {
    const fs::path dir = fs::current_path() / "build-local" / "ds_escape4";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path f = dir / "store.json";
    spit(f,
         "{\n  \"history\": [\"a\", \"b\n  \"vocab\": [\n"
         "    {\"word\":\"w\",\"defi");

    DataStoreStd ds;
    ds.set_storage_path(f.string());
    assert(ds.load());
    assert(ds.get_search_history().empty());
    assert(ds.get_vocabulary().empty());
    assert(ds.get_notes().empty());
}

// 5) set_note 幂等：同内容重写不翻 updated_at（save 逐字节幂等），
//    内容变化才翻——修复"没变伪装成更新过"+ 跨秒重写字节漂移偶挂
//    （escape 幂等断言过去偶挂的根因即此）
void test_set_note_timestamp_idempotent() {
    const fs::path dir = fs::current_path() / "build-local" / "ds_escape5";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path f = dir / "store.json";
    spit(f,
         "{\n"
         "  \"history\": [],\n"
         "  \"vocab\": [],\n"
         "  \"notes\": [\n"
         "    {\"word\":\"w\",\"text\":\"same\",\"updated_at\":1234567890}\n"
         "  ]\n"
         "}\n");

    DataStoreStd ds;
    ds.set_storage_path(f.string());
    assert(ds.load());
    ds.set_note("w", "same");  // 同内容：不翻戳
    assert(ds.get_note("w") == "same");
    const auto notes = ds.get_notes();
    assert(notes.size() == 1);
    assert(notes[0].updated_at == 1234567890);

    const std::string before = slurp(f);
    ds.set_note("w", "same");  // 再写一次：文件逐字节不变
    assert(slurp(f) == before);

    ds.set_note("w", "changed");  // 内容变了：翻戳
    assert(ds.get_note("w") == "changed");
    assert(ds.get_notes()[0].updated_at > 1234567890);
}

}  // namespace

int main() {
    test_escape_roundtrip_idempotent();
    test_brackets_inside_strings();
    test_unescape_tolerant_paths();
    test_truncated_json_tolerated();
    test_set_note_timestamp_idempotent();
    return 0;
}
