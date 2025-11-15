// Thin Qt adapter for the std-only index engine.

#ifndef UNIDICT_INDEX_ENGINE_QT_H
#define UNIDICT_INDEX_ENGINE_QT_H

#include <QString>
#include <QStringList>
#include <memory>

#include "core/std/index_engine_std.h"

namespace UnidictAdaptersQt {

class IndexEngineQt {
public:
    IndexEngineQt();

    void addWord(const QString& word, const QString& dictionaryId);
    void removeWord(const QString& word, const QString& dictionaryId);
    void clearDictionary(const QString& dictionaryId);
    void buildIndex();

    QStringList exactMatch(const QString& word) const;
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    QStringList regexSearch(const QString& pattern, int maxResults = 10) const;

    QStringList getAllWords() const;
    QStringList getDictionariesForWord(const QString& word) const;
    int getWordCount() const;

    bool saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);

private:
    std::unique_ptr<UnidictCoreStd::IndexEngineStd> impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_INDEX_ENGINE_QT_H

