// HTML rendering engine implementation (std-only).

#include <cstdint>
#include "html_renderer_std.h"
#include "html_renderer_helpers_std.h"
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <filesystem>
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
        "bword",  // MDict cross-reference (inert until rewritten to unidict)
        "unidict",  // app-internal scheme produced by link rewriting; keeping
                    // it whitelisted makes the pipeline order-insensitive
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

namespace {

// 不切断 UTF-8 序列的截断：max_bytes 落在续字节（0b10xxxxxx）中间时
// 往回退到该码点的起始字节。词典正文全是 UTF-8，切半个码点会让下游
// QString::fromStdString 产生替换字符（U+FFFD），界面上就是"乱码方块"。
size_t utf8_safe_cut(const std::string& s, size_t max_bytes) {
    if (s.size() <= max_bytes) return s.size();
    size_t cut = max_bytes;
    // 最多回退 3 字节：UTF-8 单码点最长 4 字节
    for (int back = 0; back < 4 && cut > 0; ++back) {
        const unsigned char c = (unsigned char)s[cut];
        if ((c & 0xC0) != 0x80) return cut;  // 落在首字节或其 ASCII 上，安全
        --cut;
    }
    return cut;
}

bool is_void_element(const std::string& tag) {
    // HTML void 元素表（不能有结束标签）。注意 audio/video 不是
    // void——它们带 </audio>/</video>，误列会让结束标签错误地递减
    // 外层元素深度（嵌套预算计数漂移）
    static const char* kVoid[] = {"br",  "hr",   "img", "input", "meta",  "link",
                                  "area", "base", "col", "embed", "param", "source",
                                  "track", "wbr"};
    for (const char* v : kVoid) {
        if (tag == v) return true;
    }
    return false;
}

}  // namespace

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

    // 护栏状态（max_text_length_ / max_nesting_depth_，此前从未生效）：
    //  - text 累计到上限后不再收文本，末尾补省略标记
    //  - 嵌套层数超限就丢掉该开标签
    size_t text_bytes = 0;
    size_t depth = 0;
    bool text_capped = false;
    bool depth_capped = false;

    for (const auto& token : sanitized_tokens) {
        // GCOVR_EXCL_LINE：switch 覆盖 Type 全部 5 个枚举值且无 default，
        // 落空跳转边仅在枚举值被内存破坏时可达。
        switch (token.type) {  // GCOVR_EXCL_LINE
            case HtmlToken::TEXT:
                html_output << encode_html_entities(token.value);
                if (!text_capped) {
                    if (text_bytes < max_text_length_) {
                        // 单个 TEXT token 就超限时按 UTF-8 边界切一刀，
                        // 切点之后的内容直接丢（html 侧仍保留完整，
                        // text 侧才是有上限的那个）
                        const size_t room = max_text_length_ - text_bytes;
                        const size_t take = utf8_safe_cut(token.value, room);
                        text_output << token.value.substr(0, take);
                        text_bytes += take;
                        if (take < token.value.size()) text_capped = true;
                    } else {
                        text_capped = true;
                    }
                }
                break;

            case HtmlToken::ELEMENT_START: {
                const bool is_void = is_void_element(token.value);
                if (!is_void && depth >= max_nesting_depth_) {
                    // 丢弃这个开标签（对应的闭标签照常输出也无害：
                    // 浏览器/QTextDocument 会忽略多余的 </>）
                    depth_capped = true;
                    break;
                }
                if (!is_void) ++depth;
                // 媒体探测：原先只在 SELF_CLOSING 分支里做，但真实词条写的是
                // <audio src="..."></audio>（HTML 的 audio/video/img 极少自闭合），
                // 等于这个标志几乎永不置位，UI 拿不到"这条有音频"的信号。
                if (token.value == "img") result.has_images = true;
                if (token.value == "audio" || token.value == "video") result.has_audio = true;
                html_output << "<" << token.value;
                for (const auto& attr : token.attributes) {
                    html_output << " " << attr.first << "=\"" << attr.second << "\"";
                }
                html_output << ">";
                break;
            }

            case HtmlToken::ELEMENT_END:
                if (depth > 0) --depth;
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

    // 截断标记：让调用方能分辨"词条本来就短"和"撞了护栏被砍"。
    // 省略号放在 text 末尾，html 侧不插（html 是完整结构，另行按
    // text 是否有标记判断更省事）。
    if (text_capped) {
        result.text += "...";
        result.truncated = true;
    }
    if (depth_capped) result.truncated = true;

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

namespace {


// 文本段内按转义后的字面 term 做大小写不敏感替换，命中片段用
// <span class="udict-hl">original</span> 包裹（保留原文大小写）。
std::string highlight_run(const std::string& seg, const std::string& escaped_term) {
    if (seg.empty() || escaped_term.empty()) return seg;
    std::regex re(escaped_term, std::regex::icase);
    return std::regex_replace(seg, re, "<span class=\"udict-hl\">$&</span>");
}

}  // namespace

std::string HtmlRendererStd::highlight_term(const std::string& html, const std::string& term) const {
    if (html.empty() || term.empty()) return html;
    // term 按字面匹配，先转义正则元字符
    std::string esc;
    for (const char c : term) {
        if (std::string("\\^$.|?*+()[]{}").find(c) != std::string::npos) esc += '\\';
        esc += c;
    }
    std::string out;
    out.reserve(html.size() + 96);
    size_t pos = 0;
    const size_t n = html.size();
    while (pos < n) {
        const size_t lt = html.find('<', pos);
        if (lt == std::string::npos) {  // 剩余全是文本段
            out += highlight_run(html.substr(pos), esc);
            break;
        }
        const size_t gt = html.find('>', lt + 1);
        if (gt == std::string::npos) {  // 乱刺穿 < 无配套 >：后半整体按文本段
            out += highlight_run(html.substr(pos), esc);
            break;
        }
        out += highlight_run(html.substr(pos, lt - pos), esc);  // 标签前文本段
        out += html.substr(lt, gt - lt + 1);               // 标签（含属性）原样
        pos = gt + 1;
    }
    return out;
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
                replacement = "src=\"" + html_detail::file_url_from_local_path(info.local_path) + "\"";
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

            // GCOVR_EXCL_LINE：text 来自 ≥1 字符的 substr，实体解码只会
            // 把实体换成 ≥1 字符的映射值，不会归零，假臂结构不可达。
            if (!text.empty()) {  // GCOVR_EXCL_LINE
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
                // GCOVR_EXCL_LINE：查找集含 '>'，attr_pos < close_pos 且
                // close_pos 处即 '>'，find_first_of 必命中 ≤ close_pos，
                // npos 臂结构不可达。
                if (name_end == std::string::npos || name_end >= close_pos) break;  // GCOVR_EXCL_LINE

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
                        // GCOVR_EXCL_LINE：查找集含 '>' 且 close_pos 处即
                        // '>'，value_end 必命中且 ≤ close_pos，npos 与
                        // 越界两臂皆结构不可达。
                        size_t value_end = html.find_first_of(" \t\n\r>", attr_pos);
                        if (value_end != std::string::npos && value_end <= close_pos) {  // GCOVR_EXCL_LINE
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

            // GCOVR_EXCL_LINE：close_pos 是 tag_end 之后找到的 '>'，恒
            // 大于 0，左操作数假臂结构不可达。
            bool is_self_closing =
                (close_pos > 0 && html[close_pos - 1] == '/');  // GCOVR_EXCL_LINE

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

    // Special handling for links / images。
    // GCOVR_EXCL_START：这两段当前走不到——上面那个通用属性循环里的
    // sanitize_attribute 对 href/src 已经直接返回 is_safe_url(value)，
    // 不安全的 URL 在那里就被 erase 了。留作纵深防御：万一日后
    // sanitize_attribute 的 URL 校验被放宽，这里仍会拦一道。别当冗余删掉。
    if (token.value == "a") {  // GCOVR_EXCL_LINE
        auto href_it = token.attributes.find("href");  // GCOVR_EXCL_LINE
        if (href_it != token.attributes.end()) {  // GCOVR_EXCL_LINE
            if (!is_safe_url(href_it->second)) {  // GCOVR_EXCL_LINE
                token.attributes.erase(href_it);  // GCOVR_EXCL_LINE
            } else if (is_javascript_url(href_it->second)) {  // GCOVR_EXCL_LINE
                // 兜底拦 javascript: URL。is_safe_url 已经把任何含
                // "javascript:" 的 URL 判为不安全，所以走到这里时
                // is_javascript_url 必为假。
                token.attributes.erase(href_it);  // GCOVR_EXCL_LINE
            }  // GCOVR_EXCL_LINE
        }  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_LINE

    // Special handling for images（同样已被 sanitize_attribute 覆盖）
    if (token.value == "img") {  // GCOVR_EXCL_LINE
        auto src_it = token.attributes.find("src");  // GCOVR_EXCL_LINE
        if (src_it != token.attributes.end()) {  // GCOVR_EXCL_LINE
            if (!is_safe_url(src_it->second)) {  // GCOVR_EXCL_LINE
                token.attributes.erase(src_it);  // GCOVR_EXCL_LINE
            }  // GCOVR_EXCL_LINE
        }  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_STOP

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

// 原先这里有个 normalize_url（去 #fragment、折叠重复斜杠），私有成员、
// 零调用方。URL 归一化在 rewrite_resource_urls / resolve_cross_reference
// 里各有自己的处理，重复实现只会让人误改一处。已删除。

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
        // "@@@LINK=" 共 8 个字符；此前误写 pos + 9，会把目标首字符吞掉
        return url.substr(pos + 8);
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

} // namespace UnidictCoreStd
