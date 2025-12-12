#include "performance_monitor_std.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

namespace UnidictCoreStd {

RealTimeMonitor::RealTimeMonitor(const std::string& output_file)
    : output_file_(output_file), is_monitoring_(false), should_stop_(false) {

    // 启动监控线程
    monitor_thread_ = std::thread([this]() {
        this->monitor_loop();
    });
}

RealTimeMonitor::~RealTimeMonitor() {
    stop();
}

void RealTimeMonitor::start() {
    if (is_monitoring_.exchange(true)) {
        UNIDICT_LOG(LOG_INFO, "Real-time performance monitoring started");
        is_monitoring_ = true;
        should_stop_ = false;
        last_check_ = std::chrono::steady_clock::now();
    }
}

void RealTimeMonitor::stop() {
    should_stop_ = true;
    UNIDICT_LOG(LOG_INFO, "Stopping real-time performance monitoring");
}

void RealTimeMonitor::monitor_loop() {
    auto check_interval = std::chrono::milliseconds(1000); // 每1秒检查一次

    while (!should_stop_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_);

        if (elapsed >= check_interval) {
            last_check_ = now;
            // 可以在这里添加性能检查逻辑
        }

        // 短暂避免CPU占用
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace UnidictCoreStd