#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>

namespace UnidictCoreStd {

/**
 * @brief 内存优化器 - 智能内存管理和缓存系统
 *
 * 这个类提供了高级的内存管理功能，包括：
 * - 对象池管理
 * - 智能缓存策略
 * - 内存使用监控
 * - 垃圾回收机制
 */
class MemoryOptimizerStd {
public:
    /**
     * @brief 缓存策略枚举
     */
    enum class CacheStrategy {
        LRU,           // 最近最少使用
        LFU,           // 最少使用频次
        FIFO,          // 先进先出
        ADAPTIVE       // 自适应策略
    };

    /**
     * @brief 内存池配置
     */
    struct PoolConfig {
        size_t initial_capacity = 1024;      // 初始容量
        size_t max_capacity = 65536;        // 最大容量
        size_t grow_factor = 2;              // 增长因子
        bool auto_shrink = true;              // 自动收缩
        float shrink_threshold = 0.25f;       // 收缩阈值
    };

    /**
     * @brief 缓存配置
     */
    struct CacheConfig {
        size_t max_memory_usage = 64 * 1024 * 1024;  // 最大内存使用 (64MB)
        size_t max_items = 10000;                     // 最大项目数
        CacheStrategy strategy = CacheStrategy::ADAPTIVE; // 缓存策略
        float hit_ratio_threshold = 0.8f;               // 命中率阈值
        std::chrono::seconds cleanup_interval{300};        // 清理间隔 (5分钟)
    };

    /**
     * @brief 内存统计信息
     */
    struct MemoryStats {
        size_t total_allocated = 0;        // 总分配内存
        size_t peak_usage = 0;             // 峰值内存使用
        size_t current_usage = 0;         // 当前内存使用
        size_t cache_hits = 0;             // 缓存命中次数
        size_t cache_misses = 0;           // 缓存未命中次数
        double hit_ratio = 0.0;             // 命中率
        size_t gc_runs = 0;                 // 垃圾回收次数
        std::chrono::milliseconds gc_time{0};  // 垃圾回收总时间
    };

private:
    // 对象池
    template<typename T>
    class ObjectPool {
    private:
        std::vector<std::unique_ptr<T>> pool_;
        std::vector<bool> available_;
        PoolConfig config_;
        size_t next_alloc_ = 0;

    public:
        explicit ObjectPool(const PoolConfig& config) : config_(config) {
            pool_.reserve(config.initial_capacity);
            available_.resize(config.initial_capacity, true);
        }

        T* acquire() {
            // 查找可用对象
            for (size_t i = 0; i < available_.size(); ++i) {
                if (available_[i]) {
                    available_[i] = false;
                    return pool_[i].get();
                }
            }

            // 如果没有可用对象，尝试扩容
            if (pool_.size() < config_.max_capacity) {
                auto obj = std::make_unique<T>();
                auto* ptr = obj.get();
                pool_.emplace_back(std::move(obj));
                available_.push_back(false);
                return ptr;
            }

            // 池已满，临时创建
            return new T();
        }

        void release(T* obj) {
            // 查找对象在池中的位置
            for (size_t i = 0; i < pool_.size(); ++i) {
                if (pool_[i].get() == obj) {
                    available_[i] = true;
                    return;
                }
            }
            delete obj; // 不在池中，直接删除
        }

        void shrink() {
            if (!config_.auto_shrink) return;

            size_t active_count = 0;
            for (bool avail : available_) {
                if (!avail) active_count++;
            }

            if (active_count > available_.size() * config_.shrink_threshold) {
                // 收缩池
                size_t target_size = active_count * config_.grow_factor;
                target_size = std::max(target_size, config_.initial_capacity);

                // 保留活跃对象，释放其余的
                std::vector<std::unique_ptr<T>> new_pool;
                std::vector<bool> new_available;
                new_pool.reserve(target_size);
                new_available.resize(target_size, true);

                size_t preserved = 0;
                for (size_t i = 0; i < pool_.size() && preserved < target_size; ++i) {
                    if (!available_[i]) {
                        new_pool.emplace_back(std::move(pool_[i]));
                        new_available[preserved] = false;
                        preserved++;
                    }
                }

                pool_.swap(new_pool);
                available_.swap(new_available);
                next_alloc_ = 0;
            }
        }
    };

    // 智能缓存
    template<typename K, typename V>
    class AdaptiveCache {
    private:
        struct CacheItem {
            V value;
            size_t access_count = 0;
            std::chrono::steady_clock::time_point last_access;
            size_t size_estimate = 0;

            CacheItem(const V& val, size_t size = sizeof(V))
                : value(val), size_estimate(size) {
                last_access = std::chrono::steady_clock::now();
            }
        };

        std::unordered_map<K, CacheItem> cache_;
        std::vector<K> lru_order_;
        std::unordered_map<K, size_t> lru_map_;

        CacheConfig config_;
        MemoryStats* stats_;

        size_t current_memory_ = 0;
        std::chrono::steady_clock::time_point last_cleanup_;

    public:
        AdaptiveCache(const CacheConfig& config, MemoryStats* stats)
            : config_(config), stats_(stats), last_cleanup_(std::chrono::steady_clock::now()) {}

        std::optional<V> get(const K& key) {
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                // 缓存命中
                it->second.access_count++;
                it->second.last_access = std::chrono::steady_clock::now();
                update_lru_order(key);

                if (stats_) {
                    stats_->cache_hits++;
                    stats_->hit_ratio = static_cast<double>(stats_->cache_hits) /
                                        (stats_->cache_hits + stats_->cache_misses);
                }

                return it->second.value;
            }

            // 缓存未命中
            if (stats_) {
                stats_->cache_misses++;
                stats_->hit_ratio = static_cast<double>(stats_->cache_hits) /
                                        (stats_->cache_hits + stats_->cache_misses);
            }

            return std::nullopt;
        }

        bool put(const K& key, const V& value, size_t size_hint = sizeof(V)) {
            // 检查是否需要清理
            auto now = std::chrono::steady_clock::now();
            if (now - last_cleanup_ > config_.cleanup_interval) {
                cleanup_expired();
                last_cleanup_ = now;
            }

            // 检查内存限制
            auto it = cache_.find(key);
            if (it != cache_.end()) {
                // 更新现有项
                current_memory_ -= it->second.size_estimate;
                it->second = CacheItem(value, size_hint);
                current_memory_ += size_hint;
                it->second.last_access = now;
                update_lru_order(key);
                return true;
            }

            // 检查是否需要驱逐项
            if (cache_.size() >= config_.max_items ||
                current_memory_ + size_hint > config_.max_memory_usage) {

                switch (config_.strategy) {
                    case CacheStrategy::LRU:
                        evict_lru(size_hint);
                        break;
                    case CacheStrategy::LFU:
                        evict_lfu(size_hint);
                        break;
                    case CacheStrategy::FIFO:
                        evict_fifo(size_hint);
                        break;
                    case CacheStrategy::ADAPTIVE:
                        evict_adaptive(size_hint);
                        break;
                }
            }

            // 添加新项
            cache_[key] = CacheItem(value, size_hint);
            current_memory_ += size_hint;
            update_lru_order(key);

            if (stats_) {
                stats_->current_usage = current_memory_;
                stats_->peak_usage = std::max(stats_->peak_usage, current_memory_);
            }

            return true;
        }

        void clear() {
            cache_.clear();
            lru_order_.clear();
            lru_map_.clear();
            current_memory_ = 0;
        }

    private:
        void update_lru_order(const K& key) {
            auto map_it = lru_map_.find(key);
            if (map_it != lru_map_.end()) {
                lru_order_.erase(lru_order_.begin() + map_it->second);
            }

            lru_order_.insert(lru_order_.begin(), key);

            // 重建映射
            lru_map_.clear();
            for (size_t i = 0; i < lru_order_.size(); ++i) {
                lru_map_[lru_order_[i]] = i;
            }
        }

        void evict_lru(size_t needed_space) {
            while (!lru_order_.empty() &&
                   (cache_.size() > config_.max_items ||
                    current_memory_ + needed_space > config_.max_memory_usage)) {

                K key_to_evict = lru_order_.back();
                lru_order_.pop_back();
                lru_map_.erase(key_to_evict);

                auto it = cache_.find(key_to_evict);
                if (it != cache_.end()) {
                    current_memory_ -= it->second.size_estimate;
                    cache_.erase(it);
                }
            }
        }

        void evict_lfu(size_t needed_space) {
            // 找到访问次数最少的项
            K* min_key = nullptr;
            size_t min_count = SIZE_MAX;

            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.access_count < min_count) {
                    min_count = it->second.access_count;
                    min_key = const_cast<K*>(&it->first);
                }
            }

            if (min_key) {
                auto cache_it = cache_.find(*min_key);
                if (cache_it != cache_.end()) {
                    current_memory_ -= cache_it->second.size_estimate;
                    cache_.erase(cache_it);

                    // 从LRU列表中移除
                    auto lru_it = lru_map_.find(*min_key);
                    if (lru_it != lru_map_.end()) {
                        size_t index = lru_it->second;
                        if (index < lru_order_.size()) {
                            lru_order_.erase(lru_order_.begin() + index);
                        }
                        lru_map_.erase(lru_it);
                    }
                }
            }
        }

        void evict_fifo(size_t needed_space) {
            while (!lru_order_.empty() &&
                   (cache_.size() > config_.max_items ||
                    current_memory_ + needed_space > config_.max_memory_usage)) {

                K key_to_evict = lru_order_.back();
                lru_order_.pop_back();
                lru_map_.erase(key_to_evict);

                auto it = cache_.find(key_to_evict);
                if (it != cache_.end()) {
                    current_memory_ -= it->second.size_estimate;
                    cache_.erase(it);
                }
            }
        }

        void evict_adaptive(size_t needed_space) {
            // 自适应策略：结合LFU和LRU
            if (stats_->hit_ratio < config_.hit_ratio_threshold) {
                // 命中率低，使用LFU
                evict_lfu(needed_space);
            } else {
                // 命中率高，使用LRU
                evict_lru(needed_space);
            }
        }

        void cleanup_expired() {
            auto now = std::chrono::steady_clock::now();
            auto threshold = now - std::chrono::hours(1); // 1小时过期

            std::vector<K> expired_keys;
            for (auto it = cache_.begin(); it != cache_.end(); ++it) {
                if (it->second.last_access < threshold) {
                    expired_keys.push_back(it->first);
                }
            }

            for (const auto& key : expired_keys) {
                auto cache_it = cache_.find(key);
                if (cache_it != cache_.end()) {
                    current_memory_ -= cache_it->second.size_estimate;
                    cache_.erase(cache_it);

                    // 从LRU列表中移除
                    auto lru_it = lru_map_.find(key);
                    if (lru_it != lru_map_.end()) {
                        size_t index = lru_it->second;
                        if (index < lru_order_.size()) {
                            lru_order_.erase(lru_order_.begin() + index);
                        }
                        lru_map_.erase(lru_it);
                    }
                }
            }
        }
    };

    // 成员变量
    MemoryStats stats_;
    CacheConfig cache_config_;
    PoolConfig pool_config_;

    // 专用对象池
    std::unique_ptr<ObjectPool<std::string>> string_pool_;
    std::unique_ptr<ObjectPool<std::vector<char>>> vector_pool_;

    // 各种专用缓存
    std::unique_ptr<AdaptiveCache<std::string, std::vector<uint8_t>>> data_cache_;
    std::unique_ptr<AdaptiveCache<std::string, std::string>> string_result_cache_;
    std::unique_ptr<AdaptiveCache<std::string, size_t>> size_cache_;

    // 垃圾回收定时器
    std::chrono::steady_clock::time_point last_gc_;
    std::chrono::milliseconds gc_interval_{60000}; // 1分钟

public:
    /**
     * @brief 构造函数
     */
    MemoryOptimizerStd(const CacheConfig& cache_config = CacheConfig{},
                      const PoolConfig& pool_config = PoolConfig{})
        : cache_config_(cache_config), pool_config_(pool_config),
          last_gc_(std::chrono::steady_clock::now()) {

        // 初始化对象池
        string_pool_ = std::make_unique<ObjectPool<std::string>>(pool_config);
        vector_pool_ = std::make_unique<ObjectPool<std::vector<char>>>(pool_config);

        // 初始化缓存
        data_cache_ = std::make_unique<AdaptiveCache<std::string, std::vector<uint8_t>>>(
            cache_config, &stats_);
        string_result_cache_ = std::make_unique<AdaptiveCache<std::string, std::string>>(
            cache_config, &stats_);
        size_cache_ = std::make_unique<AdaptiveCache<std::string, size_t>>(
            cache_config, &stats_);
    }

    /**
     * @brief 析构函数
     */
    ~MemoryOptimizerStd() {
        run_gc();
    }

    // 禁止拷贝
    MemoryOptimizerStd(const MemoryOptimizerStd&) = delete;
    MemoryOptimizerStd& operator=(const MemoryOptimizerStd&) = delete;

    /**
     * @brief 获取字符串对象
     */
    std::string* acquire_string() {
        return string_pool_->acquire();
    }

    /**
     * @brief 释放字符串对象
     */
    void release_string(std::string* str) {
        string_pool_->release(str);
    }

    /**
     * @brief 获取字节数组对象
     */
    std::vector<char>* acquire_vector() {
        return vector_pool_->acquire();
    }

    /**
     * @brief 释放字节数组对象
     */
    void release_vector(std::vector<char>* vec) {
        vector_pool_->release(vec);
    }

    /**
     * @brief 获取缓存的数据
     */
    std::optional<std::vector<uint8_t>> get_cached_data(const std::string& key) {
        return data_cache_->get(key);
    }

    /**
     * @brief 缓存数据
     */
    void cache_data(const std::string& key, const std::vector<uint8_t>& data) {
        data_cache_->put(key, data, data.size());
    }

    /**
     * @brief 获取缓存的字符串结果
     */
    std::optional<std::string> get_cached_string_result(const std::string& key) {
        return string_result_cache_->get(key);
    }

    /**
     * @brief 缓存字符串结果
     */
    void cache_string_result(const std::string& key, const std::string& result) {
        string_result_cache_->put(key, result, result.size());
    }

    /**
     * @brief 获取缓存的大小信息
     */
    std::optional<size_t> get_cached_size(const std::string& key) {
        return size_cache_->get(key);
    }

    /**
     * @brief 缓存大小信息
     */
    void cache_size(const std::string& key, size_t size) {
        size_cache_->put(key, size, sizeof(size_t));
    }

    /**
     * @brief 获取内存统计信息
     */
    const MemoryStats& get_stats() const {
        return stats_;
    }

    /**
     * @brief 重置统计信息
     */
    void reset_stats() {
        stats_ = MemoryStats{};
    }

    /**
     * @brief 清理所有缓存
     */
    void clear_caches() {
        data_cache_->clear();
        string_result_cache_->clear();
        size_cache_->clear();
    }

    /**
     * @brief 手动运行垃圾回收
     */
    void run_gc() {
        auto start = std::chrono::high_resolution_clock::now();

        // 收缩对象池
        string_pool_->shrink();
        vector_pool_->shrink();

        // 清理过期缓存项
        data_cache_->cleanup_expired();
        string_result_cache_->cleanup_expired();
        size_cache_->cleanup_expired();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        stats_.gc_runs++;
        stats_.gc_time += duration;
    }

    /**
     * @brief 周期性维护任务
     */
    void maintenance() {
        auto now = std::chrono::steady_clock::now();

        if (now - last_gc_ > gc_interval_) {
            run_gc();
            last_gc_ = now;
        }
    }

    /**
     * @brief 获取内存使用报告
     */
    std::string get_memory_report() const {
        std::ostringstream report;
        report << "=== 内存使用报告 ===\\n";
        report << "总分配内存: " << format_bytes(stats_.total_allocated) << "\\n";
        report << "当前使用内存: " << format_bytes(stats_.current_usage) << "\\n";
        report << "峰值内存使用: " << format_bytes(stats_.peak_usage) << "\\n";
        report << "缓存命中率: " << std::fixed << std::setprecision(2)
                << (stats_.hit_ratio * 100.0) << "%\\n";
        report << "缓存命中次数: " << stats_.cache_hits << "\\n";
        report << "缓存未命中次数: " << stats_.cache_misses << "\\n";
        report << "垃圾回收次数: " << stats_.gc_runs << "\\n";
        report << "垃圾回收总时间: " << stats_.gc_time.count() << "ms\\n";
        report << "平均GC时间: " << (stats_.gc_runs > 0 ?
                stats_.gc_time.count() / stats_.gc_runs : 0) << "ms\\n";
        return report.str();
    }

private:
    /**
     * @brief 格式化字节数为人类可读格式
     */
    static std::string format_bytes(size_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024.0 && unit < 4) {
            size /= 1024.0;
            unit++;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << units[unit];
        return oss.str();
    }
};

} // namespace UnidictCoreStd