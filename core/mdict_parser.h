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
    QJsonObject parseHeader(const QByteArray& headerData) const;
    
    QString extractDefinition(const QString& word) const;
    
    MdxHeader m_header;
    QMap<QString, QString> m_wordIndex; // word -> definition
    QStringList m_wordList;
    QMap<QString, QByteArray> m_resources; // resource files from .mdd
    bool m_loaded = false;
    QString m_filePath;
};

}

#endif // MDICT_PARSER_H