#include "path_utils_std.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace UnidictCoreStd {
namespace PathUtilsStd {

static inline const char* getenv_c(const char* name) {
    const char* v = std::getenv(name);
    return v && *v ? v : nullptr;
}

static inline std::string cwd_data_default() {
    return (fs::current_path() / "data").string();
}

std::string data_dir() {
    if (auto* v = getenv_c("UNIDICT_DATA_DIR")) return std::string(v);
    return cwd_data_default();
}

std::string cache_dir() {
    if (auto* v = getenv_c("UNIDICT_CACHE_DIR")) return std::string(v);
    return (fs::path(data_dir()) / "cache").string();
}

bool ensure_dir(const std::string& dir_path) {
    std::error_code ec;
    if (fs::exists(dir_path, ec)) return true;
    return fs::create_directories(dir_path, ec) || fs::exists(dir_path, ec);
}

bool clear_cache() {
    std::error_code ec;
    fs::path p(cache_dir());
    if (!fs::exists(p, ec)) return true;
    bool ok = true;
    for (auto it = fs::directory_iterator(p, ec); !ec && it != fs::directory_iterator(); ++it) {
        std::error_code ec2;
        fs::remove_all(it->path(), ec2);
        if (ec2) ok = false;
    }
    return ok;
}

std::uint64_t cache_size_bytes() {
    std::uint64_t total = 0;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(cache_dir(), ec); !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (it->is_regular_file()) {
            std::error_code ec2; auto sz = fs::file_size(it->path(), ec2); if (!ec2) total += (std::uint64_t)sz;
        }
    }
    return total;
}

bool prune_cache_bytes(std::uint64_t max_bytes) {
    struct Item { fs::path path; fs::file_time_type t; std::uint64_t sz; };
    std::vector<Item> items;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(cache_dir(), ec); !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        std::error_code ec1, ec2;
        auto t = fs::last_write_time(it->path(), ec1);
        auto sz = ec2 ? 0 : fs::file_size(it->path(), ec2);
        items.push_back({it->path(), t, (std::uint64_t)sz});
    }
    std::uint64_t total = 0; for (auto& i : items) total += i.sz;
    if (total <= max_bytes) return true;
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){ return a.t < b.t; });
    bool ok = true;
    for (auto& i : items) {
        if (total <= max_bytes) break;
        std::error_code ec3; fs::remove(i.path, ec3); if (ec3) ok = false; else total -= i.sz;
    }
    return ok && total <= max_bytes;
}

bool prune_cache_older_than_days(int days) {
    if (days <= 0) return true;
    using namespace std::chrono;
    auto now = fs::file_time_type::clock::now();
    auto cutoff = now - hours(24 * (std::int64_t)days);
    bool ok = true;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(cache_dir(), ec); !ec && it != fs::recursive_directory_iterator(); ++it) {
        if (!it->is_regular_file()) continue;
        std::error_code ec1; auto t = fs::last_write_time(it->path(), ec1); if (ec1) continue;
        if (t < cutoff) { std::error_code ec2; fs::remove(it->path(), ec2); if (ec2) ok = false; }
    }
    return ok;
}

} // namespace PathUtilsStd
} // namespace UnidictCoreStd
