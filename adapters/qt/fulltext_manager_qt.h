#ifndef UNIDICT_FULLTEXT_MANAGER_QT_H
#define UNIDICT_FULLTEXT_MANAGER_QT_H

#include <QObject>
#include <QVariant>
#include <memory>

#include "core/std/dictionary_manager_std.h"
#include "core/std/fulltext_index_std.h"

namespace UnidictAdaptersQt {

class FullTextManagerQt : public QObject {
    Q_OBJECT
public:
    explicit FullTextManagerQt(QObject* parent = nullptr);

    Q_INVOKABLE bool loadDictionariesFromEnv();
    Q_INVOKABLE bool saveIndex(const QString& path) const;
    Q_INVOKABLE QVariantMap statsFromFile(const QString& path) const;
    // Returns empty string on success; otherwise error message
    Q_INVOKABLE QString loadIndex(const QString& path, const QString& compatMode);
    Q_INVOKABLE bool upgrade(const QString& inPath, const QString& outPath);
    // Current in-memory index signature and stats
    Q_INVOKABLE QString currentSignature() const;
    Q_INVOKABLE QVariantMap currentStats() const;
    // Verify a saved index file against current dictionaries (does not modify state).
    // Returns an empty string when signatures match (or legacy v1 without signature),
    // otherwise returns a descriptive mismatch/error message.
    Q_INVOKABLE QString verifyIndexMatch(const QString& path) const;
    // Load index with detailed result. Returns:
    // { ok:bool, mode:string, version:int, fileSignature:string, currentSignature:string, error:string }
    Q_INVOKABLE QVariantMap loadIndexDetailed(const QString& path, const QString& compatMode);
    // Verify with details without loading into memory:
    // { ok:bool, version:int, match:bool, fileSignature:string, currentSignature:string,
    //   fileSigPrefix:string, currentSigPrefix:string, error:string,
    //   fileDicts:[{name,wordCount}], currentDicts:[{name,wordCount}] }
    Q_INVOKABLE QVariantMap verifyIndexDetailed(const QString& path) const;
    // Export source diff (from verifyIndexDetailed result) to a JSON file.
    // verifyResult should be the returned map from verifyIndexDetailed.
    // Returns true on success.
    Q_INVOKABLE bool exportSourceDiff(const QVariantMap& verifyResult, const QString& outPath) const;

private:
    std::unique_ptr<UnidictCoreStd::DictionaryManagerStd> mgr_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_FULLTEXT_MANAGER_QT_H
