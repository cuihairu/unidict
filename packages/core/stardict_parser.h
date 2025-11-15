#ifndef STARDICT_PARSER_H
#define STARDICT_PARSER_H

#include "unidict_core.h"
#include <QFile>
#include <QTextStream>
#include <QMap>
#include <QByteArray>

namespace UnidictCore {

struct StarDictHeader {
    QString version;
    QString bookName;
    int wordCount;
    int indexFileSize;
    int idxOffsetBits = 32; // default 32; 64 when specified in .ifo
    QString author;
    QString email;
    QString website;
    QString description;
    QString date;
};

class StarDictParser : public DictionaryParser {
public:
    StarDictParser();
    ~StarDictParser() override;

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
    bool loadIfoFile(const QString& ifoPath);
    bool loadIdxFile(const QString& idxPath);
    bool loadDictFile(const QString& dictPath);
    QByteArray decompressGzipFile(const QString& gzPath) const; // legacy in-memory
    bool decompressGzipToFile(const QString& gzPath, const QString& outPath) const;
    
    QString extractDefinition(qint64 offset, qint32 size) const;
    
    StarDictHeader m_header;
    QMap<QString, QPair<qint64, qint32>> m_wordIndex; // word -> (offset, size)
    QStringList m_wordList;
    QFile m_dictFile;
    QByteArray m_dictBuffer; // holds decompressed data when .dict.dz is used
    bool m_loaded = false;
    QString m_cachePath; // on-disk cache for decompressed dict

    // Optional memory-mapping of .dict file for faster random access
    uchar* m_mappedPtr = nullptr;
    qint64 m_mappedSize = 0;
};

}

#endif // STARDICT_PARSER_H
