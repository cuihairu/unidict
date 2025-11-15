#ifndef UNIDICT_CORE_H
#define UNIDICT_CORE_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QList>
#include <memory>
#include <vector>
#include "index_engine_qt.h"

namespace UnidictCore {

struct DictionaryEntry {
    QString word;
    QString definition;
    QString pronunciation;
    QStringList examples;
    QVariantMap metadata;
};

class DictionaryParser {
public:
    virtual ~DictionaryParser() = default;
    
    virtual bool loadDictionary(const QString& filePath) = 0;
    virtual bool isLoaded() const = 0;
    virtual QStringList getSupportedExtensions() const = 0;
    
    virtual DictionaryEntry lookup(const QString& word) const = 0;
    virtual QStringList findSimilar(const QString& word, int maxResults = 10) const = 0;
    virtual QStringList getAllWords() const = 0;
    
    virtual QString getDictionaryName() const = 0;
    virtual QString getDictionaryDescription() const = 0;
    virtual int getWordCount() const = 0;
};

class DictionaryManager {
public:
    static DictionaryManager& instance();
    
    bool addDictionary(const QString& filePath);
    bool removeDictionary(const QString& dictionaryId);
    QStringList getLoadedDictionaries() const;
    
    DictionaryEntry searchWord(const QString& word) const;
    QStringList searchSimilar(const QString& word, int maxResults = 10) const;
    std::vector<DictionaryEntry> searchAll(const QString& word) const;

    // Index-backed search across all loaded dictionaries
    void buildIndex();
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    QStringList regexSearch(const QString& pattern, int maxResults = 10) const;
    QStringList getDictionariesForWord(const QString& word) const;
    QStringList getAllIndexedWords() const;
    int getIndexedWordCount() const;

    // Index persistence (for faster startup on large dictionaries)
    bool saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);

    struct DictMeta { QString name; int wordCount; QString description; };
    QList<DictMeta> getDictionariesMeta() const;
    
private:
    DictionaryManager() = default;
    std::vector<std::unique_ptr<DictionaryParser>> m_parsers;
    // Lazy-created global index for prefix/fuzzy/wildcard search (Qt adapter over std-only core)
    std::unique_ptr<::UnidictAdaptersQt::IndexEngineQt> m_index;
};

QString searchWord(const QString& word);

}

#endif // UNIDICT_CORE_H
