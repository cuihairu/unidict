// Qt-free JSON dictionary parser (minimal tolerant parser for project format).

#ifndef UNIDICT_JSON_PARSER_STD_H
#define UNIDICT_JSON_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

class JsonParserStd {
public:
    JsonParserStd();

    bool load_dictionary(const std::string& file_path);
    bool is_loaded() const;

    std::string name() const;
    std::string description() const;
    int word_count() const;

    std::string lookup(const std::string& word) const; // returns empty if not found
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::unordered_map<std::string, std::string> entries_; // word -> definition
    std::vector<std::string> words_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_JSON_PARSER_STD_H

