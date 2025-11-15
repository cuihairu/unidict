#ifndef INDEX_ENGINE_H
#define INDEX_ENGINE_H

#include <QString>
#include <QStringList>
#include <memory>

namespace UnidictAdaptersQt { class IndexEngineQt; }

namespace UnidictCore {

// Compatibility wrapper preserving the old Qt-facing API while delegating
// to the std-only engine via the Qt adapter.
class IndexEngine {
public:
    IndexEngine();
    ~IndexEngine();

    void addWord(const QString& word, const QString& dictionaryId);
    void removeWord(const QString& word, const QString& dictionaryId);
    void clearDictionary(const QString& dictionaryId);

    QStringList exactMatch(const QString& word) const;
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    QStringList regexSearch(const QString& pattern, int maxResults = 10) const;

    QStringList getAllWords() const;
    int getWordCount() const;
    QStringList getDictionariesForWord(const QString& word) const;

    void buildIndex();
    void saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);

private:
    std::unique_ptr<UnidictAdaptersQt::IndexEngineQt> impl_;
};

}

#endif // INDEX_ENGINE_H
