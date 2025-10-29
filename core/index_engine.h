#ifndef INDEX_ENGINE_H
#define INDEX_ENGINE_H

#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <memory>

namespace UnidictCore {

struct IndexEntry {
    QString word;
    QString normalizedWord;
    QStringList dictionaryIds;
    qint64 frequency = 0;
};

class TrieNode {
public:
    TrieNode();
    ~TrieNode();
    
    void insert(const QString& word, const QString& dictionaryId);
    QStringList search(const QString& prefix, int maxResults = 10) const;
    bool contains(const QString& word) const;
    
private:
    QMap<QChar, std::unique_ptr<TrieNode>> children;
    QSet<QString> words;
    QMap<QString, QStringList> wordToDictionaries;
    
    void collectWords(const QString& prefix, QStringList& results, int& count, int maxResults) const;
};

class IndexEngine {
public:
    IndexEngine();
    ~IndexEngine() = default;
    
    void addWord(const QString& word, const QString& dictionaryId);
    void removeWord(const QString& word, const QString& dictionaryId);
    void clearDictionary(const QString& dictionaryId);
    
    QStringList exactMatch(const QString& word) const;
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    
    QStringList getAllWords() const;
    int getWordCount() const;
    QStringList getDictionariesForWord(const QString& word) const;
    
    void buildIndex();
    void saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);

private:
    QString normalizeWord(const QString& word) const;
    int calculateEditDistance(const QString& word1, const QString& word2) const;
    bool matchesWildcard(const QString& word, const QString& pattern) const;
    
    std::unique_ptr<TrieNode> m_trie;
    QMap<QString, IndexEntry> m_wordIndex;
    QMap<QString, QSet<QString>> m_dictionaryWords; // dictionaryId -> words
    bool m_indexBuilt = false;
};

}

#endif // INDEX_ENGINE_H