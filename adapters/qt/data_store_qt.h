#ifndef UNIDICT_DATA_STORE_QT_H
#define UNIDICT_DATA_STORE_QT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <memory>

#include "core/unidict_core.h"
#include "core/std/data_store_std.h"

namespace UnidictAdaptersQt {

class DataStoreQt {
public:
    static DataStoreQt& instance();

    void setStoragePath(const QString& filePath);
    QString storagePath() const;

    void addSearchHistory(const QString& word);
    QStringList getSearchHistory(int limit = 100) const;
    void clearHistory();

    void addVocabularyItem(const UnidictCore::DictionaryEntry& entry);
    QList<UnidictCore::DictionaryEntry> getVocabulary() const;
    void clearVocabulary();
    bool exportVocabularyCSV(const QString& filePath) const;

private:
    DataStoreQt();
    std::unique_ptr<UnidictCoreStd::DataStoreStd> impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_DATA_STORE_QT_H

