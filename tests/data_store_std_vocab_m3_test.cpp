// M3 生词本全量（桌面可测面）：标签增删/筛选 + 笔记联查 + CSV 升级口径
// （UTF-8 BOM、tags/note 列）。std-only，断言风格同 data_store_std_*_test。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "std/data_store_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;

namespace {

std::string read_file(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    fs::path p = fs::current_path() / "build-local" / "ds_m3.json";
    fs::create_directories(p.parent_path());
    DataStoreStd ds;
    ds.set_storage_path(p.string());
    ds.clear_history();
    ds.clear_vocabulary();

    // ---- 标签增删（幂等/大小写不敏感词匹配/空标签拒绝/未命中） ----
    ds.add_vocabulary_item({"Apple", "fruit"});
    ds.add_vocabulary_item({"banana", "yellow fruit"});

    assert(!ds.add_vocabulary_item_tag("missing", "x"));   // 词条未命中
    assert(!ds.add_vocabulary_item_tag("apple", ""));      // 空标签拒绝
    assert(ds.add_vocabulary_item_tag("apple", "fruit-tag"));
    assert(ds.add_vocabulary_item_tag("APPLE", "exam"));   // 词大小写不敏感
    assert(ds.add_vocabulary_item_tag("apple", "exam"));   // 幂等：不重复
    assert(!ds.remove_vocabulary_item_tag("apple", "nope"));      // 无此标签
    assert(!ds.remove_vocabulary_item_tag("missing", "exam"));    // 词条未命中
    assert(ds.remove_vocabulary_item_tag("apple", "exam"));

    auto vocab = ds.get_vocabulary();
    assert(vocab.size() == 2);
    for (const auto& v : vocab) {
        if (v.word == "Apple") {
            assert(v.tags.size() == 1);
            assert(v.tags[0] == "fruit-tag");
        }
    }

    // ---- 标签筛选（保持存储序；多标签任一命中） ----
    assert(ds.add_vocabulary_item_tag("banana", "fruit-tag"));
    assert(ds.add_vocabulary_item_tag("banana", "shopping"));
    auto by_tag = ds.get_vocabulary_by_tag("fruit-tag");
    assert(by_tag.size() == 2);
    assert(by_tag[0].word == "Apple");
    assert(by_tag[1].word == "banana");
    assert(ds.get_vocabulary_by_tag("shopping").size() == 1);
    assert(ds.get_vocabulary_by_tag("nope-tag").empty());

    // ---- 笔记（持久化面已有专测；此处喂 CSV 联查数据） ----
    ds.set_note("banana", "smoothie staple");

    // ---- CSV 升级口径：UTF-8 BOM + word,definition,tags,note 四列 ----
    const fs::path csv = fs::current_path() / "build-local" / "ds_m3_export.csv";
    assert(ds.export_vocabulary_csv(csv.string()));
    const std::string content = read_file(csv);
    assert(content.compare(0, 3, "\xEF\xBB\xBF") == 0);            // BOM
    const std::string body = content.substr(3);
    assert(body.rfind("word,definition,tags,note\n", 0) == 0);     // 表头

    // Apple：无笔记；banana：带笔记 + 双标签
    assert(body.find("\"Apple\",\"fruit\",\"fruit-tag\",\"\"\n") != std::string::npos);
    assert(body.find("\"banana\",\"yellow fruit\",\"fruit-tag;shopping\","
                     "\"smoothie staple\"\n") != std::string::npos);

    // ---- CSV 引号转义与标签/笔记共存（esc 口径回归） ----
    ds.add_vocabulary_item({"q\"uote", "has, comma"});
    assert(ds.add_vocabulary_item_tag("q\"uote", "edge;case"));    // 标签含分号原样进格
    ds.set_note("q\"uote", "note with \"quotes\" and, comma");
    assert(ds.export_vocabulary_csv(csv.string()));
    const std::string content2 = read_file(csv);
    assert(content2.find("\"q\"\"uote\",\"has, comma\",\"edge;case\","
                         "\"note with \"\"quotes\"\" and, comma\"\n")
           != std::string::npos);

    // ---- 往返持久化：标签随 store 落盘读回 ----
    DataStoreStd ds2;
    ds2.set_storage_path(p.string());
    assert(ds2.get_vocabulary_by_tag("fruit-tag").size() == 2);

    std::cout << "OK\n";
    return 0;
}
