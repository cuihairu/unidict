#ifndef MDICT_PARSER_H
#define MDICT_PARSER_H

#include "unidict_core.h"
#include <QFile>
#include <QMap>
#include <QJsonObject>

namespace UnidictCore {

struct MdxHeader {
    QString title;
    QString description;
    QString encoding;
    QString stylesheet;
    int version;
    int wordCount;
    bool encrypted;
    QString compression; // e.g., "zlib", "none", or unknown
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

private:
    bool loadMdxFile(const QString& mdxPath);
    bool loadMddFile(const QString& mddPath);
    
    QByteArray decompressData(const QByteArray& compressedData) const;
    QString decryptData(const QByteArray& encryptedData) const;
    QJsonObject parseHeader(const QByteArray& headerData) const; // legacy stub
    MdxHeader parseHeaderText(const QString& headerText) const; // XML-like header parsing
    
    QString extractDefinition(const QString& word) const;

    // Experimental: best-effort parser for zlib + unencrypted MDX to extract words/definitions
    bool tryExperimentalParse(const QString& mdxPath);
    static QList<QByteArray> scanAndDecompressZlibStreams(const QByteArray& data, int maxBlocks, int maxOutPerBlock);
    
    MdxHeader m_header;
    QMap<QString, QString> m_wordIndex; // word -> definition
    QMap<QString, quint64> m_keyOffsets; // word -> record offset (for real parser)
    QVector<QPair<quint64, quint64>> m_recordBlocks; // (compressedSize, decompressedSize)
    QStringList m_wordList;
    QMap<QString, QByteArray> m_resources; // resource files from .mdd
    bool m_loaded = false;
    QString m_filePath;
};

}

#endif // MDICT_PARSER_H
