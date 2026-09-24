#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include "unidict_core.h"
#include <QFile>
#include <QMap>

namespace UnidictCore {

// Simple custom JSON dictionary parser for testing and custom dictionaries.
// Expected format:
// {
//   "name": "My Dictionary",
//   "description": "Optional",
//   "entries": [ { "word": "hello", "definition": "..." }, ... ]
// }
class JsonParser : public DictionaryParser {
public:
    JsonParser();
    ~JsonParser() override = default;

    bool loadDictionary(const QString& filePath) override;
    bool isLoaded() const override;
    QStringList getSupportedExtensions() const override;

    DictionaryEntry lookup(const QString& word) const override;
    QStringList findSimilar(const QString& word, int maxResults = 10) const override;
    QStringList getAllWords() const override;
    QVector<QPair<QString, QString>> allEntries() const override;

    QString getDictionaryName() const override;
    QString getDictionaryDescription() const override;
    int getWordCount() const override;

    QString getSourcePath() const override;
    QString getDictionaryId() const override;
    QString getFormatName() const override;

private:
    QString m_name;
    QString m_description;
    QString m_sourcePath;
    QString m_dictionaryId;
    QMap<QString, QString> m_entries; // word -> definition
    QStringList m_words;
    bool m_loaded = false;
};

}

#endif // JSON_PARSER_H

