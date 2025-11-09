// Qt-free path utilities for data/cache directories and basic fs ops.

#ifndef UNIDICT_PATH_UTILS_STD_H
#define UNIDICT_PATH_UTILS_STD_H

#include <string>

namespace UnidictCoreStd {
namespace PathUtilsStd {

// Returns data directory path. Defaults to ./data. Overridable via UNIDICT_DATA_DIR.
std::string data_dir();

// Returns cache directory path. Defaults to <data_dir>/cache. Overridable via UNIDICT_CACHE_DIR.
std::string cache_dir();

// Ensure directory exists (recursive). Returns true on success or already exists.
bool ensure_dir(const std::string& dir_path);

// Remove all files/dirs under cache dir. Returns true on success.
bool clear_cache();

// Return total size (bytes) of files under cache dir (recursive, best-effort).
std::uint64_t cache_size_bytes();

// Prune cache so total size <= max_bytes by deleting oldest files first.
// Returns true on success.
bool prune_cache_bytes(std::uint64_t max_bytes);

// Remove files older than given number of days. Returns true on success.
bool prune_cache_older_than_days(int days);

} // namespace PathUtilsStd
} // namespace UnidictCoreStd

#endif // UNIDICT_PATH_UTILS_STD_H
