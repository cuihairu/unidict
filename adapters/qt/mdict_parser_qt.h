#ifndef UNIDICT_MDICT_PARSER_QT_H
#define UNIDICT_MDICT_PARSER_QT_H

#include "core/unidict_core.h"
#include "core/std/mdict_parser_std.h"

namespace UnidictAdaptersQt {

class MdictParserQt : public UnidictCore::DictionaryParser {
public:
    MdictParserQt() = default;
    ~MdictParserQt() override = default;

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
    UnidictCoreStd::MdictParserStd impl_;
};

} // namespace UnidictAdaptersQt

#endif // UNIDICT_MDICT_PARSER_QT_H

