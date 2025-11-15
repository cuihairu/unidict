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

private:
    std::unique_ptr<UnidictCoreStd::DictionaryManagerStd> mgr_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_FULLTEXT_MANAGER_QT_H

