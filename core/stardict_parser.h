#ifndef STARDICT_PARSER_H
#define STARDICT_PARSER_H

#include "unidict_core.h"
#include <QFile>
#include <QTextStream>
#include <QMap>

namespace UnidictCore {

struct StarDictHeader {
    QString version;
    QString bookName;
    int wordCount;
    int indexFileSize;
    QString author;
    QString email;
    QString website;
    QString description;
    QString date;
};

class StarDictParser : public DictionaryParser {
public:
    StarDictParser();
    ~StarDictParser() override = default;

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
    
    QString extractDefinition(qint64 offset, qint32 size) const;
    
    StarDictHeader m_header;
    QMap<QString, QPair<qint64, qint32>> m_wordIndex; // word -> (offset, size)
    QStringList m_wordList;
    QFile m_dictFile;
    bool m_loaded = false;
};

}

#endif // STARDICT_PARSER_H