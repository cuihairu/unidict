#pragma once

#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <sstream>

namespace UnidictCoreStd {

/**
 * @brief 性能监控器类
 *
 * 提供全面的应用程序性能监控功能，包括：
 * - 方法执行时间测量
 * - 内存使用量跟踪
 * - 系统资源监控
 * - 性能计数器
 * - 实时性能分析
 * - 性能报告生成
 */
class PerformanceMonitorStd {
public:
    /**
     * @brief 性能计时器
     */
    class Timer {
    private:
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_time_;
        bool is_running_ = false;

    public:
        explicit Timer(const std::string& name) : name_(name) {}

        void start() {
            start_time_ = std::chrono::high_resolution_clock::now();
            is_running_ = true;
        }

        void stop() {
            if (is_running_) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);
                std::cout << "[PERF] " << name_ << ": " << duration.count() << "ms" << std::endl;
                is_running_ = false;
            }
        }

        void reset() {
            start_time_ = std::chrono::high_resolution_clock::now();
            is_running_ = true;
        }

        void restart() {
            stop();
            start();
        }

        void lap() {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_);
            std::cout << "[PERF] " << name_ << " lap: " << duration.count() << "ms" << std::endl;
            start_time_ = now;
        }

        double elapsed_ms() const {
            if (is_running_) {
                auto now = std::chrono::high_resolution_clock::now();
                return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
            }
            return 0.0;
        }

        std::chrono::high_resolution_clock::time_point start_time() const {
            return start_time_;
        }
    };

    /**
     * @brief 内存使用监控器
     */
    class MemoryMonitor {
    private:
        size_t peak_usage_ = 0;
        size_t current_usage_ = 0;
        size_t allocations_ = 0;
        size_t deallocations_ = 0;
        std::chrono::steady_clock::time_point last_check_ = std::chrono::steady_clock::now();

    public:
        MemoryMonitor() = default;
        ~MemoryMonitor() = default;

        void record_allocation(size_t size) {
            current_usage_ += size;
            allocations_++;
        }

        void record_deallocation(size_t size) {
            current_usage_ -= size;
            deallocations_++;
        }

        void update_current_usage() {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_check_).count();
            if (duration >= 1000) { // 每1秒检查一次
                last_check_ = now;
                // 在实际实现中，这里应该获取真实的内存使用情况
                // 这里使用平台特定的API
                # size_t current_usage = get_current_memory_usage();
            }
        }

        void start_monitoring() {
            peak_usage_ = 0;
            current_usage_ = 0;
            allocations_ = 0;
            deallocations_ = 0;
            last_check_ = std::chrono::steady_clock::now();
        }

        void stop_monitoring() {
            update_current_usage();
        }

        size_t get_peak_usage() const { return peak_usage_; }
        size_t get_current_usage() const { return current_usage_; }
        size_t get_allocation_count() const { return allocations_; }
        size_t get_deallocation_count() const { return deallocations_; }

        std::string get_memory_report() const {
            std::ostringstream report;
            report << "Memory Usage Report:\\n";
            report << "  Peak usage: " << (peak_usage_ / 1024 / 1024) << " MB\\n";
            report << "  Current usage: " << (current_usage_ / 1024 / 1024) << " MB\\n";
            report << "  Total allocations: " << allocations_ << "\\n";
            report << "  Total deallocations: " << deallocations_ << "\\n";
            report << "  Allocation efficiency: "
                     << (allocations_ > 0 ? (current_usage_ / allocations_ / 1024 / 1024) : 0) << " MB/alloc\\n";
            return report.str();
        }
    };

    /**
     * @brief 性能计数器
     */
    class PerformanceCounter {
    private:
        std::string name_;
        std::atomic<uint64_t> count_;
        std::atomic<uint64_t> total_;
        std::atomic<double> average_;

    public:
        PerformanceCounter(const std::string& name) : name_(name), count_(0), total_(0), average_(0.0) {}

        void increment() {
            count_.fetch_add(1);
        }

        void add_value(uint64_t value) {
            count_.fetch_add(value);
        }

        void update_average() {
            total_.store(count_);
        }

        double get_count() const { return static_cast<double>(count_); }
        uint64_t get_total() const { return total_; }
        double get_average() const {
            update_average();
            return average_;
        }

        std::string get_name() const { return name_; }
        std::string get_report() const {
            std::ostringstream report;
            report << name_ << " Counter:\\n";
            report << "  Count: " << get_count() << "\\n";
            report << "  Total: " << get_total() << "\\n";
            report << "  Average: " << std::fixed << std::setprecision(2) << get_average() << "\\n";
            return report.str();
        }
    };

    /**
     * @brief 性能报告生成器
     */
    class PerformanceReporter {
    private:
        std::vector<std::unique_ptr<Timer>> timers_;
        std::vector<std::unique_ptr<PerformanceCounter>> counters_;
        MemoryMonitor memory_monitor_;
        std::chrono::steady_clock::time_point start_time_;
        std::string output_file_;

    public:
        explicit PerformanceReporter(const std::string& output_file = "")
            : output_file_(output_file), start_time_(std::chrono::steady_clock::now()) {}

        ~PerformanceReporter() = default;

        void add_timer(std::unique_ptr<Timer> timer) {
            timers_.push_back(std::move(timer));
        }

        void add_counter(std::unique_ptr<PerformanceCounter> counter) {
            counters_.push_back(std::move(counter));
        }

        void start_monitoring() {
            memory_monitor_.start_monitoring();
        }

        void stop_monitoring() {
            memory_monitor_.stop_monitoring();
        }

        void generate_report() {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time_);

            std::cout << "\\n=== Performance Report ===\\n";
            std::cout << "Total duration: " << total_duration.count() << "ms\\n\\n";

            std::cout << "\\nTimers:\\n";
            for (const auto& timer : timers_) {
                std::cout << "  " << timer->get_report() << "\\n";
            }

            std::cout << "\\nCounters:\\n";
            for (const auto& counter : counters_) {
                std::cout << "  " << counter->get_report() << "\\n";
            }

            std::cout << "\\nMemory:\\n";
            std::cout << memory_monitor_.get_memory_report() << "\\n";

            // 写入到文件
            if (!output_file_.empty()) {
                std::ofstream report_file(output_file_);
                report_file << "=== Performance Report ===\\n";
                report_file << "Generated at: " <<
                    std::chrono::duration_cast<std::chrono::seconds>(end_time.time_since_epoch()).count() << "\\n";
                report_file << "Total duration: " << total_duration.count() << "ms\\n\\n\\n";

                report_file << "\\nTimers:\\n";
                for (const auto& timer : timers_) {
                    report_file << timer->get_report() << "\\n";
                }

                report_file << "\\nCounters:\\n";
                for (const auto& counter : counters_) {
                    report_file << counter->get_report() << "\\n";
                }

                report_file << "\\nMemory:\\n";
                report_file << memory_monitor_.get_memory_report() << "\\n";

                std::cout << "Performance report saved to: " << output_file_ << "\\n";
            }
        }

        void set_output_file(const std::string& file) {
            output_file_ = file;
        }
    };

    /**
     * @brief 实时性能监控器
     */
    class RealTimeMonitor {
    private:
        std::atomic<bool> is_monitoring_{false};
        std::thread monitor_thread_;
        std::atomic<bool> should_stop_{false};
        std::chrono::milliseconds update_interval_;
        std::unique_ptr<PerformanceReporter> reporter_;

        std::vector<std::unique_ptr<PerformanceCounter>> counters_;
        std::vector<std::unique_ptr<Timer>> timers_;

    public:
        RealTimeMonitor(std::chrono::milliseconds interval = std::chrono::milliseconds(1000))
            : update_interval_(interval), should_stop_(false), is_monitoring_(false) {}

        ~RealTimeMonitor() {
            stop();
        }

        void start() {
            if (is_monitoring_.exchange(true)) {
                should_stop_.store(false);
                monitor_thread_ = std::thread([this]() { this->monitor_loop(); });
            }
        }

        void stop() {
            should_stop_.store(true);
            if (monitor_thread_.joinable()) {
                monitor_thread_.join();
            }
        }

        void add_counter(const std::string& name) {
            counters_.push_back(std::make_unique<PerformanceCounter>(name));
            if (is_monitoring_.load()) {
                counters_.back()->update_average();
            }
        }

        void add_timer(const std::string& name) {
            timers_.push_back(std::make_unique<Timer>(name));
            if (is_monitoring_.load()) {
                timers_.back()->start();
            }
        }

        void add_reporter(std::unique_ptr<PerformanceReporter> reporter) {
            reporter_ = std::move(reporter);
            if (is_monitoring_.load()) {
                reporter_->start_monitoring();
            }
        }

        void remove_counter(const std::string& name) {
            counters_.erase(
                std::remove_if(counters_.begin(), counters_.end(),
                    [&](const std::unique_ptr<PerformanceCounter>& c) { return c->get_name() == name; }
                ));
        }

        void remove_timer(const std::string& name) {
            timers_.erase(
                std::remove_if(timers_.begin(), timers_.end(),
                    [&](const std::unique_ptr<Timer>& t) { return t->get_name() == name; }
                ));
        }

    private:
        void monitor_loop() {
            auto last_time = std::chrono::steady_clock::now();

            while (!should_stop_.load()) {
                std::this_thread::sleep_for(update_interval_);

                auto now = std::chrono::steady_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);

                // 更新所有计数器
                for (auto& counter : counters_) {
                    counter->update_average();
                }

                // 生成实时报告
                generate_realtime_report(duration);

                last_time = now;
            }
        }

        void generate_realtime_report(std::chrono::milliseconds duration) {
            if (reporter_) {
                // 可以在这里生成更复杂的实时报告
                // 例如：计算每秒的操作次数、内存增长速率等
                std::cout << "[MONITOR] Interval: " << duration.count() << "ms\\n";
            }
        }
    };

} // namespace UnidictCoreStd