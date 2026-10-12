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

    // ---- CSV 导入（导入导出批）：导出件回吃 + 跳过语义 + 容差 ----
    {
        // 含换行笔记先入导出件：引号内换行是数据，回吃须逐字节还原
        ds.set_note("multiline", "line1\nline2");
        ds.add_vocabulary_item({"multiline", "multi"});
        assert(ds.export_vocabulary_csv(csv.string()));

        DataStoreStd di;
        fs::path pi = fs::current_path() / "build-local" / "ds_m3_import.json";
        di.set_storage_path(pi.string());
        di.clear_vocabulary();

        // 全量回吃：Apple/banana/q"uote/multiline 四条 + 各自标签笔记
        auto r1 = di.import_vocabulary_csv(csv.string());
        assert(r1.imported == 4);
        assert(r1.skipped == 0);
        auto iv = di.get_vocabulary();
        assert(iv.size() == 4);
        for (const auto& v : iv) {
            if (v.word == "Apple") {
                assert(v.definition == "fruit");
                assert(v.tags.size() == 1 && v.tags[0] == "fruit-tag");
            } else if (v.word == "banana") {
                assert(v.tags.size() == 2 &&
                       v.tags[0] == "fruit-tag" && v.tags[1] == "shopping");
            } else if (v.word == "q\"uote") {
                assert(v.definition == "has, comma");
                assert(di.get_note("q\"uote") ==
                       "note with \"quotes\" and, comma");
            } else if (v.word == "multiline") {
                assert(di.get_note("multiline") == "line1\nline2");
            } else {
                assert(false);  // 不速之客
            }
        }

        // 重放幂等：已存在（大小写不敏感）整行跳过，数据零变化
        auto r2 = di.import_vocabulary_csv(csv.string());
        assert(r2.imported == 0);
        assert(r2.skipped == 4);
        assert(di.get_vocabulary().size() == 4);

        // 部分已存在：小写 apple 命中 APPLE——释义不被覆盖，其余补入
        di.clear_vocabulary();
        di.add_vocabulary_item({"apple", "user edited"});
        auto r3 = di.import_vocabulary_csv(csv.string());
        assert(r3.imported == 3);
        assert(r3.skipped == 1);
        for (const auto& v : di.get_vocabulary()) {
            if (v.word == "apple") assert(v.definition == "user edited");
        }

        // 容差：空词行跳过、列缺失兜空、末行无换行收尾、全空行不计
        const fs::path rough = fs::current_path() / "build-local" /
                               "ds_m3_rough.csv";
        {
            std::ofstream o(rough, std::ios::binary);
            o << "\"\",\"x\",\"y\",\"z\"\nPlain,def only\nLast";
        }
        di.clear_vocabulary();
        auto r4 = di.import_vocabulary_csv(rough.string());
        assert(r4.imported == 2);   // Plain + Last
        assert(r4.skipped == 1);    // 空词行
        assert(di.get_note("Plain").empty());

        // 文件不可读：imported=-1
        auto r5 = di.import_vocabulary_csv(
            (fs::current_path() / "build-local" / "no_such.csv").string());
        assert(r5.imported == -1);
    }

    std::cout << "OK\n";
    return 0;
}
