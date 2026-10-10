// DictionaryManagerStd 失败隔离两档单测（P-3 3.2b）：
// 解析失败 → quarantined=true 持久隔离；文件丢失/扩展名不支持 → 不建
// 记录；retry 成功摘除记录、再失败确认隔离。语义锚 unidict_core.h:38-46。

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/dictionary_manager_std.h"

using namespace UnidictCoreStd;

static std::filesystem::path dir() {
    namespace fs = std::filesystem;
    fs::path d = fs::current_path() / "build-local" / "dict_mgr_quar";
    fs::create_directories(d);
    return d;
}

static void write_json(const std::filesystem::path& p, const std::string& content) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << content;
}

static const char* kGood =
    "{\"name\":\"QGood\",\"description\":\"q\",\"entries\":"
    "[{\"word\":\"alpha\",\"definition\":\"first\"}]}";

int main() {
    namespace fs = std::filesystem;
    fs::path base = dir();

    // --- T1 文件丢失：直接失败，不建记录 ---
    {
        DictionaryManagerStd m;
        assert(!m.add_dictionary((base / "ghost.json").string()));
        assert(m.failed_dictionaries().empty());
        assert(m.last_error().find("does not exist") != std::string::npos);
    }

    // --- T2 扩展名不支持：直接失败，不建记录 ---
    {
        fs::path xyz = base / "weird.xyz";
        write_json(xyz, "whatever");
        DictionaryManagerStd m;
        assert(!m.add_dictionary(xyz.string()));
        assert(m.failed_dictionaries().empty());
        assert(m.last_error().find("unsupported extension") != std::string::npos);
    }

    // --- T3 解析失败：入持久隔离档 ---
    {
        fs::path broken = base / "broken.json";
        write_json(broken, "{not valid json");
        DictionaryManagerStd m;
        assert(!m.add_dictionary(broken.string()));
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].file_path == broken.string());
        assert(m.failed_dictionaries()[0].quarantined);
        assert(!m.failed_dictionaries()[0].reason.empty());
        // 词典本体不在装载列表
        assert(m.loaded_dictionaries().empty());
    }

    // --- T4 重试未知路径：失败且不新建记录 ---
    {
        DictionaryManagerStd m;
        assert(!m.retry_failed_dictionary((base / "nope.json").string()));
        assert(m.failed_dictionaries().empty());
        assert(m.last_error().find("not in the failed list") != std::string::npos);
    }

    // --- T5 修复后重试：成功并摘除记录；再弄坏再试：确认隔离 ---
    {
        fs::path d = base / "cycle.json";
        write_json(d, "{bad");
        DictionaryManagerStd m;
        assert(!m.add_dictionary(d.string()));
        assert(m.failed_dictionaries().size() == 1);

        write_json(d, kGood);
        assert(m.retry_failed_dictionary(d.string()));
        assert(m.failed_dictionaries().empty());
        assert(m.loaded_dictionaries() == std::vector<std::string>{"QGood"});
        assert(m.search_word("alpha") == "first");

        write_json(d, "{bad again");
        // 已加载实例不受文件变坏影响（内存态）；直接重加会失败入隔离
        assert(!m.add_dictionary(d.string()));
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].quarantined);
        // 重试又失败 → 原因刷新 + 确认隔离
        assert(!m.retry_failed_dictionary(d.string()));
        assert(m.failed_dictionaries().size() == 1);
        assert(m.failed_dictionaries()[0].quarantined);
        assert(m.failed_dictionaries()[0].reason.find("Failed to load") != std::string::npos);
    }

    // --- T6 重加成功摘记录、他典记录不受牵连、clear 全清 ---
    {
        fs::path good = base / "good.json";
        fs::path bad = base / "still_bad.json";
        write_json(good, kGood);
        write_json(bad, "{bad");
        DictionaryManagerStd m;
        assert(!m.add_dictionary(bad.string()));
        assert(m.failed_dictionaries().size() == 1);
        // 加载好词典：自己的记录（无）不受影响，坏词典记录保留
        assert(m.add_dictionary(good.string()));
        assert(m.failed_dictionaries().size() == 1);
        // 坏词典修好后直接 add_dictionary（不经 retry）同样摘除记录
        write_json(bad, kGood);
        assert(m.add_dictionary(bad.string()));
        assert(m.failed_dictionaries().empty());
        assert(m.loaded_dictionaries().size() == 2);
        // clear：词典与失败记录一起清
        m.clear_dictionaries();
        assert(m.failed_dictionaries().empty());
        assert(m.loaded_dictionaries().empty());
    }

    // --- T7 显式遗忘：不在册记 last_error 假返回；在册摘除清 last_error ---
    {
        fs::path d = base / "forgotten.json";
        write_json(d, "{bad");
        DictionaryManagerStd m;
        assert(!m.add_dictionary(d.string()));
        assert(m.failed_dictionaries().size() == 1);
        assert(!m.forget_failed_dictionary((base / "absent.json").string()));
        assert(m.last_error().find("not in the failed list") != std::string::npos);
        // 遗忘后可重新 add（记录摘除解除重复装载去重挡板之外，
        // 坏文件依旧失败——遗忘只是放弃重试入口）
        assert(m.forget_failed_dictionary(d.string()));
        assert(m.failed_dictionaries().empty());
        assert(m.last_error().empty());
        assert(!m.add_dictionary(d.string()));
        assert(m.failed_dictionaries().size() == 1);
    }

    return 0;
}