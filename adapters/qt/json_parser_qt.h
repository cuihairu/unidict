#ifndef UNIDICT_JSON_PARSER_QT_H
#define UNIDICT_JSON_PARSER_QT_H

#include "core/unidict_core.h"
#include "core/std/json_parser_std.h"

namespace UnidictAdaptersQt {

class JsonParserQt : public UnidictCore::DictionaryParser {
public:
    JsonParserQt() = default;
    ~JsonParserQt() override = default;

    bool loadDictionary(const QString& filePath) override;
    bool isLoaded() const override;
    QStringList getSupportedExtensions() const override;

    UnidictCore::DictionaryEntry lookup(const QString& word) const override;
    QStringList findSimilar(const QString& word, int maxResults = 10) const override;
    QStringList getAllWords() const override;

    QString getDictionaryName() const override;
    QString getDictionaryDescription() const override;
    int getWordCount() const override;
    QString getSourcePath() const override;
    QString getDictionaryId() const override;
    QString getFormatName() const override;

private:
    UnidictCoreStd::JsonParserStd impl_;
    QString sourcePath_;
    QString dictionaryId_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_JSON_PARSER_QT_H
