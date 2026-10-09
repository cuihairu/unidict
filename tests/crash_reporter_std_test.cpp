// 崩溃采集薄封装的失败路径回归（docs/crash_report_plan.md §9.4）。
// 只钉「采集失败不拖垮应用」契约：init 各失败形态返回假、dumpCount
// 对不存在/空目录返回 0。真崩溃→dump→符号化链路不进单测（会杀测试
// 进程），由 UNIDICT_CRASH_TEST=nullderef 验收口径覆盖。
#if defined(_WIN32)
#include <process.h>  // _getpid
static int test_pid() { return _getpid(); }
#else
#include <unistd.h>
static int test_pid() { return getpid(); }
#endif

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

#include "crash_reporter.h"

namespace fs = std::filesystem;

static std::string makeTempDb() {
    const auto base = fs::temp_directory_path() /
                      ("unidict_crash_test_" + std::to_string(test_pid()));
    fs::create_directories(base);
    return base.string();
}

int main() {
    // init 失败形态：空 handler 路径 / 空库目录 → 假，且不动文件系统
    assert(!crash::init("", "/tmp/whatever"));
    assert(!crash::init("/tmp/whatever", ""));

    // handler 缺失（真实失败路径）：crashpad 的 StartHandler 在 execv
    // 失败时死等握手不返回（实测挂死，见 crash_reporter.cpp 前置检查
    // 注释）——薄封装必须在发起 fork 前拦下并返回假
    const std::string db = makeTempDb();
    assert(!crash::init("/nonexistent/crashpad_handler", db));

    // dumpCount：目录不存在 → 0；库目录存在但无转储 → 0
    assert(crash::dumpCount("/nonexistent/unidict/crash/db") == 0);
    assert(crash::dumpCount(db) == 0);

    std::printf("crash_reporter_std_test: all failure-path pins passed\n");
    return 0;
}
