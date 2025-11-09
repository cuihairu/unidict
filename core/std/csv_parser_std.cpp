#include "csv_parser_std.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>

namespace UnidictCoreStd {

static inline std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static inline std::string lcase(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

CsvParserStd::CsvParserStd() = default;

bool CsvParserStd::load_dictionary(const std::string& csv_path) {
    entries_.clear();
    words_.clear();
    loaded_ = false;
    name_.clear();
    desc_.clear();

    std::ifstream file(csv_path);
    if (!file) {
        return false;
    }

    // Set name based on filename
    std::filesystem::path p(csv_path);
    name_ = p.stem().string();

    std::string line;
    char separator = '\0';
    int line_count = 0;

    while (std::getline(file, line)) {
        line_count++;
        line = trim(line);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Auto-detect separator on first data line
        if (separator == '\0') {
            separator = detect_separator(line);
            if (separator == '\0') {
                continue; // Skip this line if no separator found
            }
        }

        parse_line(line, separator);
    }

    loaded_ = !entries_.empty();
    if (name_.empty()) {
        name_ = "CSV Dictionary";
    }

    return loaded_;
}

char CsvParserStd::detect_separator(const std::string& line) const {
    // Try tab first (TSV), then comma (CSV)
    if (line.find('\t') != std::string::npos) {
        return '\t';
    } else if (line.find(',') != std::string::npos) {
        return ',';
    } else if (line.find(';') != std::string::npos) {
        return ';';
    } else if (line.find('|') != std::string::npos) {
        return '|';
    }
    return '\0'; // No separator found
}

void CsvParserStd::parse_line(const std::string& line, char separator) {
    size_t pos = line.find(separator);
    if (pos == std::string::npos) {
        return; // No separator found in this line
    }

    std::string word = trim(line.substr(0, pos));
    std::string definition = trim(line.substr(pos + 1));

    // Handle quoted fields (basic CSV quoting)
    if (!word.empty() && word.front() == '"' && word.back() == '"') {
        word = word.substr(1, word.length() - 2);
    }
    if (!definition.empty() && definition.front() == '"' && definition.back() == '"') {
        definition = definition.substr(1, definition.length() - 2);
    }

    if (!word.empty() && !definition.empty()) {
        entries_[word] = definition;
        words_.push_back(word);
    }
}

bool CsvParserStd::is_loaded() const {
    return loaded_;
}

std::string CsvParserStd::dictionary_name() const {
    return name_.empty() ? "CSV Dictionary" : name_;
}

std::string CsvParserStd::dictionary_description() const {
    return desc_ + " (" + std::to_string(entries_.size()) + " entries)";
}

int CsvParserStd::word_count() const {
    return static_cast<int>(words_.size());
}

std::string CsvParserStd::lookup(const std::string& word) const {
    auto it = entries_.find(word);
    if (it != entries_.end()) {
        return it->second;
    }

    // Try case-insensitive search
    std::string lower_word = lcase(word);
    for (const auto& entry : entries_) {
        if (lcase(entry.first) == lower_word) {
            return entry.second;
        }
    }

    return "";
}

std::vector<std::string> CsvParserStd::find_similar(const std::string& word, int max_results) const {
    std::vector<std::string> results;
    std::string lower_word = lcase(word);

    for (const auto& w : words_) {
        if (results.size() >= static_cast<size_t>(max_results)) break;

        std::string lower_w = lcase(w);
        if (lower_w.find(lower_word) == 0) { // starts with
            results.push_back(w);
        }
    }

    return results;
}

std::vector<std::string> CsvParserStd::all_words() const {
    return words_;
}

} // namespace UnidictCoreStd