#include "epub_parser_std.h"

#include <cctype>
#include <fstream>

#include "zip_reader_std.h"

namespace UnidictCoreStd {

namespace {

std::string to_lower(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string trim(const std::string& text) {
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

// HTML 实体解码：命名最小集 + ASCII 数字实体；未知实体原样保留
std::string decode_entities(const std::string& text) {
    if (text.find('&') == std::string::npos) {
        return text;
    }
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out.push_back(text[i]);
            continue;
        }
        const size_t semicolon = text.find(';', i);
        if (semicolon == std::string::npos || semicolon - i > 10) {
            out.push_back('&');
            continue;
        }
        const std::string entity = text.substr(i + 1, semicolon - i - 1);
        if (entity == "amp") {
            out.push_back('&');
        } else if (entity == "lt") {
            out.push_back('<');
        } else if (entity == "gt") {
            out.push_back('>');
        } else if (entity == "quot") {
            out.push_back('"');
        } else if (entity == "apos") {
            out.push_back('\'');
        } else if (entity == "nbsp") {
            out.push_back(' ');
        } else if (!entity.empty() && entity[0] == '#') {
            int code = 0;
            bool ok = true;
            if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
                for (size_t k = 2; k < entity.size() && ok; ++k) {
                    const char h = entity[k];
                    if (std::isdigit(static_cast<unsigned char>(h))) {
                        code = code * 16 + (h - '0');
                    } else if (h >= 'a' && h <= 'f') {
                        code = code * 16 + (h - 'a' + 10);
                    } else if (h >= 'A' && h <= 'F') {
                        code = code * 16 + (h - 'A' + 10);
                    } else {
                        ok = false;
                    }
                }
            } else {
                for (size_t k = 1; k < entity.size() && ok; ++k) {
                    if (std::isdigit(static_cast<unsigned char>(entity[k]))) {
                        code = code * 10 + (entity[k] - '0');
                    } else {
                        ok = false;
                    }
                }
            }
            if (ok && code > 0 && code < 128) { // 只解 ASCII 码位，多字节不瞎造
                out.push_back(static_cast<char>(code));
            } else {
                out.append(text, i, semicolon - i + 1);
            }
        } else {
            out.append(text, i, semicolon - i + 1);
        }
        i = semicolon;
    }
    return out;
}

// 把 text 追加进 buffer，折叠连续空白为单空格
void append_folded(std::string& buffer, const std::string& text) {
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!buffer.empty() && buffer.back() != ' ') {
                buffer.push_back(' ');
            }
        } else {
            buffer.push_back(c);
        }
    }
}

// 找 tag 里 attr="value" / attr='value' 的值；不存在返回空
std::string extract_attribute(const std::string& element, const std::string& attr) {
    for (const char quote : {'"', '\''}) {
        const std::string needle = attr + "=" + quote;
        size_t pos = element.find(needle);
        while (pos != std::string::npos) {
            // 前一字符必须是边界，避免误匹配 data-href 之类
            if (pos == 0 || std::isspace(static_cast<unsigned char>(element[pos - 1]))) {
                const size_t value_begin = pos + needle.size();
                const size_t value_end = element.find(quote, value_begin);
                if (value_end != std::string::npos) {
                    return element.substr(value_begin, value_end - value_begin);
                }
            }
            pos = element.find(needle, pos + needle.size());
        }
    }
    return {};
}

// 取首个 <tag ...>...</tag> 的文本（剥嵌套标签）
std::string extract_element_text(const std::string& xml, const std::string& tag) {
    const std::string open = "<" + tag;
    size_t pos = xml.find(open);
    while (pos != std::string::npos) {
        const char next = pos + open.size() < xml.size() ? xml[pos + open.size()] : '\0';
        if (next == '>' || std::isspace(static_cast<unsigned char>(next))) {
            const size_t content_begin = xml.find('>', pos);
            const size_t content_end = xml.find("</" + tag + ">", content_begin);
            if (content_begin != std::string::npos && content_end != std::string::npos) {
                std::string text;
                bool in_nested_tag = false;
                for (size_t i = content_begin + 1; i < content_end; ++i) {
                    const char c = xml[i];
                    if (c == '<') {
                        in_nested_tag = true;
                    } else if (c == '>') {
                        in_nested_tag = false;
                    } else if (!in_nested_tag) {
                        text.push_back(c);
                    }
                }
                return trim(decode_entities(text));
            }
        }
        pos = xml.find(open, pos + open.size());
    }
    return {};
}

// <h1>~<h6> 开标签 / 闭标签判定；返回 heading 级别（1-6），非 heading 返回 0。
// open_tags 为真判开标签，否则判闭标签。
int heading_level(const std::string& lower_tag, bool open_tags) {
    for (int level = 1; level <= 6; ++level) {
        const std::string name = "h" + std::to_string(level);
        if (open_tags) {
            // <hN> 或 <hN attr...>
            if (lower_tag.rfind("<" + name, 0) == 0) {
                const size_t name_end = name.size() + 1; // 跳过 '<'
                if (lower_tag.size() == name_end + 1 && lower_tag[name_end] == '>') {
                    return level;
                }
                if (lower_tag.size() > name_end + 1 &&
                    std::isspace(static_cast<unsigned char>(lower_tag[name_end]))) {
                    return level;
                }
            }
        } else if (lower_tag == "</" + name + ">") {
            return level;
        }
    }
    return 0;
}

} // namespace

bool EpubParserStd::load_dictionary(const std::string& path) {
    loaded_ = false;
    name_.clear();
    description_.clear();
    entries_.clear();
    words_.clear();

    ZipReaderStd zip;
    if (!zip.open(path)) {
        return false;
    }

    std::string container_xml;
    if (!zip.read_entry("META-INF/container.xml", container_xml)) {
        return false;
    }
    std::string opf_path;
    if (!parse_container(container_xml, opf_path) || opf_path.empty()) {
        return false;
    }

    std::string opf_xml;
    if (!zip.read_entry(opf_path, opf_xml)) {
        return false;
    }
    // OPF 相对目录：opf 条目路径去掉文件名
    std::string opf_dir;
    const size_t slash = opf_path.rfind('/');
    if (slash != std::string::npos) {
        opf_dir = opf_path.substr(0, slash);
    }
    name_ = extract_element_text(opf_xml, "dc:title");
    description_ = extract_element_text(opf_xml, "dc:description");
    if (name_.empty()) {
        name_ = "EPUB Dictionary";
    }

    for (const std::string& href : collect_opf_documents(opf_xml, opf_dir)) {
        std::string html;
        if (zip.read_entry(href, html)) {
            add_entries_from_html(html);
        }
    }

    // 一个词头都没提出来 → 视为不是词典式 epub，加载失败
    loaded_ = !entries_.empty();
    return loaded_;
}

bool EpubParserStd::parse_container(const std::string& container_xml,
                                    std::string& opf_path) const {
    size_t pos = container_xml.find("<rootfile");
    while (pos != std::string::npos) {
        const size_t element_end = container_xml.find('>', pos);
        if (element_end == std::string::npos) {
            return false;
        }
        const std::string element = container_xml.substr(pos, element_end - pos + 1);
        opf_path = extract_attribute(element, "full-path");
        if (!opf_path.empty()) {
            return true;
        }
        pos = container_xml.find("<rootfile", element_end);
    }
    return false;
}

std::vector<std::string> EpubParserStd::collect_opf_documents(const std::string& opf_xml,
                                                              const std::string& opf_dir) const {
    std::vector<std::string> hrefs;
    size_t pos = opf_xml.find("<item");
    while (pos != std::string::npos) {
        const size_t element_end = opf_xml.find('>', pos);
        if (element_end == std::string::npos) {
            break;
        }
        const std::string element = opf_xml.substr(pos, element_end - pos + 1);
        if (element.find("xhtml") != std::string::npos) {
            std::string href = extract_attribute(element, "href");
            while (href.rfind("./", 0) == 0) {
                href.erase(0, 2);
            }
            if (!href.empty() && href.rfind("/", 0) != 0 && !opf_dir.empty()) {
                href = opf_dir + "/" + href; // zip entry 名不会有前导 /
            }
            if (!href.empty()) {
                hrefs.push_back(href);
            }
        }
        pos = opf_xml.find("<item", element_end);
    }
    return hrefs;
}

void EpubParserStd::add_entries_from_html(const std::string& html) {
    // head 里的 title/style 不参与切词
    std::string body = html;
    const size_t head_begin = body.find("<head");
    if (head_begin != std::string::npos) {
        const size_t head_end = body.find("</head>", head_begin);
        if (head_end != std::string::npos) {
            body.erase(head_begin, head_end + 7 - head_begin);
        }
    }

    // 状态机：<hN> 开 → 词头缓冲；</hN> → 开始收释义；下一个 <hN> / 文档尾 → 入库
    enum class State { BeforeHeading, InHeading, Collecting };
    State state = State::BeforeHeading;
    std::string word;
    std::string definition;

    auto finish_entry = [&] {
        const std::string clean_word = trim(word);
        if (!clean_word.empty()) {
            const std::string clean_definition = trim(definition);
            const std::string key = to_lower(clean_word);
            if (entries_.find(key) == entries_.end()) {
                words_.push_back(clean_word); // 首次出现的原词形
            }
            entries_[key] = {clean_word, clean_definition}; // 同词后写覆盖
        }
        word.clear();
        definition.clear();
    };

    size_t pos = 0;
    while (pos < body.size()) {
        if (body[pos] != '<') {
            const size_t text_end = body.find('<', pos);
            const std::string text = body.substr(pos, text_end - pos);
            if (state == State::InHeading) {
                append_folded(word, decode_entities(text));
            } else if (state == State::Collecting) {
                append_folded(definition, decode_entities(text));
            }
            pos = text_end == std::string::npos ? body.size() : text_end;
            continue;
        }

        const size_t tag_end = body.find('>', pos);
        if (tag_end == std::string::npos) {
            break;
        }
        const std::string tag = body.substr(pos, tag_end - pos + 1);
        const std::string lower_tag = to_lower(tag);
        if (const int level = heading_level(lower_tag, true); level > 0) {
            finish_entry(); // 上一词条收尾（BeforeHeading 时无副作用）
            state = State::InHeading;
        } else if (heading_level(lower_tag, false) > 0) {
            if (state == State::InHeading) {
                state = State::Collecting; // 词头结束，开始收释义
            }
        } else if (state == State::Collecting &&
                   (lower_tag.rfind("<p", 0) == 0 || lower_tag == "</p>" ||
                    lower_tag.rfind("<div", 0) == 0 || lower_tag.rfind("<br", 0) == 0)) {
            // 块级边界当空白折叠（相邻开闭标签各推一个会出双空格）
            append_folded(definition, " ");
        }
        pos = tag_end + 1;
    }
    if (state == State::Collecting) {
        finish_entry(); // 文档尾收尾
    }
}

int EpubParserStd::word_count() const {
    return static_cast<int>(entries_.size());
}

std::string EpubParserStd::lookup(const std::string& word) const {
    const auto it = entries_.find(to_lower(trim(word)));
    if (it == entries_.end()) {
        return {};
    }
    return it->second.second;
}

std::vector<std::string> EpubParserStd::find_similar(const std::string& word,
                                                     int max_results) const {
    std::vector<std::string> results;
    if (max_results <= 0) {
        return results;
    }
    const std::string prefix = to_lower(trim(word));
    for (const std::string& w : words_) {
        if (to_lower(w).rfind(prefix, 0) == 0) {
            results.push_back(w);
            if (static_cast<int>(results.size()) >= max_results) {
                break;
            }
        }
    }
    return results;
}

std::vector<std::string> EpubParserStd::all_words() const {
    return words_;
}

} // namespace UnidictCoreStd
