// DictionaryManagerStd::scan_directory 回归（P-6 词典库 UI 导入面 +
// cli --scan-dir 共用口径）：registry 扩展名收集、装载/隔离去重、
// 排序稳定性、边界（不存在目录/空目录/不支持扩展名）。
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "std/dictionary_manager_std.h"
#include "std/parser_registry_std.h"

namespace fs = std::filesystem;
using namespace UnidictCoreStd;

static fs::path fresh_dir(const char* leaf) {
    const fs::path dir = fs::current_path() / "build-local" / "dm_scan" / leaf;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

static void write_text(const fs::path& p, const std::string& s) {
    std::ofstream o(p, std::ios::trunc);
    o << s;
}

static fs::path write_dict(const fs::path& p, const std::string& name) {
    std::ofstream o(p, std::ios::trunc);
    o << "{\"name\":\"" << name << "\",\"description\":\"d\",\"entries\":["
      << "{\"word\":\"w\",\"definition\":\"def\"}]}";
    return p;
}

static bool contains(const std::vector<std::string>& v, const fs::path& p) {
    const std::string key = fs::canonical(p).string();
    return std::find(v.begin(), v.end(), key) != v.end();
}

int main() {
    DictionaryManagerStd mgr;

    // ---- T1 不存在目录 / 空目录：空表，不动 last_error ----
    {
        assert(mgr.scan_directory((fresh_dir("t1") / "nope").string()).empty());
        assert(mgr.last_error().empty());
        assert(mgr.scan_directory(fresh_dir("t1").string()).empty());
    }

    // ---- T2 registry 扩展名收集 + 递归 + 排序确定 ----
    {
        const fs::path root = fresh_dir("t2");
        const fs::path a = write_dict(root / "a.json", "A");
        const fs::path b = write_dict(root / "b.mdx", "B");  // 内容假但扩展名对
        fs::create_directories(root / "sub" / "deep");
        const fs::path c = write_dict(root / "sub" / "deep" / "c.ifo", "C");
        write_text(root / "readme.txt", "hi");  // .txt 已注册 → 应收
        write_text(root / "skip.bin", "x");     // 未注册 → 排除
        fs::create_directories(root / "adir.md") ; // 目录不该收（扩展名巧合）

        const std::vector<std::string> found = mgr.scan_directory(root.string());
        assert(contains(found, a));
        assert(contains(found, b));
        assert(contains(found, c));
        assert(contains(found, root / "readme.txt"));
        assert(!contains(found, root / "skip.bin"));
        assert(!contains(found, root / "adir.md"));
        // 排序稳定：等于自身排序副本
        assert(std::is_sorted(found.begin(), found.end()));
    }

    // ---- T3 已装载去重：add 后同文件不再返回 ----
    {
        const fs::path root = fresh_dir("t3");
        const fs::path a = write_dict(root / "loaded.json", "Loaded");
        const fs::path b = write_dict(root / "fresh.json", "Fresh");
        assert(mgr.add_dictionary(a.string()));
        const std::vector<std::string> found = mgr.scan_directory(root.string());
        assert(!contains(found, a));
        assert(contains(found, b));
    }

    // ---- T4 持久隔离去重：解析失败进隔离档后不再返回 ----
    {
        const fs::path root = fresh_dir("t4");
        // .json 扩展名支持但内容非法 → 解析失败 → quarantined
        const fs::path bad = root / "broken.json";
        write_text(bad, "{ this is not valid json");
        const fs::path good = write_dict(root / "ok.json", "OK");
        assert(!mgr.add_dictionary(bad.string()));
        bool quarantined = false;
        for (const auto& f : mgr.failed_dictionaries()) {
            if (fs::canonical(f.file_path) == fs::canonical(bad) && f.quarantined)
                quarantined = true;
        }
        assert(quarantined);
        const std::vector<std::string> found = mgr.scan_directory(root.string());
        assert(!contains(found, bad));
        assert(contains(found, good));
    }

    // ---- T5 相对路径输入亦可扫（canonical 归一不破损）----
    {
        const fs::path root = fresh_dir("t5");
        write_dict(root / "one.json", "One");
        const std::vector<std::string> found = mgr.scan_directory(root.string());
        assert(found.size() == 1);
        assert(fs::path(found[0]).is_absolute());
    }

    std::printf("dictionary_manager_scan_std_test: all assertions passed\n");
    return 0;
}
