#ifndef UNIDICT_DATA_STORE_QT_H
#define UNIDICT_DATA_STORE_QT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <memory>

#include "core/unidict_core.h"
#include "core/std/data_store_std.h"

namespace UnidictAdaptersQt {

// 搜索历史条目（P-7 双存储合一：单源在 DataStoreStd 的结构化记录，
// 本桥只做 QString 类型映射）
struct SearchHistoryEntry {
    QString query;
    bool success = true;
    QString dictionaryName;
    bool pinned = false;
};

class DataStoreQt {
public:
    static DataStoreQt& instance();

    void setStoragePath(const QString& filePath);
    QString storagePath() const;

    void addSearchHistory(const QString& word);
    QStringList getSearchHistory(int limit = 100) const;
    void clearHistory();
    // 结构化面（legacy manager 转发此面）：查词记录 + 置顶/删除
    void addSearchHistoryEntry(const QString& query, bool success,
                               const QString& dictionaryName);
    QList<SearchHistoryEntry> getSearchHistoryEntries(int limit = 100) const;
    bool setSearchHistoryPinned(const QString& query, bool pinned);
    bool removeSearchHistoryItem(const QString& query);
    // 整表重建（同步回放面）：按给定序原样替换（index 0 = 最新），单次落盘
    void restoreSearchHistory(const QStringList& queries);

    void addVocabularyItem(const UnidictCore::DictionaryEntry& entry);
    void addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt);
    QList<UnidictCore::DictionaryEntry> getVocabulary() const;
    QVariantList getVocabularyMeta() const; // [{word,definition,added_at,tags}]
    // 按词（大小写不敏感）设置分组标签；命中返回真，未命中返回假不动数据
    bool setVocabularyItemTags(const QString& word, const QStringList& tags);
    // M3 标签管理：增/删单个标签 + 按标签筛选（语义同 std 侧，见彼处注释）
    bool addVocabularyItemTag(const QString& word, const QString& tag);
    bool removeVocabularyItemTag(const QString& word, const QString& tag);
    QVariantList getVocabularyByTag(const QString& tag) const; // [{word,definition,added_at,tags}]
    void removeVocabularyItem(const QString& word);
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
    DataStoreQt();
    std::unique_ptr<UnidictCoreStd::DataStoreStd> impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_DATA_STORE_QT_H
