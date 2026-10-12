// reset_fulltext_index（候选#4 F12）：内存索引丢弃 → stats 归零 →
// 下次全文检索惰性重建 docs 回升；空词典集拒绝（与幂等清零区分）。
#include "dictionary_manager_std.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

using UnidictCoreStd::DictionaryManagerStd;

namespace {

void write_json(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f << body;
}

} // namespace

int main() {
    // 空词典集：reset 拒绝（无从重建）
    DictionaryManagerStd empty;
    assert(!empty.reset_fulltext_index());

    DictionaryManagerStd mgr;
    const std::string dict = "/tmp/unidict_fts_reset_dict.json";
    write_json(dict, R"({"name":"Reset Fixture","entries":[
        {"word":"alpha","definition":"first letter of the alphabet"},
        {"word":"beta","definition":"second letter of the alphabet"}]})");
    assert(mgr.add_dictionary(dict));
    assert(mgr.fulltext_stats().docs == 0);  // 未建

    // 触发惰建（全文兜底口径：无精确/前缀命中的词进全文层）
    auto hits = mgr.search_all("alphabet", false, true);
    assert(mgr.fulltext_stats().docs > 0);

    // reset：内存索引丢弃，stats 归零
    assert(mgr.reset_fulltext_index());
    assert(mgr.fulltext_stats().docs == 0);

    // 下次全文检索惰性重建
    hits = mgr.search_all("alphabet", false, true);
    assert(mgr.fulltext_stats().docs > 0);
    assert(!hits.empty());

    // 幂等：已空再 reset 仍成功（有词典即清零语义）
    assert(mgr.reset_fulltext_index());
    assert(mgr.fulltext_stats().docs == 0);

    std::puts("fulltext_manager_reset_std_test: all assertions passed");
    return 0;
}
