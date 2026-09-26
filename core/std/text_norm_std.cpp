#include "text_norm_std.h"

#include "text_norm_case_std.inc"
#include "text_norm_fold_std.inc"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>

namespace UnidictCoreStd::TextNorm {

namespace {

// ---------------------------------------------------------------------------
// 表的定义
// ---------------------------------------------------------------------------
// 全部表都必须**严格按 lo/cp 升序**排列：查找用 std::lower_bound，乱序或
// 有重复不会编译报错、不会崩，只会把某个字母折成另一个字母或者直接吞掉，
// 用户看到的是"词典里明明有这个词却查不到"。
// scripts/check_text_norm_tables.py 逐条核升序/重叠/与 Python str.lower()
// 的一致性；改这些表之后请跑一遍。
// ---------------------------------------------------------------------------

// 单码点替换表（标点/符号 → ASCII 等价），按 cp 升序。
struct PunctMap {
    uint32_t cp;
    const char* repl;
};

// UTF-8 编码一个码点。
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
    // GCOVR_EXCL_START
    // 4 字节（星平面）分支对当前全部调用方都不可达：
    //   - kFold 折出的都是 ASCII 字母；
    //   - 大小写表的区间全在 BMP 内（生成脚本只扫到 U+30000，但 kCaseOffset
    //     与 kCaseOddEven 里落进本函数的都是 BMP 内的成对音系）；
    //   - kPunct 的替换串全是 ASCII。
    // 保留它是为了让这个函数作为 UTF-8 编码器是完整正确的（将来接星平面
    // 的组合记号时不必重写），而不是留一段"理论上够用"的残缺实现。
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    // GCOVR_EXCL_STOP
}

// ---------------------------------------------------------------------------
// 1) 标点/符号 → ASCII 等价
// ---------------------------------------------------------------------------
// 场景：用户从 Word / 网页 / PDF 复制文本。这些地方用的都是排版字符而不是
// ASCII：Word 自动弯引号、破折号、省略号、乘号。一个都没折的后果是
// "don't"（U+2019）查不到词典里的 "don't"（U+0027）。用户不会手动改引号。
// 全角标点（U+FF0C 等）由"全角→半角"那一步处理，不在这里重复列出。
const PunctMap kPunct[] = {
    {0x00A0, " "},
    {0x00A9, "(c)"},
    {0x00AB, "\""},
    {0x00AE, "(r)"},
    {0x00B7, "."},
    {0x00BB, "\""},
    {0x00BC, "1/4"},
    {0x00BD, "1/2"},
    {0x00BE, "3/4"},
    {0x00D7, "x"},
    {0x02D7, "-"},
    {0x2000, " "},
    {0x2001, " "},
    {0x2002, " "},
    {0x2003, " "},
    {0x2004, " "},
    {0x2005, " "},
    {0x2006, " "},
    {0x2007, " "},
    {0x2008, " "},
    {0x2009, " "},
    {0x200A, " "},
    {0x2010, "-"},
    {0x2011, "-"},
    {0x2012, "-"},
    {0x2013, "-"},
    {0x2014, "-"},
    {0x2015, "-"},
    {0x2018, "'"},
    {0x2019, "'"},
    {0x201A, ","},
    {0x201B, "'"},
    {0x201C, "\""},
    {0x201D, "\""},
    {0x201E, "\""},
    {0x201F, "\""},
    {0x2020, "-"},
    {0x2022, "-"},
    {0x2023, "-"},
    {0x2026, "..."},
    {0x2028, " "},
    {0x2029, " "},
    {0x202F, " "},
    {0x2032, "'"},
    {0x2033, "\""},
    {0x2035, "`"},
    {0x2039, "'"},
    {0x203A, "'"},
    {0x2042, "-"},
    {0x2043, "-"},
    {0x2044, "/"},
    {0x205F, " "},
    {0x2122, "(tm)"},
    {0x2212, "-"},
    {0x2215, "/"},
    {0x2217, "*"},
    {0x2219, "*"},
    {0x22C5, "*"},
    {0x22EF, "..."},
};

// ---------------------------------------------------------------------------
// 2) 不可见字符：直接丢弃
// ---------------------------------------------------------------------------
// PDF/网页复制最常见的"看不见的脏东西"。它们出现在词典键里会让匹配永远
// 失败，而界面上完全看不出来（用户根本不知道粘贴的内容里有 BOM）。
const FoldRange kInvisible[] = {
    {0x00AD, 0x00AD, ""},
    {0x034F, 0x034F, ""},
    {0x061C, 0x061C, ""},
    {0x180E, 0x180E, ""},
    {0x200B, 0x200F, ""},
    {0x2060, 0x2064, ""},
    {0xFE00, 0xFE0F, ""},
    {0xFEFF, 0xFEFF, ""},
    {0xE0000, 0xE007F, ""},
    {0xE0100, 0xE01EF, ""},
};

// ---------------------------------------------------------------------------
// 3) 组合记号：丢弃（基础字母已在输出里）
// ---------------------------------------------------------------------------
// 原先只覆盖 U+0300–U+036F（拉丁组合附加符号）。希伯来语尼基德、阿拉伯语
// 哈拉卡、给符号加音标的 U+20D0 段都漏了——带注音的希伯来/阿拉伯词条一个
// 都匹配不上。印度系文字的元音符号（matra）同理：它们是独立码点，不折就
// 折不出正确的键。
const FoldRange kCombining[] = {
    {0x0300, 0x036F, ""},
    {0x0483, 0x0489, ""},
    {0x0591, 0x05BD, ""},
    {0x05BF, 0x05BF, ""},
    {0x05C1, 0x05C2, ""},
    {0x05C4, 0x05C5, ""},
    {0x05C7, 0x05C7, ""},
    {0x0610, 0x061A, ""},
    {0x064B, 0x065F, ""},
    {0x0670, 0x0670, ""},
    {0x06D6, 0x06DC, ""},
    {0x06DF, 0x06E4, ""},
    {0x06E7, 0x06E8, ""},
    {0x06EA, 0x06ED, ""},
    {0x0711, 0x0711, ""},
    {0x0730, 0x074A, ""},
    {0x07A6, 0x07B0, ""},
    {0x07EB, 0x07F3, ""},
    {0x0816, 0x0819, ""},
    {0x081B, 0x0823, ""},
    {0x0825, 0x0827, ""},
    {0x0829, 0x082D, ""},
    {0x0859, 0x085B, ""},
    {0x08D3, 0x0903, ""},
    {0x093A, 0x094F, ""},
    {0x0951, 0x0957, ""},
    {0x0962, 0x0963, ""},
    {0x0981, 0x0983, ""},
    {0x09BC, 0x09BC, ""},
    {0x09BE, 0x09C4, ""},
    {0x09C7, 0x09C8, ""},
    {0x09CB, 0x09CD, ""},
    {0x09D7, 0x09D7, ""},
    {0x09E2, 0x09E3, ""},
    {0x0A01, 0x0A03, ""},
    {0x0A3C, 0x0A3C, ""},
    {0x0A3E, 0x0A42, ""},
    {0x0A47, 0x0A48, ""},
    {0x0A4B, 0x0A4D, ""},
    {0x0A51, 0x0A51, ""},
    {0x0A70, 0x0A71, ""},
    {0x0A75, 0x0A75, ""},
    {0x0A81, 0x0A83, ""},
    {0x0ABC, 0x0ABC, ""},
    {0x0ABE, 0x0AC5, ""},
    {0x0AC7, 0x0AC9, ""},
    {0x0ACB, 0x0ACD, ""},
    {0x0B01, 0x0B03, ""},
    {0x0B3C, 0x0B3C, ""},
    {0x0B3E, 0x0B44, ""},
    {0x0B47, 0x0B48, ""},
    {0x0B4B, 0x0B4D, ""},
    {0x0B56, 0x0B57, ""},
    {0x0B82, 0x0B82, ""},
    {0x0BBE, 0x0BC2, ""},
    {0x0BC6, 0x0BC8, ""},
    {0x0BCA, 0x0BCD, ""},
    {0x0BD7, 0x0BD7, ""},
    {0x0C00, 0x0C03, ""},
    {0x0C3E, 0x0C44, ""},
    {0x0C46, 0x0C48, ""},
    {0x0C4A, 0x0C4D, ""},
    {0x0C55, 0x0C56, ""},
    {0x0C81, 0x0C83, ""},
    {0x0CBC, 0x0CBC, ""},
    {0x0CBE, 0x0CC4, ""},
    {0x0CC6, 0x0CC8, ""},
    {0x0CCA, 0x0CCD, ""},
    {0x0CD5, 0x0CD6, ""},
    {0x0CE2, 0x0CE3, ""},
    {0x0D01, 0x0D03, ""},
    {0x0D3E, 0x0D44, ""},
    {0x0D46, 0x0D48, ""},
    {0x0D4A, 0x0D4D, ""},
    {0x0D57, 0x0D57, ""},
    {0x0D82, 0x0D83, ""},
    {0x0DCA, 0x0DCA, ""},
    {0x0DCF, 0x0DD4, ""},
    {0x0DD6, 0x0DD6, ""},
    {0x0DD8, 0x0DDF, ""},
    {0x0E31, 0x0E31, ""},
    {0x0E34, 0x0E3A, ""},
    {0x0E47, 0x0E4E, ""},
    {0x0EB1, 0x0EB1, ""},
    {0x0EB4, 0x0EB9, ""},
    {0x0EBB, 0x0EBC, ""},
    {0x0EC8, 0x0ECD, ""},
    {0x0F18, 0x0F19, ""},
    {0x0F35, 0x0F35, ""},
    {0x0F37, 0x0F37, ""},
    {0x0F39, 0x0F39, ""},
    {0x0F3E, 0x0F3F, ""},
    {0x0F71, 0x0F84, ""},
    {0x0F86, 0x0F87, ""},
    {0x0F8D, 0x0FBC, ""},
    {0x0FC6, 0x0FC6, ""},
    {0x102D, 0x1030, ""},
    {0x1032, 0x1037, ""},
    {0x1039, 0x103A, ""},
    {0x103D, 0x103E, ""},
    {0x1058, 0x1059, ""},
    {0x105E, 0x1060, ""},
    {0x1071, 0x1074, ""},
    {0x1082, 0x1082, ""},
    {0x1085, 0x1086, ""},
    {0x108D, 0x108D, ""},
    {0x135D, 0x135F, ""},
    {0x1712, 0x1714, ""},
    {0x1732, 0x1734, ""},
    {0x1752, 0x1753, ""},
    {0x1772, 0x1773, ""},
    {0x17B4, 0x17D3, ""},
    {0x17DD, 0x17DD, ""},
    {0x180B, 0x180D, ""},
    {0x1885, 0x1886, ""},
    {0x18A9, 0x18A9, ""},
    {0x1920, 0x192B, ""},
    {0x1930, 0x193B, ""},
    {0x1A17, 0x1A1B, ""},
    {0x1A55, 0x1A5E, ""},
    {0x1A60, 0x1A7C, ""},
    {0x1A7F, 0x1A7F, ""},
    {0x1AB0, 0x1AFF, ""},
    {0x1B00, 0x1B04, ""},
    {0x1B34, 0x1B44, ""},
    {0x1B6B, 0x1B73, ""},
    {0x1B80, 0x1B82, ""},
    {0x1BA1, 0x1BAD, ""},
    {0x1BE6, 0x1BF3, ""},
    {0x1C24, 0x1C37, ""},
    {0x1CD0, 0x1CF3, ""},
    {0x1DC0, 0x1DFF, ""},
    {0x20D0, 0x20F0, ""},
    {0x2CEF, 0x2CF1, ""},
    {0x2D7F, 0x2D7F, ""},
    {0x2DE0, 0x2DFF, ""},
    {0x302A, 0x302F, ""},
    {0x3099, 0x309A, ""},
    {0xA66F, 0xA672, ""},
    {0xA674, 0xA67D, ""},
    {0xA69E, 0xA69F, ""},
    {0xA6F0, 0xA6F1, ""},
    {0xA802, 0xA802, ""},
    {0xA806, 0xA806, ""},
    {0xA80B, 0xA80B, ""},
    {0xA823, 0xA827, ""},
    {0xA880, 0xA881, ""},
    {0xA8B4, 0xA8C4, ""},
    {0xA8E0, 0xA8F1, ""},
    {0xA926, 0xA92D, ""},
    {0xA947, 0xA953, ""},
    {0xA980, 0xA983, ""},
    {0xA9B3, 0xA9C0, ""},
    {0xA9E5, 0xA9E5, ""},
    {0xAA29, 0xAA36, ""},
    {0xAA43, 0xAA43, ""},
    {0xAA4C, 0xAA4D, ""},
    {0xAA7B, 0xAA7D, ""},
    {0xAAB0, 0xAAB0, ""},
    {0xAAB2, 0xAAB4, ""},
    {0xAAB7, 0xAAB8, ""},
    {0xAABE, 0xAABF, ""},
    {0xAAC1, 0xAAC1, ""},
    {0xAAEB, 0xAAEF, ""},
    {0xAAF5, 0xAAF6, ""},
    {0xABE3, 0xABEA, ""},
    {0xABEC, 0xABED, ""},
    {0xFB1E, 0xFB1E, ""},
    {0xFE20, 0xFE2F, ""},
};

// ---------------------------------------------------------------------------
// 4) 连字 → ASCII 字母连写
// ---------------------------------------------------------------------------
// 排版连字在词典键里就是普通字母连写；不折的话 ﬁle 查不到 file。
const FoldRange kLigature[] = {
    {0xFB00, 0xFB00, "ff"},
    {0xFB01, 0xFB01, "fi"},
    {0xFB02, 0xFB02, "fl"},
    {0xFB03, 0xFB03, "ffi"},
    {0xFB04, 0xFB04, "ffl"},
    {0xFB05, 0xFB06, "st"},
};

// ---------------------------------------------------------------------------
// 6/7) 大小写折叠表
// ---------------------------------------------------------------------------
// 区间表由 scripts/gen_text_norm_tables.py 从 Python str.lower()（即 Unicode
// 自己的数据）生成，见 text_norm_case_std.inc。


// decode s[i] 起始的 UTF-8 码点，i 推进到下一码点起点。
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
        i += 1;
        return false;
    }
    out_cp = cp;
    i += static_cast<size_t>(len);
    return true;
}

// 命中区间表返回替换串（可能是空串 = 丢弃）；未命中返回 nullptr。
template <std::size_t N>
const char* lookup_range(const FoldRange (&table)[N], uint32_t cp) {
    const auto* end = table + N;
    auto it = std::lower_bound(
        table, end, cp,
        [](const FoldRange& r, uint32_t v) { return r.hi < v; });
    if (it != end && cp >= it->lo && cp <= it->hi) {
        return it->repl;
    }
    return nullptr;
}

// 命中单码点表返回替换串；未命中返回 nullptr。
template <std::size_t N>
const char* lookup_punct(const PunctMap (&table)[N], uint32_t cp) {
    const auto* end = table + N;
    auto it = std::lower_bound(
        table, end, cp, [](const PunctMap& p, uint32_t v) { return p.cp < v; });
    if (it != end && it->cp == cp) {
        return it->repl;
    }
    return nullptr;
}

// 大小写折叠。返回 true 表示 cp 已被改成小写。
// 两条规则：奇偶交替（偶数 +1）、成对区间（统一 +delta）。
bool fold_case_of(uint32_t& cp) {
    for (const auto& r : kCaseOddEven) {
        // 奇数码点本身就是小写，**不要**在这里 return false：后面那条
        // kCaseOffset 规则仍可能命中它（两表区间不互斥），提前返回会漏折。
        // 走到底返回 false 与原样输出小写码点是等价的。
        if (cp >= r.lo && cp <= r.hi && (cp & 1u) == 0) {  // 偶数 = 大写
            cp += 1;
            return true;
        }
    }
    for (const auto& r : kCaseOffset) {
        if (cp >= r.lo && cp <= r.hi) {
            // delta 是有符号的：少数大写码点排在对应小写**之后**（Ÿ U+0178 的
            // 小写是 y U+0079，偏移 -0x79）。走 int32 中间量再转回 uint32，
            // 否则无符号回绕会算出一个天马行行的码点。
            cp = static_cast<uint32_t>(static_cast<int32_t>(cp) + r.delta);
            return true;
        }
    }
    return false;
}

}  // namespace

std::string fold_key(const std::string& s, const Options& opt) {
    std::string out;
    out.reserve(s.size());
    size_t i = 0;
    while (i < s.size()) {
        const size_t start = i;
        uint32_t cp = 0;
        if (!decode_utf8(s, i, cp)) {
            out.push_back(s[start]);  // 非法字节：原样透传，不纠错
            continue;
        }

        // 1) 不可见字符：丢弃（零宽/BOM/软连字符/变体选择符）
        if (opt.strip_invisible) {
            if (const char* rep = lookup_range(kInvisible, cp)) {
                out += rep;  // 表里全是空串，即丢弃
                continue;
            }
        }
        // 2) 组合记号：丢弃（基础字母已在输出里）
        if (opt.strip_combining) {
            if (const char* rep = lookup_range(kCombining, cp)) {
                out += rep;
                continue;
            }
        }
        // 3) 连字展开
        if (opt.fold_ligatures) {
            if (const char* rep = lookup_range(kLigature, cp)) {
                out += rep;
                continue;
            }
        }
        // 4) 全角 → 半角。这一步**不受任何开关控制**（全角标点与全角字母
        //    折成半角是查词的基本要求），所以要记住码点被改过：末尾的兜底
        //    分支是"原样拷贝源字节"，如果不改就会把这一步的结果悄悄丢掉
        //    ——fold_case 关掉时全角 Ａ 会原样留在输出里。
        bool cp_rewritten = false;
        if (cp >= 0xFF01 && cp <= 0xFF5E) {
            cp -= 0xFEE0;
            cp_rewritten = true;
        } else if (cp == 0x3000) {
            cp = 0x20;
            cp_rewritten = true;
        }
        // 5) 标点归一（弯引号、破折号、省略号…）
        if (opt.fold_punctuation) {
            if (const char* rep = lookup_punct(kPunct, cp)) {
                out += rep;
                continue;
            }
        }
        // 6) 重音/声调折叠
        if (opt.fold_diacritics) {
            if (const char* rep = lookup_range(kFold, cp)) {
                out += rep;
                continue;
            }
        }
        // 7) 大小写折叠（含非拉丁）
        if (opt.fold_case) {
            if (cp < 0x80) {
                out.push_back(static_cast<char>(std::tolower(static_cast<int>(cp))));
                continue;
            }
            // 希腊 final sigma ς 与 σ 是同一个字母的词尾/词中两形，
            // 词形不同但查的是同一条词条，键必须相同
            if (cp == 0x03C2) {
                append_utf8(out, 0x03C3);
                continue;
            }
            uint32_t folded = cp;
            if (fold_case_of(folded) && folded != cp && folded > 0 &&
                folded <= 0x10FFFF && !(folded >= 0xD800 && folded <= 0xDFFF)) {
                append_utf8(out, folded);
                continue;
            }
        }
        // 无规则命中的码点（中文等）：原样拷贝源字节。
        // 但全角步改过码点时要按**新码点**重新编码，否则那一步等于没做。
        if (cp_rewritten) {
            append_utf8(out, cp);
        } else {
            out.append(s, start, i - start);
        }
    }

    // 8) trim 首尾空白
    if (opt.trim) {
        auto is_ws = [](char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                   c == '\v' || c == '\f';
        };
        size_t b = 0, e = out.size();
        while (b < e && is_ws(out[b])) ++b;
        while (e > b && is_ws(out[e - 1])) --e;
        out = out.substr(b, e - b);
    }
    return out;
}

std::string fold_key(const std::string& s) {
    return fold_key(s, Options{});
}

}  // namespace UnidictCoreStd::TextNorm
