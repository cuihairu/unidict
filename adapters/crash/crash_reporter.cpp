#include "crash_reporter.h"

#include <client/crash_report_database.h>
#include <client/crashpad_client.h>
#include <client/settings.h>

#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <base/strings/string_util.h>
#endif

namespace fs = std::filesystem;

namespace crash {
namespace {

// crashpad 的 base::FilePath 在 Windows 包 wstring、POSIX 包 string；
// 统一入口收 UTF-8 std::string，Windows 侧转宽（mini_chromium 工具）
#ifdef _WIN32
base::FilePath toFilePath(const std::string& utf8) {
    return base::FilePath(base::UTF8ToWide(utf8));
}
#else
base::FilePath toFilePath(const std::string& utf8) {
    return base::FilePath(utf8);
}
#endif

}  // namespace

bool init(const std::string& handlerPath, const std::string& dbDir,
          const std::map<std::string, std::string>& annotations) {
    if (handlerPath.empty() || dbDir.empty()) {
        return false;
    }
    // handler 存在性前置检查：crashpad 的 StartHandler 在 execv 失败时
    // 不返回假而是死等握手（double_fork_and_exec 打 FATAL 后父进程永
    // 挂——实测钉进 tests/crash_reporter_std_test.cpp）。缺失必须在此
    // 拦下，否则「采集失败不拖垮应用」会被最糟形态违反：启动挂死
    std::error_code ec;
    if (!fs::is_regular_file(handlerPath, ec)) {
        return false;
    }
    fs::create_directories(dbDir, ec);
    if (ec) {
        return false;
    }

    // 崩溃库先行：settings 显式关上传（双保险——即使将来误传非空
    // URL，库层也不外发，见头文件隐私口径）
    std::unique_ptr<crashpad::CrashReportDatabase> db =
        crashpad::CrashReportDatabase::Initialize(toFilePath(dbDir));
    if (!db) {
        return false;
    }
    crashpad::Settings* settings = db->GetSettings();
    if (settings) {
        settings->SetUploadsEnabled(false);
    }

    // 独立 handler 进程（url 空=只落盘不外发；restartable=true：
    // handler 意外退出后下次崩溃自动重启；同步启动=返回即可覆盖）
    crashpad::CrashpadClient client;
    return client.StartHandler(
        toFilePath(handlerPath),        // handler
        toFilePath(dbDir),              // database
        toFilePath(dbDir),              // metrics_dir（同库根）
        "",                             // url：空=不外发（隐私口径）
        annotations,                    // 进 dump 元数据的键值
        std::vector<std::string>(),     // handler 额外参数
        /*restartable=*/true,
        /*asynchronous_start=*/false);
}

int dumpCount(const std::string& dbDir) {
    std::error_code ec;
    if (!fs::is_directory(dbDir, ec)) {
        return 0;
    }
    int count = 0;
    for (auto it = fs::recursive_directory_iterator(
             dbDir, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            break;
        }
        if (it->is_regular_file(ec) &&
            it->path().extension() == ".dmp") {
            ++count;
        }
    }
    return count;
}

void triggerNullDerefCrash() {
    volatile int* null_pointer = nullptr;
    *null_pointer = 1;  // SIGSEGV：crashpad 捕获 → handler 写 minidump
    // 编译器理论上不可消除 volatile 写；真被消除则兜底 SIGILL
    __builtin_trap();
}

}  // namespace crash
