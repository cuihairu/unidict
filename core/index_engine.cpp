#include "index_engine.h"
#include <QDebug>
#include <QFile>
#include <QDataStream>
#include <QRegularExpression>
#include <algorithm>

namespace UnidictCore {

// TrieNode Implementation
TrieNode::TrieNode() = default;

TrieNode::~TrieNode() = default;

void TrieNode::insert(const QString& word, const QString& dictionaryId) {
    TrieNode* current = this;
    
    for (const QChar& ch : word.toLower()) {
        if (current->children.find(ch) == current->children.end()) {
            current->children[ch] = std::make_unique<TrieNode>();
        }
        current = current->children[ch].get();
    }
    
    current->words.insert(word);
    current->wordToDictionaries[word].append(dictionaryId);
}

QStringList TrieNode::search(const QString& prefix, int maxResults) const {
    const TrieNode* current = this;
    QString lowerPrefix = prefix.toLower();
    
    // Navigate to the prefix
    for (const QChar& ch : lowerPrefix) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            return QStringList(); // Prefix not found
        }
        current = it->second.get();
    }
    
    // Collect words with this prefix
    QStringList results;
    int count = 0;
    current->collectWords(prefix, results, count, maxResults);
    
    return results;
}

bool TrieNode::contains(const QString& word) const {
    const TrieNode* current = this;
    QString lowerWord = word.toLower();
    
    for (const QChar& ch : lowerWord) {
        auto it = current->children.find(ch);
        if (it == current->children.end()) {
            return false;
        }
        current = it->second.get();
    }
    
    return current->words.contains(word);
}

void TrieNode::collectWords(const QString& prefix, QStringList& results, int& count, int maxResults) const {
    if (count >= maxResults) {
        return;
    }
    
    // Add words at current node
    for (const QString& word : words) {
        if (count >= maxResults) {
            break;
        }
        results.append(word);
        count++;
    }
    
    // Recursively collect from children
    for (auto it = children.begin(); it != children.end() && count < maxResults; ++it) {
        it->second->collectWords(prefix + it->first, results, count, maxResults);
    }
}

// IndexEngine Implementation
IndexEngine::IndexEngine() : m_trie(std::make_unique<TrieNode>()) {}

void IndexEngine::addWord(const QString& word, const QString& dictionaryId) {
    if (word.isEmpty()) {
        return;
    }
    
    QString normalizedWord = normalizeWord(word);
    
    // Add to trie
    m_trie->insert(word, dictionaryId);
    
    // Add to word index
    IndexEntry& entry = m_wordIndex[normalizedWord];
    entry.word = word;
    entry.normalizedWord = normalizedWord;
    if (!entry.dictionaryIds.contains(dictionaryId)) {
        entry.dictionaryIds.append(dictionaryId);
    }
    entry.frequency++;
    
    // Add to dictionary words mapping
    m_dictionaryWords[dictionaryId].insert(word);
}

void IndexEngine::removeWord(const QString& word, const QString& dictionaryId) {
    QString normalizedWord = normalizeWord(word);
    
    // Remove from word index
    auto it = m_wordIndex.find(normalizedWord);
    if (it != m_wordIndex.end()) {
        it->dictionaryIds.removeAll(dictionaryId);
        if (it->dictionaryIds.isEmpty()) {
            m_wordIndex.erase(it);
        }
    }
    
    // Remove from dictionary words mapping
    m_dictionaryWords[dictionaryId].remove(word);
}

void IndexEngine::clearDictionary(const QString& dictionaryId) {
    // Remove all words from this dictionary
    const QSet<QString>& words = m_dictionaryWords[dictionaryId];
    for (const QString& word : words) {
        removeWord(word, dictionaryId);
    }
    
    m_dictionaryWords.remove(dictionaryId);
    
    // Rebuild trie
    buildIndex();
}

QStringList IndexEngine::exactMatch(const QString& word) const {
    QString normalizedWord = normalizeWord(word);
    auto it = m_wordIndex.find(normalizedWord);
    if (it != m_wordIndex.end()) {
        return QStringList() << it->word;
    }
    return QStringList();
}

QStringList IndexEngine::prefixSearch(const QString& prefix, int maxResults) const {
    return m_trie->search(prefix, maxResults);
}

QStringList IndexEngine::fuzzySearch(const QString& word, int maxResults) const {
    QStringList results;
    QString lowerWord = word.toLower();
    
    // Find words with edit distance <= 2
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ++it) {
        if (results.size() >= maxResults) {
            break;
        }
        
        int distance = calculateEditDistance(lowerWord, it->normalizedWord.toLower());
        if (distance <= 2) {
            results.append(it->word);
        }
    }
    
    // Sort by edit distance (simplified - could be more sophisticated)
    std::sort(results.begin(), results.end(), [&](const QString& a, const QString& b) {
        int distA = calculateEditDistance(lowerWord, normalizeWord(a).toLower());
        int distB = calculateEditDistance(lowerWord, normalizeWord(b).toLower());
        return distA < distB;
    });
    
    return results;
}

QStringList IndexEngine::wildcardSearch(const QString& pattern, int maxResults) const {
    QStringList results;
    
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ++it) {
        if (results.size() >= maxResults) {
            break;
        }
        
        if (matchesWildcard(it->word, pattern)) {
            results.append(it->word);
        }
    }
    
    return results;
}

QStringList IndexEngine::getAllWords() const {
    QStringList words;
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ++it) {
        words.append(it->word);
    }
    return words;
}

int IndexEngine::getWordCount() const {
    return m_wordIndex.size();
}

QStringList IndexEngine::getDictionariesForWord(const QString& word) const {
    QString normalizedWord = normalizeWord(word);
    auto it = m_wordIndex.find(normalizedWord);
    if (it != m_wordIndex.end()) {
        return it->dictionaryIds;
    }
    return QStringList();
}

void IndexEngine::buildIndex() {
    m_trie = std::make_unique<TrieNode>();
    
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ++it) {
        for (const QString& dictionaryId : it->dictionaryIds) {
            m_trie->insert(it->word, dictionaryId);
        }
    }
    
    m_indexBuilt = true;
}

void IndexEngine::saveIndex(const QString& filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << "Failed to save index to:" << filePath;
        return;
    }
    
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    
    // Save word index
    stream << static_cast<quint32>(m_wordIndex.size());
    for (auto it = m_wordIndex.begin(); it != m_wordIndex.end(); ++it) {
        stream << it->word << it->normalizedWord << it->dictionaryIds << it->frequency;
    }
    
    qDebug() << "Index saved successfully to:" << filePath;
}

bool IndexEngine::loadIndex(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to load index from:" << filePath;
        return false;
    }
    
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    
    m_wordIndex.clear();
    m_dictionaryWords.clear();
    
    // Load word index
    quint32 wordCount;
    stream >> wordCount;
    
    for (quint32 i = 0; i < wordCount; ++i) {
        IndexEntry entry;
        stream >> entry.word >> entry.normalizedWord >> entry.dictionaryIds >> entry.frequency;
        
        m_wordIndex[entry.normalizedWord] = entry;
        
        for (const QString& dictionaryId : entry.dictionaryIds) {
            m_dictionaryWords[dictionaryId].insert(entry.word);
        }
    }
    
    buildIndex();
    
    qDebug() << "Index loaded successfully from:" << filePath;
    return true;
}

QString IndexEngine::normalizeWord(const QString& word) const {
    return word.toLower().trimmed();
}

int IndexEngine::calculateEditDistance(const QString& word1, const QString& word2) const {
    int len1 = word1.length();
    int len2 = word2.length();
    
    QVector<QVector<int>> dp(len1 + 1, QVector<int>(len2 + 1));
    
    for (int i = 0; i <= len1; ++i) {
        dp[i][0] = i;
    }
    
    for (int j = 0; j <= len2; ++j) {
        dp[0][j] = j;
    }
    
    for (int i = 1; i <= len1; ++i) {
        for (int j = 1; j <= len2; ++j) {
            if (word1[i-1] == word2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    
    return dp[len1][len2];
}

bool IndexEngine::matchesWildcard(const QString& word, const QString& pattern) const {
    QString regexPattern = pattern;
    regexPattern.replace("*", ".*").replace("?", ".");
    
    QRegularExpression regex("^" + regexPattern + "$", QRegularExpression::CaseInsensitiveOption);
    return regex.match(word).hasMatch();
}

}