#ifndef UNIDICT_CORE_H
#define UNIDICT_CORE_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <memory>

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
    
private:
    DictionaryManager() = default;
    std::vector<std::unique_ptr<DictionaryParser>> m_parsers;
};

QString searchWord(const QString& word);

}

#endif // UNIDICT_CORE_H
