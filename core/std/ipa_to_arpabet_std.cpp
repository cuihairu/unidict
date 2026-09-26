#include "std/ipa_to_arpabet_std.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "std/espeak_arpabet_std.h"
#include "std/pronunciation_score_std.h"

namespace UnidictCoreStd {
namespace {

// 模式一：空白分词，每段剥掉词尾重音数字后须全是合法 ARPAbet。
// 全部通过才算 ARPAbet 域文本；有一个不合法就整体转 IPA 路径
bool try_arpabet_tokens(const std::string& text,
                        std::vector<std::string>& out) {
    bool any_token = false;
    size_t pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() &&
               std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        if (pos >= text.size()) {
            break;
        }
        const size_t start = pos;
        while (pos < text.size() &&
               !std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        std::string token = text.substr(start, pos - start);
        while (!token.empty() &&
               std::isdigit(static_cast<unsigned char>(token.back()))) {
            token.pop_back();
        }
        for (char& c : token) {
            c = static_cast<char>(
                std::toupper(static_cast<unsigned char>(c)));
        }
        if (token.empty() || !is_valid_phoneme(token)) {
            return false;
        }
        out.push_back(std::move(token));
        any_token = true;
    }
    return any_token;
}

// 单个 UTF-8 码点解码：返回字节长度，cp 写出码点；非法字节序列
// 返回 0（调用方整串拒收——发音字段不该有坏字节）
size_t decode_utf8(const std::string& text, size_t pos,
                   unsigned int& cp) {
    const auto b = [&text](size_t i) {
        return static_cast<unsigned char>(text[i]);
    };
    const unsigned char c0 = b(pos);
    if (c0 < 0x80) {
        cp = c0;
        return 1;
    }
    size_t len = 0;
    unsigned int value = 0;
    if ((c0 & 0xE0) == 0xC0) {
        len = 2;
        value = c0 & 0x1F;
    } else if ((c0 & 0xF0) == 0xE0) {
        len = 3;
        value = c0 & 0x0F;
    } else if ((c0 & 0xF8) == 0xF0) {
        len = 4;
        value = c0 & 0x07;
    } else {
        return 0;
    }
    if (pos + len > text.size()) {
        return 0;
    }
    for (size_t i = 1; i < len; ++i) {
        if ((b(pos + i) & 0xC0) != 0x80) {
            return 0;
        }
        value = (value << 6) | (b(pos + i) & 0x3F);
    }
    // overlong 编码拒收（C0 AF 形式的 '/' 之类不该被当成合法标注
    // 跳过——发音字段里的坏字节就该整串拒收）
    if ((len == 2 && value < 0x80) || (len == 3 && value < 0x800) ||
        (len == 4 && value < 0x10000)) {
        return 0;
    }
    cp = value;
    return len;
}

// 标注符号（不改变音素序列的记号）：解析失败才逐码点走到这里，
// 能匹配音素表的码点永远不会进来（最长前缀匹配优先）
bool is_skippable_mark(unsigned int cp) {
    if (cp < 0x80) {
        switch (cp) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '/':
            case '[':
            case ']':
            case '(':
            case ')':
            case '{':
            case '}':
            case '.':
            case '-':
            case '|':
                return true;
            default:
                return false;
        }
    }
    switch (cp) {
        case 0x02C8:  // ˈ 主重音
        case 0x02CC:  // ˌ 次重音
        case 0x02D0:  // ː 长音符（表键内已随音素吃掉，孤立的跳过）
        case 0x02D1:  // ˑ 半长
        case 0x2016:  // ‖ 韵律切分
            return true;
        default:
            // 组合附加符号区 U+0300-U+036F：去 tie bar 后剩下的杂类
            // 变音（次要发音等）按标注跳过；音节符 l̩ 由表键优先处理
            return cp >= 0x0300 && cp <= 0x036F;
    }
}

// ---- extract_phonetic_text 的本地件 ----

// 去 HTML 标签（MDX 释义是 HTML，音标夹在标签之间；纯文本没有
// 标签原样通过）。只剥标签不解实体——&#x0259; 形式收不到，已知局限
std::string strip_tags(const std::string& html) {
    std::string out;
    out.reserve(html.size());
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') {
            in_tag = true;
            continue;
        }
        if (c == '>') {
            in_tag = false;
            continue;
        }
        if (!in_tag) {
            out.push_back(c);
        }
    }
    return out;
}

// 纯 ASCII（没有 IPA 符号佐证的候选只认 ARPAbet 域，见头文件注释）
bool is_pure_ascii(const std::string& s) {
    for (char c : s) {
        if (static_cast<unsigned char>(c) >= 0x80) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<std::vector<std::string>> phonetic_text_to_arpabet(
    const std::string& text) {
    std::vector<std::string> out;
    if (try_arpabet_tokens(text, out)) {
        return out;
    }

    // 连读 tie bar（t͡ʃ/d͡ʒ 的 U+0361、t͜s 的 U+035C；都是 U+0300 区
    // 组合符，UTF-8 两字节）摘掉后，"tʃ" 自然作为表键命中单音素，
    // 而不是被拆成 T+SH
    std::string ipa = text;
    for (const std::string& tie : {std::string("\xCD\xA1"),
                                   std::string("\xCD\x9C")}) {
        for (size_t p = ipa.find(tie); p != std::string::npos;
             p = ipa.find(tie, p)) {
            ipa.erase(p, tie.size());
        }
    }

    out.clear();
    size_t pos = 0;
    while (pos < ipa.size()) {
        size_t consumed = 0;
        if (auto phones = match_espeak_prefix(ipa, pos, consumed)) {
            for (std::string& p : *phones) {
                out.push_back(std::move(p));
            }
            pos += consumed;
            continue;
        }
        unsigned int cp = 0;
        const size_t len = decode_utf8(ipa, pos, cp);
        if (len == 0 || !is_skippable_mark(cp)) {
            return std::nullopt;  // 未收录音素/坏字节：整串拒收
        }
        pos += len;
    }
    if (out.empty()) {
        return std::nullopt;
    }
    return out;
}

std::optional<std::string> extract_phonetic_text(const std::string& text) {
    const std::string plain = strip_tags(text);
    // 音标按惯例在释义开头：只扫前 256 字节，长释义尾部的斜杠文本
    // （URL 等）根本不进候选
    const size_t limit = std::min(plain.size(), size_t{256});
    for (size_t i = 0; i < limit;) {
        const char open = plain[i];
        if (open != '/' && open != '[') {
            ++i;
            continue;
        }
        const char close = (open == '/') ? '/' : ']';
        const size_t end = plain.find(close, i + 1);
        // 无闭合或字段跨出扫描窗：发音字段应整体落在开头，不再续扫
        // （剩余文本里的分隔符只会拼出更可疑的候选）
        if (end == std::string::npos || end > limit) {
            break;
        }
        const std::string span = plain.substr(i + 1, end - i - 1);
        // 空字段（"//"）跳过；候选内部再出现分隔符说明是括住的正文
        // 而非发音字段；长度上限防整段释义被一个斜杠罩进来
        const bool nested = span.find('/') != std::string::npos ||
                            span.find('[') != std::string::npos ||
                            span.find(']') != std::string::npos;
        if (!span.empty() && span.size() <= 128 && !nested) {
            std::vector<std::string> phones;
            if (is_pure_ascii(span)
                    ? try_arpabet_tokens(span, phones)
                    : phonetic_text_to_arpabet(span).has_value()) {
                return span;
            }
        }
        i = end + 1;
    }
    return std::nullopt;
}

}  // namespace UnidictCoreStd
