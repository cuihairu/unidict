// 词条笔记：upsert/删除/大小写不敏感、跨实例持久往返、旧格式（无 notes 数组）兼容
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::NoteItemStd;

int main() {
    namespace fs = std::filesystem;
    auto p = (fs::current_path() / "build-local" / "unidict_std_notes_test.json").string();
    fs::create_directories(fs::path(p).parent_path());
    fs::remove(p);

    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        // 未加载过：get_note 空串
        assert(ds.get_note("apple").empty());
        ds.set_note("apple", "a fruit I always forget");
        assert(ds.get_note("apple") == "a fruit I always forget");
        // upsert：覆盖旧文本
        ds.set_note("apple", "updated note");
        assert(ds.get_note("apple") == "updated note");
        // 大小写不敏感命中同一条
        ds.set_note("APPLE", "case-insensitive hit");
        assert(ds.get_note("apple") == "case-insensitive hit");
        assert(ds.get_notes().size() == 1);
        // 空文本=移除
        ds.set_note("apple", "");
        assert(ds.get_note("apple").empty());
        assert(ds.get_notes().empty());
        ds.set_note("pear", "keep me");
        // 笔记内检索（Search within notes）：大小写不敏感子串 + 空 query 全量
        ds.set_note("apple", "A Fruit I Always Forget");
        ds.set_note("plum", "中文备注：水果");
        const auto hits = ds.search_notes("fruit");
        assert(hits.size() == 1 && hits[0].word == "apple");
        assert(ds.search_notes("FRUIT").size() == 1);
        // 非 ASCII 子串原样参与（中文命中）
        assert(ds.search_notes("水果").size() == 1);
        // 无命中 / 空 query = 全部
        assert(ds.search_notes("no-such-note-text").empty());
        assert(ds.search_notes("").size() == 3);
    }

    // 跨实例持久往返
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        const auto notes = ds.get_notes();
        assert(notes.size() == 3);
        // 检索面跨实例一致（持久往返后 filter 与文本都还在）
        assert(ds.search_notes("fruit").size() == 1);
        assert(ds.search_notes("水果").size() == 1);
        bool saw_pear = false;
        for (const auto& n : notes) {
            if (n.word == "pear") { saw_pear = true; assert(n.text == "keep me" && n.updated_at > 0); }
        }
        assert(saw_pear);
        assert(ds.get_note("PEAR") == "keep me");
    }

    // 旧格式兼容：无 notes 字段的数据文件 load 后笔记为空
    {
        auto legacy = (fs::current_path() / "build-local" / "unidict_std_legacy2.json").string();
        std::ofstream out(legacy, std::ios::binary | std::ios::trunc);
        out << "{\n  \"history\": [],\n  \"vocab\": [\n"
            << "    {\"word\":\"old\",\"definition\":\"legacy\"}\n"
            << "  ]\n}\n";
        out.close();
        DataStoreStd ds;
        ds.set_storage_path(legacy);
        assert(ds.get_vocabulary().size() == 1);
        assert(ds.get_notes().empty() && ds.get_note("old").empty());
    }

    std::cout << "OK\n";
    return 0;
}
