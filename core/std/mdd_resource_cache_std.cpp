// MDD 资源缓存（LAY-003 自 mdd_resource_std.cpp 拆出）——缓存落盘、
// 元数据与 prune 策略独立成 TU；解析 TU 只留格式/头部/块编解码。
// fnv1a64 / file_extension 经 mdd_resource_helpers_std.h 共享。

#include "mdd_resource_std.h"
#include "mdd_resource_helpers_std.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

namespace {
    // 缓存文件名的长度上限。ext4/APFS/NTFS 的单个文件名上限都是 255 字节，
    // 留出余量取 200（路径总长另见 MAX_CACHE_PATH_LEN）。
    const size_t MAX_CACHE_NAME_LEN = 200;

    // 路径总长上限：Windows MAX_PATH=260（含终止符，可用 259）。POSIX 无
    // 此硬限（4096），统一按同一口径收——文件名本来就是摘要名，短一点
    // 无害，换回的是"目录深也绝不静默写失败"的单口径。
    const size_t MAX_CACHE_PATH_LEN = 259;
}  // namespace

// ============================================================================
// MddResourceCache Implementation
// ============================================================================

MddResourceCache::MddResourceCache() {
    // Set default cache directory
    const char* home = std::getenv("HOME");
    if (home) {
        cache_dir_ = std::string(home) + "/.cache/unidict/mdd_resources";
    } else {
        cache_dir_ = "/tmp/unidict_mdd_cache";
    }
}

MddResourceCache::MddResourceCache(const std::string& cache_dir)
    : cache_dir_(cache_dir) {
}

void MddResourceCache::set_cache_directory(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
    // 同上：畸形路径不该抛异常打穿调用方，建不出来就留给后续写操作去失败
    std::error_code ec;
    fs::create_directories(cache_dir_, ec);
}

bool MddResourceCache::cache_resource(const std::vector<uint8_t>& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    if (data.empty()) {
        return false;
    }

    std::error_code ec;
    fs::create_directories(cache_dir_, ec);
    if (ec) {
        return false;  // 畸形 cache_dir，见 extract_to_cache 的说明
    }

    std::string cache_path = get_cache_file_path(key);

    // Write to file
    std::ofstream out(cache_path, std::ios::binary);
    if (!out) {
        return false;
    }

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!out.good()) {
        return false;
    }

    // Update metadata
    CachedResource info;
    info.key = key;
    info.local_path = cache_path;
    info.mime_type = mime_type;
    info.size = data.size();
    info.last_used = std::time(nullptr);
    info.access_count = 1;

    cache_meta_[key] = info;

    return true;
}

bool MddResourceCache::cache_resource(const std::string& data,
                                     const std::string& key,
                                     const std::string& mime_type) {
    std::vector<uint8_t> vec(data.begin(), data.end());
    return cache_resource(vec, key, mime_type);
}

std::string MddResourceCache::get_cached_path(const std::string& key) const {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        return it->second.local_path;
    }
    return "";
}

bool MddResourceCache::is_cached(const std::string& key) const {
    return cache_meta_.find(key) != cache_meta_.end();
}

std::vector<uint8_t> MddResourceCache::get_from_cache(const std::string& key) const {
    std::string path = get_cached_path(key);
    if (path.empty()) {
        return {};
    }

    // 元数据说有，但那条路径可能已被换成目录。这类文件系统上 ifstream
    // 能"打开"目录而 tellg 报出 INT64_MAX——vector 按它分配直接
    // bad_alloc（实测：tmpfs 上打不开、ext4 上打得开，行为随文件系统
    // 类型漂移；test_cache_read_of_directory_target 曾因此炸出
    // std::bad_alloc）。所以打开之后、取 size 之前先验正身：目录
    // （tmpfs 打不开就靠 !in 短路）和权限问题都挡在同一道门里。
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    std::error_code ec;
    if (!in || !fs::is_regular_file(path, ec)) {
        return {};
    }

    size_t size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(size);
    // GCOVR_EXCL_LINE：常规文件在 tellg 报出的 size 上读不满只可能发生
    // 在"读期间文件被截短/IO 错误"，单线程测试无法稳定构造。
    if (!in.read(reinterpret_cast<char*>(data.data()), size)) {  // GCOVR_EXCL_LINE
        return {};  // GCOVR_EXCL_LINE
    }

    return data;
}

void MddResourceCache::clear_cache(const std::string& prefix) {
    if (prefix.empty()) {
        // Clear all
        for (const auto& entry : cache_meta_) {
            fs::remove(entry.second.local_path);
        }
        cache_meta_.clear();
    } else {
        // Clear matching entries
        auto it = cache_meta_.begin();
        while (it != cache_meta_.end()) {
            if (it->first.find(prefix) == 0) {
                fs::remove(it->second.local_path);
                it = cache_meta_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void MddResourceCache::prune_by_size(size_t max_bytes) {
    size_t total_size = get_cache_size();
    if (total_size <= max_bytes) {
        return;
    }

    // Sort by last used time (LRU)
    std::vector<CachedResource*> sorted;
    for (auto& entry : cache_meta_) {
        sorted.push_back(&entry.second);
    }

    std::sort(sorted.begin(), sorted.end(),
        [](const CachedResource* a, const CachedResource* b) {
            return a->last_used < b->last_used;
        });

    // Remove oldest entries until under limit
    for (auto* entry : sorted) {
        if (total_size <= max_bytes) {
            break;
        }

        fs::remove(entry->local_path);
        total_size -= entry->size;
        cache_meta_.erase(entry->key);
    }
}

void MddResourceCache::prune_by_age(uint64_t max_age_seconds) {
    uint64_t now = std::time(nullptr);

    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (now - it->second.last_used > max_age_seconds) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

void MddResourceCache::prune_by_access(uint64_t min_access_count) {
    auto it = cache_meta_.begin();
    while (it != cache_meta_.end()) {
        if (it->second.access_count < min_access_count) {
            fs::remove(it->second.local_path);
            it = cache_meta_.erase(it);
        } else {
            ++it;
        }
    }
}

size_t MddResourceCache::get_cache_size() const {
    size_t total = 0;
    for (const auto& entry : cache_meta_) {
        total += entry.second.size;
    }
    return total;
}

int MddResourceCache::get_cached_count() const {
    return static_cast<int>(cache_meta_.size());
}

std::vector<CachedResource> MddResourceCache::get_cache_info() const {
    std::vector<CachedResource> result;
    result.reserve(cache_meta_.size());

    for (const auto& entry : cache_meta_) {
        result.push_back(entry.second);
    }

    return result;
}

std::string MddResourceCache::get_cache_file_path(const std::string& key) const {
    // 把文件系统上非法/易歧义的字符换成 '-'（斜杠同时充当了目录分隔的
    // 扁平化：缓存是单层目录，不还原层级）
    std::string filename = key;
    static const char kBad[] = "/\\:?*\"<>|";
    for (char& c : filename) {
        if (std::strchr(kBad, c) != nullptr && c != '\0') {
            c = '-';
        }
    }

    // 文件名长度必须有界。cache_key 是 "<词典id>_<资源键>"，词典 id 在 Qt 层
    // 是从**绝对路径**派生的，资源键又是 "sounds/oxford/word_00001.mp3" 这种
    // 带目录的键——两者一叠加，词典装在稍深一点的目录里就轻松超过 255 字节。
    //
    // 原先没有这道护栏：超长名被 std::ofstream 直接拒掉，cache_resource()
    // 返回 false，get_resource_path() 返回空串。也就是 .mdd 里明明有这张图，
    // QML 却什么都拿不到，而且没有任何报错——静默失败最难查。
    //
    // 超限时截断并追加 FNV-1a-64 的 16 位十六进制摘要：既把名字压回界内，
    // 又让不同的超长键仍映射到不同文件（纯截断会让所有超长键挤到同一个
    // 名字，后写的覆盖先写的，图/音频就串了）。扩展名另作保留——QML 的
    // Image/Audio 靠它嗅格式。
    // 文件名分量达标还不够——总长要一起收（CI Windows 曾挂在超长文件名
    // 块：目录 61 + 1 + 名 200 = 262 > 259，ofstream 拒开、返回 false）。
    // 按实测目录长度把文件名预算折进来，目录越深名字越短，总长钉在
    // MAX_CACHE_PATH_LEN 内。
    size_t name_budget = MAX_CACHE_NAME_LEN;
    const size_t dir_len = cache_dir_.size() + 1;
    if (dir_len < MAX_CACHE_PATH_LEN) {
        name_budget = std::min(name_budget, MAX_CACHE_PATH_LEN - dir_len);
    }

    if (filename.size() > name_budget) {
        const std::string ext = mdd_detail::file_extension(filename);
        char digest[17];
        std::snprintf(digest, sizeof(digest), "%016llx",
                      static_cast<unsigned long long>(mdd_detail::fnv1a64(key.data(), key.size())));
        // '_' + 16 进制摘要 + 扩展名是不可再压的骨架；目录深到连骨架都
        // 放不下时预算归 0，名字退化成 "_digest.ext"，让 ofstream 按真实
        // 边界自然失败返回 false（这种深度 <filesystem> 建目录那关在
        // Windows 上本来就已经挡掉了）
        const size_t overhead = 17 + ext.size();
        const size_t budget =
            name_budget > overhead ? name_budget - overhead : 0;
        filename = filename.substr(0, budget) + "_" + digest + ext;
    }

    return cache_dir_ + "/" + filename;
}

void MddResourceCache::update_access_time(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.last_used = std::time(nullptr);
    }
}

void MddResourceCache::increment_access_count(const std::string& key) {
    auto it = cache_meta_.find(key);
    if (it != cache_meta_.end()) {
        it->second.access_count++;
    }
}
} // namespace UnidictCoreStd
