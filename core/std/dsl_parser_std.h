#ifndef UNIDICT_DSL_PARSER_STD_H
#define UNIDICT_DSL_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>

#include "dictionary_parser_std.h"

namespace UnidictCoreStd {

// DSL (Dictionary Specification Language) parser for ABBYY Lingvo dictionaries
// DSL format is a text-based format with special markup for dictionary entries
class DslParserStd : public DictionaryParserStd {
public:
    DslParserStd();

    bool load_dictionary(const std::string& dsl_path) override;
    bool is_loaded() const override;

    std::string dictionary_name() const override;
    std::string dictionary_description() const override;
    int word_count() const;

    std::string lookup(const std::string& word) const override;
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const override;

private:
    bool parse_header(const std::string& line);
    void parse_entry(const std::string& headword, const std::string& definition);
    std::string clean_markup(const std::string& text) const;
    std::string extract_headword(const std::string& line) const;

    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::string source_lang_;
    std::string target_lang_;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DSL_PARSER_STD_H