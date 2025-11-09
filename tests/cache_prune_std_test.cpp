#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include "std/path_utils_std.h"

using namespace std::chrono;

int main() {
    namespace fs = std::filesystem;
    using UnidictCoreStd::PathUtilsStd::cache_dir;
    using UnidictCoreStd::PathUtilsStd::clear_cache;
    using UnidictCoreStd::PathUtilsStd::prune_cache_bytes;
    using UnidictCoreStd::PathUtilsStd::cache_size_bytes;

    // Clean slate
    assert(clear_cache());
    fs::path base(cache_dir());
    fs::create_directories(base);

    // Create three files with different mtimes and sizes
    fs::path f1 = base / "p_f1";
    fs::path f2 = base / "p_f2";
    fs::path f3 = base / "p_f3";
    {
        std::ofstream o1(f1.string(), std::ios::binary); o1 << std::string(1000, 'a');
        std::ofstream o2(f2.string(), std::ios::binary); o2 << std::string(2000, 'b');
        std::ofstream o3(f3.string(), std::ios::binary); o3 << std::string(3000, 'c');
    }
    auto now = fs::file_time_type::clock::now();
    fs::last_write_time(f1, now - minutes(3));
    fs::last_write_time(f2, now - minutes(2));
    fs::last_write_time(f3, now - minutes(1));

    // Prune to <= 3500 bytes; expect only newest (3000) remains
    bool ok = prune_cache_bytes(3500);
    assert(ok);
    auto total = cache_size_bytes();
    assert(total <= 3500);
    assert(!fs::exists(f1));
    assert(!fs::exists(f2));
    assert(fs::exists(f3));
    return 0;
}

