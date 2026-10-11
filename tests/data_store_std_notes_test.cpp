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

    // 荧光笔多色（UI 批二）：色位写入/保留/清除/往返
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        // 3 参入口落色位；2 参入口读侧兜 0
        ds.set_note("kiwi", "green inside", 2);
        assert(ds.get_note("kiwi") == "green inside");
        assert(ds.get_note_color("kiwi") == 2);
        // 2 参入口：正文编辑保留既有色位
        ds.set_note("kiwi", "green inside, sweet");
        assert(ds.get_note_color("kiwi") == 2);
        // 同文本同色重写幂等：文件字节不变（不翻 updated_at，同款口径）
        const std::string before = [&]{
            std::ifstream in(p, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), {});
        }();
        ds.set_note("kiwi", "green inside, sweet", 2);
        const std::string after = [&]{
            std::ifstream in(p, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(in), {});
        }();
        assert(before == after);
        // 只换色位也翻 updated_at（真更新），色位换 0 = 去色
        ds.set_note("kiwi", "green inside, sweet", 0);
        assert(ds.get_note_color("kiwi") == 0);
        // 无笔记词读色 0
        assert(ds.get_note_color("no-such") == 0);
        // 落盘字面：非零色位才写 color 键（0 不写）
        assert(before.find("\"color\":2") != std::string::npos);
        // 空文本移除笔记（色位随之消失）
        ds.set_note("kiwi", "with color back", 3);
        ds.set_note("kiwi", "");
        assert(ds.get_note("kiwi").empty() && ds.get_note_color("kiwi") == 0);
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

    // 荧光色位跨实例持久往返（UI 批二）：色位随文件往返、大小写不敏感读色
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        ds.set_note("fig", "round trip color", 4);
    }
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        assert(ds.get_note_color("FIG") == 4);
        const auto notes = ds.get_notes();
        bool saw_fig = false;
        for (const auto& n : notes)
            if (n.word == "fig") { saw_fig = true; assert(n.color == 4); }
        assert(saw_fig);
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
