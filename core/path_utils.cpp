#include "path_utils.h"
#include "path_utils_qt.h"

namespace UnidictCore {
namespace PathUtils {

QString dataDir() { return UnidictAdaptersQt::PathUtilsQt::dataDir(); }
QString cacheDir() { return UnidictAdaptersQt::PathUtilsQt::cacheDir(); }
bool ensureDir(const QString& dirPath) { return UnidictAdaptersQt::PathUtilsQt::ensureDir(dirPath); }
bool clearCache() { return UnidictAdaptersQt::PathUtilsQt::clearCache(); }
quint64 cacheSizeBytes() { return UnidictAdaptersQt::PathUtilsQt::cacheSizeBytes(); }
bool pruneCacheBytes(quint64 maxBytes) { return UnidictAdaptersQt::PathUtilsQt::pruneCacheBytes(maxBytes); }
bool pruneCacheOlderThanDays(int days) { return UnidictAdaptersQt::PathUtilsQt::pruneCacheOlderThanDays(days); }

} // namespace PathUtils
} // namespace UnidictCore
