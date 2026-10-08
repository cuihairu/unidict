// DictionaryManagerStd search_grouped 降级链单测（P-3 3.3 + P-11 查词能力线）：
// 层 0 词头精确 / 层 1 词形还原（relevance 1，屈折形→原形词条）/
// 层 2 前缀（relevance 2）/ 层 3 释义包含（relevance 3 + fulltext 标注）/
// 层 4 词头模糊（relevance 4，编辑距离 ≤2，前四层全空才走；字节长 <3
// 短查询不进），命中层即止；组按词典聚拢；组内同词头折叠去重保留首条。

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;

// Windows: fs::path::string() 返回 wstring；POSIX: 返回 string。
// 测试用临时路径均为 ASCII，逐字符转 char 即可跨平台编译通过。
static std::string path_to_utf8(const fs::path& p) {
    std::string s;
    auto native = p.native();  // string_type (wstring on Windows, string on POSIX)
    s.reserve(native.size());
    for (auto c : native) s.push_back(static_cast<char>(c));
    return s;
}

static std::filesystem::path write_json(const std::string& name, const std::string& dict_name,
                                        const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr_grouped";
    fs::create_directories(dir);
    fs::path p = dir / name;
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << "{\"name\":\"" << dict_name << "\",\"description\":\"g\",\"entries\":[";
    for (size_t i = 0; i < entries.size(); ++i) {
        if (i) o << ",";
        o << "{\"word\":\"" << entries[i].first << "\",\"definition\":\"" << entries[i].second << "\"}";
    }
    o << "]}";
    return p;
}

static const DictionaryManagerStd::GroupedEntryStd*
find_entry(const DictionaryManagerStd::DictionaryGroupStd& g, const std::string& word) {
    for (const auto& e : g.entries) {
        if (e.word == word) return &e;
    }
    return nullptr;
}

// 最小 stardict 三件套（.dict/.idx/.ifo）：词头按原始词形进 index_——
// 层 0 大小写/全角变体的端到端用例依赖这个"不折叠"的装载事实
static std::filesystem::path write_stardict(const std::string& name, const std::string& dict_name,
                                            const std::vector<std::pair<std::string, std::string>>& entries) {
    fs::path dir = fs::current_path() / "build-local" / "dict_mgr_grouped";
    fs::create_directories(dir);
    fs::path base = dir / name;
    auto be32 = [](std::ofstream& out, uint32_t v) {
        unsigned char b[4] = {(unsigned char)((v >> 24) & 0xFF), (unsigned char)((v >> 16) & 0xFF),
                              (unsigned char)((v >> 8) & 0xFF), (unsigned char)(v & 0xFF)};
        out.write((const char*)b, 4);
    };
    std::ofstream dict((base.string() + ".dict").c_str(), std::ios::binary | std::ios::trunc);
    std::ofstream idx((base.string() + ".idx").c_str(), std::ios::binary | std::ios::trunc);
    uint32_t off = 0;
    size_t idx_bytes = 0;
    for (const auto& e : entries) {
        dict.write(e.second.data(), (std::streamsize)e.second.size());
        idx.write(e.first.data(), (std::streamsize)e.first.size());
        idx.put('\0');
        be32(idx, off);
        be32(idx, (uint32_t)e.second.size());
        off += (uint32_t)e.second.size();
        idx_bytes += e.first.size() + 1 + 8;
    }
    dict.close();
    idx.close();
    std::ofstream ifo((base.string() + ".ifo").c_str(), std::ios::binary | std::ios::trunc);
    ifo << "bookname=" << dict_name << "\n";
    ifo << "wordcount=" << entries.size() << "\n";
    ifo << "idxfilesize=" << idx_bytes << "\n";
    ifo << "idxoffsetbits=32\n";
    ifo.close();
    return base.string() + ".ifo";
}

int main() {
    auto a = write_json("a.json", "GA", {{"apple", "A def"},
                                         {"Apple", "A upper"},
                                         {"apple pie", "pie def"}});
    auto b = write_json("b.json", "GB", {{"apple", "B def"}, {"apply", "B apply"}});
    auto c = write_json("c.json", "GC", {{"Apple", "sweet greeting"}, {"APPLE", "another greeting"},
                                         {"问候", "hi"}});

    // --- T1 层 0：词头精确即止，两词典各一组 relevance 0 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        auto g = m.search_grouped("apple");
        assert(g.size() == 2);
        assert(g[0].dictionary_name == "GA" && g[1].dictionary_name == "GB");
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "apple" && g[0].entries[0].definition == "A def");
        assert(g[0].entries[0].relevance == 0 && !g[0].entries[0].fulltext);
        // 层级即止：全结果都是 relevance 0（无前缀/全文混入）
        for (const auto& grp : g) {
            for (const auto& e : grp.entries) assert(e.relevance == 0);
        }
    }

    // --- T2 层 2：前缀候选逐词回查 + trie 自愈补建（未显式 build_index）---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        auto g = m.search_grouped("app");
        assert(g.size() == 2);
        // 候选遍历序随 trie 子节点容器非确定 → 组定位按词典名，不按下标
        const DictionaryManagerStd::DictionaryGroupStd* ga = nullptr;
        const DictionaryManagerStd::DictionaryGroupStd* gb = nullptr;
        for (const auto& grp : g) {
            if (grp.dictionary_name == "GA") ga = &grp;
            if (grp.dictionary_name == "GB") gb = &grp;
        }
        assert(ga && gb);
        // GA：apple + apple pie（候选取 12 条，逐词回查命中进组）
        assert(ga->entries.size() == 2);
        const auto* pie = find_entry(*ga, "apple pie");
        assert(pie && pie->relevance == 2 && !pie->fulltext && pie->definition == "pie def");
        const auto* ap = find_entry(*ga, "apple");
        assert(ap && ap->relevance == 2);
        // GB：apple + apply
        assert(gb->entries.size() == 2);
        assert(find_entry(*gb, "apply") && find_entry(*gb, "apply")->relevance == 2);
    }

    // --- T3 层 3：释义包含兜底 + 组内折叠键去重（Apple/APPLE 同键）---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(c.string()));
        auto g = m.search_grouped("greeting");
        assert(g.size() == 1);
        assert(g[0].dictionary_name == "GC");
        assert(g[0].entries.size() == 1);  // Apple/APPLE 折叠同键，保留首条
        assert(g[0].entries[0].relevance == 3);
        assert(g[0].entries[0].fulltext);
    }

    // --- T4 查询词修剪：前后空白等价 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        auto g1 = m.search_grouped("apple");
        auto g2 = m.search_grouped("  apple \t");
        assert(g1.size() == g2.size());
        assert(g2[0].entries[0].definition == g1[0].entries[0].definition);
    }

    // --- T5 空/纯空白查询 → 空 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.search_grouped("").empty());
        assert(m.search_grouped("   ").empty());
    }

    // --- T6 禁用与标签过滤口径一致 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_enabled("GB", false));
        auto g = m.search_grouped("apple");
        assert(g.size() == 1 && g[0].dictionary_name == "GA");
        // 标签过滤：只留 GA
        assert(m.set_dictionary_tags("GA", {"keep"}));
        m.set_tag_filter({"keep"});
        assert(m.search_grouped("apple").size() == 1);
        m.set_tag_filter({"drop"});
        assert(m.search_grouped("apple").empty());
    }

    // --- T7 优先级决定组序 ---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(a.string()));
        assert(m.add_dictionary(b.string()));
        assert(m.set_dictionary_priority("GB", 5));
        auto g = m.search_grouped("apple");
        assert(g.size() == 2);
        assert(g[0].dictionary_name == "GB" && g[1].dictionary_name == "GA");
    }

    // --- T8 空定义不算命中：层 0 miss 滑层 2（ghost 无 lemma 候选）---
    {
        auto e = write_json("e.json", "GE", {{"ghost", ""}, {"ghostly", "near ghost"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(e.string()));
        auto g = m.search_grouped("ghost");
        assert(g.size() == 1);
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "ghostly" && g[0].entries[0].relevance == 2);
    }

    // --- T9 层 0 真实词例抽查：大小写/全角变体 + 释义原文（stardict 词头原形进表）---
    {
        auto sd = write_stardict("sd0", "SDict", {{"Hello", "greeting: hello there"},
                                                  {"Bank", "n. financial institution"}});
        auto j = write_json("t9.json", "G9", {{"good", "adj. 好的"}, {"run", "v. 跑；经营"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(path_to_utf8(sd)));
        assert(m.add_dictionary(path_to_utf8(j)));
        // stardict 词头 "Hello"：小写查询经 fold 回退层 0 命中，释义原文
        auto g = m.search_grouped("hello");
        assert(g.size() == 1 && g[0].dictionary_name == "SDict");
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].relevance == 0 && !g[0].entries[0].fulltext);
        assert(g[0].entries[0].definition == "greeting: hello there");
        // 全角查询 ＨＥＬＬＯ → fold_key 全角→半角归一，同层 0（字节转义：
        // H=EF BC A8 E=EF BC A5 L=EF BC AC O=EF BC AF）
        auto gf = m.search_grouped("\xEF\xBC\xA8\xEF\xBC\xA5\xEF\xBC\xAC\xEF\xBC\xAC\xEF\xBC\xAF");
        assert(gf.size() == 1 && gf[0].entries.size() == 1 && gf[0].entries[0].relevance == 0);
        assert(gf[0].entries[0].definition == "greeting: hello there");
        // 常用词层 0 释义原文抽查（json 词典，大小写变体同断）
        auto g1 = m.search_grouped("good");
        assert(g1.size() == 1 && g1[0].entries.size() == 1);
        assert(g1[0].entries[0].relevance == 0);
        assert(g1[0].entries[0].definition == "adj. 好的");
        auto g2 = m.search_grouped("Run");
        assert(g2.size() == 1 && g2[0].entries.size() == 1);
        assert(g2[0].entries[0].definition == "v. 跑；经营");
    }

    // --- T10 层 3 中文全文（CJK 分词）：命中含词释义、单字不噪声、词头是真词头 ---
    {
        auto j = write_json("t10.json", "GZ",
                            {{"hello", "int. 你好；招呼语"},
                             {"pure", "你真棒"},
                             {"bye", "再见；告辞"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(j.string()));
        // 「你好」三层全 miss 词头 → 层 2 bigram 命中 hello；只含单字「你」
        // 的 pure 不被召回（查询侧多字不发单字的精度契约）
        auto g = m.search_grouped("\xE4\xBD\xA0\xE5\xA5\xBD");
        assert(g.size() == 1);
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "hello");  // 返回真词头，不是查询串
        assert(g[0].entries[0].relevance == 3 && g[0].entries[0].fulltext);
        assert(g[0].entries[0].definition == "int. 你好；招呼语");
        // 单字「你」→ 层 2 unigram 召回 hello + pure
        auto g2 = m.search_grouped("\xE4\xBD\xA0");
        assert(g2.size() == 1 && g2[0].entries.size() == 2);
        // 签名掺分词器版本（TV=）：tokenize 规则变更时旧 UDFT 缓存失配重建
        assert(m.fulltext_signature().find("TV=2") != std::string::npos);
    }

    // --- T11 层 4：词头模糊（前四层全空才走；短查询不进模糊层）---
    {
        auto j = write_json("t11.json", "G11",
                            {{"hello", "int. 你好；招呼语"}, {"ax", "x def"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(j.string()));
        // "helo"：精确/词形还原（无规则臂）/前缀（helo 非 hello 前缀）/
        // 全文（释义不含 helo）全 miss → 层 4 编辑距离 1 召回 hello
        auto g = m.search_grouped("helo");
        assert(g.size() == 1 && g[0].dictionary_name == "G11");
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "hello");  // 返回真词头，不是查询串
        assert(g[0].entries[0].relevance == 4 && !g[0].entries[0].fulltext);
        assert(g[0].entries[0].definition == "int. 你好；招呼语");
        // 短查询闸门："ab" 两字节，距 ax 编辑距离 1，但 <3 不进模糊层 →
        // 前四层全 miss 后整体空手（防短串全表噪声）
        assert(m.search_grouped("ab").empty());
        // 对照：同词典 "bx"（2 字节）同样空——闸门在层 4 入口，与词典内容无关
        assert(m.search_grouped("bx").empty());
        // 层级即止对照：精确命中不混模糊（"ax" 层 0 即止 relevance 0）
        auto g0 = m.search_grouped("ax");
        assert(g0.size() == 1 && g0[0].entries.size() == 1);
        assert(g0[0].entries[0].relevance == 0);
    }

    // --- T12 层 1：词形还原（屈折形直命原形词条；优先级高于前缀）---
    {
        auto j = write_json("t12.json", "G12",
                            {{"run", "v. 跑；经营"}, {"study", "v. 学习"},
                             {"running", "n. 跑步（词头自身在册）"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(j.string()));
        // "running" 层 0 精确即止（词头自己在册时不做还原）
        auto g0 = m.search_grouped("running");
        assert(g0.size() == 1);
        assert(g0[0].entries.size() == 1 && g0[0].entries[0].word == "running");
        assert(g0[0].entries[0].relevance == 0);
        // "running" 无精确词头 → 层 1 还原 run 命中，relevance 1，返回
        // 真词头 run 与其释义
        auto j2 = write_json("t12b.json", "G12B", {{"run", "v. 跑；经营"}});
        DictionaryManagerStd m2;
        assert(m2.add_dictionary(j2.string()));
        auto g = m2.search_grouped("Running");  // 大小写不敏感走 Lemma lcase
        assert(g.size() == 1 && g[0].dictionary_name == "G12B");
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "run" && g[0].entries[0].relevance == 1);
        assert(!g[0].entries[0].fulltext);
        assert(g[0].entries[0].definition == "v. 跑；经营");
        // "studies" → study 同层
        auto j3 = write_json("t12c.json", "G12C", {{"study", "v. 学习；研究"}});
        DictionaryManagerStd m3;
        assert(m3.add_dictionary(j3.string()));
        auto g3 = m3.search_grouped("studies");
        assert(g3.size() == 1 && g3[0].entries.size() == 1);
        assert(g3[0].entries[0].word == "study" && g3[0].entries[0].relevance == 1);
        // 不规则表：wolves → wolf
        auto j4 = write_json("t12d.json", "G12D", {{"wolf", "n. 狼"}});
        DictionaryManagerStd m4;
        assert(m4.add_dictionary(j4.string()));
        auto g4 = m4.search_grouped("wolves");
        assert(g4.size() == 1 && g4[0].entries.size() == 1);
        assert(g4[0].entries[0].word == "wolf" && g4[0].entries[0].relevance == 1);
        // 进行式 studying：-ing 裸去得 study → 层 1 命中（多候选序内
        // 先到先得，study 在册即中）
        auto g5 = m3.search_grouped("studying");
        assert(g5.size() == 1 && g5[0].entries.size() == 1);
        assert(g5[0].entries[0].word == "study" && g5[0].entries[0].relevance == 1);
    }

    // --- T13 miss 建议：suggest_corrections 模糊优先 + 前缀补位 + 去重 ---
    {
        auto j = write_json("t13.json", "G13",
                            {{"hello", "int. hello"}, {"help", "v. help"},
                             {"world", "n. world"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(j.string()));
        // "helo"：模糊命中 hello/help（各距 1，同分按词头字典序 hello<
        // help——fuzzy_search 确定性排序）；前缀("helo") 空
        auto s1 = m.suggest_corrections("helo", 10);
        assert(s1.size() == 2 && s1[0] == "hello" && s1[1] == "help");
        // "worl"：模糊 world(1)；前缀补位只回 world，fold 去重不重复
        auto s2 = m.suggest_corrections("worl", 10);
        assert(s2.size() == 1 && s2[0] == "world");
        // 短词 <3 只走前缀（trie 子节点 unordered，只断成员不钉序）
        auto s3 = m.suggest_corrections("he", 10);
        assert(s3.size() == 2);
        bool has_hello = false, has_help = false;
        for (const auto& w : s3) {
            if (w == "hello") has_hello = true;
            if (w == "help") has_help = true;
        }
        assert(has_hello && has_help);
        // 全 miss 与空白查询
        assert(m.suggest_corrections("zzzz", 10).empty());
        assert(m.suggest_corrections("", 10).empty());
        assert(m.suggest_corrections("  ", 10).empty());
        // max_results 截断：同分字典序取首
        auto s4 = m.suggest_corrections("helo", 1);
        assert(s4.size() == 1 && s4[0] == "hello");
    }

    return 0;
}