#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::SearchHistoryEntryStd;
using UnidictCoreStd::VocabItemStd;

int main() {
    namespace fs = std::filesystem;
    auto p = (fs::current_path() / "build-local" / "unidict_std_test.json").string();
    fs::create_directories(fs::path(p).parent_path());
    DataStoreStd ds;
    ds.set_storage_path(p);
    ds.clear_history();
    ds.add_search_history("hello");
    ds.add_search_history("world");
    ds.add_search_history("hello");
    // P-7 双存储合一后为 manager 口径：新→旧序，重查即回到表头
    auto h = ds.get_search_history(10);
    assert(h.size() == 2);
    assert(h[0] == "hello" && h[1] == "world");

    // --- P-7 双存储合一：结构化条目（查词元数据 + 置顶）单源钉 ---
    {
        ds.clear_history();
        SearchHistoryEntryStd e1{"apple", true, "Oxford", false};
        SearchHistoryEntryStd e2{"zzz", false, std::string(), false};   // 未找到
        ds.add_search_history_entry(e1);
        ds.add_search_history_entry(e2);
        auto es = ds.get_search_history_entries(10);
        assert(es.size() == 2);
        assert(es[0].query == "zzz" && !es[0].success);          // 新→旧序
        assert(es[1].query == "apple" && es[1].success);
        assert(es[1].dictionary_name == "Oxford" && !es[1].pinned);
    }
    // 置顶语义：pin 插到 pinned 块尾（块内新尾）；新词插在置顶区之后
    {
        ds.clear_history();
        ds.add_search_history("a");
        ds.add_search_history("b");
        ds.add_search_history("c");
        assert(ds.set_search_history_pinned("a", true));          // [a*, b, c]
        assert(ds.set_search_history_pinned("b", true));          // [a*, b*, c]
        auto es = ds.get_search_history_entries(10);
        assert(es[0].query == "a" && es[0].pinned);
        assert(es[1].query == "b" && es[1].pinned);
        assert(es[2].query == "c" && !es[2].pinned);
        // pin 过的词重查：保留 pin、移到置顶块尾（元数据刷新不丢置顶）
        ds.add_search_history("a");                               // [a*, b*, c] → [b*, a*, c]
        es = ds.get_search_history_entries(10);
        assert(es[0].query == "b" && es[0].pinned);
        assert(es[1].query == "a" && es[1].pinned);
        // unpin：移出置顶块到非置顶区头
        assert(ds.set_search_history_pinned("a", false));         // [b*, a, c]
        es = ds.get_search_history_entries(10);
        assert(!es[1].pinned && es[1].query == "a");
        // 大小写不敏感 + 未命中
        assert(ds.set_search_history_pinned("B", true));          // [a?, b*...] b 到块尾
        assert(!ds.set_search_history_pinned("missing", true));
        // 空词假
        assert(!ds.set_search_history_pinned("", true));
    }
    // 删除：大小写不敏感、命中真/未命中假
    {
        ds.clear_history();
        ds.add_search_history("Apple");
        assert(ds.remove_search_history("apple"));
        assert(ds.get_search_history(10).empty());
        assert(!ds.remove_search_history("apple"));
        assert(!ds.remove_search_history(""));
    }
    // limit 取前 N（最新在头）+ 上限 100 裁尾
    {
        ds.clear_history();
        for (int i = 0; i < 105; ++i)
            ds.add_search_history("w" + std::to_string(i));       // 最新=w104 在头
        auto es = ds.get_search_history_entries(200);
        assert(es.size() == 100);
        assert(es.front().query == "w104");                       // 最新在头
        assert(es.back().query == "w5");                          // 最旧 w0..w4 被裁
        auto top3 = ds.get_search_history_entries(3);
        assert(top3.size() == 3 && top3[0].query == "w104");
    }
    // 结构化条目落盘往返（含转义词、全字段）
    {
        ds.clear_history();
        SearchHistoryEntryStd e{"qu\"o\te", false, " dict\\name ", true};
        ds.add_search_history_entry(e);
        DataStoreStd ds2;
        ds2.set_storage_path(p);
        assert(ds2.load());
        auto es = ds2.get_search_history_entries(10);
        assert(es.size() == 1);
        assert(es[0].query == "qu\"o\te" && !es[0].success);
        assert(es[0].dictionary_name == " dict\\name " && es[0].pinned);
    }
    // 整表重建（同步回放面）：按给定序原样替换 + 空词丢弃 + 上限裁剪
    {
        ds.clear_history();
        std::vector<SearchHistoryEntryStd> in{
            {"first", true, std::string(), false},
            {std::string(), true, std::string(), false},   // 空词丢弃
            {"second", false, "d", true}};
        in.emplace_back(SearchHistoryEntryStd{"third", true, std::string(), false});
        ds.set_search_history(std::move(in));
        auto es = ds.get_search_history_entries(10);
        assert(es.size() == 3);                            // 空词没进来
        assert(es[0].query == "first");                    // 序=给定序
        assert(es[1].query == "second" && !es[1].success && es[1].dictionary_name == "d");
        assert(es[2].query == "third");
        std::vector<SearchHistoryEntryStd> many;
        for (int i = 0; i < 103; ++i)
            many.push_back({"n" + std::to_string(i), true, std::string(), false});
        ds.set_search_history(std::move(many));
        assert(ds.get_search_history_entries(1000).size() == 100);  // 裁到上限
        assert(ds.get_search_history_entries(1000).front().query == "n0");
        // 重建后 add 语义接续：新词插到置顶块前（无置顶即表头）
        ds.add_search_history("fresh");
        assert(ds.get_search_history_entries(1000).front().query == "fresh");
    }
    // 畸形布尔值兜底：键在但值非 true/false 字面 → obj_bool 走缺省
    {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << "{\n  \"history\": [{\"query\":\"weird\",\"success\":1,"
               "\"pinned\":null}],\n  \"vocab\": [],\n"
               "  \"notes\": [],\n  \"pron_records\": []\n}\n";
        out.close();
        DataStoreStd ds4;
        ds4.set_storage_path(p);
        auto es = ds4.get_search_history_entries(10);
        assert(es.size() == 1);
        assert(es[0].query == "weird" && es[0].success && !es[0].pinned);
    }
    // 旧格式字符串数组兼容读：success=true、无词典名、不置顶
    {
        std::ofstream out(p, std::ios::binary | std::ios::trunc);
        out << "{\n  \"history\": [\"old_a\", \"old_b\"],\n  \"vocab\": [],\n"
               "  \"notes\": [],\n  \"pron_records\": []\n}\n";
        out.close();
        DataStoreStd ds3;
        ds3.set_storage_path(p);
        auto es = ds3.get_search_history_entries(10);
        assert(es.size() == 2);
        assert(es[0].query == "old_a" && es[0].success && !es[0].pinned);
        assert(es[0].dictionary_name.empty());
        // 旧格式载入后再次保存升级为新格式且往返一致
        ds3.add_search_history("old_a");                          // 重查回头部
        auto es2 = ds3.get_search_history_entries(10);
        assert(es2.size() == 2 && es2[0].query == "old_a" && es2[1].query == "old_b");
    }
    ds.clear_history();

    ds.clear_vocabulary();
    ds.add_vocabulary_item({"foo", "bar"});
    auto v = ds.get_vocabulary();
    bool found = false; for (auto& it : v) if (it.word == "foo" && it.definition == "bar") found = true;
    assert(found);

    std::cout << "OK\n";
    return 0;
}
