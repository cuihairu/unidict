#ifndef UNIDICT_STARDICT_PARSER_QT_H
#define UNIDICT_STARDICT_PARSER_QT_H

#include "core/unidict_core.h"
#include "core/std/stardict_parser_std.h"

namespace UnidictAdaptersQt {

class StarDictParserQt : public UnidictCore::DictionaryParser {
public:
    StarDictParserQt() = default;
    ~StarDictParserQt() override = default;

    bool loadDictionary(const QString& filePath) override;
    bool isLoaded() const override;
    QStringList getSupportedExtensions() const override;

    UnidictCore::DictionaryEntry lookup(const QString& word) const override;
    QStringList findSimilar(const QString& word, int maxResults = 10) const override;
    QStringList getAllWords() const override;

    QString getDictionaryName() const override;
    QString getDictionaryDescription() const override;
    int getWordCount() const override;

private:
    UnidictCoreStd::StarDictParserStd impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_STARDICT_PARSER_QT_H

