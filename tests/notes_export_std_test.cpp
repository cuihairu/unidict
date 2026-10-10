// 笔记导出（roadmap Export notes HTML 面）：HTML 转义、换行归一、
// epoch 时间格式化（含 0=未知留空）、空库、不可写路径、成员包装
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "std/data_store_std.h"
#include "std/notes_export_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::NotesExportResultStd;

static std::string read_file(const std::string& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

static bool has_substr(const std::string& hay, const std::string& n) {
    return hay.find(n) != std::string::npos;
}

int main() {
    namespace fs = std::filesystem;
    auto dir = (fs::current_path() / "build-local").string();
    fs::create_directories(dir);
    auto data = (fs::current_path() / "build-local" / "notes_export_src.json").string();
    auto out = (fs::current_path() / "build-local" / "notes_export_out.html").string();
    fs::remove(data);
    fs::remove(out);

    // 手写最小 state：一条缺 updated_at（载入即 0）+ 一条带值
    {
        std::ofstream src(data, std::ios::binary | std::ios::trunc);
        src << "{\n";
        src << "  \"notes\": [\n";
        src << "    {\"word\":\"zero\",\"text\":\"no timestamp\"},\n";
        src << "    {\"word\":\"dated\",\"text\":\"has stamp\",\"updated_at\":1700000000}\n";
        src << "  ]\n";
        src << "}\n";
    }
    {
        DataStoreStd ds;
        ds.set_storage_path(data);
        assert(ds.get_notes().size() == 2);
        NotesExportResultStd r = UnidictCoreStd::export_notes_html(ds, out);
        assert(r.ok);
        assert(r.error.empty());
        assert(r.note_count == 2);
        const std::string html = read_file(out);
        assert(has_substr(html, "<!DOCTYPE html>"));
        assert(has_substr(html, "<meta charset=\"utf-8\">"));
        assert(has_substr(html, "<h2>zero</h2>"));
        assert(has_substr(html, "<p>no timestamp</p>"));
        // updated_at=0 → 留空；1700000000 → UTC 2023-11-14 22:13:20
        assert(has_substr(html, "<p class=\"updated\"></p>"));
        assert(has_substr(html, "<p class=\"updated\">2023-11-14 22:13:20</p>"));
    }

    // 转义 + 换行归一（\n / \r\n / 裸 \r 都转 <br>）+ 非 ASCII 直通
    fs::remove(data);
    fs::remove(out);
    {
        DataStoreStd ds;
        ds.set_storage_path(data);
        ds.set_note("hello", "a<b & \"q\" 's'");
        ds.set_note("fruit", "line1\nline2\r\nline3\rlast");
        ds.set_note("中文词", "中文备注：<p>段落</p>");
        NotesExportResultStd r = UnidictCoreStd::export_notes_html(ds, out);
        assert(r.ok && r.note_count == 3);
        const std::string html = read_file(out);
        assert(has_substr(html, "<h2>hello</h2>"));
        assert(has_substr(html, "a&lt;b &amp; &quot;q&quot; &#39;s&#39;"));
        assert(has_substr(html, "line1<br>line2<br>line3<br>last"));
        assert(has_substr(html, "<h2>中文词</h2>"));
        assert(has_substr(html, "中文备注：&lt;p&gt;段落&lt;/p&gt;"));
    }

    // 成员包装：同一导出能力走 store 成员（返回 bool 口径）
    fs::remove(out);
    {
        DataStoreStd ds;
        ds.set_storage_path(data);
        assert(ds.export_notes_html(out));
        assert(fs::file_size(out) > 0);
    }

    // 空库：合法文档零 section
    fs::remove(data);
    fs::remove(out);
    {
        DataStoreStd ds;
        ds.set_storage_path(data);
        NotesExportResultStd r = UnidictCoreStd::export_notes_html(ds, out);
        assert(r.ok && r.note_count == 0);
        const std::string html = read_file(out);
        assert(has_substr(html, "<h1>Unidict notes</h1>"));
        assert(!has_substr(html, "<section>"));
    }

    // 不可写路径：父路径是已存在文件 → 打开失败兜底
    auto block = (fs::current_path() / "build-local" / "notes_export_block").string();
    {
        std::ofstream b(block, std::ios::binary | std::ios::trunc);
        b << "x";
    }
    {
        DataStoreStd ds;
        ds.set_storage_path(data);
        NotesExportResultStd r =
            UnidictCoreStd::export_notes_html(ds, block + "/out.html");
        assert(!r.ok);
        assert(r.note_count == 0);
        assert(r.error.find("cannot open output file") != std::string::npos);
    }

    std::cout << "notes_export_std_test: all assertions passed\n";
    return 0;
}
