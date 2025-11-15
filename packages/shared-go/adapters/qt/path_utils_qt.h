// Qt adapter wrapping std-only path utils.

#ifndef UNIDICT_PATH_UTILS_QT_H
#define UNIDICT_PATH_UTILS_QT_H

#include <QString>

namespace UnidictAdaptersQt {
namespace PathUtilsQt {

QString dataDir();
QString cacheDir();
bool ensureDir(const QString& dirPath);
bool clearCache();

quint64 cacheSizeBytes();
bool pruneCacheBytes(quint64 maxBytes);
bool pruneCacheOlderThanDays(int days);

} // namespace PathUtilsQt
} // namespace UnidictAdaptersQt

#endif // UNIDICT_PATH_UTILS_QT_H
