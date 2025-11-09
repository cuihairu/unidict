#ifndef UNIDICT_CSV_PARSER_STD_H
#define UNIDICT_CSV_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

// CSV/TSV parser for simple tab or comma-separated dictionary files
// Format: word<separator>definition (one entry per line)
class CsvParserStd {
public:
    CsvParserStd();

    bool load_dictionary(const std::string& csv_path);
    bool is_loaded() const;

    std::string dictionary_name() const;
    std::string dictionary_description() const;
    int word_count() const;

    std::string lookup(const std::string& word) const;
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    char detect_separator(const std::string& line) const;
    void parse_line(const std::string& line, char separator);

    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_CSV_PARSER_STD_H