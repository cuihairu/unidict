#ifndef UNIDICT_SYNC_SERVICE_QT_H
#define UNIDICT_SYNC_SERVICE_QT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace UnidictAdaptersQt {

// Very lightweight sync service that merges local DataStore with a JSON file
// at a user-chosen path. Merge strategy:
// - history: ordered union, preserving local order, then append remote-only items.
// - vocabulary: case-insensitive by word; local wins on conflict; remote-only items appended.
class SyncServiceQt : public QObject {
    Q_OBJECT
public:
    explicit SyncServiceQt(QObject* parent = nullptr);

    Q_INVOKABLE void setSyncFile(const QString& path);
    Q_INVOKABLE QString syncFile() const;
    // Last error message (set on failures of operations in this service).
    Q_INVOKABLE QString lastError() const { return lastError_; }

    // Performs a two-way merge and writes merged result to both local store and sync file.
    // Returns true on success.
    Q_INVOKABLE bool syncNow();
    // Preview diff between local store and sync file without modifying anything.
    // Returns: { ok:bool, error:string,
    //   localOnly:[string], remoteOnly:[string],
    //   remoteNewer:[{word,local_ts,remote_ts}], localNewer:[{word,local_ts,remote_ts}] }
    Q_INVOKABLE QVariantMap previewDiff() const;
    // Apply a selective merge based on preview categories.
    // takeRemoteNewer: update local where remote is newer
    // takeLocalNewer:  update remote where local is newer
    // includeRemoteOnly: pull remote-only items into local
    // includeLocalOnly:  push local-only items into remote
    Q_INVOKABLE bool applyPreview(bool takeRemoteNewer, bool takeLocalNewer,
                                  bool includeRemoteOnly, bool includeLocalOnly);
    // Read last_changes section from sync file for quick inspection.
    // Returns: { ok:bool, error:string, changes:object }
    Q_INVOKABLE QVariantMap lastChanges() const;
    // Apply selection from preview UI. The map may contain string lists:
    // { remoteOnly:[word...], localOnly:[word...], remoteNewer:[word...], localNewer:[word...] }
    // Words are matched case-insensitively.
    Q_INVOKABLE bool applySelection(const QVariantMap& selection);
    // Export/import selection JSON for collaboration.
    // Selection object uses the same shape as applySelection expects.
    Q_INVOKABLE bool exportSelection(const QVariantMap& selection, const QString& path) const;
    // Returns { ok:bool, error:string, selection:object }
    Q_INVOKABLE QVariantMap importSelection(const QString& path) const;

private:
    QString syncPath_;
    QString lastError_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_SYNC_SERVICE_QT_H
