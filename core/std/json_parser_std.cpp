#include "json_parser_std.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace UnidictCoreStd {

static inline std::string lcase(const std::string& s) {
    std::string t; t.reserve(s.size());
    for (unsigned char c : s) t.push_back((char)std::tolower(c));
    return t;
}

JsonParserStd::JsonParserStd() = default;

bool JsonParserStd::load_dictionary(const std::string& file_path) {
    entries_.clear(); words_.clear(); loaded_ = false; name_.clear(); desc_.clear();
    std::ifstream in(file_path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();

    auto find_str_val = [&](const std::string& key) -> std::string {
        const std::string pat = '"' + key + '"';
        size_t p = s.find(pat); if (p == std::string::npos) return {};
        p = s.find(':', p); if (p == std::string::npos) return {};
        size_t q = s.find('"', p); if (q == std::string::npos) return {};
        size_t r = s.find('"', q + 1); if (r == std::string::npos) return {};
        return s.substr(q + 1, r - q - 1);
    };

    name_ = find_str_val("name");
    desc_ = find_str_val("description");

    // entries array scan
    const std::string ent_pat = '"' + std::string("entries") + '"';
    size_t ep = s.find(ent_pat); if (ep == std::string::npos) return false;
    ep = s.find('[', ep); if (ep == std::string::npos) return false;
    int depth = 0; size_t i = ep;
    for (; i < s.size(); ++i) { if (s[i] == '[') { ++depth; break; } }
    if (depth == 0) return false;
    ++i;
    while (i < s.size()) {
        // find next object
        size_t obj = s.find('{', i);
        if (obj == std::string::npos) break;
        int d = 1; size_t j = obj + 1;
        for (; j < s.size() && d > 0; ++j) {
            if (s[j] == '{') ++d; else if (s[j] == '}') --d;
        }
        if (d == 0) {
            std::string o = s.substr(obj, j - obj);
            auto get_val = [&](const std::string& key) -> std::string {
                const std::string pat = '"' + key + '"';
                size_t p = o.find(pat); if (p == std::string::npos) return {};
                p = o.find(':', p); if (p == std::string::npos) return {};
                size_t q = o.find('"', p); if (q == std::string::npos) return {};
                size_t r = o.find('"', q + 1); if (r == std::string::npos) return {};
                return o.substr(q + 1, r - q - 1);
            };
            std::string w = get_val("word");
            std::string dfn = get_val("definition");
            if (!w.empty()) { entries_[w] = dfn; words_.push_back(w); }
        }
        i = j + 1;
        // break at end of entries array
        size_t close = s.find(']', i);
        if (close != std::string::npos && close < s.find('{', i)) break;
    }

    loaded_ = !entries_.empty();
    return loaded_;
}

bool JsonParserStd::is_loaded() const { return loaded_; }
std::string JsonParserStd::name() const { return name_.empty() ? std::string("JSON Dictionary") : name_; }
std::string JsonParserStd::description() const { return desc_; }
int JsonParserStd::word_count() const { return (int)words_.size(); }

std::string JsonParserStd::lookup(const std::string& word) const {
    auto it = entries_.find(word);
    if (it == entries_.end()) return {};
    return it->second;
}

std::vector<std::string> JsonParserStd::find_similar(const std::string& word, int max_results) const {
    std::vector<std::string> out;
    const std::string lw = lcase(word);
    for (const auto& w : words_) {
        if ((int)out.size() >= max_results) break;
        std::string wl = lcase(w);
        if (wl.rfind(lw, 0) == 0) out.push_back(w);
    }
    return out;
}

std::vector<std::string> JsonParserStd::all_words() const { return words_; }

} // namespace UnidictCoreStd

