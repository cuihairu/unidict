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

    // Vocabulary book (word + definition + grouping tags)
    void addVocabularyItem(const DictionaryEntry& entry);
    void addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt);
    void removeVocabularyItem(const QString& word);
    QList<DictionaryEntry> getVocabulary() const;
    QVariantList getVocabularyMeta() const; // [{word,definition,added_at,tags}]
    // 按词（大小写不敏感）设置生词分组标签；命中返回真，未命中返回假
    bool setVocabularyItemTags(const QString& word, const QStringList& tags);
    // M3 标签管理：增/删单个标签 + 按标签筛选（语义同 std 侧，见彼处注释）
    bool addVocabularyItemTag(const QString& word, const QString& tag);
    bool removeVocabularyItemTag(const QString& word, const QString& tag);
    QVariantList getVocabularyByTag(const QString& tag) const; // [{word,definition,added_at,tags}]
    void clearVocabulary();
    bool exportVocabularyCSV(const QString& filePath) const;

    // 词条笔记：按词（大小写不敏感）upsert；text 空串即移除该词笔记
    void setNote(const QString& word, const QString& text);
    QString getNote(const QString& word) const;
    QVariantList getNotes() const; // [{word,text,updated_at}]

    // 发音练习历史（M9）：按词 upsert，word 空串即忽略
    void setPronRecord(const QString& word, double lastScore, double bestScore,
                       int attempts, qlonglong lastAt);
    QVariantMap getPronRecord(const QString& word) const;   // 空 map = 没练过
    QVariantList getPronRecords() const;                    // [{word,last_score,best_score,attempts,last_at}]
    void clearPronRecords();

private:
    DataStore();
};

}

#endif // DATA_STORE_H
