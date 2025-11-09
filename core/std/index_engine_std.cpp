#include "index_engine_std.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>

namespace UnidictCoreStd {

// to-lower ascii and trim spaces
static inline std::string lcase(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (unsigned char c : s) out.push_back((char)std::tolower(c));
    return out;
}

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e-1])) --e;
    return s.substr(b, e - b);
}

void TrieNode::insert(const std::string& word) {
    TrieNode* cur = this;
    for (char ch : lcase(word)) {
        auto it = cur->children.find(ch);
        if (it == cur->children.end()) {
            cur->children.emplace(ch, std::make_unique<TrieNode>());
            it = cur->children.find(ch);
        }
        cur = it->second.get();
    }
    cur->words.insert(word);
}

void TrieNode::collect(const std::string& prefix, std::vector<std::string>& out, int& count, int max_results) const {
    if (count >= max_results) return;
    for (const auto& w : words) {
        if (count >= max_results) break;
        out.push_back(w);
        ++count;
    }
    for (const auto& kv : children) {
        if (count >= max_results) break;
        kv.second->collect(prefix + kv.first, out, count, max_results);
    }
}

IndexEngineStd::IndexEngineStd() : trie_(new TrieNode) {}

void IndexEngineStd::add_word(const std::string& word, const std::string& dictionary_id) {
    if (word.empty()) return;
    const std::string norm = normalize(word);
    IndexEntry& e = word_index_[norm];
    if (e.word.empty()) { e.word = word; e.normalized_word = norm; }
    if (std::find(e.dictionary_ids.begin(), e.dictionary_ids.end(), dictionary_id) == e.dictionary_ids.end()) {
        e.dictionary_ids.push_back(dictionary_id);
    }
    ++e.frequency;
    dict_[dictionary_id].insert(word);
}

void IndexEngineStd::remove_word(const std::string& word, const std::string& dictionary_id) {
    const std::string norm = normalize(word);
    auto it = word_index_.find(norm);
    if (it != word_index_.end()) {
        auto& vec = it->second.dictionary_ids;
        vec.erase(std::remove(vec.begin(), vec.end(), dictionary_id), vec.end());
        if (vec.empty()) word_index_.erase(it);
    }
    auto dit = dict_.find(dictionary_id);
    if (dit != dict_.end()) {
        dit->second.erase(word);
        if (dit->second.empty()) dict_.erase(dit);
    }
}

void IndexEngineStd::clear_dictionary(const std::string& dictionary_id) {
    auto it = dict_.find(dictionary_id);
    if (it == dict_.end()) return;
    // Copy to a stable list to avoid iterator invalidation while erasing
    std::vector<std::string> words(it->second.begin(), it->second.end());
    for (const auto& w : words) remove_word(w, dictionary_id);
}

void IndexEngineStd::build_index() {
    trie_.reset(new TrieNode);
    for (const auto& kv : word_index_) {
        trie_->insert(kv.second.word);
    }
    built_ = true;
}

std::vector<std::string> IndexEngineStd::exact_match(const std::string& word) const {
    const std::string norm = normalize(word);
    auto it = word_index_.find(norm);
    if (it == word_index_.end()) return {};
    return {it->second.word};
}

std::vector<std::string> IndexEngineStd::prefix_search(const std::string& prefix, int max_results) const {
    std::vector<std::string> out;
    if (!trie_) return out;
    // Walk down the trie to the node matching the lowercased prefix
    const std::string lp = lcase(prefix);
    const TrieNode* cur = trie_.get();
    for (char ch : lp) {
        auto it = cur->children.find(ch);
        if (it == cur->children.end()) return out;
        cur = it->second.get();
    }
    int count = 0;
    cur->collect(prefix, out, count, max_results);
    return out;
}

std::vector<std::string> IndexEngineStd::fuzzy_search(const std::string& word, int max_results) const {
    std::vector<std::pair<int, std::string>> scored;
    const std::string lw = lcase(word);
    for (const auto& kv : word_index_) {
        int d = edit_distance(lw, lcase(kv.second.word));
        if (d <= 2) scored.emplace_back(d, kv.second.word);
    }
    std::sort(scored.begin(), scored.end(), [](auto& a, auto& b){ return a.first < b.first; });
    std::vector<std::string> out; out.reserve(std::min<int>(max_results, (int)scored.size()));
    for (auto& p : scored) { if ((int)out.size() >= max_results) break; out.push_back(p.second); }
    return out;
}

std::vector<std::string> IndexEngineStd::wildcard_search(const std::string& pattern, int max_results) const {
    std::vector<std::string> out;
    for (const auto& kv : word_index_) {
        if ((int)out.size() >= max_results) break;
        if (wildcard_match(kv.second.word, pattern)) out.push_back(kv.second.word);
    }
    return out;
}

std::vector<std::string> IndexEngineStd::regex_search(const std::string& pattern, int max_results) const {
    std::vector<std::string> out;
    try {
        std::regex re(pattern, std::regex::icase);
        for (const auto& kv : word_index_) {
            if ((int)out.size() >= max_results) break;
            if (std::regex_search(kv.second.word, re)) out.push_back(kv.second.word);
        }
    } catch (const std::regex_error&) {
        // invalid pattern: return empty
        return {};
    }
    return out;
}

std::vector<std::string> IndexEngineStd::all_words() const {
    std::vector<std::string> v; v.reserve(word_index_.size());
    for (const auto& kv : word_index_) v.push_back(kv.second.word);
    return v;
}

std::vector<std::string> IndexEngineStd::dictionaries_for_word(const std::string& word) const {
    const std::string norm = normalize(word);
    auto it = word_index_.find(norm);
    if (it == word_index_.end()) return {};
    return it->second.dictionary_ids;
}

int IndexEngineStd::word_count() const { return (int)word_index_.size(); }

bool IndexEngineStd::save_index(const std::string& file_path) const {
    std::ofstream out(file_path, std::ios::binary);
    if (!out) return false;
    // Simple line format: word\tfrequency\tdict1|dict2|...\n
    for (const auto& kv : word_index_) {
        const auto& e = kv.second;
        out << e.word << "\t" << e.frequency << "\t";
        for (size_t i = 0; i < e.dictionary_ids.size(); ++i) {
            if (i) out << '|';
            out << e.dictionary_ids[i];
        }
        out << "\n";
    }
    return true;
}

bool IndexEngineStd::load_index(const std::string& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    word_index_.clear();
    dict_.clear();
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string word; int freq = 0; std::string dicts;
        if (!std::getline(iss, word, '\t')) continue;
        std::string freq_s; if (!std::getline(iss, freq_s, '\t')) continue; freq = std::stoi(freq_s);
        std::getline(iss, dicts);
        IndexEntry e; e.word = word; e.normalized_word = normalize(word); e.frequency = freq;
        std::istringstream ds(dicts);
        std::string id;
        while (std::getline(ds, id, '|')) {
            if (!id.empty()) { e.dictionary_ids.push_back(id); dict_[id].insert(word); }
        }
        word_index_[e.normalized_word] = std::move(e);
    }
    build_index();
    return true;
}

std::string IndexEngineStd::normalize(const std::string& s) { return trim(lcase(s)); }

int IndexEngineStd::edit_distance(const std::string& a, const std::string& b) {
    const int n = (int)a.size(), m = (int)b.size();
    std::vector<int> prev(m + 1), cur(m + 1);
    for (int j = 0; j <= m; ++j) prev[j] = j;
    for (int i = 1; i <= n; ++i) {
        cur[0] = i;
        for (int j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost });
        }
        prev.swap(cur);
    }
    return prev[m];
}

bool IndexEngineStd::wildcard_match(const std::string& word, const std::string& pattern) {
    std::string re;
    re.reserve(pattern.size() * 2);
    re.push_back('^');
    for (char c : pattern) {
        switch (c) {
            case '*': re += ".*"; break;
            case '?': re += '.'; break;
            case '.': case '\\': case '+': case '(': case ')': case '[': case ']': case '{': case '}': case '^': case '$': case '|':
                re.push_back('\\'); re.push_back(c); break;
            default: re.push_back(c); break;
        }
    }
    re.push_back('$');
    return std::regex_match(word, std::regex(re, std::regex::icase));
}

} // namespace UnidictCoreStd
