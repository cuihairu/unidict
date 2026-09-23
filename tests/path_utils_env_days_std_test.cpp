#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "std/path_utils_std.h"

using namespace UnidictCoreStd;
namespace fs = std::filesystem;
using namespace std::chrono;

// Windows (MSVC) 无 POSIX setenv，走 _putenv_s
static void set_env(const char* key, const std::string& value) {
#if defined(_WIN32)
    _putenv_s(key, value.c_str());
#else
    ::setenv(key, value.c_str(), 1);
#endif
}

int main() {
    // Override data/cache dirs via env
    fs::path base = fs::current_path() / "build-local" / "pu_env";
    fs::path data = base / "data";
    fs::path cache = base / "cache";
    fs::create_directories(data);
    fs::create_directories(cache);
    set_env("UNIDICT_DATA_DIR", data.string());
    set_env("UNIDICT_CACHE_DIR", cache.string());

    // Ensure data/cache resolution
    assert(PathUtilsStd::data_dir() == data.string());
    assert(PathUtilsStd::cache_dir() == cache.string());

    // Create files in cache with different ages
    fs::path f_old = cache / "old.txt";
    fs::path f_new = cache / "new.txt";
    {
        std::ofstream o1(f_old.string(), std::ios::binary); o1 << std::string(16, 'o');
        std::ofstream o2(f_new.string(), std::ios::binary); o2 << std::string(32, 'n');
    }
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(f_old, now - hours(24 * 3));
    fs::last_write_time(f_new, now - hours(8));

    // Prune files older than 1 day; expect old removed, new remains
    bool ok = PathUtilsStd::prune_cache_older_than_days(1);
    assert(ok);
    assert(!fs::exists(f_old));
    assert(fs::exists(f_new));

    // Size API reflects remaining file
    auto sz = PathUtilsStd::cache_size_bytes();
    assert(sz >= 32);

    // Clear cache
    ok = PathUtilsStd::clear_cache();
    assert(ok);
    assert(!fs::exists(f_new));
    return 0;
}

