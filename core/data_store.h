#ifndef DATA_STORE_H
#define DATA_STORE_H

#include <QString>
#include <QStringList>
#include <QList>
#include <memory>

#include "unidict_core.h"

namespace UnidictCore {

// Very lightweight JSON-backed data store for history and vocabulary.
class DataStore {
public:
    static DataStore& instance();

    // Configure storage location. Defaults to ./data/unidict.json under CWD.
    void setStoragePath(const QString& filePath);
    QString storagePath() const;

    // Search history
    void addSearchHistory(const QString& word);
    QStringList getSearchHistory(int limit = 100) const;
    void clearHistory();

    // Vocabulary book (stores word + definition only for now)
    void addVocabularyItem(const DictionaryEntry& entry);
    void addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt);
    void removeVocabularyItem(const QString& word);
    QList<DictionaryEntry> getVocabulary() const;
    QVariantList getVocabularyMeta() const; // [{word,definition,added_at}]
    void clearVocabulary();
    bool exportVocabularyCSV(const QString& filePath) const;

    // Persistence
    bool load();
    bool save() const;

private:
    DataStore();
    void ensureLoaded() const;
};

}

#endif // DATA_STORE_H
