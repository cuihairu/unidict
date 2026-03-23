#ifndef MDICT_PARSER_H
#define MDICT_PARSER_H

#include "unidict_core.h"
#include <QFile>
#include <QMap>
#include <QJsonObject>
#include <QVector>

namespace UnidictCore {

struct MdxHeader {
    QString title;
    QString description;
    QString encoding;
    QString stylesheet;
    int version;
    int wordCount;
    bool encrypted;
};

class MdictParser : public DictionaryParser {
public:
    MdictParser();
    ~MdictParser() override = default;

    bool loadDictionary(const QString& filePath) override;
    bool isLoaded() const override;
    QStringList getSupportedExtensions() const override;
    
    DictionaryEntry lookup(const QString& word) const override;
    QStringList findSimilar(const QString& word, int maxResults = 10) const override;
    QStringList getAllWords() const override;
    
    QString getDictionaryName() const override;
    QString getDictionaryDescription() const override;
    int getWordCount() const override;
    QString getSourcePath() const override;
    QString getDictionaryId() const override;
    QString getFormatName() const override;

private:
    bool loadMdxFile(const QString& mdxPath);
    bool loadMddFile(const QString& mddPath);
    
    QByteArray decompressData(const QByteArray& compressedData) const;
    QJsonObject parseHeader(const QByteArray& headerData) const;
    
    QString extractDefinition(const QString& word) const;
    QString decodeText(const QByteArray& data, bool stripTrailingNull = false) const;
    quint64 readBigEndianNumber(QIODevice& device, int width) const;
    quint16 readBigEndianWord(QIODevice& device) const;

    struct KeyBlockInfo {
        quint64 entryCount = 0;
        QString firstWord;
        QString lastWord;
        quint64 compressedSize = 0;
        quint64 decompressedSize = 0;
    };

    struct KeyEntry {
        quint64 offset = 0;
        QString word;
    };

    bool decodeKeyBlocks(QIODevice& device, quint64 keyBlockInfoSize, quint64 keyBlocksSize);
    QVector<KeyBlockInfo> decodeKeyBlockInfo(const QByteArray& data) const;
    QVector<KeyEntry> decodeKeyBlockData(const QByteArray& data, quint64 expectedEntryCount) const;
    bool decodeRecordBlocks(QIODevice& device);
    
    MdxHeader m_header;
    QMap<QString, QString> m_wordIndex; // word -> definition
    QMap<QString, QString> m_canonicalWords;
    QStringList m_wordList;
    QMap<QString, QByteArray> m_resources; // resource files from .mdd
    QVector<KeyEntry> m_keyEntries;
    bool m_loaded = false;
    QString m_filePath;
    QString m_dictionaryId;
    int m_numberWidth = 8;
    QString m_encoding = "UTF-8";
};

}

#endif // MDICT_PARSER_H
