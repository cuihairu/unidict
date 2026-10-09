// 本地词典导出/打包（dictionary_export_std）：导出 → 回读 round-trip
// 全保真（引号/反斜杠/换行/制表/UTF-8/裸括号）、多词典隔离、禁用词典
// 照导、未知名报错。
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "std/dictionary_export_std.h"
#include "std/dictionary_manager_std.h"
#include "std/json_parser_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

static fs::path write_file(const std::string& content, const std::string& name) {
    fs::path p = fs::current_path() / "build-local" / name;
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << content;
    return p;
}

int main() {
    // 源词典：转义敏感字符全集（引号/反斜杠/换行/制表/UTF-8/裸括号）
    const fs::path src_a = write_file(
        "{\n"
        "  \"name\": \"Tricky Dict\",\n"
        "  \"description\": \"escape \\\"stress\\\" C:\\\\path\",\n"
        "  \"entries\": [\n"
        "    { \"word\": \"quote\", \"definition\": \"say \\\"hi\\\" now\" },\n"
        "    { \"word\": \"slash\", \"definition\": \"path C:\\\\tmp\\\\x\" },\n"
        "    { \"word\": \"nl\", \"definition\": \"a\\nb\\tc\\rd\" },\n"
        "    { \"word\": \"zh\", \"definition\": \"中文释义：问候\" },\n"
        "    { \"word\": \"brace\", \"definition\": \"set {a,b} and } too\" }\n"
        "  ]\n"
        "}\n", "export_src_a.json");
    const fs::path src_b = write_file(
        "{\n"
        "  \"name\": \"Other Dict\",\n"
        "  \"entries\": [\n"
        "    { \"word\": \"other\", \"definition\": \"belongs elsewhere\" }\n"
        "  ]\n"
        "}\n", "export_src_b.json");

    DictionaryManagerStd mgr;
    assert(mgr.add_dictionary(src_a.string()));
    assert(mgr.add_dictionary(src_b.string()));

    // round-trip：导出 A → 回读 → 全词条逐字相等（含 name/description）
    const fs::path out_a = fs::current_path() / "build-local" / "export_out_a.json";
    const DictionaryExportResultStd r =
        export_dictionary_json(mgr, "Tricky Dict", out_a.string());
    assert(r.ok);
    assert(r.error.empty());
    assert(r.entry_count == 5);

    DictionaryManagerStd mgr2;
    assert(mgr2.add_dictionary(out_a.string()));
    const std::vector<DictEntryStd> orig = mgr.dictionary_entries("Tricky Dict");
    const std::vector<DictEntryStd> back = mgr2.dictionary_entries("Tricky Dict");
    assert(orig.size() == back.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        assert(orig[i].word == back[i].word);
        assert(orig[i].definition == back[i].definition);
    }
    // meta 同源：description round-trip
    auto meta = mgr2.dictionaries_meta();
    assert(meta.size() == 1 && meta[0].name == "Tricky Dict");
    assert(meta[0].description == "escape \"stress\" C:\\path");

    // 多词典隔离：导出的 A 回读后不含 B 的词条
    assert(mgr2.search_word("other").empty());
    assert(!mgr2.search_word("quote").empty());

    // 禁用词典照导（数据面操作不走查询过滤）
    assert(mgr.set_dictionary_enabled("Other Dict", false));
    const fs::path out_b = fs::current_path() / "build-local" / "export_out_b.json";
    const DictionaryExportResultStd rb =
        export_dictionary_json(mgr, "Other Dict", out_b.string());
    assert(rb.ok && rb.entry_count == 1);
    DictionaryManagerStd mgr3;
    assert(mgr3.add_dictionary(out_b.string()));
    assert(mgr3.search_word("other") == "belongs elsewhere");

    // 未知名 → ok=false + 可读 error
    const DictionaryExportResultStd rx =
        export_dictionary_json(mgr, "No Such Dict", (fs::current_path() / "build-local" / "x.json").string());
    assert(!rx.ok);
    assert(rx.error.find("No Such Dict") != std::string::npos);
    assert(rx.entry_count == 0);

    // 输出路径父目录是文件 → 打开失败 ok=false
    const DictionaryExportResultStd ro =
        export_dictionary_json(mgr, "Tricky Dict", (src_a / "child.json").string());
    assert(!ro.ok);
    assert(ro.error.find("cannot open") != std::string::npos);

    std::cout << "OK\n";
    return 0;
}
