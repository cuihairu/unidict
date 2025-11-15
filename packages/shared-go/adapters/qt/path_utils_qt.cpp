#include "path_utils_qt.h"

#include <QString>

#include "core/std/path_utils_std.h"

namespace UnidictAdaptersQt {
namespace PathUtilsQt {

static inline QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }
static inline std::string cs(const QString& s) { return std::string(s.toUtf8().constData()); }

QString dataDir() { return qs(UnidictCoreStd::PathUtilsStd::data_dir()); }
QString cacheDir() { return qs(UnidictCoreStd::PathUtilsStd::cache_dir()); }
bool ensureDir(const QString& dirPath) { return UnidictCoreStd::PathUtilsStd::ensure_dir(cs(dirPath)); }
bool clearCache() { return UnidictCoreStd::PathUtilsStd::clear_cache(); }

quint64 cacheSizeBytes() { return (quint64)UnidictCoreStd::PathUtilsStd::cache_size_bytes(); }
bool pruneCacheBytes(quint64 maxBytes) { return UnidictCoreStd::PathUtilsStd::prune_cache_bytes((std::uint64_t)maxBytes); }
bool pruneCacheOlderThanDays(int days) { return UnidictCoreStd::PathUtilsStd::prune_cache_older_than_days(days); }

} // namespace PathUtilsQt
} // namespace UnidictAdaptersQt
