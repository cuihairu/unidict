#include "dictionary_manager_std.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

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
    h.src_paths.push_back(path);
    if (ext == ".json") {
        auto p = std::make_shared<JsonParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.json = p; h.name = p->name(); h.words = p->all_words();
    } else if (ext == ".ifo") {
        auto p = std::make_shared<StarDictParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.stardict = p; h.name = p->dictionary_name(); h.words = p->all_words();
        // Companion files: .idx and .dict/.dict.dz next to .ifo
        fs::path base = fs::path(path);
        base.replace_extension("");
        fs::path idx = base; idx += ".idx";
        fs::path dict = base; dict += ".dict";
        fs::path dz = base; dz += ".dict.dz";
        std::error_code ec;
        if (fs::exists(idx, ec)) h.src_paths.push_back(idx.string());
        if (fs::exists(dict, ec)) h.src_paths.push_back(dict.string());
        else if (fs::exists(dz, ec)) h.src_paths.push_back(dz.string());
    } else if (ext == ".mdx") {
        auto p = std::make_shared<MdictParserStd>();
        if (!p->load_dictionary(path)) return false;
        h.mdict = p; h.name = p->dictionary_name(); h.words = p->all_words();
        // Companion files: any .mdd with same stem
        fs::path mdx(path);
        fs::path dir = mdx.parent_path();
        std::string stem = mdx.stem().string();
        std::error_code ec;
        for (auto& de : fs::directory_iterator(dir, ec)) {
            if (!de.is_regular_file()) continue;
            fs::path q = de.path();
            if (lcase(q.extension().string()) == ".mdd" && q.stem().string() == stem) {
                h.src_paths.push_back(q.string());
            }
        }
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

std::vector<DictEntryStd> DictionaryManagerStd::full_text_search(const std::string& query, int max_results) const {
    std::vector<DictEntryStd> out;
    if (query.empty() || max_results <= 0) return out;
    ensure_fulltext_index_built();
    if (!ft_index_) return out;
    auto refs = ft_index_->search(query, max_results);
    out.reserve((int)refs.size());
    for (auto& r : refs) {
        if (r.dict < 0 || r.dict >= (int)dicts_.size()) continue;
        const auto& d = dicts_[r.dict];
        if (r.word < 0 || r.word >= (int)d.words.size()) continue;
        const std::string& w = d.words[r.word];
        std::string def = d.lookup(w);
        if (!def.empty()) out.push_back({ d.name, w, std::move(def) });
        if ((int)out.size() >= max_results) break;
    }
    return out;
}

void DictionaryManagerStd::ensure_fulltext_index_built() const {
    if (ft_index_) return;
    // Build lazily: index all definitions into an inverted index
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    std::vector<std::pair<std::string, FullTextIndexStd::DocRef>> docs;
    // Pre-collect documents for parallel build
    for (int di = 0; di < (int)dicts_.size(); ++di) {
        const auto& d = dicts_[di];
        for (int wi = 0; wi < (int)d.words.size(); ++wi) {
            const std::string& w = d.words[wi];
            std::string def = d.lookup(w);
            if (!def.empty()) docs.push_back({std::move(def), {di, wi}});
        }
    }
    idx->build_from_documents(docs, 0);
    const_cast<DictionaryManagerStd*>(this)->ft_index_ = std::move(idx);
}

bool DictionaryManagerStd::save_fulltext_index(const std::string& file) const {
    ensure_fulltext_index_built();
    if (!ft_index_) return false;
    ft_index_->set_signature(fulltext_signature());
    return ft_index_->save(file);
}

bool DictionaryManagerStd::load_fulltext_index(const std::string& file) {
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    if (!idx->load(file)) return false;
    // Check signature consistency
    const std::string cur = fulltext_signature();
    if (idx->signature() != cur) return false;
    ft_index_ = std::move(idx);
    return true;
}

static inline uint64_t fnv1a64(const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

std::string DictionaryManagerStd::fulltext_signature() const {
    // Deterministic signature combining names/word stats AND filesystem metadata of source paths.
    std::ostringstream ss;
    ss << "N=" << dicts_.size() << ';';
    for (const auto& d : dicts_) {
        ss << d.name << '|' << d.words.size() << '|';
        if (!d.words.empty()) ss << d.words.front() << '|' << d.words.back();
        ss << '|';
        // filesystem metadata for all companion source paths (stable order)
        std::vector<std::string> srcs = d.src_paths;
        std::sort(srcs.begin(), srcs.end());
        std::error_code ec;
        for (const auto& sp : srcs) {
            fs::path p = sp;
            if (fs::exists(p, ec)) {
                auto sz = fs::is_regular_file(p, ec) ? fs::file_size(p, ec) : 0ull;
                auto ts = fs::last_write_time(p, ec).time_since_epoch().count();
                ss << p.string() << '|' << (unsigned long long)sz << '|' << (long long)ts;
            } else {
                ss << p.string() << "|(missing)";
            }
            ss << '#';
        }
        ss << ';';
    }
    std::string s = ss.str();
    uint64_t hv = fnv1a64(s.data(), s.size());
    std::ostringstream out; out << std::hex << hv << '|' << s;
    return out.str();
}

bool DictionaryManagerStd::load_fulltext_index_relaxed(const std::string& file, int* out_version, std::string* out_error) {
    std::unique_ptr<FullTextIndexStd> idx(new FullTextIndexStd());
    if (!idx->load(file)) {
        if (out_error) *out_error = idx->last_error();
        return false;
    }
    if (out_version) *out_version = idx->version();
    // Ignore signature; accept any version we can parse
    ft_index_ = std::move(idx);
    return true;
}

} // namespace UnidictCoreStd
