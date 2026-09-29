// Android 模拟器运行时冒烟（M0 可行性验证的留存源码，见 docs/mobile_plan.md §5）。
// 不进构建系统：NDK clang++ 直链 build 出的静态库（命令行见 mobile_plan.md §5）。
// 覆盖面 = 移动端四功能里的 core 侧：查词（词典装载/精确/前缀/全文）、
// 生词本（词单+标签+持久化往返）、词典文件读取；TTS/SAF 属平台壳，不在此。
// 实测结果：AVD test30（android-30 x86_64）上 ANDROID-SMOKE-OK（2026-09-29）。
#include <cassert>
#include <cstdlib>
#include <iostream>

#include "std/data_store_std.h"
#include "std/dictionary_manager_std.h"
#include "std/path_utils_std.h"

using namespace UnidictCoreStd;

int main() {
    // 1) path_utils：环境回落面（SAF 导入目录的基础）
    ::setenv("UNIDICT_DATA_DIR", "/data/local/tmp/unidict_smoke/data", 1);
    assert(PathUtilsStd::data_dir() == "/data/local/tmp/unidict_smoke/data");

    // 2) 查词：JSON 词典装载 + 精确/前缀/全文
    DictionaryManagerStd dm;
    assert(dm.add_dictionary("/data/local/tmp/unidict_smoke/dict.json"));
    dm.build_index();  // 前缀/模糊等索引检索须先建索引（真实使用路径）
    assert(dm.exact_search("hello").size() == 1);
    const auto pref = dm.prefix_search("wo");
    bool has_world = false;
    for (const auto& w : pref) has_world |= (w == "world");
    assert(has_world);
    assert(!dm.full_text_search("greeting").empty());
    assert(dm.search_word("unidict").find("language workbench") != std::string::npos);

    // 3) 生词本：加入 + 标签 + CSV 导出 + 重载往返
    const std::string store = "/data/local/tmp/unidict_smoke/store.json";
    {
        DataStoreStd ds;
        ds.set_storage_path(store);
        VocabItemStd item;
        item.word = "unidict";
        item.definition = "An open-source dictionary workbench.";
        item.tags = {"mobile"};
        ds.add_vocabulary_item(item);
        ds.add_search_history("hello");
        ds.set_note("unidict", "smoke note");
        assert(ds.get_vocabulary().size() == 1);
    }
    {
        DataStoreStd ds2;
        ds2.set_storage_path(store);
        const auto vocab = ds2.get_vocabulary();
        assert(vocab.size() == 1 && vocab[0].word == "unidict");
        assert(vocab[0].tags.size() == 1 && vocab[0].tags[0] == "mobile");
        assert(ds2.get_note("unidict") == "smoke note");
        assert(!ds2.get_search_history(10).empty());
        assert(ds2.export_vocabulary_csv("/data/local/tmp/unidict_smoke/vocab.csv"));
    }

    std::cout << "ANDROID-SMOKE-OK" << std::endl;
    return 0;
}
