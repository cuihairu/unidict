// Cross-reference link handling implementation (std-only).

#include "cross_reference_std.h"
#include <sstream>
#include <algorithm>
#include <regex>
#include <ctime>
#include <fstream>
#include <cstring>

namespace UnidictCoreStd {

// ============================================================================
// CrossReferenceManager Implementation
// ============================================================================

namespace {
    // Protocol patterns
    const char* PROTOCOL_ENTRY = "entry://";
    const char* PROTOCOL_FILE = "file://";
    const char* PROTOCOL_SOUND = "sound://";
    const char* PROTOCOL_BWORD = "bword://";
    const char* PROTOCOL_HTTP = "http://";
    const char* PROTOCOL_HTTPS = "https://";

    // MDict internal link marker
    const char* MDD_LINK_PREFIX = "@@@LINK=";

    // Internal lookup URL format
    const char* LOOKUP_URL_PREFIX = "#lookup:";

    // URL decode helper
    std::string url_decode_impl(const std::string& s) {
        std::string result;
        result.reserve(s.length());

        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] == '%' && i + 2 < s.length()) {
                char hex[3] = { s[i + 1], s[i + 2], '\0' };
                char c = static_cast<char>(std::strtol(hex, nullptr, 16));
                result += c;
                i += 2;
            } else if (s[i] == '+') {
                result += ' ';
            } else {
                result += s[i];
            }
        }

        return result;
    }

    // URL encode helper
    std::string url_encode(const std::string& s) {
        std::ostringstream result;
        result.fill('0');
        result << std::hex;

        for (char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c)) ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                result << c;
            } else {
                result << '%';
                result << std::setw(2) << std::uppercase
                      << static_cast<int>(static_cast<unsigned char>(c));
            }
        }

        return result.str();
    }
}

CrossReferenceManager::CrossReferenceManager() {
}

ParsedLink CrossReferenceManager::parse_link(const std::string& link) const {
    ParsedLink result;
    result.raw_url = link;
    result.is_valid = false;

    if (link.empty()) {
        return result;
    }

    // Detect link type
    std::string lower = link;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check MDict internal link
    if (link.find(MDD_LINK_PREFIX) == 0) {
        result.type = LinkType::INTERNAL;
        result.target_word = link.substr(strlen(MDD_LINK_PREFIX));
        result.is_valid = !result.target_word.empty();
        return result;
    }

    // Check entry:// protocol
    if (lower.find(PROTOCOL_ENTRY) == 0) {
        result.type = LinkType::ENTRY;
        std::string rest = link.substr(strlen(PROTOCOL_ENTRY));
        result.target_word = rest;

        // Check for dictionary_id parameter
        size_t pipe_pos = rest.find('|');
        if (pipe_pos != std::string::npos) {
            result.target_word = rest.substr(0, pipe_pos);
            result.target_dictionary_id = rest.substr(pipe_pos + 1);
        }

        result.is_valid = !result.target_word.empty();
        return result;
    }

    // Check bword:// protocol
    if (lower.find(PROTOCOL_BWORD) == 0) {
        result.type = LinkType::BWORD;
        std::string rest = link.substr(strlen(PROTOCOL_BWORD));

        // Format: bword://word?dict=dict_id
        size_t query_pos = rest.find('?');
        if (query_pos != std::string::npos) {
            result.target_word = rest.substr(0, query_pos);
            std::string query = rest.substr(query_pos + 1);
            result.params = parse_query_params(query);

            auto dict_it = result.params.find("dict");
            if (dict_it != result.params.end()) {
                result.target_dictionary_id = dict_it->second;
            }
        } else {
            result.target_word = rest;
        }

        result.target_word = url_decode(result.target_word);
        result.is_valid = !result.target_word.empty();
        return result;
    }

    // Check sound:// protocol
    if (lower.find(PROTOCOL_SOUND) == 0) {
        result.type = LinkType::SOUND;
        result.target_word = link.substr(strlen(PROTOCOL_SOUND));
        result.is_valid = !result.target_word.empty();
        return result;
    }

    // Check file:// protocol
    if (lower.find(PROTOCOL_FILE) == 0) {
        result.type = LinkType::FILE;
        result.target_word = link.substr(strlen(PROTOCOL_FILE));
        result.is_valid = !result.target_word.empty();
        return result;
    }

    // Check http/https protocols
    if (lower.find(PROTOCOL_HTTP) == 0 || lower.find(PROTOCOL_HTTPS) == 0) {
        result.type = LinkType::HTTP;
        result.target_word = link;
        result.is_valid = true;
        return result;
    }

    // Unknown format - treat as word reference
    result.type = LinkType::UNKNOWN;
    result.target_word = link;
    result.is_valid = !link.empty();

    return result;
}

std::string CrossReferenceManager::format_link(const ParsedLink& link) const {
    switch (link.type) {
        case LinkType::INTERNAL:
            return std::string(MDD_LINK_PREFIX) + link.target_word;

        case LinkType::ENTRY: {
            std::string result = PROTOCOL_ENTRY;
            result += link.target_word;
            if (!link.target_dictionary_id.empty()) {
                result += "|" + link.target_dictionary_id;
            }
            return result;
        }

        case LinkType::BWORD: {
            std::string result = PROTOCOL_BWORD;
            result += url_encode(link.target_word);
            if (!link.target_dictionary_id.empty()) {
                result += "?dict=" + link.target_dictionary_id;
            }
            return result;
        }

        case LinkType::SOUND:
            return std::string(PROTOCOL_SOUND) + link.target_word;

        case LinkType::FILE:
            return std::string(PROTOCOL_FILE) + link.target_word;

        case LinkType::HTTP:
            return link.target_word;

        default:
            return link.raw_url;
    }
}

bool CrossReferenceManager::is_cross_reference(const std::string& url) const {
    auto parsed = parse_link(url);
    return parsed.is_valid &&
           (parsed.type == LinkType::INTERNAL ||
            parsed.type == LinkType::ENTRY ||
            parsed.type == LinkType::BWORD);
}

std::string CrossReferenceManager::resolve_link(const std::string& link,
                                                 const std::string& current_dictionary_id) const {
    auto parsed = parse_link(link);
    return resolve_link(parsed, current_dictionary_id);
}

std::string CrossReferenceManager::resolve_link(const ParsedLink& link,
                                                 const std::string& current_dictionary_id) const {
    if (!link.is_valid) {
        return "";
    }

    switch (link.type) {
        case LinkType::INTERNAL:
            return resolve_internal_link(link.target_word);

        case LinkType::ENTRY:
            return resolve_entry_link(link.target_word,
                link.target_dictionary_id.empty() ? current_dictionary_id : link.target_dictionary_id);

        case LinkType::BWORD:
            return resolve_bword_link(link.target_word);

        case LinkType::SOUND:
        case LinkType::FILE:
        case LinkType::HTTP:
            // These are not cross-references to dictionary entries
            return link.raw_url;

        default:
            return "";
    }
}

std::string CrossReferenceManager::resolve_entry_link(const std::string& target,
                                                      const std::string& current_dict) const {
    std::string target_dict = current_dict;

    // Use custom resolver if available
    if (link_resolver_) {
        std::string resolved = link_resolver_(target, target_dict);
        if (!resolved.empty()) {
            return resolved;
        }
    }

    // Default: return internal lookup format
    return std::string(LOOKUP_URL_PREFIX) + target;
}

std::string CrossReferenceManager::resolve_internal_link(const std::string& target) const {
    // MDict internal links are always to the same dictionary
    if (link_resolver_) {
        std::string resolved = link_resolver_(target, "");
        if (!resolved.empty()) {
            return resolved;
        }
    }

    return std::string(LOOKUP_URL_PREFIX) + target;
}

std::string CrossReferenceManager::resolve_bword_link(const std::string& target) const {
    // bword:// is similar to entry://
    if (link_resolver_) {
        std::string resolved = link_resolver_(target, "");
        if (!resolved.empty()) {
            return resolved;
        }
    }

    return std::string(LOOKUP_URL_PREFIX) + target;
}

void CrossReferenceManager::navigate_to(const std::string& word, const std::string& dictionary_id) {
    HistoryEntry entry;
    entry.word = word;
    entry.dictionary_id = dictionary_id;
    entry.timestamp = static_cast<uint64_t>(std::time(nullptr));
    navigate_to(entry);
}

void CrossReferenceManager::navigate_to(const HistoryEntry& entry) {
    // Add current to back stack if we have a current entry
    if (navigation_.current.timestamp > 0) {
        add_to_back_stack(navigation_.current);
    }

    // Clear forward stack on new navigation
    navigation_.forward_stack.clear();

    // Set new current
    navigation_.current = entry;

    // Trim history if needed
    trim_history();
}

HistoryEntry CrossReferenceManager::go_back() {
    if (!can_go_back()) {
        return navigation_.current;
    }

    // Add current to forward stack
    navigation_.forward_stack.push_front(navigation_.current);

    // Pop from back stack and set as current
    navigation_.current = navigation_.back_stack.front();
    navigation_.back_stack.pop_front();

    return navigation_.current;
}

HistoryEntry CrossReferenceManager::go_forward() {
    if (!can_go_forward()) {
        return navigation_.current;
    }

    // Add current to back stack
    add_to_back_stack(navigation_.current);

    // Pop from forward stack and set as current
    navigation_.current = navigation_.forward_stack.front();
    navigation_.forward_stack.pop_front();

    return navigation_.current;
}

void CrossReferenceManager::clear_history() {
    navigation_.clear();
}

std::vector<HistoryEntry> CrossReferenceManager::get_history() const {
    std::vector<HistoryEntry> result;

    for (const auto& entry : navigation_.back_stack) {
        result.push_back(entry);
    }

    if (navigation_.current.timestamp > 0) {
        result.push_back(navigation_.current);
    }

    for (auto it = navigation_.forward_stack.rbegin(); it != navigation_.forward_stack.rend(); ++it) {
        result.push_back(*it);
    }

    return result;
}

void CrossReferenceManager::add_to_back_stack(const HistoryEntry& entry) {
    navigation_.back_stack.push_front(entry);

    // Trim if too large
    while (navigation_.back_stack.size() > max_history_size_) {
        navigation_.back_stack.pop_back();
    }
}

void CrossReferenceManager::trim_history() {
    // Trim back stack
    while (navigation_.back_stack.size() > max_history_size_) {
        navigation_.back_stack.pop_back();
    }

    // Trim forward stack
    while (navigation_.forward_stack.size() > max_history_size_) {
        navigation_.forward_stack.pop_back();
    }
}

std::string CrossReferenceManager::rewrite_links_in_html(const std::string& html,
                                                         const std::string& dictionary_id) const {
    // Regex to find href attributes
    std::regex href_regex(R"(href\s*=\s*["']([^"']+)["'])");

    std::string result = html;

    std::sregex_iterator it(result.begin(), result.end(), href_regex);
    std::sregex_iterator end;

    // Process matches in reverse to maintain positions
    std::vector<std::tuple<size_t, size_t, std::string>> replacements;

    for (; it != end; ++it) {
        std::string href_value = (*it)[1].str();
        auto parsed = parse_link(href_value);

        if (is_cross_reference(href_value)) {
            std::string resolved = resolve_link(parsed, dictionary_id);

            if (!resolved.empty()) {
                size_t pos = it->position(1);
                size_t len = href_value.length();
                replacements.push_back(std::make_tuple(pos, len, resolved));
            }
        }
    }

    // Apply replacements (in reverse order)
    std::sort(replacements.begin(), replacements.end(),
        [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });

    for (const auto& repl : replacements) {
        result.replace(std::get<0>(repl), std::get<1>(repl), std::get<2>(repl));
    }

    return result;
}

std::string CrossReferenceManager::export_history() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"current\": {\n";
    json << "    \"word\": \"" << navigation_.current.word << "\",\n";
    json << "    \"dictionary_id\": \"" << navigation_.current.dictionary_id << "\",\n";
    json << "    \"timestamp\": " << navigation_.current.timestamp << "\n";
    json << "  },\n";

    json << "  \"back\": [";
    bool first = true;
    for (const auto& entry : navigation_.back_stack) {
        if (!first) json << ", ";
        json << "\n    {\"word\": \"" << entry.word
             << "\", \"dict\": \"" << entry.dictionary_id
             << "\", \"time\": " << entry.timestamp << "}";
        first = false;
    }
    json << "\n  ],\n";

    json << "  \"forward\": [";
    first = true;
    for (const auto& entry : navigation_.forward_stack) {
        if (!first) json << ", ";
        json << "\n    {\"word\": \"" << entry.word
             << "\", \"dict\": \"" << entry.dictionary_id
             << "\", \"time\": " << entry.timestamp << "}";
        first = false;
    }
    json << "\n  ]\n";
    json << "}";

    return json.str();
}

bool CrossReferenceManager::import_history(const std::string& json) {
    // Simple JSON parsing (for production, use a proper JSON library)
    // This is a minimal implementation

    navigation_.clear();

    // Extract word values between quotes after "word":
    std::regex word_regex(R"RRR("word"\s*:\s*"([^"]+)")RRR");
    std::regex dict_regex(R"RRR(("dict"|"dictionary_id")\s*:\s*"([^"]+)")RRR");
    std::regex time_regex(R"RRR(("time"|"timestamp")\s*:\s*(\d+))RRR");

    auto words_begin = std::sregex_iterator(json.begin(), json.end(), word_regex);
    auto words_end = std::sregex_iterator();

    std::vector<std::string> words;
    for (auto it = words_begin; it != words_end; ++it) {
        words.push_back((*it)[1].str());
    }

    if (words.empty()) {
        return false;
    }

    // Set current
    if (!words.empty()) {
        navigation_.current.word = words[0];
        navigation_.current.timestamp = static_cast<uint64_t>(std::time(nullptr));
    }

    return true;
}

std::string CrossReferenceManager::extract_protocol(const std::string& url) {
    size_t pos = url.find("://");
    if (pos != std::string::npos) {
        return url.substr(0, pos);
    }
    return "";
}

std::unordered_map<std::string, std::string> CrossReferenceManager::parse_query_params(
    const std::string& query) {

    std::unordered_map<std::string, std::string> params;

    std::istringstream iss(query);
    std::string pair;

    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            params[url_decode_impl(key)] = url_decode_impl(value);
        } else {
            params[url_decode_impl(pair)] = "";
        }
    }

    return params;
}

std::string CrossReferenceManager::url_decode(const std::string& s) {
    return url_decode_impl(s);
}

// ============================================================================
// HtmlLinkRewriter Implementation
// ============================================================================

std::string HtmlLinkRewriter::rewrite_for_lookup(const std::string& html,
                                                  const std::string& dictionary_id) const {
    return manager_->rewrite_links_in_html(html, dictionary_id);
}

std::vector<ParsedLink> HtmlLinkRewriter::extract_links(const std::string& html) const {
    std::vector<ParsedLink> result;

    // Regex to find href attributes
    std::regex href_regex(R"(href\s*=\s*["']([^"']+)["'])");

    std::sregex_iterator it(html.begin(), html.end(), href_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string href_value = (*it)[1].str();
        ParsedLink parsed = manager_->parse_link(href_value);
        if (parsed.is_valid) {
            result.push_back(parsed);
        }
    }

    return result;
}

std::string HtmlLinkRewriter::rewrite_for_display(const std::string& html) const {
    // Convert internal #lookup: links back to entry:// format
    std::string result = html;

    std::regex lookup_regex(R"(href\s*=\s*["']#lookup:([^"']+)["'])");
    std::sregex_iterator it(result.begin(), result.end(), lookup_regex);
    std::sregex_iterator end;

    std::vector<std::tuple<size_t, size_t, std::string>> replacements;
    for (; it != end; ++it) {
        std::string replacement = "href=\"entry://" + (*it)[1].str() + "\"";
        replacements.push_back(std::make_tuple(it->position(), it->length(), replacement));
    }

    for (auto rit = replacements.rbegin(); rit != replacements.rend(); ++rit) {
        result.replace(std::get<0>(*rit), std::get<1>(*rit), std::get<2>(*rit));
    }

    return result;
}

// ============================================================================
// LinkPatternFactory Implementation
// ============================================================================

std::string LinkPatternFactory::create_internal_link(const std::string& target_word) {
    return std::string(MDD_LINK_PREFIX) + target_word;
}

std::string LinkPatternFactory::create_entry_link(const std::string& target_word,
                                                   const std::string& dictionary_id) {
    std::string result = PROTOCOL_ENTRY;
    result += target_word;
    if (!dictionary_id.empty()) {
        result += "|" + dictionary_id;
    }
    return result;
}

std::string LinkPatternFactory::create_bword_link(const std::string& target_word,
                                                   const std::string& dictionary_id) {
    std::string result = PROTOCOL_BWORD;
    result += url_encode(target_word);
    if (!dictionary_id.empty()) {
        result += "?dict=" + dictionary_id;
    }
    return result;
}

std::string LinkPatternFactory::create_file_link(const std::string& resource_path) {
    return std::string(PROTOCOL_FILE) + resource_path;
}

std::string LinkPatternFactory::create_sound_link(const std::string& audio_path) {
    return std::string(PROTOCOL_SOUND) + audio_path;
}

std::string LinkPatternFactory::create_http_link(const std::string& url) {
    // Ensure protocol is present
    if (url.find("://") == std::string::npos) {
        return std::string(PROTOCOL_HTTP) + url;
    }
    return url;
}

LinkType LinkPatternFactory::detect_link_type(const std::string& url) {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (url.find(MDD_LINK_PREFIX) == 0) {
        return LinkType::INTERNAL;
    }
    if (lower.find(PROTOCOL_ENTRY) == 0) {
        return LinkType::ENTRY;
    }
    if (lower.find(PROTOCOL_BWORD) == 0) {
        return LinkType::BWORD;
    }
    if (lower.find(PROTOCOL_SOUND) == 0) {
        return LinkType::SOUND;
    }
    if (lower.find(PROTOCOL_FILE) == 0) {
        return LinkType::FILE;
    }
    if (lower.find(PROTOCOL_HTTP) == 0 || lower.find(PROTOCOL_HTTPS) == 0) {
        return LinkType::HTTP;
    }

    return LinkType::UNKNOWN;
}

bool LinkPatternFactory::is_valid_link(const std::string& url) {
    if (url.empty()) {
        return false;
    }

    LinkType type = detect_link_type(url);

    // For UNKNOWN type, check if it's a non-empty word
    if (type == LinkType::UNKNOWN) {
        return !url.empty() && url.find_first_not_of(" \t\n\r") != std::string::npos;
    }

    return true;
}

// ============================================================================
// WordVariationManager Implementation
// ============================================================================

void WordVariationManager::add_variations(const std::string& word,
                                         const std::vector<std::string>& variations) {
    std::string lower_word = word;
    std::transform(lower_word.begin(), lower_word.end(), lower_word.begin(), ::tolower);

    variations_[lower_word] = variations;

    // Set canonical form
    canonical_[lower_word] = lower_word;
    for (const auto& v : variations) {
        std::string lower_v = v;
        std::transform(lower_v.begin(), lower_v.end(), lower_v.begin(), ::tolower);
        canonical_[lower_v] = lower_word;
    }
}

std::vector<std::string> WordVariationManager::get_variations(const std::string& word) const {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto it = variations_.find(lower);
    if (it != variations_.end()) {
        return it->second;
    }
    return {};
}

bool WordVariationManager::are_variations(const std::string& a, const std::string& b) const {
    std::string lower_a = a;
    std::transform(lower_a.begin(), lower_a.end(), lower_a.begin(), ::tolower);

    std::string lower_b = b;
    std::transform(lower_b.begin(), lower_b.end(), lower_b.begin(), ::tolower);

    // Check if they share the same canonical form
    auto ca = canonical_.find(lower_a);
    auto cb = canonical_.find(lower_b);

    if (ca != canonical_.end() && cb != canonical_.end()) {
        return ca->second == cb->second;
    }

    return lower_a == lower_b;
}

std::string WordVariationManager::get_canonical_form(const std::string& word) const {
    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    auto it = canonical_.find(lower);
    if (it != canonical_.end()) {
        return it->second;
    }
    return lower;
}

bool WordVariationManager::load_from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Split by comma
        std::vector<std::string> parts;
        std::istringstream iss(line);
        std::string part;

        while (std::getline(iss, part, ',')) {
            // Trim whitespace
            size_t start = part.find_first_not_of(" \t");
            size_t end = part.find_last_not_of(" \t");

            if (start != std::string::npos) {
                parts.push_back(part.substr(start, end - start + 1));
            }
        }

        if (!parts.empty()) {
            std::string word = parts[0];
            std::vector<std::string> variations(parts.begin() + 1, parts.end());
            add_variations(word, variations);
        }
    }

    return true;
}

bool WordVariationManager::save_to_file(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file) {
        return false;
    }

    file << "# Word variations file for Unidict\n";
    file << "# Format: canonical_form,variation1,variation2,...\n\n";

    for (const auto& entry : variations_) {
        file << entry.first;
        for (const auto& v : entry.second) {
            file << "," << v;
        }
        file << "\n";
    }

    return file.good();
}

} // namespace UnidictCoreStd
