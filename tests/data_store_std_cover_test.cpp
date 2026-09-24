// DataStoreStd 补覆盖：storage_path getter、首次 load 时对不存在
// 路径建目录并落盘、损坏 JSON 段（键后无数组/对象）容错、词条含
// 引号/换行/制表符时的 json_escape 转义写盘。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;

static std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    namespace fs = std::filesystem;
    fs::path base = fs::current_path() / "build-local" / "ds_cover";
    fs::create_directories(base);

    // ===== 1) storage_path getter =====
    {
        DataStoreStd ds;
        fs::path f = base / "store.json";
        ds.set_storage_path(f.string());
        assert(ds.storage_path() == f.string());
    }

    // ===== 2) 首次 load：路径不存在 → 建父目录 + 空库落盘 =====
    {
        fs::path f = base / "fresh" / "sub" / "store.json";
        fs::remove_all(base / "fresh");
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        assert(ds.load());   // 走 create_directories(parent) + save()
        assert(fs::exists(f));
    }

    // ===== 3) 损坏 JSON：段落有开头无闭合 → 扫描到串尾仍未归零，回落空 =====
    {
        fs::path f = base / "broken.json";
        {
            std::ofstream out(f.string(), std::ios::binary | std::ios::trunc);
            out << "{\"history\": [\"a\", \"b\"";
        }
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        assert(ds.load());
        assert(ds.get_search_history().empty());
    }

    // ===== 4) 特殊字符转义：json_escape 的 \" \n \r \t 分支写盘 =====
    {
        fs::path f = base / "escape.json";
        DataStoreStd ds;
        ds.set_storage_path(f.string());
        ds.add_search_history("say \"hi\"\nnext\rrow\tend");
        VocabItemStd v;
        v.word = "quo\"te";
        v.definition = "line1\nline2\tend\r";
        ds.add_vocabulary_item(v);
        assert(ds.save());

        const std::string data = read_file(f.string());
        assert(data.find("\\\"") != std::string::npos);
        assert(data.find("\\n") != std::string::npos);
        assert(data.find("\\r") != std::string::npos);
        assert(data.find("\\t") != std::string::npos);

        // 重载：宽容解析器按转义还原字符串
        DataStoreStd back;
        back.set_storage_path(f.string());
        assert(back.load());
        auto hist = back.get_search_history();
        // 历史首条应还原出引号与换行
        assert(!hist.empty());
        assert(hist.front().find("say \"hi\"") == 0);
    }

    return 0;
}
