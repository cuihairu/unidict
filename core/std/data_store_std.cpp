#include "data_store_std.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

DataStoreStd::DataStoreStd() {
    // default: ./data/unidict.json
    path_ = (fs::current_path() / "data" / "unidict.json").string();
}

void DataStoreStd::set_storage_path(const std::string& file_path) { path_ = file_path; }
std::string DataStoreStd::storage_path() const { return path_; }

void DataStoreStd::ensure_loaded() const {
    if (loaded_) return;
    const_cast<DataStoreStd*>(this)->load();
}

bool DataStoreStd::load() {
    loaded_ = true;
    history_.clear();
    vocab_.clear();

    std::error_code ec;
    fs::path p(path_);
    if (!fs::exists(p, ec)) {
        fs::create_directories(p.parent_path(), ec);
        return save();
    }

    std::ifstream in(path_, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();

    // Minimal tolerant parser for our own JSON format
    auto find_section = [&](const std::string& key) -> std::string {
        const std::string pattern = '"' + key + '"';
        size_t pos = s.find(pattern);
        if (pos == std::string::npos) return {};
        pos = s.find(':', pos);
        if (pos == std::string::npos) return {};
        size_t start = s.find_first_of("[{", pos);
        if (start == std::string::npos) return {};
        int depth = 0;
        for (size_t i = start; i < s.size(); ++i) {
            char c = s[i];
            if (c == '[' || c == '{') ++depth;
            else if (c == ']' || c == '}') { --depth; if (depth == 0) { return s.substr(start, i - start + 1); } }
        }
        return {};
    };

    auto unquote = [](const std::string& t) -> std::string {
        if (t.size() >= 2 && t.front() == '"' && t.back() == '"') return t.substr(1, t.size() - 2);
        return t;
    };

    // Parse history array ["a","b",...]
    std::string hsec = find_section("history");
    if (!hsec.empty() && hsec.front() == '[') {
        std::string cur;
        bool in_str = false, esc = false;
        for (size_t i = 1; i + 1 < hsec.size(); ++i) {
            char c = hsec[i];
            if (!in_str) {
                if (c == '"') { in_str = true; cur.clear(); }
            } else {
                if (esc) { cur.push_back(c); esc = false; }
                else if (c == '\\') esc = true;
                else if (c == '"') { in_str = false; history_.push_back(cur); }
                else cur.push_back(c);
            }
        }
    }

    // Parse vocab array of objects [{"word":"","definition":""},...]
    std::string vsec = find_section("vocab");
    if (!vsec.empty() && vsec.front() == '[') {
        size_t i = 1;
        while (i < vsec.size()) {
            size_t obj = vsec.find('{', i);
            if (obj == std::string::npos) break;
            int depth = 1; size_t j = obj + 1;
            for (; j < vsec.size() && depth > 0; ++j) {
                if (vsec[j] == '{') ++depth; else if (vsec[j] == '}') --depth;
            }
            if (depth == 0) {
                std::string o = vsec.substr(obj, j - obj);
                auto get_val = [&](const std::string& key) -> std::string {
                    const std::string pat = '"' + key + '"';
                    size_t p = o.find(pat); if (p == std::string::npos) return {};
                    p = o.find(':', p); if (p == std::string::npos) return {};
                    size_t q = o.find('"', p); if (q == std::string::npos) return {};
                    size_t r = o.find('"', q + 1); if (r == std::string::npos) return {};
                    return o.substr(q + 1, r - q - 1);
                };
                VocabItemStd vi{ get_val("word"), get_val("definition") };
                if (!vi.word.empty()) vocab_.push_back(std::move(vi));
            }
            i = j + 1;
        }
    }

    return true;
}

bool DataStoreStd::save() const {
    std::error_code ec;
    fs::create_directories(fs::path(path_).parent_path(), ec);
    std::ofstream out(path_, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "{\n";
    out << "  \"history\": [";
    for (size_t i = 0; i < history_.size(); ++i) {
        if (i) out << ",";
        out << '"' << json_escape(history_[i]) << '"';
    }
    out << "],\n";
    out << "  \"vocab\": [\n";
    for (size_t i = 0; i < vocab_.size(); ++i) {
        const auto& v = vocab_[i];
        out << "    {\"word\":\"" << json_escape(v.word) << "\",\"definition\":\"" << json_escape(v.definition) << "\"}";
        if (i + 1 < vocab_.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

void DataStoreStd::add_search_history(const std::string& word) {
    ensure_loaded();
    // dedupe old entries (case-insensitive, ASCII)
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    history_.erase(std::remove_if(history_.begin(), history_.end(), eq), history_.end());
    history_.push_back(word);
    save();
}

std::vector<std::string> DataStoreStd::get_search_history(int limit) const {
    ensure_loaded();
    std::vector<std::string> out;
    if (limit <= 0) return out;
    const int n = (int)history_.size();
    const int start = std::max(0, n - limit);
    for (int i = start; i < n; ++i) out.push_back(history_[i]);
    return out;
}

void DataStoreStd::clear_history() {
    ensure_loaded();
    history_.clear();
    save();
}

void DataStoreStd::add_vocabulary_item(const VocabItemStd& item) {
    ensure_loaded();
    vocab_.push_back(item);
    save();
}

std::vector<VocabItemStd> DataStoreStd::get_vocabulary() const {
    ensure_loaded();
    return vocab_;
}

void DataStoreStd::clear_vocabulary() {
    ensure_loaded();
    vocab_.clear();
    save();
}

bool DataStoreStd::export_vocabulary_csv(const std::string& file_path) const {
    ensure_loaded();
    std::ofstream out(file_path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "word,definition\n";
    auto esc = [](const std::string& s) {
        std::string t; t.reserve(s.size() + 8);
        for (char c : s) { if (c == '"') t.push_back('"'); t.push_back(c); }
        return t;
    };
    for (const auto& v : vocab_) {
        out << '"' << esc(v.word) << '"' << ',' << '"' << esc(v.definition) << '"' << '\n';
    }
    return true;
}

std::string DataStoreStd::json_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back((char)c); break;
        }
    }
    return out;
}

} // namespace UnidictCoreStd
