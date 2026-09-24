#ifndef UNIDICT_DATA_STORE_QT_H
#define UNIDICT_DATA_STORE_QT_H

#include <QString>
#include <QStringList>
#include <QList>
#include <memory>

#include "core/unidict_core.h"
#include "core/std/data_store_std.h"

namespace UnidictAdaptersQt {

class DataStoreQt {
public:
    static DataStoreQt& instance();

    void setStoragePath(const QString& filePath);
    QString storagePath() const;

    void addSearchHistory(const QString& word);
    QStringList getSearchHistory(int limit = 100) const;
    void clearHistory();

    void addVocabularyItem(const UnidictCore::DictionaryEntry& entry);
    void addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt);
    QList<UnidictCore::DictionaryEntry> getVocabulary() const;
    QVariantList getVocabularyMeta() const; // [{word,definition,added_at,tags}]
    // 按词（大小写不敏感）设置分组标签；命中返回真，未命中返回假不动数据
    bool setVocabularyItemTags(const QString& word, const QStringList& tags);
    void removeVocabularyItem(const QString& word);
    void clearVocabulary();
    bool exportVocabularyCSV(const QString& filePath) const;

    // 词条笔记：按词（大小写不敏感）upsert；text 空串即移除该词笔记
    void setNote(const QString& word, const QString& text);
    QString getNote(const QString& word) const;
    QVariantList getNotes() const; // [{word,text,updated_at}]

private:
    DataStoreQt();
    std::unique_ptr<UnidictCoreStd::DataStoreStd> impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_DATA_STORE_QT_H
