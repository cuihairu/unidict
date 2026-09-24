// 生词本分组标签：set 命中/未命中、跨实例持久往返、空标签清除、旧格式兼容
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::VocabItemStd;

int main() {
    namespace fs = std::filesystem;
    auto p = (fs::current_path() / "build-local" / "unidict_std_tags_test.json").string();
    fs::create_directories(fs::path(p).parent_path());
    fs::remove(p);

    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        ds.clear_vocabulary();
        ds.add_vocabulary_item({"apple", "a fruit"});
        ds.add_vocabulary_item({"pear", "another fruit"});

        // 未命中词：返回假且不动数据
        assert(!ds.set_vocabulary_item_tags("banana", {"fruit"}));
        assert(ds.get_vocabulary().size() == 2);

        // 命中（大小写不敏感）：设置标签
        assert(ds.set_vocabulary_item_tags("Apple", {"fruit", "basic"}));
        assert(ds.set_vocabulary_item_tags("pear", {})); // 空标签=合法状态
    }

    // 跨实例持久往返
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        const auto v = ds.get_vocabulary();
        assert(v.size() == 2);
        bool apple_ok = false, pear_ok = false;
        for (const auto& it : v) {
            if (it.word == "apple") {
                apple_ok = it.tags.size() == 2 && it.tags[0] == "fruit" && it.tags[1] == "basic";
            } else if (it.word == "pear") {
                pear_ok = it.tags.empty();
            }
        }
        assert(apple_ok && pear_ok);

        // 清标签后往返也应为空
        assert(ds.set_vocabulary_item_tags("apple", {}));
        DataStoreStd ds2;
        ds2.set_storage_path(p);
        for (const auto& it : ds2.get_vocabulary()) {
            if (it.word == "apple") assert(it.tags.empty());
        }
    }

    // 旧格式兼容：手写无 tags 字段的数据文件，load 后 tags 为空不崩
    {
        auto legacy = (fs::current_path() / "build-local" / "unidict_std_legacy.json").string();
        std::ofstream out(legacy, std::ios::binary | std::ios::trunc);
        out << "{\n  \"history\": [\"hi\"],\n  \"vocab\": [\n"
            << "    {\"word\":\"old\",\"definition\":\"legacy\",\"added_at\":123}\n"
            << "  ]\n}\n";
        out.close();
        DataStoreStd ds;
        ds.set_storage_path(legacy);
        const auto v = ds.get_vocabulary();
        assert(v.size() == 1 && v[0].word == "old" && v[0].tags.empty());
    }

    std::cout << "OK\n";
    return 0;
}
