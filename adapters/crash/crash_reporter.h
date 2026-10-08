#ifndef UNIDICT_CRASH_REPORTER_H
#define UNIDICT_CRASH_REPORTER_H

#include <map>
#include <string>

// Crashpad 崩溃采集薄封装（docs/crash_report_plan.md）。
//
// 职责边界：这里只做 crashpad 客户端接线（启动独立 handler 进程、
// 崩溃库初始化、转储计数、诊断触发），平台路径与日志上报由壳
// （qmlui）负责——保持无 Qt，与 adapters/pron 同一口径。
//
// 隐私口径（§7）：上传 URL 恒为空 = handler 只落盘不外发；库级
// SetUploadsEnabled(false) 双保险。外发属数据外发默认关，未来若开
// 须过 §7 三前置（显式授权/端点拍板/隐私声明）。
namespace crash {

// 启动崩溃采集：handlerPath=crashpad_handler 可执行文件路径，
// dbDir=崩溃库根目录（自动创建），annotations 进 dump 元数据
// （建议带应用版本，符号还原时对版本）。
// 同步等待 handler 就绪（asynchronous_start=false）——返回真即可
// 覆盖后续所有崩溃。失败返回假，调用方照常运行：采集是增强不是依赖。
bool init(const std::string& handlerPath, const std::string& dbDir,
          const std::map<std::string, std::string>& annotations = {});

// 崩溃库里 *.dmp 计数（new/pending/completed 递归；目录不存在或
// 未初始化返回 0）——启动时检出历史转储用
int dumpCount(const std::string& dbDir);

// 诊断样例：空指针解引用。仅 UNIDICT_CRASH_TEST=nullderef 时由
// main 调用（验收口径 §9.2），volatile 防优化消除
[[noreturn]] void triggerNullDerefCrash();

}  // namespace crash

#endif  // UNIDICT_CRASH_REPORTER_H
