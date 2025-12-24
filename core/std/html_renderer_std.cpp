// HTML rendering engine implementation (std-only).

#include "html_renderer_std.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <fstream>
#include <cstring>

namespace UnidictCoreStd {

// ============================================================================
// HtmlRendererStd Implementation
// ============================================================================

namespace {
    // Default safe HTML tags for dictionary rendering
    const std::unordered_set<std::string> DEFAULT_ALLOWED_TAGS = {
        // Text structure
        "div", "span", "p", "br", "hr",
        // Headings
        "h1", "h2", "h3", "h4", "h5", "h6",
        // Lists
        "ul", "ol", "li", "dl", "dt", "dd",
        // Tables
        "table", "thead", "tbody", "tr", "th", "td",
        // Text formatting
        "b", "i", "u", "s", "strong", "em", "mark", "small", "sub", "sup",
        // Semantic
        "code", "pre", "blockquote", "abbr", "acronym",
        // Media
        "img", "audio", "video", "source",
        // Links
        "a"
    };

    // Default safe attributes
    const std::unordered_set<std::string> DEFAULT_ALLOWED_ATTRIBUTES = {
        "id", "class", "style",
        "href", "target", "title", "alt", "src",
        "colspan", "rowspan", "align", "valign",
        "data-dict", "data-word", "lang", "dir",
        "width", "height", "controls", "preload"
    };

    // Default safe CSS properties
    const std::unordered_set<std::string> DEFAULT_ALLOWED_CSS = {
        // Text & Fonts
        "color", "background-color", "font-family", "font-size", "font-weight",
        "font-style", "text-decoration", "text-align", "text-indent", "line-height",
        // Spacing
        "margin", "margin-top", "margin-right", "margin-bottom", "margin-left",
        "padding", "padding-top", "padding-right", "padding-bottom", "padding-left",
        // Layout
        "width", "height", "max-width", "max-height", "display", "float", "clear",
        "position", "top", "right", "bottom", "left", "overflow",
        // Borders
        "border", "border-top", "border-right", "border-bottom", "border-left",
        "border-color", "border-style", "border-width", "border-radius",
        // Other
        "opacity", "visibility"
    };

    // Allowed URL protocols
    const std::unordered_set<std::string> ALLOWED_PROTOCOLS = {
        "http", "https", "entry",  // Cross-reference links
        "data",  // Embedded data (carefully validated)
        "file"   // Local files (carefully validated)
    };

    // HTML entity mapping
    const std::unordered_map<std::string, std::string> HTML_ENTITIES = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"},
        {"&quot;", "\""}, {"&apos;", "'"}, {"&nbsp;", " "},
        {"&copy;", "\u00A9"}, {"&reg;", "\u00AE"}, {"&trade;", "\u2122"},
        {"&euro;", "\u20AC"}, {"&pound;", "\u00A3"}, {"&yen;", "\u00A5"},
        {"&cent;", "\u00A2"}, {"&ldquo;", "\u201C"}, {"&rdquo;", "\u201D"},
        {"&lsquo;", "\u2018"}, {"&rsquo;", "\u2019"}, {"&hellip;", "\u2026"},
        {"&mdash;", "\u2014"}, {"&ndash;", "\u2013"}
    };

    // Common CSS values that could be dangerous
    const std::unordered_set<std::string> DANGEROUS_CSS_VALUES = {
        "expression(", "javascript:", "vbscript:", "data:text/html",
        "-o-link", "-moz-binding"
    };
}

HtmlRendererStd::HtmlRendererStd()
    : allowed_tags_(DEFAULT_ALLOWED_TAGS),
      allowed_attributes_(DEFAULT_ALLOWED_ATTRIBUTES),
      allowed_css_properties_(DEFAULT_ALLOWED_CSS),
      allowed_protocols_(ALLOWED_PROTOCOLS) {
}

HtmlRendererStd::HtmlRendererStd(std::shared_ptr<ResourceResolverStd> resource_resolver)
    : HtmlRendererStd() {
    resource_resolver_ = std::move(resource_resolver);
}

RenderedHtml HtmlRendererStd::render(const std::string& html, const HtmlRenderOptions& options) const {
    RenderedHtml result;

    // Store options for use in internal methods
    // In a real implementation, we'd store this as a member variable or pass through

    // Tokenize the HTML
    auto tokens = tokenize(html);

    // Sanitize tokens
    std::vector<HtmlToken> sanitized_tokens;
    sanitized_tokens.reserve(tokens.size());

    for (const auto& token : tokens) {
        HtmlToken mutable_token = token;
        if (sanitize_token(mutable_token)) {
            sanitized_tokens.push_back(std::move(mutable_token));
        }
    }

    // Build HTML string
    std::ostringstream html_output;
    std::ostringstream text_output;

    for (const auto& token : sanitized_tokens) {
        switch (token.type) {
            case HtmlToken::TEXT:
                html_output << encode_html_entities(token.value);
                text_output << token.value;
                break;

            case HtmlToken::ELEMENT_START: {
                html_output << "<" << token.value;
                for (const auto& attr : token.attributes) {
                    html_output << " " << attr.first << "=\"" << attr.second << "\"";
                }
                html_output << ">";
                break;
            }

            case HtmlToken::ELEMENT_END:
                html_output << "</" << token.value << ">";
                break;

            case HtmlToken::SELF_CLOSING: {
                html_output << "<" << token.value;
                for (const auto& attr : token.attributes) {
                    html_output << " " << attr.first << "=\"" << attr.second << "\"";
                }
                html_output << " />";

                // Detect media types
                if (token.value == "img") result.has_images = true;
                if (token.value == "audio" || token.value == "video") result.has_audio = true;
                break;
            }

            case HtmlToken::COMMENT:
                // Skip comments in output
                break;
        }
    }

    result.html = html_output.str();
    result.text = text_output.str();

    // Extract linked words
    if (options.resolve_links) {
        std::regex link_regex(R"(href\s*=\s*["']entry://([^"']+)["'])");
        std::sregex_iterator it(result.html.begin(), result.html.end(), link_regex);
        std::sregex_iterator end;

        for (; it != end; ++it) {
            result.linked_words.push_back((*it)[1].str());
        }
    }

    // Rewrite resource URLs if resolver is available
    if (resource_resolver_ && options.resolve_links) {
        result.html = rewrite_resource_urls(result.html, {});
    }

    return result;
}

std::string HtmlRendererStd::sanitize(const std::string& html) const {
    return render(html).html;
}

std::string HtmlRendererStd::strip_tags(const std::string& html) const {
    auto tokens = tokenize(html);
    std::ostringstream result;

    for (const auto& token : tokens) {
        if (token.type == HtmlToken::TEXT) {
            result << token.value;
        }
    }

    return result.str();
}

std::string HtmlRendererStd::extract_text(const std::string& html) const {
    return render(html).text;
}

std::string HtmlRendererStd::rewrite_links(const std::string& html, const std::string& dictionary_id) const {
    std::string result = html;

    // Handle MDict-style @@@LINK= references (typically outside HTML)
    {
        std::regex link_regex(R"(@@@LINK=(\S+))");
        std::sregex_iterator it(result.begin(), result.end(), link_regex);
        std::sregex_iterator end;

        // Collect replacements
        std::vector<std::tuple<size_t, size_t, std::string>> replacements;
        for (; it != end; ++it) {
            std::string target = (*it)[1].str();
            std::string replacement = "<a href=\"entry://" + target + "\" data-dict=\"" + dictionary_id + "\">" + target + "</a>";
            replacements.push_back(std::make_tuple(it->position(), it->length(), replacement));
        }

        // Apply in reverse order
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
            result.replace(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
        }
    }

    // Rewrite entry:// links
    {
        std::regex entry_regex(R"(href\s*=\s*["']entry://([^"']+)["'])");
        std::sregex_iterator it(result.begin(), result.end(), entry_regex);
        std::sregex_iterator end;

        std::vector<std::tuple<size_t, size_t, std::string>> replacements;
        for (; it != end; ++it) {
            std::string target = (*it)[1].str();
            std::string replacement;
            if (custom_link_resolver_) {
                replacement = "href=\"" + custom_link_resolver_(target, "") + "\"";
            } else {
                replacement = "href=\"#lookup:" + target + "\"";
            }
            replacements.push_back(std::make_tuple(it->position(), it->length(), replacement));
        }

        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
            result.replace(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
        }
    }

    return result;
}

std::string HtmlRendererStd::resolve_cross_reference(const std::string& link, const std::string& dictionary_id) const {
    if (!is_cross_reference_link(link)) {
        return link;
    }

    std::string target = extract_link_target(link);

    if (custom_link_resolver_) {
        return custom_link_resolver_(target, dictionary_id);
    }

    // Default: return internal lookup format
    return "#lookup:" + target;
}

std::string HtmlRendererStd::rewrite_resource_urls(const std::string& html,
                                                   const std::unordered_map<std::string, std::string>& url_map) const {
    std::string result = html;

    // Rewrite src attributes
    std::regex src_regex(R"(src\s*=\s*["']([^"']+)["'])");
    std::sregex_iterator it(result.begin(), result.end(), src_regex);
    std::sregex_iterator end;

    std::vector<std::tuple<size_t, size_t, std::string>> replacements;
    for (; it != end; ++it) {
        std::string url = (*it)[1].str();
        std::string replacement;

        // Check URL map first
        auto map_it = url_map.find(url);
        if (map_it != url_map.end()) {
            replacement = "src=\"" + map_it->second + "\"";
        } else if (resource_resolver_ && resource_resolver_->is_dictionary_resource(url)) {
            std::string key = resource_resolver_->extract_resource_key(url);
            auto info = resource_resolver_->resolve(url, "");

            if (!info.local_path.empty()) {
                replacement = "src=\"file://" + info.local_path + "\"";
            } else {
                std::string data_url = resource_resolver_->get_data_url(url, "");
                if (!data_url.empty()) {
                    replacement = "src=\"" + data_url + "\"";
                } else {
                    replacement = "src=\"" + url + "\"";
                }
            }
        } else {
            replacement = "src=\"" + url + "\"";
        }

        replacements.push_back(std::make_tuple(it->position(), it->length(), replacement));
    }

    // Apply in reverse order
    for (auto rit = replacements.rbegin(); rit != replacements.rend(); ++rit) {
        result.replace(std::get<0>(*rit), std::get<1>(*rit), std::get<2>(*rit));
    }

    return result;
}

std::vector<HtmlToken> HtmlRendererStd::tokenize(const std::string& html) const {
    std::vector<HtmlToken> tokens;
    size_t pos = 0;
    const size_t len = html.length();

    while (pos < len) {
        // Skip whitespace
        while (pos < len && std::isspace(static_cast<unsigned char>(html[pos]))) {
            ++pos;
        }
        if (pos >= len) break;

        // Text content
        if (html[pos] != '<') {
            size_t start = pos;
            while (pos < len && html[pos] != '<') {
                ++pos;
            }
            std::string text = html.substr(start, pos - start);
            text = decode_html_entities(text);

            if (!text.empty()) {
                HtmlToken token;
                token.type = HtmlToken::TEXT;
                token.value = text;
                token.position = start;
                tokens.push_back(std::move(token));
            }
            continue;
        }

        // Comment
        if (pos + 4 < len && html[pos + 1] == '!' && html[pos + 2] == '-' && html[pos + 3] == '-') {
            size_t end = html.find("-->", pos + 4);
            if (end == std::string::npos) break;

            HtmlToken token;
            token.type = HtmlToken::COMMENT;
            token.value = html.substr(pos, end + 3 - pos);
            token.position = pos;
            tokens.push_back(std::move(token));

            pos = end + 3;
            continue;
        }

        // Tag
        if (pos + 1 < len) {
            bool is_closing = (html[pos + 1] == '/');
            size_t tag_start = is_closing ? pos + 2 : pos + 1;
            size_t tag_end = html.find_first_of(" \t\n\r/>", tag_start);

            if (tag_end == std::string::npos) break;

            std::string tag_name = html.substr(tag_start, tag_end - tag_start);
            std::transform(tag_name.begin(), tag_name.end(), tag_name.begin(), ::tolower);

            size_t close_pos = html.find('>', tag_end);
            if (close_pos == std::string::npos) break;

            // Parse attributes
            std::unordered_map<std::string, std::string> attributes;
            size_t attr_pos = tag_end;

            while (attr_pos < close_pos) {
                // Skip whitespace
                while (attr_pos < close_pos && std::isspace(static_cast<unsigned char>(html[attr_pos]))) {
                    ++attr_pos;
                }
                if (attr_pos >= close_pos) break;

                // Find attribute name
                size_t name_end = html.find_first_of(" \t\n\r=>", attr_pos);
                if (name_end == std::string::npos || name_end >= close_pos) break;

                std::string attr_name = html.substr(attr_pos, name_end - attr_pos);
                std::transform(attr_name.begin(), attr_name.end(), attr_name.begin(), ::tolower);

                // Skip to value
                attr_pos = name_end;
                while (attr_pos < close_pos && std::isspace(static_cast<unsigned char>(html[attr_pos]))) {
                    ++attr_pos;
                }

                std::string attr_value;
                if (attr_pos < close_pos && html[attr_pos] == '=') {
                    ++attr_pos;
                    while (attr_pos < close_pos && std::isspace(static_cast<unsigned char>(html[attr_pos]))) {
                        ++attr_pos;
                    }

                    if (attr_pos < close_pos && (html[attr_pos] == '"' || html[attr_pos] == '\'')) {
                        char quote = html[attr_pos];
                        ++attr_pos;
                        size_t value_end = html.find(quote, attr_pos);
                        if (value_end != std::string::npos && value_end < close_pos) {
                            attr_value = html.substr(attr_pos, value_end - attr_pos);
                            attr_pos = value_end + 1;
                        }
                    } else {
                        // Unquoted value (read until whitespace or >)
                        size_t value_end = html.find_first_of(" \t\n\r>", attr_pos);
                        if (value_end != std::string::npos && value_end <= close_pos) {
                            attr_value = html.substr(attr_pos, value_end - attr_pos);
                            attr_pos = value_end;
                        }
                    }
                }

                if (!attr_name.empty()) {
                    attributes[attr_name] = attr_value;
                }
            }

            // Determine token type
            HtmlToken token;
            token.position = pos;
            token.value = tag_name;
            token.attributes = std::move(attributes);

            bool is_self_closing = (close_pos > 0 && html[close_pos - 1] == '/');

            if (is_closing) {
                token.type = HtmlToken::ELEMENT_END;
            } else if (is_self_closing) {
                token.type = HtmlToken::SELF_CLOSING;
            } else {
                token.type = HtmlToken::ELEMENT_START;
            }

            tokens.push_back(std::move(token));
            pos = close_pos + 1;
        } else {
            ++pos;
        }
    }

    return tokens;
}

bool HtmlRendererStd::sanitize_token(HtmlToken& token) const {
    // Skip text and comments
    if (token.type == HtmlToken::TEXT || token.type == HtmlToken::COMMENT) {
        return true;
    }

    // Check if tag is allowed
    if (!is_tag_allowed(token.value)) {
        return false;
    }

    // Sanitize attributes
    for (auto it = token.attributes.begin(); it != token.attributes.end();) {
        if (!sanitize_attribute(it->first, it->second)) {
            it = token.attributes.erase(it);
        } else {
            ++it;
        }
    }

    // Special handling for links
    if (token.value == "a") {
        auto href_it = token.attributes.find("href");
        if (href_it != token.attributes.end()) {
            if (!is_safe_url(href_it->second)) {
                token.attributes.erase(href_it);
            } else if (is_javascript_url(href_it->second)) {
                // Block javascript: URLs
                token.attributes.erase(href_it);
            }
        }
    }

    // Special handling for images
    if (token.value == "img") {
        auto src_it = token.attributes.find("src");
        if (src_it != token.attributes.end()) {
            if (!is_safe_url(src_it->second)) {
                token.attributes.erase(src_it);
            }
        }
    }

    return true;
}

bool HtmlRendererStd::sanitize_attribute(const std::string& name, std::string& value) const {
    // Check if attribute is allowed
    if (!is_attribute_allowed(name)) {
        return false;
    }

    // Special handling for style attribute
    if (name == "style") {
        return sanitize_css_style(value);
    }

    // Check URL attributes
    if (name == "href" || name == "src") {
        return is_safe_url(value);
    }

    return true;
}

bool HtmlRendererStd::sanitize_css_style(std::string& style) const {
    // Parse CSS and filter properties
    std::vector<std::string> clean_properties;
    std::istringstream iss(style);
    std::string property;

    while (std::getline(iss, property, ';')) {
        // Trim whitespace
        size_t start = property.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) continue;
        size_t end = property.find_last_not_of(" \t\n\r");
        property = property.substr(start, end - start + 1);

        // Split property name and value
        size_t colon_pos = property.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string prop_name = property.substr(0, colon_pos);
        std::string prop_value = property.substr(colon_pos + 1);

        // Trim
        prop_name.erase(prop_name.find_last_not_of(" \t\n\r") + 1);
        prop_value.erase(0, prop_value.find_first_not_of(" \t\n\r"));

        // Check if property is allowed
        std::transform(prop_name.begin(), prop_name.end(), prop_name.begin(), ::tolower);
        if (!is_css_property_allowed(prop_name)) {
            continue;
        }

        // Check for dangerous values
        std::string lower_value = prop_value;
        std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
        bool is_dangerous = false;
        for (const auto& dangerous : DANGEROUS_CSS_VALUES) {
            if (lower_value.find(dangerous) != std::string::npos) {
                is_dangerous = true;
                break;
            }
        }
        if (is_dangerous) continue;

        // Rebuild property
        clean_properties.push_back(prop_name + ":" + prop_value);
    }

    style.clear();
    for (size_t i = 0; i < clean_properties.size(); ++i) {
        if (i > 0) style += ";";
        style += clean_properties[i];
    }

    return !style.empty();
}

std::string HtmlRendererStd::decode_html_entities(const std::string& s) const {
    std::string result = s;
    size_t pos = 0;

    while (pos < result.length()) {
        if (result[pos] == '&') {
            size_t end = result.find(';', pos);
            if (end == std::string::npos) break;

            std::string entity = result.substr(pos, end - pos + 1);
            auto it = HTML_ENTITIES.find(entity);

            if (it != HTML_ENTITIES.end()) {
                result.replace(pos, entity.length(), it->second);
                pos += it->second.length();
            } else {
                pos = end + 1;
            }
        } else {
            ++pos;
        }
    }

    return result;
}

std::string HtmlRendererStd::encode_html_entities(const std::string& s) const {
    std::string result;
    result.reserve(s.length() * 1.2);

    for (char c : s) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c; break;
        }
    }

    return result;
}

bool HtmlRendererStd::is_safe_url(const std::string& url) const {
    if (url.empty()) return false;

    // Check protocol
    std::string lower_url = url;
    std::transform(lower_url.begin(), lower_url.end(), lower_url.begin(), ::tolower);

    // Check for dangerous patterns
    if (lower_url.find("javascript:") != std::string::npos ||
        lower_url.find("vbscript:") != std::string::npos ||
        lower_url.find("data:text/html") != std::string::npos) {
        return false;
    }

    // Check allowed protocols
    size_t colon_pos = lower_url.find(':');
    if (colon_pos != std::string::npos) {
        std::string protocol = lower_url.substr(0, colon_pos);
        if (allowed_protocols_.find(protocol) == allowed_protocols_.end()) {
            return false;
        }
    }

    return true;
}

bool HtmlRendererStd::is_javascript_url(const std::string& url) const {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("javascript:") == 0;
}

std::string HtmlRendererStd::normalize_url(const std::string& url) const {
    // Remove fragments and normalize slashes
    std::string result = url;

    // Remove #fragment
    size_t frag_pos = result.find('#');
    if (frag_pos != std::string::npos) {
        result = result.substr(0, frag_pos);
    }

    // Normalize multiple slashes
    size_t pos = 0;
    while ((pos = result.find("//", pos)) != std::string::npos) {
        // Don't touch :// in protocol
        if (pos == 0 || result[pos - 1] != ':') {
            result.erase(pos, 1);
        } else {
            pos += 2;
        }
    }

    return result;
}

bool HtmlRendererStd::is_cross_reference_link(const std::string& url) const {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower.find("entry://") == 0 || lower.find("@@@link=") != std::string::npos;
}

std::string HtmlRendererStd::extract_link_target(const std::string& url) const {
    if (url.find("entry://") == 0) {
        return url.substr(8);
    }
    if (url.find("@@@LINK=") != std::string::npos) {
        size_t pos = url.find("@@@LINK=");
        return url.substr(pos + 9);
    }
    return url;
}

void HtmlRendererStd::add_allowed_tag(const std::string& tag) {
    std::string lower = tag;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    allowed_tags_.insert(lower);
}

void HtmlRendererStd::remove_allowed_tag(const std::string& tag) {
    std::string lower = tag;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    allowed_tags_.erase(lower);
}

void HtmlRendererStd::add_allowed_attribute(const std::string& attr) {
    std::string lower = attr;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    allowed_attributes_.insert(lower);
}

void HtmlRendererStd::add_allowed_css_property(const std::string& property) {
    std::string lower = property;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    allowed_css_properties_.insert(lower);
}

bool HtmlRendererStd::is_tag_allowed(const std::string& tag) const {
    return allowed_tags_.find(tag) != allowed_tags_.end();
}

bool HtmlRendererStd::is_attribute_allowed(const std::string& attr) const {
    return allowed_attributes_.find(attr) != allowed_attributes_.end();
}

bool HtmlRendererStd::is_css_property_allowed(const std::string& property) const {
    return allowed_css_properties_.find(property) != allowed_css_properties_.end();
}

// ============================================================================
// DefaultResourceResolverStd Implementation
// ============================================================================

DefaultResourceResolverStd::DefaultResourceResolverStd() {
    // Set default cache directory
    const char* home = std::getenv("HOME");
    if (home) {
        cache_dir_ = std::string(home) + "/.cache/unidict/resources";
    } else {
        cache_dir_ = "/tmp/unidict_cache";
    }
}

ResourceResolverStd::ResourceInfo DefaultResourceResolverStd::resolve(
    const std::string& url, const std::string& dictionary_id) {

    ResourceInfo info;

    if (!is_dictionary_resource(url)) {
        return info;
    }

    std::string key = extract_resource_key(url);
    std::string normalized_key = normalize_key(key);

    // Find dictionary resource directory
    auto dict_it = dictionary_resources_.find(dictionary_id);
    if (dict_it == dictionary_resources_.end()) {
        return info;
    }

    // Try to find resource file
    std::string resource_path = find_resource_file(normalized_key, dictionary_id);
    if (!resource_path.empty()) {
        info.local_path = resource_path;
        info.is_cached = true;

        // Get file info
        std::ifstream file(resource_path, std::ios::binary | std::ios::ate);
        if (file) {
            info.size = file.tellg();

            // Simple MIME type detection
            if (key.find(".png") != std::string::npos) {
                info.mime_type = "image/png";
            } else if (key.find(".jpg") != std::string::npos || key.find(".jpeg") != std::string::npos) {
                info.mime_type = "image/jpeg";
            } else if (key.find(".gif") != std::string::npos) {
                info.mime_type = "image/gif";
            } else if (key.find(".svg") != std::string::npos) {
                info.mime_type = "image/svg+xml";
            } else if (key.find(".mp3") != std::string::npos) {
                info.mime_type = "audio/mpeg";
            } else if (key.find(".wav") != std::string::npos) {
                info.mime_type = "audio/wav";
            } else if (key.find(".ogg") != std::string::npos) {
                info.mime_type = "audio/ogg";
            }
        }
    }

    return info;
}

bool DefaultResourceResolverStd::exists(const std::string& url, const std::string& dictionary_id) {
    auto info = resolve(url, dictionary_id);
    return !info.local_path.empty();
}

std::string DefaultResourceResolverStd::get_data_url(const std::string& url, const std::string& dictionary_id) {
    auto info = resolve(url, dictionary_id);

    if (info.local_path.empty() || info.mime_type.empty()) {
        return "";
    }

    // Read file and encode as base64
    std::ifstream file(info.local_path, std::ios::binary);
    if (!file) {
        return "";
    }

    std::vector<uint8_t> buffer(info.size);
    file.read(reinterpret_cast<char*>(buffer.data()), info.size);

    // Simple base64 encoding (for production, use a proper library)
    static const char* b64_table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result = "data:" + info.mime_type + ";base64,";

    for (size_t i = 0; i < buffer.size(); i += 3) {
        uint32_t triple = (buffer[i] << 16) |
                         (i + 1 < buffer.size() ? buffer[i + 1] << 8 : 0) |
                         (i + 2 < buffer.size() ? buffer[i + 2] : 0);

        result += b64_table[(triple >> 18) & 0x3F];
        result += b64_table[(triple >> 12) & 0x3F];
        result += (i + 1 < buffer.size()) ? b64_table[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < buffer.size()) ? b64_table[triple & 0x3F] : '=';
    }

    return result;
}

bool DefaultResourceResolverStd::preload_resources(
    const std::vector<std::string>& urls, const std::string& dictionary_id) {
    // For now, this is a no-op since resources are read directly from disk
    // In the future, this could extract and cache resources from .mdd files
    return true;
}

void DefaultResourceResolverStd::clear_cache(const std::string& dictionary_id) {
    // Clear cached resources for a specific dictionary or all
    if (dictionary_id.empty()) {
        dictionary_resources_.clear();
    } else {
        dictionary_resources_.erase(dictionary_id);
    }
}

std::vector<std::string> DefaultResourceResolverStd::get_cached_resources() const {
    std::vector<std::string> resources;
    for (const auto& entry : dictionary_resources_) {
        resources.push_back(entry.first);
    }
    return resources;
}

bool DefaultResourceResolverStd::is_dictionary_resource(const std::string& url) const {
    // Check for common dictionary resource patterns
    if (url.find("://") != std::string::npos) {
        std::string lower = url;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        // Allow entry:// for cross-references, but not for resources
        if (lower.find("entry://") == 0) {
            return false;
        }
        // data:, http:, https: are external resources
        return true;
    }
    // Relative URLs are assumed to be dictionary resources
    return true;
}

std::string DefaultResourceResolverStd::extract_resource_key(const std::string& url) const {
    // Remove protocol if present
    size_t proto_pos = url.find("://");
    if (proto_pos != std::string::npos) {
        return url.substr(proto_pos + 3);
    }
    return url;
}

void DefaultResourceResolverStd::register_dictionary(
    const std::string& dictionary_id, const std::string& resource_path) {
    dictionary_resources_[dictionary_id] = resource_path;
}

void DefaultResourceResolverStd::unregister_dictionary(const std::string& dictionary_id) {
    dictionary_resources_.erase(dictionary_id);
}

void DefaultResourceResolverStd::set_cache_directory(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
}

std::string DefaultResourceResolverStd::find_resource_file(
    const std::string& key, const std::string& dictionary_id) const {

    auto dict_it = dictionary_resources_.find(dictionary_id);
    if (dict_it == dictionary_resources_.end()) {
        return "";
    }

    const std::string& resource_dir = dict_it->second;

    // Try exact match first
    std::string full_path = resource_dir + "/" + key;
    std::ifstream test(full_path, std::ios::binary);
    if (test) {
        return full_path;
    }

    // Try case-insensitive match
    // (Note: This requires directory listing; for now, just try common variations)
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

    // Try with common extensions
    static const char* image_exts[] = {".png", ".jpg", ".jpeg", ".gif", ".svg"};
    static const char* audio_exts[] = {".mp3", ".wav", ".ogg", ".m4a"};

    std::string base_name = key;
    for (const auto* ext : image_exts) {
        if (base_name.length() > strlen(ext) &&
            base_name.substr(base_name.length() - strlen(ext)) == ext) {
            base_name = base_name.substr(0, base_name.length() - strlen(ext));
            break;
        }
    }

    for (const auto* ext : image_exts) {
        full_path = resource_dir + "/" + base_name + ext;
        test.open(full_path, std::ios::binary);
        if (test) return full_path;
    }

    for (const auto* ext : audio_exts) {
        full_path = resource_dir + "/" + base_name + ext;
        test.open(full_path, std::ios::binary);
        if (test) return full_path;
    }

    return "";
}

std::string DefaultResourceResolverStd::normalize_key(const std::string& key) const {
    std::string result = key;

    // Replace backslashes with forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove leading slashes
    size_t start = result.find_first_not_of("/");
    if (start != std::string::npos) {
        result = result.substr(start);
    }

    // URL decode basic entities
    size_t pos = 0;
    while ((pos = result.find("%20", pos)) != std::string::npos) {
        result.replace(pos, 3, " ");
        pos += 1;
    }

    return result;
}

// ============================================================================
// HtmlRendererFactory Implementation
// ============================================================================

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_with_defaults() {
    auto resolver = std::make_shared<DefaultResourceResolverStd>();
    return std::make_unique<HtmlRendererStd>(resolver);
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_with_resource_resolver(
    std::shared_ptr<ResourceResolverStd> resolver) {
    return std::make_unique<HtmlRendererStd>(resolver);
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_strict() {
    auto renderer = std::make_unique<HtmlRendererStd>();

    // Minimal allowed tags
    renderer->remove_allowed_tag("script");
    renderer->remove_allowed_tag("iframe");
    renderer->remove_allowed_tag("object");
    renderer->remove_allowed_tag("embed");
    renderer->remove_allowed_tag("form");
    renderer->remove_allowed_tag("input");
    renderer->remove_allowed_tag("button");

    return renderer;
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_permissive() {
    auto renderer = std::make_unique<HtmlRendererStd>();

    // Add more permissive tags for trusted dictionaries
    renderer->add_allowed_tag("details");
    renderer->add_allowed_tag("summary");
    renderer->add_allowed_tag("figure");
    renderer->add_allowed_tag("figcaption");

    return renderer;
}

// ============================================================================
// ResourceResolverStd Implementation
// ============================================================================

std::string ResourceResolverStd::get_data_url(const std::string& url, const std::string& dictionary_id) {
    // Default implementation: resolve to local file and encode as data URL
    // Subclasses can override for more efficient implementations
    ResourceInfo info = resolve(url, dictionary_id);
    if (!info.local_path.empty()) {
        // For now, return file:// URL instead of full data URL encoding
        // to avoid memory overhead for large resources
        return "file://" + info.local_path;
    }
    return "";
}

bool ResourceResolverStd::preload_resources(const std::vector<std::string>& urls, const std::string& dictionary_id) {
    // Default implementation: do nothing
    (void)urls;
    (void)dictionary_id;
    return true;
}

void ResourceResolverStd::clear_cache(const std::string& dictionary_id) {
    // Default implementation: do nothing
    (void)dictionary_id;
}

std::vector<std::string> ResourceResolverStd::get_cached_resources() const {
    // Default implementation: return empty list
    return {};
}

bool ResourceResolverStd::is_dictionary_resource(const std::string& url) const {
    // Default implementation: check for common patterns
    return url.find("://") == std::string::npos;  // No protocol = likely local resource
}

std::string ResourceResolverStd::extract_resource_key(const std::string& url) const {
    // Default implementation: return URL as-is
    return url;
}

} // namespace UnidictCoreStd
