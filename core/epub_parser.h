#ifndef EPUB_PARSER_H
#define EPUB_PARSER_H

#include "unidict_core.h"

#include <QMap>
#include <QString>
#include <QStringList>

#include "epub_parser_std.h"

namespace UnidictCore {

// EPUB 词典 parser：薄 Qt 适配，词条提取在 std/epub_parser_std（零 Qt）。
// 词条约定与大小写语义见 std 层注释；lookup 大小写不敏感，前缀补全走
// m_lowerWords 有序键二分（与 JsonParser 同款）。
class EpubParser : public DictionaryParser {
public:
    EpubParser() = default;
    ~EpubParser() override = default;

    bool loadDictionary(const QString& filePath) override;
    bool isLoaded() const override;
    QStringList getSupportedExtensions() const override;

    DictionaryEntry lookup(const QString& word) const override;
    QStringList findSimilar(const QString& word, int maxResults = 10) const override;
    QStringList getAllWords() const override;
    QVector<QPair<QString, QString>> allEntries() const override;
    QStringList prefixSearch(const QString& prefix, int maxResults = 20) const override;

    QString getDictionaryName() const override;
    QString getDictionaryDescription() const override;
    int getWordCount() const override;
    QString getSourcePath() const override;
    QString getDictionaryId() const override;
    QString getFormatName() const override;

private:
    UnidictCoreStd::EpubParserStd parser_;
    QString m_sourcePath;
    QString m_dictionaryId;
    QString m_name;
    QStringList m_words;                 // 原词形
    QMap<QString, QString> m_definitions; // lower(word) -> 释义
    QMap<QString, QString> m_lowerWords;  // lower(word) -> 原词形（前缀二分索引）
    bool m_loaded = false;
};

}

#endif // EPUB_PARSER_H
