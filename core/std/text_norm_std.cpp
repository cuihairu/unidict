#include "text_norm_std.h"

#include <algorithm>
#include <cctype>
#include <cstdint>

namespace UnidictCoreStd::TextNorm {

namespace {

// 码点区间折叠表：[lo, hi] 内的码点统一替换为 repl（1–2 个 ASCII 字符）。
// 按 lo 升序排列，std::lower_bound 二分查找。
struct FoldRange {
    uint32_t lo;
    uint32_t hi;
    const char* repl;
};

// 覆盖：Latin-1 Supplement（西欧重音）、Latin Extended-A（欧洲扩展）、
// Latin Extended-B 中的拼音声调字母（ǎǐǒǔǖǘǚǜ）。
const FoldRange kRanges[] = {
    // Latin-1 Supplement：大写重音 -> 小写基础字母
    {0x00C0, 0x00C5, "a"}, {0x00C7, 0x00C7, "c"},
    {0x00C8, 0x00CB, "e"}, {0x00CC, 0x00CF, "i"},
    {0x00D1, 0x00D1, "n"}, {0x00D2, 0x00D6, "o"}, {0x00D8, 0x00D8, "o"},
    {0x00D9, 0x00DC, "u"}, {0x00DD, 0x00DD, "y"}, {0x00DF, 0x00DF, "ss"},
    // Latin-1 Supplement：小写
    {0x00E0, 0x00E5, "a"}, {0x00E7, 0x00E7, "c"},
    {0x00E8, 0x00EB, "e"}, {0x00EC, 0x00EF, "i"},
    {0x00F1, 0x00F1, "n"}, {0x00F2, 0x00F6, "o"}, {0x00F8, 0x00F8, "o"},
    {0x00F9, 0x00FC, "u"}, {0x00FD, 0x00FD, "y"}, {0x00FF, 0x00FF, "y"},
    // Latin Extended-A（段内混排大小写，同段同基础字母）
    {0x0100, 0x0105, "a"}, {0x0106, 0x010D, "c"}, {0x010E, 0x0111, "d"},
    {0x0112, 0x011B, "e"}, {0x011C, 0x0123, "g"}, {0x0124, 0x0127, "h"},
    {0x0128, 0x0131, "i"}, {0x0134, 0x0135, "j"}, {0x0136, 0x0137, "k"},
    {0x0139, 0x0142, "l"}, {0x0143, 0x0148, "n"}, {0x0149, 0x0149, "n"},
    {0x014C, 0x0151, "o"}, {0x0154, 0x0159, "r"}, {0x015A, 0x0161, "s"},
    {0x0162, 0x0167, "t"}, {0x0168, 0x0173, "u"}, {0x0174, 0x0175, "w"},
    {0x0176, 0x0178, "y"}, {0x0179, 0x017E, "z"}, {0x017F, 0x017F, "s"},
    // Latin Extended-B：拼音声调字母（ǎǐǒǔǖǘǚǜ）+ 罗马尼亚/克罗地亚字母
    // 注意：区间必须严格按 lo 升序排列，lower_bound 二分才正确
    {0x01CD, 0x01CE, "a"}, {0x01CF, 0x01D0, "i"},
    {0x01D1, 0x01D2, "o"}, {0x01D3, 0x01DC, "u"},
    {0x0218, 0x0219, "s"}, {0x021A, 0x021B, "t"},
};

// 解码 s[i] 起始的 UTF-8 码点，i 推进到下一码点起点。
// 非法序列返回 false（i 只推进 1 字节，由调用方原样透传）。
bool decode_utf8(const std::string& s, size_t& i, uint32_t& out_cp) {
    const unsigned char c0 = static_cast<unsigned char>(s[i]);
    if (c0 < 0x80) { out_cp = c0; i += 1; return true; }
    int len = 0;
    uint32_t cp = 0;
    if ((c0 & 0xE0) == 0xC0) { len = 2; cp = c0 & 0x1F; }
    else if ((c0 & 0xF0) == 0xE0) { len = 3; cp = c0 & 0x0F; }
    else if ((c0 & 0xF8) == 0xF0) { len = 4; cp = c0 & 0x07; }
    else { i += 1; return false; }
    if (i + len > s.size()) { i += 1; return false; }
    for (int k = 1; k < len; ++k) {
        const unsigned char ck = static_cast<unsigned char>(s[i + k]);
        if ((ck & 0xC0) != 0x80) { i += 1; return false; }
        cp = (cp << 6) | (ck & 0x3F);
    }
    // 拒绝过长编码与代理区
    if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) ||
        (len == 4 && cp < 0x10000) || (cp >= 0xD800 && cp <= 0xDFFF)) {
        i += 1; return false;
    }
    out_cp = cp;
    i += static_cast<size_t>(len);
    return true;
}

// 命中折叠表返回替换串；否则返回 nullptr。
const char* lookup_fold(uint32_t cp) {
    const auto* end = kRanges + std::size(kRanges);
    auto it = std::lower_bound(kRanges, end, cp,
                               [](const FoldRange& r, uint32_t v) { return r.hi < v; });
    if (it != end && cp >= it->lo && cp <= it->hi) return it->repl;
    return nullptr;
}

} // namespace

std::string fold_key(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const size_t start = i;
        uint32_t cp = 0;
        if (!decode_utf8(s, i, cp)) { out.push_back(s[start]); continue; }
        // NFD 组合变音符：丢弃（基础字母已在输出中）
        if (cp >= 0x0300 && cp <= 0x036F) continue;
        // 全角 -> 半角
        if (cp >= 0xFF01 && cp <= 0xFF5E) cp -= 0xFEE0;
        else if (cp == 0x3000) cp = 0x20;
        // 重音/拼音折叠表
        if (const char* repl = lookup_fold(cp)) { out += repl; continue; }
        if (cp < 0x80) {
            out.push_back(static_cast<char>(std::tolower(cp)));
            continue;
        }
        // 无规则命中的码点（中文等）：原样拷贝源字节
        out.append(s, start, i - start);
    }
    // trim 首尾空白
    size_t b = 0, e = out.size();
    auto is_ws = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
    };
    while (b < e && is_ws(out[b])) ++b;
    while (e > b && is_ws(out[e - 1])) --e;
    return out.substr(b, e - b);
}

} // namespace UnidictCoreStd::TextNorm
