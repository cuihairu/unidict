#ifndef PATH_UTILS_H
#define PATH_UTILS_H

#include <QString>

namespace UnidictCore {

// Utility functions for determining data/cache directories.
namespace PathUtils {
    // Returns data directory. Defaults to ./data. Can be overridden via UNIDICT_DATA_DIR.
    QString dataDir();

    // Returns cache directory. Defaults to <dataDir>/cache. Can be overridden via UNIDICT_CACHE_DIR.
    QString cacheDir();

    // Ensure directory exists (mkpath). Returns true on success or already exists.
    bool ensureDir(const QString& dirPath);

    // Remove all files/directories under cache dir. Returns true on success.
    bool clearCache();

    // Return total size of files under cache dir (best-effort).
    quint64 cacheSizeBytes();

    // Prune cache to keep total size <= maxBytes. Returns true on success.
    bool pruneCacheBytes(quint64 maxBytes);

    // Remove cache files older than given number of days. Returns true on success.
    bool pruneCacheOlderThanDays(int days);
}

}

#endif // PATH_UTILS_H
