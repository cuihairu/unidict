#include "data_store_std.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>

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

// ---------- tolerant JSON 辅助（解析本文件自产格式，容错即可） ----------

// 从对象字符串提取字符串字段（无该字段返回空）
static std::string obj_val(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find(':', p);
    if (p == std::string::npos) return {};
    size_t q = o.find('"', p);
    if (q == std::string::npos) return {};
    size_t r = o.find('"', q + 1);
    if (r == std::string::npos) return {};
    return o.substr(q + 1, r - q - 1);
}

// 从对象字符串提取整数字段（无该字段/无数字返回 0）
static long long obj_int(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return 0;
    p = o.find(':', p);
    if (p == std::string::npos) return 0;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    bool neg = false;
    if (p < o.size() && o[p] == '-') { neg = true; ++p; }
    long long v = 0;
    bool any = false;
    while (p < o.size() && o[p] >= '0' && o[p] <= '9') { v = v * 10 + (o[p] - '0'); ++p; any = true; }
    if (!any) return 0;
    return neg ? -v : v;
}

// 从对象字符串提取字符串数组字段（生词标签；旧格式无此字段返回空）
static std::vector<std::string> obj_str_array(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find('[', p);
    if (p == std::string::npos) return {};
    std::vector<std::string> out;
    std::string cur;
    bool in_str = false, esc = false;
    for (size_t k = p + 1; k < o.size(); ++k) {
        char c = o[k];
        if (!in_str) {
            if (c == ']') break;
            if (c == '"') { in_str = true; cur.clear(); }
        } else {
            if (esc) { cur.push_back(c); esc = false; }
            else if (c == '\\') esc = true;
            else if (c == '"') { in_str = false; out.push_back(cur); }
            else cur.push_back(c);
        }
    }
    return out;
}

// 遍历对象数组区段里的每个对象字符串（深度计数定界）
template <typename F>
static void for_each_object(const std::string& sec, F&& fn) {
    size_t i = 1;
    while (i < sec.size()) {
        size_t obj = sec.find('{', i);
        if (obj == std::string::npos) break;
        int depth = 1;
        size_t j = obj + 1;
        for (; j < sec.size() && depth > 0; ++j) {
            if (sec[j] == '{') ++depth;
            else if (sec[j] == '}') --depth;
        }
        if (depth == 0) {
            fn(sec.substr(obj, j - obj));
        }
        i = j + 1;
    }
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
    notes_.clear();

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

    // Parse vocab array of objects [{"word":"","definition":"","added_at":123,...},...]
    std::string vsec = find_section("vocab");
    if (!vsec.empty() && vsec.front() == '[') {
        for_each_object(vsec, [&](const std::string& o) {
            VocabItemStd vi{ obj_val(o, "word"), obj_val(o, "definition"),
                             obj_int(o, "added_at"), obj_str_array(o, "tags") };
            if (!vi.word.empty()) vocab_.push_back(std::move(vi));
        });
    }

    // Parse notes array of objects [{"word":"","text":"","updated_at":123},...]
    std::string nsec = find_section("notes");
    if (!nsec.empty() && nsec.front() == '[') {
        for_each_object(nsec, [&](const std::string& o) {
            NoteItemStd ni{ obj_val(o, "word"), obj_val(o, "text"),
                            obj_int(o, "updated_at") };
            if (!ni.word.empty()) notes_.push_back(std::move(ni));
        });
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
        out << "    {\"word\":\"" << json_escape(v.word) << "\",\"definition\":\"" << json_escape(v.definition) << "\"";
        if (v.added_at > 0) out << ",\"added_at\":" << v.added_at;
        if (!v.tags.empty()) {
            out << ",\"tags\":[";
            for (size_t t = 0; t < v.tags.size(); ++t) {
                if (t) out << ",";
                out << '"' << json_escape(v.tags[t]) << '"';
            }
            out << "]";
        }
        out << "}";
        if (i + 1 < vocab_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"notes\": [\n";
    for (size_t i = 0; i < notes_.size(); ++i) {
        const auto& n = notes_[i];
        out << "    {\"word\":\"" << json_escape(n.word) << "\",\"text\":\"" << json_escape(n.text) << "\"";
        if (n.updated_at > 0) out << ",\"updated_at\":" << n.updated_at;
        out << "}";
        if (i + 1 < notes_.size()) out << ",";
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
    // upsert by word (case-insensitive ASCII)
    auto eq = [&](const std::string& s){
        if (s.size() != item.word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)item.word[i])) return false;
        return true;
    };
    bool updated = false;
    for (auto& v : vocab_) {
        if (eq(v.word)) { v.definition = item.definition; /* keep original added_at */ updated = true; break; }
    }
    if (!updated) {
        VocabItemStd vi = item;
        if (vi.added_at == 0) {
            vi.added_at = (long long)std::time(nullptr);
        }
        vocab_.push_back(std::move(vi));
    }
    save();
}

void DataStoreStd::remove_vocabulary_item(const std::string& word) {
    ensure_loaded();
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    vocab_.erase(std::remove_if(vocab_.begin(), vocab_.end(), [&](const VocabItemStd& v){ return eq(v.word); }), vocab_.end());
    save();
}

std::vector<VocabItemStd> DataStoreStd::get_vocabulary() const {
    ensure_loaded();
    return vocab_;
}

bool DataStoreStd::set_vocabulary_item_tags(const std::string& word,
                                            const std::vector<std::string>& tags) {
    ensure_loaded();
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    for (auto& v : vocab_) {
        if (eq(v.word)) {
            v.tags = tags;
            save();
            return true;
        }
    }
    return false;
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

void DataStoreStd::set_note(const std::string& word, const std::string& text) {
    ensure_loaded();
    auto eq = [&](const std::string& s){
        if (s.size() != word.size()) return false;
        for (size_t i = 0; i < s.size(); ++i) if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)word[i])) return false;
        return true;
    };
    if (text.empty()) { // 空文本=移除该词笔记
        notes_.erase(std::remove_if(notes_.begin(), notes_.end(),
                                    [&](const NoteItemStd& n){ return eq(n.word); }),
                     notes_.end());
        save();
        return;
    }
    for (auto& n : notes_) {
        if (eq(n.word)) {
            n.text = text;
            n.updated_at = (long long)std::time(nullptr);
            save();
            return;
        }
    }
    NoteItemStd ni;
    ni.word = word;
    ni.text = text;
    ni.updated_at = (long long)std::time(nullptr);
    notes_.push_back(std::move(ni));
    save();
}

std::string DataStoreStd::get_note(const std::string& word) const {
    ensure_loaded();
    for (const auto& n : notes_) {
        if (n.word.size() == word.size()) {
            bool same = true;
            for (size_t i = 0; i < n.word.size(); ++i) {
                if (std::tolower((unsigned char)n.word[i]) != std::tolower((unsigned char)word[i])) { same = false; break; }
            }
            if (same) return n.text;
        }
    }
    return {};
}

std::vector<NoteItemStd> DataStoreStd::get_notes() const {
    ensure_loaded();
    return notes_;
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
