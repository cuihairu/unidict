// Qt-free dictionary manager: loads std parsers and indexes words.

#ifndef UNIDICT_DICTIONARY_MANAGER_STD_H
#define UNIDICT_DICTIONARY_MANAGER_STD_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "index_engine_std.h"
#include "json_parser_std.h"
#include "stardict_parser_std.h"
#include "mdict_parser_std.h"
#include "dsl_parser_std.h"
#include "csv_parser_std.h"
#include "fulltext_index_std.h"

namespace UnidictCoreStd {

struct DictEntryStd { std::string dict_name; std::string word; std::string definition; };

class DictionaryManagerStd {
public:
    DictionaryManagerStd();

    bool add_dictionary(const std::string& path);
    bool remove_dictionary(const std::string& dict_name);
    std::vector<std::string> loaded_dictionaries() const;
    struct DictMeta { std::string name; int word_count; std::string description; };
    std::vector<DictMeta> dictionaries_meta() const;

    std::string search_word(const std::string& word) const; // returns first match
    std::vector<DictEntryStd> search_all(const std::string& word) const;

    // Indexed searches
    void build_index();
    std::vector<std::string> exact_search(const std::string& word) const;
    std::vector<std::string> prefix_search(const std::string& prefix, int max_results = 10) const;
    std::vector<std::string> fuzzy_search(const std::string& word, int max_results = 10) const;
    std::vector<std::string> wildcard_search(const std::string& pattern, int max_results = 10) const;
    std::vector<std::string> regex_search(const std::string& pattern, int max_results = 10) const;
    std::vector<std::string> dictionaries_for_word(const std::string& word) const;
    std::vector<std::string> all_indexed_words() const;
    int indexed_word_count() const;
    bool save_index(const std::string& file) const;
    bool load_index(const std::string& file);

    // Minimal full-text search (MVP): scans definitions for substring matches.
    // Returns matching entries across all loaded dictionaries, in load order.
    std::vector<DictEntryStd> full_text_search(const std::string& query, int max_results = 10) const;

    // Full-text inverted index persistence (must match the same dictionary set/order)
    bool save_fulltext_index(const std::string& file) const;
    bool load_fulltext_index(const std::string& file);

private:
    struct Holder {
        // Only one of these is non-null
        std::shared_ptr<JsonParserStd> json;
        std::shared_ptr<StarDictParserStd> stardict;
        std::shared_ptr<MdictParserStd> mdict;
        std::shared_ptr<DslParserStd> dsl;
        std::shared_ptr<CsvParserStd> csv;
        std::string name;
        std::vector<std::string> words;
        std::string lookup(const std::string& w) const;
    };

    std::vector<Holder> dicts_;
    IndexEngineStd index_;
    mutable std::unique_ptr<FullTextIndexStd> ft_index_; // built lazily
    void ensure_fulltext_index_built() const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_MANAGER_STD_H
