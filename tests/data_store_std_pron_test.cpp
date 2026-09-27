// M9 发音练习记录持久化：upsert/大小写不敏感/空词名忽略、跨实例往返
// （词分小数不被截断）、手写数据文件的容错读取（旧格式无该区段、
// 无键记录、缺字段、非数字字段）、区段名取长名的理由（释义里的
// `"pron":` 字面不该把区段认错）、清空。
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "std/data_store_std.h"

using UnidictCoreStd::DataStoreStd;
using UnidictCoreStd::PronRecordStd;

namespace {

bool near(double a, double b, double eps = 1e-6) {
    return std::fabs(a - b) < eps;
}

PronRecordStd rec(const std::string& word, double last, double best, int attempts,
                  long long at) {
    PronRecordStd r;
    r.word = word;
    r.last_score = last;
    r.best_score = best;
    r.attempts = attempts;
    r.last_at = at;
    return r;
}

void write_file(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << content;
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    const std::string dir = (fs::current_path() / "build-local").string();
    fs::create_directories(dir);
    const std::string p = dir + "/unidict_std_pron_test.json";
    fs::remove(p);

    // ---- 写入侧：upsert / 空词名忽略 / 大小写不敏感 ----
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        assert(!ds.get_pron_record("cat").has_value());
        ds.set_pron_record(rec("cat", 0.72, 0.72, 1, 1000));
        auto r = ds.get_pron_record("cat");
        assert(r.has_value());
        assert(near(r->last_score, 0.72) && r->attempts == 1 && r->last_at == 1000);
        // 累计更新同一条（大小写不敏感命中）
        ds.set_pron_record(rec("CAT", 0.85, 0.85, 2, 2000));
        assert(ds.get_pron_records().size() == 1);
        r = ds.get_pron_record("cat");
        assert(near(r->last_score, 0.85) && r->attempts == 2 && r->last_at == 2000);
        // 空词名没有键：忽略，不落无名记录
        ds.set_pron_record(rec("", 0.5, 0.5, 1, 3000));
        assert(ds.get_pron_records().size() == 1);
        assert(!ds.get_pron_record("dog").has_value());
        ds.set_pron_record(rec("dog", 0.3, 0.4, 1, 3000));
        assert(ds.get_pron_records().size() == 2);
    }

    // ---- 跨实例往返：词分小数不被截断（obj_int 会把 0.795 读成 0） ----
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        const auto all = ds.get_pron_records();
        assert(all.size() == 2);
        assert(near(all[0].last_score, 0.85) && near(all[0].best_score, 0.85));
        assert(near(all[1].last_score, 0.3) && near(all[1].best_score, 0.4));
        // 清空后落盘也清
        ds.clear_pron_records();
        assert(ds.get_pron_records().empty());
    }
    {
        DataStoreStd ds;
        ds.set_storage_path(p);
        assert(ds.get_pron_records().empty());
        assert(!ds.get_pron_record("cat").has_value());
    }

    // ---- 手写数据文件：旧格式 / 无键 / 缺字段 / 非数字字段 ----
    {
        const std::string legacy = dir + "/unidict_std_pron_legacy.json";
        write_file(legacy,
                   "{\n  \"history\": [],\n  \"vocab\": [\n"
                   "    {\"word\":\"old\",\"definition\":\"legacy\"}\n"
                   "  ]\n}\n");
        DataStoreStd ds;
        ds.set_storage_path(legacy);
        assert(ds.get_vocabulary().size() == 1);
        assert(ds.get_pron_records().empty());  // 旧格式无该区段
    }
    {
        const std::string messy = dir + "/unidict_std_pron_messy.json";
        // 释义里带 `"pron":` 字面（pron. 缩写不算罕见）不该把区段认错
        write_file(messy,
                   "{\n  \"history\": [],\n  \"vocab\": [\n"
                   "    {\"word\":\"pro\",\"definition\":\"abbr of pron. /ˈprɒn/\"}\n"
                   "  ],\n  \"notes\": [],\n"
                   "  \"pron_records\": [\n"
                   "    {\"word\":\"cat\",\"last_score\":0.795,\"best_score\":0.9,"
                   "\"attempts\":3,\"last_at\":42},\n"
                   "    {\"word\":\"\",\"last_score\":0.5,\"best_score\":0.5,"
                   "\"attempts\":1,\"last_at\":7},\n"
                   "    {\"word\":\"dog\",\"attempts\":2},\n"
                   "    {\"word\":\"bad\",\"last_score\":\"x\",\"best_score\":true,"
                   "\"attempts\":\"y\",\"last_at\":\"z\"}\n"
                   "  ]\n}\n");
        DataStoreStd ds;
        ds.set_storage_path(messy);
        const auto all = ds.get_pron_records();
        assert(all.size() == 3);  // 无键那条被跳过
        assert(all[0].word == "cat" && near(all[0].last_score, 0.795) &&
               near(all[0].best_score, 0.9) && all[0].attempts == 3 &&
               all[0].last_at == 42);
        assert(all[1].word == "dog" && near(all[1].last_score, 0.0) &&
               all[1].attempts == 2 && all[1].last_at == 0);  // 缺字段取默认
        assert(all[2].word == "bad" && near(all[2].last_score, 0.0) &&
               all[2].attempts == 0);  // 非数字字段不解析、不崩
    }
    {
        // 数值形态：整数词分、负数、小数后带垃圾、字段缺失
        const std::string nums = dir + "/unidict_std_pron_nums.json";
        write_file(nums,
                   "{\n  \"history\": [],\n  \"vocab\": [],\n  \"notes\": [],\n"
                   "  \"pron_records\": [\n"
                   "    {\"word\":\"one\",\"last_score\":1,\"best_score\":1},\n"
                   "    {\"word\":\"neg\",\"last_score\":-0.5,\"best_score\":-0.5},\n"
                   "    {\"word\":\"junk\",\"last_score\":0.5x,\"best_score\":0.25},\n"
                   "    {\"word\":\"none\"}\n"
                   "  ]\n}\n");
        DataStoreStd ds;
        ds.set_storage_path(nums);
        const auto all = ds.get_pron_records();
        assert(all.size() == 4);
        assert(near(all[0].last_score, 1.0));
        assert(near(all[1].last_score, -0.5));
        assert(near(all[2].last_score, 0.5));  // 小数部分读到位，尾垃圾忽略
        assert(near(all[3].last_score, 0.0) && all[3].attempts == 0);
    }

    fs::remove(p);
    std::cout << "data_store_std_pron_test: all assertions passed\n";
    return 0;
}
