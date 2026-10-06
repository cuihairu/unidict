// DictionaryManagerStd search_grouped 三层降级单测（P-3 3.3）：
// 契约对齐 legacy searchGrouped——层 0 词头精确 / 层 1 前缀（relevance 1）
// / 层 2 释义包含（relevance 2 + fulltext 标注），命中层即止；组按词典
// 聚拢；组内同词头折叠去重保留首条。

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

namespace fs = std::filesystem;

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

    // --- T2 层 1：前缀候选逐词回查 + trie 自愈补建（未显式 build_index）---
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
        assert(pie && pie->relevance == 1 && !pie->fulltext && pie->definition == "pie def");
        const auto* ap = find_entry(*ga, "apple");
        assert(ap && ap->relevance == 1);
        // GB：apple + apply
        assert(gb->entries.size() == 2);
        assert(find_entry(*gb, "apply") && find_entry(*gb, "apply")->relevance == 1);
    }

    // --- T3 层 2：释义包含兜底 + 组内折叠键去重（Apple/APPLE 同键）---
    {
        DictionaryManagerStd m;
        assert(m.add_dictionary(c.string()));
        auto g = m.search_grouped("greeting");
        assert(g.size() == 1);
        assert(g[0].dictionary_name == "GC");
        assert(g[0].entries.size() == 1);  // Apple/APPLE 折叠同键，保留首条
        assert(g[0].entries[0].relevance == 2);
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

    // --- T8 空定义不算命中：层 0 miss 滑层 1 ---
    {
        auto e = write_json("e.json", "GE", {{"ghost", ""}, {"ghostly", "near ghost"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(e.string()));
        auto g = m.search_grouped("ghost");
        assert(g.size() == 1);
        assert(g[0].entries.size() == 1);
        assert(g[0].entries[0].word == "ghostly" && g[0].entries[0].relevance == 1);
    }

    // --- T9 层 0 真实词例抽查：大小写/全角变体 + 释义原文（stardict 词头原形进表）---
    {
        auto sd = write_stardict("sd0", "SDict", {{"Hello", "greeting: hello there"},
                                                  {"Bank", "n. financial institution"}});
        auto j = write_json("t9.json", "G9", {{"good", "adj. 好的"}, {"run", "v. 跑；经营"}});
        DictionaryManagerStd m;
        assert(m.add_dictionary(sd.string()));
        assert(m.add_dictionary(j.string()));
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

    // --- T10 层 2 中文全文（CJK 分词）：命中含词释义、单字不噪声、词头是真词头 ---
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
        assert(g[0].entries[0].relevance == 2 && g[0].entries[0].fulltext);
        assert(g[0].entries[0].definition == "int. 你好；招呼语");
        // 单字「你」→ 层 2 unigram 召回 hello + pure
        auto g2 = m.search_grouped("\xE4\xBD\xA0");
        assert(g2.size() == 1 && g2[0].entries.size() == 2);
        // 签名掺分词器版本（TV=）：tokenize 规则变更时旧 UDFT 缓存失配重建
        assert(m.fulltext_signature().find("TV=2") != std::string::npos);
    }

    return 0;
}