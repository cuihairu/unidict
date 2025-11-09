#include "dictionary_manager_std.h"

#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string lcase(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

std::string DictionaryManagerStd::Holder::lookup(const std::string& w) const {
    if (json) return json->lookup(w);
    if (stardict) return stardict->lookup(w);
    if (mdict) return mdict->lookup(w);
    if (dsl) return dsl->lookup(w);
    if (csv) return csv->lookup(w);
    return {};
}

DictionaryManagerStd::DictionaryManagerStd() = default;

bool DictionaryManagerStd::add_dictionary(const std::string& path) {
    auto ext = lcase(fs::path(path).extension().string());
    Holder h;
    if (ext == ".json") {
        auto p = std::make_shared<JsonParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.json = p; h.name = p->name(); h.words = p->all_words();
    } else if (ext == ".ifo") {
        auto p = std::make_shared<StarDictParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.stardict = p; h.name = p->dictionary_name(); h.words = p->all_words();
    } else if (ext == ".mdx") {
        auto p = std::make_shared<MdictParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.mdict = p; h.name = p->dictionary_name(); h.words = p->all_words();
    } else if (ext == ".dsl") {
        auto p = std::make_shared<DslParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.dsl = p; h.name = p->dictionary_name(); h.words = p->all_words();
    } else if (ext == ".csv" || ext == ".tsv" || ext == ".txt") {
        auto p = std::make_shared<CsvParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.csv = p; h.name = p->dictionary_name(); h.words = p->all_words();
    } else {
        return false;
    }
    for (const auto& w : h.words) index_.add_word(w, h.name);
    dicts_.push_back(std::move(h));
    return true;
}

bool DictionaryManagerStd::remove_dictionary(const std::string& dict_name) {
    bool removed = false;
    auto it = dicts_.begin();
    while (it != dicts_.end()) {
        if (it->name == dict_name) {
            for (const auto& w : it->words) index_.remove_word(w, dict_name);
            it = dicts_.erase(it); removed = true;
        } else { ++it; }
    }
    index_.build_index();
    return removed;
}

std::vector<std::string> DictionaryManagerStd::loaded_dictionaries() const {
    std::vector<std::string> v; v.reserve(dicts_.size());
    for (auto& d : dicts_) v.push_back(d.name);
    return v;
}

std::vector<DictionaryManagerStd::DictMeta> DictionaryManagerStd::dictionaries_meta() const {
    std::vector<DictMeta> out; out.reserve(dicts_.size());
    for (auto& d : dicts_) {
        int wc = (int)d.words.size();
        std::string desc;
        if (d.json) desc = d.json->description();
        else if (d.stardict) desc = d.stardict->dictionary_description();
        else if (d.mdict) desc = d.mdict->dictionary_description();
        else if (d.dsl) desc = d.dsl->dictionary_description();
        else if (d.csv) desc = d.csv->dictionary_description();
        out.push_back({d.name, wc, desc});
    }
    return out;
}

std::string DictionaryManagerStd::search_word(const std::string& word) const {
    for (auto& d : dicts_) { auto def = d.lookup(word); if (!def.empty()) return def; }
    return {};
}

std::vector<DictEntryStd> DictionaryManagerStd::search_all(const std::string& word) const {
    std::vector<DictEntryStd> out;
    for (auto& d : dicts_) {
        auto def = d.lookup(word);
        if (!def.empty()) out.push_back({d.name, word, def});
    }
    return out;
}

void DictionaryManagerStd::build_index() { index_.build_index(); }

std::vector<std::string> DictionaryManagerStd::exact_search(const std::string& word) const { return index_.exact_match(word); }

std::vector<std::string> DictionaryManagerStd::prefix_search(const std::string& prefix, int max_results) const { return index_.prefix_search(prefix, max_results); }
std::vector<std::string> DictionaryManagerStd::fuzzy_search(const std::string& word, int max_results) const { return index_.fuzzy_search(word, max_results); }
std::vector<std::string> DictionaryManagerStd::wildcard_search(const std::string& pattern, int max_results) const { return index_.wildcard_search(pattern, max_results); }
std::vector<std::string> DictionaryManagerStd::regex_search(const std::string& pattern, int max_results) const { return index_.regex_search(pattern, max_results); }
std::vector<std::string> DictionaryManagerStd::dictionaries_for_word(const std::string& word) const { return index_.dictionaries_for_word(word); }
std::vector<std::string> DictionaryManagerStd::all_indexed_words() const { return index_.all_words(); }
int DictionaryManagerStd::indexed_word_count() const { return index_.word_count(); }
bool DictionaryManagerStd::save_index(const std::string& f) const { return index_.save_index(f); }
bool DictionaryManagerStd::load_index(const std::string& f) { return index_.load_index(f); }

} // namespace UnidictCoreStd
