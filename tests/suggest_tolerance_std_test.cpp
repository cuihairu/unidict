// 输入联想容错面回归（下拉联想 / 乱序 anagram / 忘词子串+通配）：
// 索引全部随导入即建（add_word 增量维护），查询面按「精确 → 前缀 →
// 乱序 → 通配/包含 → 词条」分段，跨段去重，扫描段带耗时上限。
//
// 覆盖：
//   1. anagram 命中（hlelo→hello）/ 无命中 / 查询词自身不出 / 截断
//   2. remove 对称摘除——anagram 索引无鬼影（词典级 + 引擎级）
//   3. 子串（ello→hello+yellow）与通配（hel*o→hello）入联想分段
//   4. suggest_sections 分段集成：命中即止、跨段去重、通配独占、中英同款
//   5. 导入即建索引（不调 build_index）+ load_index 置脏自愈 + index_ready

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"
#include "std/index_engine_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;

static bool contains(const std::vector<std::string>& v, const std::string& w) {
    for (const auto& s : v) if (s == w) return true;
    return false;
}

static fs::path write_json(const std::string& name, const std::string& dict_name,
                           const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path dir = fs::current_path() / "build-local" / "suggest_tolerance";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"t\",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) o << ",";
        o << "{\"word\":\"" << entries[i].first << "\",\"definition\":\""
          << entries[i].second << "\"}";
    }
    o << "]}";
    return p;
}

static const DictionaryManagerStd::SuggestSectionStd* find_section(
    const std::vector<DictionaryManagerStd::SuggestSectionStd>& secs, const std::string& kind) {
    for (const auto& s : secs) if (s.kind == kind) return &s;
    return nullptr;
}

int main() {
    // ---------------- 1. 引擎层：乱序索引的建/查/截断 ----------------
    {
        IndexEngineStd idx;
        idx.add_word("hello", "d1");
        idx.add_word("helmet", "d1");
        idx.add_word("world", "d1");

        // 命中：hlelo 与 hello 同字母集合
        auto a1 = idx.anagram_search("hlelo", 10);
        assert(contains(a1, "hello"));
        assert(!contains(a1, "helmet"));

        // 查询词自身不出（精确/前缀层已给，重复即噪声）
        assert(idx.anagram_search("hello", 10).empty());

        // 无命中：字母集合不同 → 空
        assert(idx.anagram_search("zzqq", 10).empty());
        assert(idx.anagram_search("", 10).empty());
        assert(idx.anagram_search("hlelo", 0).empty());

        // max_results 截断（桶内字典序取首）
        assert(idx.anagram_search("hlelo", 1).size() == 1);
    }

    // ---------------- 2. remove 对称摘除：无鬼影 ----------------
    {
        // 引擎级：双词典归属，摘到只剩一个仍命中，全摘才空
        IndexEngineStd idx;
        idx.add_word("hello", "d1");
        idx.add_word("hello", "d2");
        idx.remove_word("hello", "d1");
        assert(contains(idx.anagram_search("hlelo", 10), "hello"));
        idx.remove_word("hello", "d2");
        assert(idx.anagram_search("hlelo", 10).empty());
        assert(idx.prefix_search("hel", 10).empty());

        // 词典级：remove_dictionary 后乱序层不留旧词
        auto j = write_json("ghost.json", "GhostDict",
                            {{"hello", "int. hi"}, {"helmet", "n. 头盔"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(j.string()));
        assert(contains(m.anagram_search("hlelo", 10), "hello"));
        assert(m.remove_dictionary("GhostDict"));
        assert(m.anagram_search("hlelo", 10).empty());
        assert(m.prefix_search("hel", 10).empty());
    }

    // ---------------- 3. 导入即建索引（不调 build_index） ----------------
    auto en = write_json("en.json", "EnDict",
                         {{"hello", "int. 你好"}, {"helmet", "n. 头盔"},
                          {"helium", "n. 氦"}, {"help", "v. 帮助"},
                          {"yellow", "adj. 黄色"}, {"world", "n. 世界"}});
    DictionaryManagerStd m;
    assert(m.add_dictionary(en.string()));

    // 前缀（既有回归护栏，此处钉「不 build 也即查」）
    auto pref = m.prefix_search("hel", 10);
    assert(contains(pref, "hello") && contains(pref, "helmet"));
    // 乱序 / 子串 同样导入即建
    assert(contains(m.anagram_search("hlelo", 10), "hello"));
    assert(contains(m.substring_search("ello", 10), "hello"));
    assert(contains(m.substring_search("ello", 10), "yellow"));
    // 通配：字面前缀收窄到 "hel" 子树
    assert(contains(m.wildcard_search("hel*o", 10), "hello"));
    assert(m.wildcard_search("zz*o", 10).empty());

    // 子串大小写不敏感 + 耗时上限口径一致（小词典两种预算结果相同）
    assert(contains(m.substring_search("ELLO", 10, 15), "hello"));
    assert(m.substring_search("ello", 10, 15) == m.substring_search("ello", 10));

    // ---------------- 4. suggest_sections 分段集成 ----------------
    {
        // 正确前缀：只出前缀段（"hel" 非词头，无精确段），无容错段
        auto s = m.suggest_sections("hel", 50);
        assert(s.size() == 1 && s[0].kind == "prefix");
        assert(contains(s[0].words, "hello") && contains(s[0].words, "helmet"));
        assert(!find_section(s, "anagram"));
        assert(!find_section(s, "substring"));

        // 精确命中即止：只有精确段（前缀段的唯一词已被跨段去重吃掉）
        auto s2 = m.suggest_sections("hello", 50);
        assert(s2.size() == 1 && s2[0].kind == "exact" && s2[0].words.size() == 1);

        // 乱序容错：精确/前缀全 miss 才出乱序段；模糊段不得重复同一个词
        auto s3 = m.suggest_sections("hlelo", 50);
        assert(!s3.empty() && s3[0].kind == "anagram");
        assert(contains(s3[0].words, "hello"));
        assert(!find_section(s3, "exact"));
        assert(!find_section(s3, "prefix"));
        for (const auto& sec : s3)
            for (const auto& w : sec.words)
                assert(w != "hello" || sec.kind == "anagram");  // 跨段去重

        // 忘词容错（子串）：只记得词尾 → 包含段
        auto s4 = m.suggest_sections("ello", 50);
        assert(!s4.empty() && s4[0].kind == "substring");
        assert(contains(s4[0].words, "hello") && contains(s4[0].words, "yellow"));

        // 忘词容错（通配）：含 * 的查询只出通配段
        auto s5 = m.suggest_sections("hel*o", 50);
        assert(s5.size() == 1 && s5[0].kind == "wildcard");
        assert(contains(s5[0].words, "hello"));
        auto s5b = m.suggest_sections("hel?et", 50);   // hel + 任意 + et → helmet
        assert(s5b.size() == 1 && s5b[0].kind == "wildcard");
        assert(contains(s5b[0].words, "helmet"));

        // 全 miss：不返回空段（乱序/包含/模糊三层都空）
        assert(m.suggest_sections("zzzqqq", 50).empty());
        // 空白与空查询
        assert(m.suggest_sections("", 50).empty());
        assert(m.suggest_sections("   ", 50).empty());
        // 空白包裹的查询照常走（与 suggest_corrections 同口径）
        assert(!m.suggest_sections("  hel  ", 50).empty());
        // max_results 截断
        auto s6 = m.suggest_sections("hel", 1);
        assert(s6.size() == 1 && s6[0].words.size() == 1);
    }

    // ---------------- 5. 中英同款（中文走同一套索引） ----------------
    {
        auto zh = write_json("zh.json", "ZhDict",
                             {{"你好", "int. hello"}, {"世界", "n. world"}});
        DictionaryManagerStd zm;
        assert(zm.add_dictionary(zh.string()));
        // 前缀（UTF-8 字节前缀）
        assert(contains(zm.prefix_search("你", 10), "你好"));
        // 乱序：字节集合相同的换序词形 → 同一索引命中
        auto za = zm.suggest_sections("好你", 50);
        assert(!za.empty() && za[0].kind == "anagram");
        assert(contains(za[0].words, "你好"));
        // 子串
        assert(contains(zm.substring_search("世界", 10), "世界"));
        // 精确命中即止
        auto ze = zm.suggest_sections("你好", 50);
        assert(ze.size() == 1 && ze[0].kind == "exact");
    }

    // ---------------- 6. load_index 置脏自愈 + index_ready ----------------
    {
        const auto idx_file = fs::current_path() / "build-local" / "suggest_tolerance" / "en.idx";
        assert(m.save_index(idx_file.string()));
        DictionaryManagerStd m2;
        assert(m2.load_index(idx_file.string()));
        // 整表替换 → 待补建（false）；查询面自愈后回真
        assert(!m2.index_ready());
        auto healed = m2.anagram_search("hlelo", 10);
        assert(m2.index_ready());
        assert(contains(healed, "hello"));
        assert(contains(m2.substring_search("ello", 10), "yellow"));
        // 未自愈前 index_ready 假；add 路径恒真
        DictionaryManagerStd m3;
        assert(m3.index_ready());
        assert(m3.add_dictionary(en.string()));
        assert(m3.index_ready());
    }

    std::cout << "suggest_tolerance_std_test: all assertions passed\n";
    return 0;
}
