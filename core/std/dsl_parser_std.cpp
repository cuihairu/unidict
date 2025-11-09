#include "dsl_parser_std.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

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

DslParserStd::DslParserStd() = default;

bool DslParserStd::load_dictionary(const std::string& dsl_path) {
    entries_.clear();
    words_.clear();
    loaded_ = false;
    name_.clear();
    desc_.clear();

    std::ifstream file(dsl_path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::string line;
    std::string current_headword;
    std::string current_definition;
    bool in_header = true;
    bool in_entry = false;
    int line_count = 0;

    while (std::getline(file, line)) {
        line_count++;

        // Remove BOM if present
        if (line_count == 1 && !line.empty() && (unsigned char)line[0] == 0xEF) {
            if (line.size() >= 3 && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
                line = line.substr(3);
            }
        }

        line = trim(line);

        // Skip empty lines but process accumulated entry if any
        if (line.empty()) {
            if (in_entry && !current_headword.empty() && !current_definition.empty()) {
                parse_entry(current_headword, current_definition);
                current_headword.clear();
                current_definition.clear();
                in_entry = false;
            }
            continue;
        }

        // Header parsing - lines starting with #
        if (line[0] == '#') {
            if (parse_header(line)) {
                in_header = true;
                continue;
            }
        } else {
            in_header = false;
        }

        // Entry parsing
        if (!in_header) {
            // If line doesn't start with whitespace and we're not already expecting a definition, it's a new headword
            if (!line.empty() && !std::isspace(line[0]) && line[0] != '\t') {
                // Check if we just started an entry and this might be the definition
                if (in_entry && current_definition.empty() && !current_headword.empty()) {
                    // This is likely the definition for the current headword
                    current_definition = line;
                } else {
                    // Save previous entry if exists
                    if (in_entry && !current_headword.empty() && !current_definition.empty()) {
                        parse_entry(current_headword, current_definition);
                    }

                    // Start new entry
                    current_headword = extract_headword(line);
                    current_definition.clear();
                    in_entry = true;
                }
            } else if (in_entry) {
                // Continuation of definition (indented line or additional content)
                if (!current_definition.empty()) {
                    current_definition += " ";
                }
                current_definition += trim(line);
            }
        }
    }

    // Handle last entry
    if (in_entry && !current_headword.empty() && !current_definition.empty()) {
        parse_entry(current_headword, current_definition);
    }

    loaded_ = !entries_.empty();
    if (name_.empty()) {
        name_ = "DSL Dictionary";
    }

    return loaded_;
}

bool DslParserStd::parse_header(const std::string& line) {
    if (line.find("#NAME") == 0) {
        size_t pos = line.find_first_of(" \t");
        if (pos != std::string::npos) {
            name_ = trim(line.substr(pos));
            // Remove quotes if present
            if (name_.size() >= 2 && name_[0] == '"' && name_.back() == '"') {
                name_ = name_.substr(1, name_.size() - 2);
            }
        }
        return true;
    } else if (line.find("#INDEX_LANGUAGE") == 0) {
        size_t pos = line.find_first_of(" \t");
        if (pos != std::string::npos) {
            source_lang_ = trim(line.substr(pos));
        }
        return true;
    } else if (line.find("#CONTENTS_LANGUAGE") == 0) {
        size_t pos = line.find_first_of(" \t");
        if (pos != std::string::npos) {
            target_lang_ = trim(line.substr(pos));
        }
        return true;
    } else if (line[0] == '#') {
        // Other header fields, just ignore
        return true;
    }

    return false;
}

void DslParserStd::parse_entry(const std::string& headword, const std::string& definition) {
    if (headword.empty()) return;

    std::string clean_headword = clean_markup(headword);
    std::string clean_definition = clean_markup(definition);

    if (!clean_headword.empty()) {
        entries_[clean_headword] = clean_definition;
        words_.push_back(clean_headword);

        // Also index alternative forms if present (separated by commas)
        size_t pos = 0;
        while ((pos = clean_headword.find(',', pos)) != std::string::npos) {
            std::string alt = trim(clean_headword.substr(0, pos));
            if (!alt.empty() && entries_.find(alt) == entries_.end()) {
                entries_[alt] = clean_definition;
                words_.push_back(alt);
            }
            pos++;
        }
    }
}

std::string DslParserStd::clean_markup(const std::string& text) const {
    std::string result = text;

    // Remove DSL markup tags
    // [b]...[/b] - bold
    // [i]...[/i] - italic
    // [u]...[/u] - underline
    // [c]...[/c] - color
    // [p]...[/p] - paragraph
    // {{...}} - sound files
    // <<...>> - references

    // Simple approach: remove all markup
    std::string cleaned;
    bool in_tag = false;
    bool in_sound = false;
    bool in_ref = false;

    for (size_t i = 0; i < result.size(); ++i) {
        char c = result[i];

        if (c == '[') {
            in_tag = true;
            continue;
        } else if (c == ']' && in_tag) {
            in_tag = false;
            continue;
        } else if (i + 1 < result.size() && c == '{' && result[i+1] == '{') {
            in_sound = true;
            i++; // skip next '{'
            continue;
        } else if (i + 1 < result.size() && c == '}' && result[i+1] == '}' && in_sound) {
            in_sound = false;
            i++; // skip next '}'
            continue;
        } else if (i + 1 < result.size() && c == '<' && result[i+1] == '<') {
            in_ref = true;
            i++; // skip next '<'
            continue;
        } else if (i + 1 < result.size() && c == '>' && result[i+1] == '>' && in_ref) {
            in_ref = false;
            i++; // skip next '>'
            continue;
        }

        if (!in_tag && !in_sound && !in_ref) {
            cleaned += c;
        }
    }

    return trim(cleaned);
}

std::string DslParserStd::extract_headword(const std::string& line) const {
    // Headword is typically the first part of the line before any definition
    // It may contain markup, which we'll clean later
    return line;
}

bool DslParserStd::is_loaded() const {
    return loaded_;
}

std::string DslParserStd::dictionary_name() const {
    return name_.empty() ? "DSL Dictionary" : name_;
}

std::string DslParserStd::dictionary_description() const {
    std::string desc = desc_;
    if (!source_lang_.empty() || !target_lang_.empty()) {
        if (!desc.empty()) desc += " ";
        desc += "(" + source_lang_ + " -> " + target_lang_ + ")";
    }
    return desc;
}

int DslParserStd::word_count() const {
    return static_cast<int>(words_.size());
}

std::string DslParserStd::lookup(const std::string& word) const {
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

std::vector<std::string> DslParserStd::find_similar(const std::string& word, int max_results) const {
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

std::vector<std::string> DslParserStd::all_words() const {
    return words_;
}

} // namespace UnidictCoreStd