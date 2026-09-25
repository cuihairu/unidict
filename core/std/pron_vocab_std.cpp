#include "std/pron_vocab_std.h"

#include <cstddef>
#include <cstdint>

namespace UnidictCoreStd {
namespace {

// JSON 游标：只前进、不回头，任何一步不符合文法即失败——词表是
// 模型资产的一部分，格式约定死，宽容解析只会把错位延迟到评分
struct Cursor {
    const std::string& s;
    size_t pos = 0;

    bool eof() const { return pos >= s.size(); }
    char peek() const { return s[pos]; }

    void skip_ws() {
        while (!eof() && (peek() == ' ' || peek() == '\t' || peek() == '\n' ||
                          peek() == '\r')) {
            ++pos;
        }
    }

    bool consume(char c) {
        skip_ws();
        if (!eof() && peek() == c) {
            ++pos;
            return true;
        }
        return false;
    }

    bool expect(char c) { return consume(c); }
};

// \uXXXX → UTF-8（含代理对拼接；词表 IPA 都是 BMP，代理分支只为
// 不给畸形输入留半截字符串）
void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// JSON 字符串字面量（含转义）。进入时游标停在 '"' 上
bool parse_string(Cursor& c, std::string& out) {
    c.skip_ws();
    if (c.eof() || c.peek() != '"') {
        return false;
    }
    ++c.pos;
    out.clear();
    while (!c.eof()) {
        const char ch = c.peek();
        if (ch == '"') {
            ++c.pos;
            return true;
        }
        if (static_cast<unsigned char>(ch) < 0x20) {
            return false;  // 裸控制字符非法
        }
        if (ch != '\\') {
            out.push_back(ch);
            ++c.pos;
            continue;
        }
        ++c.pos;  // 吃掉反斜杠
        if (c.eof()) {
            return false;
        }
        switch (c.peek()) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                ++c.pos;
                uint32_t cp = 0;
                for (int i = 0; i < 4; ++i) {
                    if (c.eof()) return false;
                    const int v = hex_val(c.peek());
                    if (v < 0) return false;
                    cp = (cp << 4) | static_cast<uint32_t>(v);
                    ++c.pos;
                }
                // 代理对：高代理后必须跟 \uDC00-\uDFFF
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    const uint32_t hi = cp;
                    if (c.pos + 1 >= c.s.size() || c.s[c.pos] != '\\' ||
                        c.s[c.pos + 1] != 'u') {
                        return false;
                    }
                    c.pos += 2;
                    uint32_t lo = 0;
                    for (int i = 0; i < 4; ++i) {
                        if (c.eof()) return false;
                        const int v = hex_val(c.peek());
                        if (v < 0) return false;
                        lo = (lo << 4) | static_cast<uint32_t>(v);
                        ++c.pos;
                    }
                    if (lo < 0xDC00 || lo > 0xDFFF) {
                        return false;
                    }
                    cp = 0x10000 + ((hi - 0xD800) << 10) + (lo - 0xDC00);
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    return false;  // 落单低代理
                }
                append_utf8(out, cp);
                continue;  // \u 分支已自行推进
            }
            default:
                return false;
        }
        ++c.pos;  // 单字符转义吃掉转义字符
    }
    return false;  // 未闭合
}

// 非负整数（词表 id 域）；前导零/正负号/小数一律拒收
bool parse_id(Cursor& c, long long& out) {
    c.skip_ws();
    if (c.eof() || c.peek() < '0' || c.peek() > '9') {
        return false;
    }
    if (c.peek() == '0' && c.pos + 1 < c.s.size() &&
        c.s[c.pos + 1] >= '0' && c.s[c.pos + 1] <= '9') {
        return false;  // "01" 非法
    }
    long long v = 0;
    while (!c.eof() && c.peek() >= '0' && c.peek() <= '9') {
        v = v * 10 + (c.peek() - '0');
        if (v > 1000000) {
            return false;  // 词表规模护栏：392 类的模型不该有百万 id
        }
        ++c.pos;
    }
    c.skip_ws();
    // 后面必须跟 , 或 }——"1.5"/"1e3"/"1 x" 在这里现形
    if (c.eof() || (c.peek() != ',' && c.peek() != '}')) {
        return false;
    }
    out = v;
    return true;
}

// CTC blank 符号候选，按生态常见度排序：HF wav2vec2 系是 <pad>，
// k2/sherpa tokens.txt 常用 <blk>，CTC 传统惯用 "|"。第一个命中的胜出
const char* const kBlankCandidates[] = {
    "<pad>", "[PAD]", "<blk>", "<blank>", "<ctc_blank>", "|",
};

}  // namespace

std::optional<PronVocab> parse_vocab_json(const std::string& text) {
    Cursor c{text};
    if (!c.expect('{')) {
        return std::nullopt;
    }

    std::vector<std::string> labels;
    int blank_index = -1;
    auto place = [&](const std::string& sym, long long id) {
        if (sym.empty()) {
            return false;  // 空符号 = 稀疏空洞无法区分，病态词表拒收
        }
        if (static_cast<long long>(labels.size()) <= id) {
            labels.resize(static_cast<size_t>(id) + 1);
        }
        const size_t at = static_cast<size_t>(id);
        if (!labels[at].empty()) {
            return false;  // 重复 id：后写的会静默顶掉先写的，分数全错位
        }
        labels[at] = sym;
        for (const char* cand : kBlankCandidates) {
            if (sym == cand) {
                // 两个 blank 候选同时在场（病态词表）→ 拒收
                if (blank_index >= 0 && blank_index != id) {
                    return false;
                }
                blank_index = static_cast<int>(id);
            }
        }
        return true;
    };

    c.skip_ws();
    if (c.consume('}')) {
        return std::nullopt;  // 空词表无从谈起 blank
    }
    while (true) {
        std::string sym;
        if (!parse_string(c, sym) || !c.expect(':')) {
            return std::nullopt;
        }
        long long id = 0;
        if (!parse_id(c, id)) {
            return std::nullopt;
        }
        // GCOVR_EXCL_LINE：parse_id 只接受数字串，id 恒 >= 0，这道检查
        // 不可触发。留着是为了"解析器只前进不回头"的风格一致——真有人
        // 日后放宽 parse_id（比如接受负 id 偏移），这里会是第一道拦截。
        if (id < 0) {  // GCOVR_EXCL_LINE
            return std::nullopt;  // GCOVR_EXCL_LINE
        }
        if (!place(sym, id)) {
            return std::nullopt;
        }
        if (c.consume(',')) {
            continue;
        }
        if (c.consume('}')) {
            break;
        }
        // GCOVR_EXCL_LINE：parse_id 成功时游标必停在 ',' 或 '}' 上，
        // 所以下面两个 consume 必有一个命中，这里走不到。它是循环终止的
        // 兜底：万一日后 parse_id 放宽了约定，退化行为应是"报错"而不是
        // "游标不前进的死循环"。
        return std::nullopt;  // GCOVR_EXCL_LINE
    }
    c.skip_ws();
    if (!c.eof()) {
        return std::nullopt;  // 尾部垃圾：拼接损坏的词表文件长这样
    }

    PronVocab vocab;
    vocab.labels = std::move(labels);
    vocab.blank_index = blank_index;
    if (!vocab.valid()) {
        return std::nullopt;
    }
    return vocab;
}

}  // namespace UnidictCoreStd
