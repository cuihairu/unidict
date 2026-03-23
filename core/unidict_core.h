#ifndef UNIDICT_CORE_H
#define UNIDICT_CORE_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QJsonObject>
#include <memory>
#include <vector>

namespace UnidictCore {

struct DictionaryEntry {
    QString word;
    QString definition;
    QString pronunciation;
    QStringList examples;
    QVariantMap metadata;
};

struct DictionaryInfo {
    QString id;
    QString name;
    QString description;
    QString filePath;
    QString format;
    QStringList tags;
    int wordCount = 0;
    bool enabled = true;
    int priority = 0;
};

struct DictionaryMatch {
    DictionaryEntry entry;
    QString dictionaryId;
    QString dictionaryName;
};

struct LookupResult {
    bool success = false;
    QString query;
    QString message;
    DictionaryEntry entry;
    QVector<DictionaryMatch> matches;
    QStringList suggestions;
    QString dictionaryId;
    QString dictionaryName;
};

struct SearchHistoryItem {
    QString query;
    bool success = false;
    QString dictionaryName;
    bool pinned = false;
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
    virtual QString getSourcePath() const = 0;
    virtual QString getDictionaryId() const = 0;
    virtual QString getFormatName() const = 0;
};

class DictionaryManager {
public:
    static DictionaryManager& instance();
    
    bool addDictionary(const QString& filePath);
    int addDictionariesFromDirectory(const QString& directoryPath);
    bool removeDictionary(const QString& dictionaryId);
    bool setDictionaryEnabled(const QString& dictionaryId, bool enabled);
    bool setDictionaryTags(const QString& dictionaryId, const QStringList& tags);
    bool moveDictionaryUp(const QString& dictionaryId);
    bool moveDictionaryDown(const QString& dictionaryId);
    bool loadState(const QString& stateFilePath = QString());
    bool saveState(const QString& stateFilePath = QString()) const;
    QString defaultStateFilePath() const;
    void clear();
    bool hasDictionaries() const;
    QStringList getLoadedDictionaries() const;
    QVector<DictionaryInfo> getLoadedDictionaryInfos() const;
    QVector<SearchHistoryItem> getSearchHistory(int maxItems = 50) const;
    void clearSearchHistory();
    bool removeSearchHistoryItem(const QString& query);
    bool setSearchHistoryPinned(const QString& query, bool pinned);
    bool exportSearchHistory(const QString& filePath) const;
    bool importSearchHistory(const QString& filePath, bool replaceExisting = false);
    
    LookupResult searchWord(const QString& word) const;
    QStringList searchSimilar(const QString& word, int maxResults = 10) const;
    QString lastError() const;
    
private:
    QString resolveStateFilePath(const QString& stateFilePath) const;
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject& object);
    void recordSearch(const LookupResult& result);

    struct DictionaryRecord {
        std::unique_ptr<DictionaryParser> parser;
        bool enabled = true;
        QStringList tags;
    };

    DictionaryManager() = default;
    std::vector<DictionaryRecord> m_parsers;
    QVector<SearchHistoryItem> m_history;
    QString m_lastError;
};

QString searchWord(const QString& word);
LookupResult lookupWord(const QString& word);
QString formatLookupResult(const LookupResult& result);

}

#endif // UNIDICT_CORE_H
