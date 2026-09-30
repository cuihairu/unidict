// path_utils_std 分支缺口补测（真实缺边 33 条/21 行，排除既定止步口径后
// 的最大未收口簇——既有两个测试只驱动快乐路径：env 覆写非空值、days=1、
// 超限裁剪一次；空环境值、目录缺失/是文件/为空、under-limit 早退、
// days<=0 早退、缓存内子目录跳过等真实条件臂全部冷）。
//
// 逐边定性（gcov --json-format -b，throw 过滤后）：
//  - 可驱动真实缺边：getenv_c 空值臂、ensure_dir 新建/失败臂、
//    clear_cache 缺目录/目录是文件/空目录、cache_size_bytes 缺目录/
//    子目录、prune_cache_bytes 缺目录/子目录/under-limit 早退/循环
//    break/限额低于最小文件的循环耗尽、prune_days days<=0 早退/缺
//    目录/子目录。
//  - 源码侧 1 处死三元已清理：prune_cache_bytes 的 `ec2 ? 0 : file_size`
//    检查刚默认构造的 error_code（恒清除态），"? 0" 臂结构性不可达——
//    改为取尺寸后按 ec2 跳过，坏条目不再把 uintmax_t(-1) 混进 total。
//  - 止步（不加 EXCL，沿环境依赖臂口径）：三处 remove 失败臂
//    （clear_cache 的 ec2、prune_bytes 的 ec3、prune_days 的 ec2——
//    POSIX 卫兵 + root 下 chmod 假绿）、cache_size_bytes 的 file_size
//    失败臂与 prune_days 的 last_write_time 失败臂（is_regular_file
//    刚成功后 race 才可构造）、ensure_dir 的 create_directories 假 ∧
//    exists 真 臂（两次 stat 之间目录被建出的 race）、
//    prune_bytes 返回表达式的两个假臂（循环语义保证 total≤max 或
//    ok 已为假，见函数内注释）。

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/path_utils_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;
using namespace std::chrono;

namespace {

#if defined(_WIN32)
void set_env(const char* key, const std::string& value) {
    _putenv_s(key, value.c_str());
}
#else
void set_env(const char* key, const std::string& value) {
    ::setenv(key, value.c_str(), 1);
}
#endif

fs::path base_dir() {
    fs::path d = fs::current_path() / "build-local" / "pubr";
    fs::remove_all(d);  // hermetic：清掉上轮（可能中途夭折）的遗留状态
    fs::create_directories(d);
    return d;
}

fs::path put_file(const fs::path& p, size_t bytes) {
    std::ofstream o(p, std::ios::binary | std::ios::trunc);
    o << std::string(bytes, 'x');
    assert(o.good());
    return p;
}

std::string str_of(const fs::path& p) {
    return p.string();
}

} // namespace

// T1 环境解析矩阵：空值回落（getenv_c 的 `*v` 假臂）、非空生效、
// 未设置回落 cwd/data
static void test_env_matrix() {
    const fs::path base = base_dir();
    const fs::path cwd_data = fs::current_path() / "data";

    // 空值 = 未设置：回落 cwd/data
    set_env("UNIDICT_DATA_DIR", "");
    set_env("UNIDICT_CACHE_DIR", "");
    assert(PathUtilsStd::data_dir() == str_of(cwd_data));
    // 回落拼接断言按 fs::path 口径比较（期望值也用 operator/ 构造）：
    // Windows 原生分隔符是 '\'，生产侧 (fs::path(data_dir()) / "cache")
    // 返回 "...\data\cache"，字符串字面拼 "/cache" 的期望值只在 POSIX
    // 成立——此前 Windows CI 挂在本断言的根因正是口径错在期望值一侧，
    // 生产侧 fs 拼接合规（所有调用方都经 fs::path 消化分隔符）
    assert(fs::path(PathUtilsStd::cache_dir()) == cwd_data / "cache");

    // 非空：cache_dir 只认 UNIDICT_CACHE_DIR
    set_env("UNIDICT_DATA_DIR", str_of(base / "d1"));
    set_env("UNIDICT_CACHE_DIR", str_of(base / "c1"));
    assert(PathUtilsStd::data_dir() == str_of(base / "d1"));
    assert(PathUtilsStd::cache_dir() == str_of(base / "c1"));

    // cache_dir 未设置时落到 data_dir/cache（同上按 fs::path 口径）
    set_env("UNIDICT_CACHE_DIR", "");
    assert(fs::path(PathUtilsStd::cache_dir()) == base / "d1" / "cache");

    // 后续组统一用可用的 cache 目录
    set_env("UNIDICT_CACHE_DIR", str_of(base / "cache"));
}

// T2 ensure_dir：已存在早退真、新建嵌套真、父级是普通文件假
static void test_ensure_dir() {
    const fs::path base = base_dir();
    fs::create_directories(base / "exists");
    assert(PathUtilsStd::ensure_dir(str_of(base / "exists")));        // 早退真
    assert(PathUtilsStd::ensure_dir(str_of(base / "a" / "b" / "c"))); // 递归新建真
    assert(fs::is_directory(base / "a" / "b" / "c"));
    put_file(base / "reg.txt", 4);
    assert(!PathUtilsStd::ensure_dir(str_of(base / "reg.txt" / "sub"))); // ENOTDIR 假
}

// T3 clear_cache 矩阵：缺目录早退真 / 目录是普通文件（迭代器带 ec，
// 循环零次）/ 空目录（循环零次）/ 有内容逐项删除
static void test_clear_cache() {
    const fs::path base = base_dir();

    // 缺目录：exists 假臂早退
    const fs::path missing = base / "nope";
    set_env("UNIDICT_CACHE_DIR", str_of(missing));
    assert(PathUtilsStd::clear_cache());

    // 目录位置被普通文件占着：directory_iterator 构造带 ec，循环零次
    const fs::path as_file = put_file(base / "notdir", 2);
    set_env("UNIDICT_CACHE_DIR", str_of(as_file));
    assert(PathUtilsStd::clear_cache());

    // 空目录：`it != end` 首测即假
    const fs::path empty = base / "empty";
    fs::create_directories(empty);
    set_env("UNIDICT_CACHE_DIR", str_of(empty));
    assert(PathUtilsStd::clear_cache());

    // 有内容：全部删除
    const fs::path cache = base / "cache";
    fs::create_directories(cache / "sub");
    put_file(cache / "f1", 8);
    put_file(cache / "sub" / "f2", 8);
    set_env("UNIDICT_CACHE_DIR", str_of(cache));
    assert(PathUtilsStd::clear_cache());
    assert(fs::is_directory(cache) && fs::is_empty(cache));
}

// T4 cache_size_bytes 矩阵：缺目录 0 / 空目录 0 / 子目录不计入、
// 普通文件累加
static void test_cache_size() {
    const fs::path base = base_dir();

    set_env("UNIDICT_CACHE_DIR", str_of(base / "nope"));
    assert(PathUtilsStd::cache_size_bytes() == 0);  // 迭代器 ec 假臂

    const fs::path cache = base / "cache";
    fs::remove_all(cache);
    fs::create_directories(cache);
    set_env("UNIDICT_CACHE_DIR", str_of(cache));
    assert(PathUtilsStd::cache_size_bytes() == 0);  // 空目录

    fs::create_directories(cache / "sub");          // 子目录跳过臂
    put_file(cache / "a", 100);
    put_file(cache / "sub" / "b", 23);
    assert(PathUtilsStd::cache_size_bytes() == 123);
}

// T5 prune_cache_bytes：缺目录 under-limit 真（零条目）/ 空目录
// under-limit 真（76 早退 + 79 零进环）/ 精确超限删最旧到达标
// （79 break 臂）/ 缓存内子目录跳过（69 continue 臂）
static void test_prune_bytes() {
    const fs::path base = base_dir();
    const auto now = fs::file_time_type::clock::now();

    // 缺目录：迭代器 ec 假臂，total=0 <= max → 真
    set_env("UNIDICT_CACHE_DIR", str_of(base / "nope"));
    assert(PathUtilsStd::prune_cache_bytes(0));

    // 空目录 + 限额 0：循环零次，早退真
    const fs::path cache = base / "cache";
    fs::remove_all(cache);
    fs::create_directories(cache);
    set_env("UNIDICT_CACHE_DIR", str_of(cache));
    assert(PathUtilsStd::prune_cache_bytes(0));

    // 子目录跳过 + under-limit 真：目录不计入 total
    fs::create_directories(cache / "sub");
    put_file(cache / "keep", 10);
    assert(PathUtilsStd::prune_cache_bytes(10));
    assert(fs::exists(cache / "keep"));

    // 超限：最旧先删，达标即 break（最新文件存活）
    put_file(cache / "old1", 1000);
    put_file(cache / "old2", 2000);
    put_file(cache / "new", 3000);
    fs::last_write_time(cache / "old1", now - minutes(3));
    fs::last_write_time(cache / "old2", now - minutes(2));
    fs::last_write_time(cache / "new", now - minutes(1));
    assert(PathUtilsStd::prune_cache_bytes(3500));
    assert(!fs::exists(cache / "old1"));
    assert(!fs::exists(cache / "old2"));
    assert(fs::exists(cache / "new"));
    assert(PathUtilsStd::cache_size_bytes() == 3010);  // keep 10 + new 3000

    // 限额低于最小文件：删光全部条目仍超限 → 循环自然耗尽（非 break
    // 出环），清空后返回真。keep 的自然 mtime 晚于 new 的回写值，需
    // 显式回退保证"最旧先删"次序
    fs::last_write_time(cache / "keep", now - minutes(5));
    assert(PathUtilsStd::prune_cache_bytes(50));
    assert(PathUtilsStd::cache_size_bytes() == 0);
}

// T6 prune_cache_older_than_days：days<=0 早退真 / 缺目录真 /
// 子目录跳过 / 回写 mtime 驱动新旧两侧（删除与保留）
static void test_prune_days() {
    const fs::path base = base_dir();
    const auto now = fs::file_time_type::clock::now();

    // days = 0：87 早退真（既有测试只打过 days=1）
    assert(PathUtilsStd::prune_cache_older_than_days(0));
    assert(PathUtilsStd::prune_cache_older_than_days(-1));

    // 缺目录：迭代器 ec 假臂 → 真
    set_env("UNIDICT_CACHE_DIR", str_of(base / "nope"));
    assert(PathUtilsStd::prune_cache_older_than_days(1));

    // 子目录条目本身驱动 94 的 continue 假臂（目录不是普通文件），
    // 其内部文件仍被递归访问并按龄处理
    const fs::path cache = base / "cache";
    fs::remove_all(cache);
    fs::create_directories(cache / "sub");
    set_env("UNIDICT_CACHE_DIR", str_of(cache));
    const fs::path old_f = put_file(cache / "old", 8);
    const fs::path new_f = put_file(cache / "new", 8);
    const fs::path buried = put_file(cache / "sub" / "buried", 8);
    fs::last_write_time(old_f, now - hours(24 * 3));   // 3 天前
    fs::last_write_time(buried, now - hours(24 * 30)); // 30 天前（递归命中）
    assert(PathUtilsStd::prune_cache_older_than_days(1));
    assert(!fs::exists(old_f));    // 过期删除
    assert(fs::exists(new_f));     // 新鲜保留（t < cutoff 假臂）
    assert(!fs::exists(buried));   // 子目录内过期文件同样删除
}

int main() {
    test_env_matrix();
    test_ensure_dir();
    test_clear_cache();
    test_cache_size();
    test_prune_bytes();
    test_prune_days();
    return 0;
}
