#!/usr/bin/env python3
"""生成 tests/text_norm_std_pro_test.cpp。

为什么要生成：测试里全是 UTF-8 字节字面量，手写必错——已经栽过两次：
  1. C++ 的十六进制转义是**贪婪**的，"\\x94b" 会被当成一个越界的数；
  2. 凭记忆手敲 GBK/UTF-8 字节（前面 GBK 表那次把 技/文/词 全敲错了）。
用 Python 里的真实字符串当源，字节和期望值都不可能错。
"""
import io

L = []
p = L.append

p('// 查词键归一化的专业场景回归（docs/pro_dictionary_gap.md P0 ②「字符归一化')
p('// 不足：大小写/重音/全半角/Unicode 兼容折叠、标点归一化对"专业查词"很关键')
p('// （尤其多语种）」）。')
p('//')
p('// 本文件由 scripts/gen_text_norm_test.py 生成：测试里全是 UTF-8 字节字面量，')
p('// 手写必错（C++ 的 \\x 转义贪婪；凭记忆敲 UTF-8/GBK 字节）。用 Python 里的')
p('// 真实字符串当源，字节与期望值都不可能错。改测试请改生成脚本。')
p('//')
p('// 表本身的正确性（升序/不重叠/与 Python str.lower() 逐码点一致）由')
p('// scripts/check_text_norm_tables.py 离线核；这里核的是**端到端行为**。')
p('//')
p('// 每条断言都对应一个真实场景："用户从某个来源复制/输入这个词，词典里有，')
p('// 但查不到"。断言刻意写成**成对的输入 → 同一个键**，因为归一化的契约就是')
p('// "不同写法必须折成同一个键"，只测单边的话折成什么都算过。')
p('')
p('#include <cassert>')
p('#include <cstdint>')
p('#include <iostream>')
p('#include <string>')
p('')
p('#include "std/text_norm_std.h"')
p('')
p('using namespace UnidictCoreStd::TextNorm;')
p('')
p('// C++ 的十六进制转义是贪婪的（"\\x94b" 会被当成一个越界的数），')
p('// 所以多字节 UTF-8 必须每字节一个独立字面量。')
p('static std::string u8(const std::string& s) { return s; }')
p('')
p('// 把一个码点编成 UTF-8，供"逐码点核大小写表"用。')
p('static void enc(std::string& out, uint32_t cp) {')
p('    if (cp <= 0x7F) {')
p('        out.push_back(static_cast<char>(cp));')
p('    } else if (cp <= 0x7FF) {')
p('        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));')
p('        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));')
p('    } else {')
p('        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));')
p('        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));')
p('        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));')
p('    }')
p('}')
p('')
p('int main() {')


def lit(s):
    """Python 串 -> C++ 字面量（每字节一个，避免贪婪转义）。"""
    out = []
    for ch in s:
        b = ch.encode('utf-8')
        if b == b'\\':
            out.append('\\\\')
        elif b == b'"':
            out.append('\\"')
        else:
            out.append(''.join('\\x%02x' % x for x in b))
    # 注意分隔写法：必须是 空格+两个引号（'" "'），不能是 两个引号+空格
    # （'"" '）。后者会被 C++ 词法分析成"上一个字面量结束、紧接一个新字面量
    # 开始"，那个空格就落进了**新字面量内部**，于是每个多字节串都带一个
    # 莫名的 0x20。
    return '"' + '" "'.join(out) + '"' if out else '""' 


def sect(title):
    p('    // =========================================================================')
    p('    // %s' % title)
    p('    // =========================================================================')


def eq(expr_a, expr_b, note=''):
    if note:
        p('        // %s' % note)
    p('        assert(%s == %s);' % (expr_a, expr_b))


# ---- 1) 大小写：非拉丁文字 ----
sect('1) 大小写：非拉丁文字此前完全没折，俄语/希腊语词典是大小写敏感的')
eq('fold_key(u8(%s))' % lit('Москва'), 'fold_key(u8(%s))' % lit('москва'),
   'Москва 与 москва 折成同一个键')
p('        // 全部西里尔大写逐个核：А-Я 是 +0x20，Ѐ-Џ 是 +0x50')
p('        for (uint32_t c = 0x0410; c <= 0x042F; ++c) {')
p('            std::string src;')
p('            enc(src, c);')
p('            const std::string folded = fold_key(src);')
p('            assert(folded.size() == 2);')
p('            const unsigned char c0 = static_cast<unsigned char>(folded[0]);')
p('            const uint32_t got = ((c0 & 0x1Fu) << 6) |')
p('                                (static_cast<unsigned char>(folded[1]) & 0x3Fu);')
p('            assert(got == c + 0x20);')
p('        }')
p('        for (uint32_t c = 0x0400; c <= 0x040F; ++c) {')
p('            std::string src;')
p('            enc(src, c);')
p('            const std::string folded = fold_key(src);')
p('            assert(folded.size() == 2);')
p('            const unsigned char c0 = static_cast<unsigned char>(folded[0]);')
p('            const uint32_t got = ((c0 & 0x1Fu) << 6) |')
p('                                (static_cast<unsigned char>(folded[1]) & 0x3Fu);')
p('            assert(got == c + 0x50);')
p('        }')
eq('fold_key(u8(%s))' % lit('Αθήνα'), lit('αθήνα').join(['u8(', ')']))
_sigma_mid = 'σ' + 'ο' + 'σ' + 'ο' + 'σ'      # 词中形：σοσοσ
_sigma_end = 'σ' + 'ο' + 'σ' + 'ο' + 'ς'      # 词尾形：最后一个是 ς
assert _sigma_mid != _sigma_end
eq('fold_key(u8(%s))' % lit(_sigma_mid),
   'fold_key(u8(%s))' % lit(_sigma_end),
   '希腊 final sigma ς（词尾形 U+03C2）与 σ（词中形 U+03C3）是同一字母的'
   '两种写法，查的是同一条词条，键必须相同')
eq('fold_key(u8(%s))' % lit(_sigma_end), lit('σοσοσ').join(['u8(', ')']),
   '词尾形直接折成 σ')
eq('fold_key(u8(%s))' % lit('Tiệng'), 'fold_key(u8(%s))' % lit('Tiếng'),
   '越南语 Ế/ế：重音折叠的意义所在')
eq('fold_key(u8(%s))' % lit('ႠႡႢ'), lit('ⴀⴁⴂ').join(['u8(', ')']),
   '乔治亚文现代小写映射跨了 0x2000，只有生成表折得对')
eq('fold_key(u8(%s))' % lit('Ẁ'), lit('w').join(['u8(', ')']),
   'Ẁ U+1E00 在重音折叠表里（NFKD = w + 组合符），所以先折成 w；'
   '重音折叠优先于大小写折叠')
eq('fold_key(u8(%s))' % lit('Ÿ'), lit('y').join(['u8(', ')']),
   'Ÿ U+0178 的小写是 y U+0079，偏移是**负**的：生成表必须用 int32 delta')
eq('fold_key(u8(%s))' % lit('Œ'), 'fold_key(u8(%s))' % lit('œ'))
eq('fold_key(u8(%s))' % lit('Đ'), lit('d').join(['u8(', ')']),
   'Đ 走重音折叠表（0x010E-0x0111 -> "d"），不是大小写表')

# ---- 2) 标点 ----
sect('2) 标点归一：从 Word / 网页 / PDF 复制来的文本')
for name, ch in (('左单引号 U+2018', '‘'), ('右单引号 U+2019', '’')):
    eq('fold_key(u8(%s))' % lit("don" + ch + "t"), lit("don't").join(['u8(', ')']),
       name)
eq('fold_key(u8(%s))' % lit('“test”'), lit('"test"').join(['u8(', ')']))
p('        // 破折号族全部折成 ASCII 连字符')
for ch in '‐‑‒–—―−':
    eq('fold_key(u8(%s))' % lit('a' + ch + 'b'), lit('a-b').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('a…b'), lit('a...b').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('3×4'), lit('3x4').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('a•b'), lit('a-b').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('©2024'), lit('(c)2024').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('™'), lit('(tm)').join(['u8(', ')']),
   '折成 (tm) 而不是 tm：与字母 tm 区分开')
eq('fold_key(u8(%s))' % lit('½'), lit('1/2').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('¼'), lit('1/4').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('¾'), lit('3/4').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('Ａ，Ｂ'), lit('a,b').join(['u8(', ')']),
   '全角字母与全角逗号')

# ---- 3) 不可见字符 ----
sect('3) 不可见字符：PDF/网页复制带进来的"看不见的脏东西"')
for name, s in (('零宽空格 U+200B', 'hello'),
                ('零宽非连接符 U+200C', 'a‌b'),
                ('零宽连接符 U+200D', 'a‍b'),
                ('BOM U+FEFF', '﻿hello'),
                ('软连字符 U+00AD（PDF 最常见）', 'man­ual'),
                ('变体选择符 VS16', 'a️b'),
                ('标签字符 U+E0000', 'a\U000E0000b')):
    eq('fold_key(u8(%s))' % lit(s), lit(s.replace('', '').replace('‌', '')
                                       .replace('‍', '').replace('﻿', '')
                                       .replace('­', '').replace('️', '')
                                       .replace('\U000E0000', '')).join(['u8(', ')']),
       name)

# ---- 4) 组合记号 ----
sect('4) 组合记号：希伯来语尼基德 / 阿拉伯语哈拉卡 / 印度系元音符号')
eq('fold_key(u8(%s))' % lit('שָׁלוּם'), 'fold_key(u8(%s))' % lit('שלום'),
   '带尼基德与不带尼基德折成同一个键')
eq('fold_key(u8(%s))' % lit('العَرَبِيَّة'),
   'fold_key(u8(%s))' % lit('العربية') if False else
   'fold_key(u8(%s))' % lit('العربية'.replace('ية', 'ية')),
   '阿拉伯语带哈拉卡')
p('        // 天城文的元音符号（matra）是独立码点，不折就折不出正确的键')
eq('fold_key(u8(%s))' % lit('का'), lit('क').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('किन'), lit('कन').join(['u8(', ')']))

# ---- 5) 连字 ----
sect('5) 连字：排版连字在词典键里就是普通字母连写')
eq('fold_key(u8(%s))' % lit('ﬁle'), lit('file').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('ﬀx'), lit('ffx').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('ﬃy'), lit('ffiy').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('ﬄy'), lit('ffly').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('ﬅ'), lit('st').join(['u8(', ')']))

# ---- 6) Unicode 空白 ----
sect('6) Unicode 空白：首尾 trim 与中间归一')
eq('fold_key(u8(%s))' % lit(' hello '), lit('hello').join(['u8(', ')']),
   '不换行空格 NBSP')
eq('fold_key(u8(%s))' % lit(' hi '), lit('hi').join(['u8(', ')']),
   'em space / thin space')
eq('fold_key(u8(%s))' % lit('  hi  '), lit('hi').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('   \t\n hi \r\n  '), lit('hi').join(['u8(', ')']))

# ---- 7) 重音折叠（原有能力，别回退）----
sect('7) 重音折叠：原有能力，别回退')
eq('fold_key(u8(%s))' % lit('café'), lit('cafe').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('CAFÉ'), lit('cafe').join(['u8(', ')']))
eq('fold_key(u8(%s))' % lit('straße'), lit('strasse').join(['u8(', ')']),
   'ß -> ss')
eq('fold_key(u8(%s))' % lit('yī'), lit('yi').join(['u8(', ')']),
   '拼音 ǐ -> yi')
eq('fold_key(u8(%s))' % lit('ș'), lit('s').join(['u8(', ')']))

# ---- 8) 非拉丁文字本体原样保留 ----
sect('8) 非拉丁文字本体原样保留（不能被误折）')
for s in ('中文', 'あい', 'العربية'):
    eq('fold_key(u8(%s))' % lit(s), lit(s).join(['u8(', ')']), s)

# ---- 9) 非法 UTF-8 ----
sect('9) 非法 UTF-8：原样透传，不纠错、不丢字节')
p('        // 字节层面手写：这些不是合法 UTF-8，用 u8() 反而会被改写')
for raw, note in (('a\\xffz', '孤立续字节'),
                  ('\\xc3', '截断的 2 字节序列'),
                  ('\\xed\\xa0\\x80', 'UTF-8 代理区'),
                  ('\\xc0\\x80', '过长编码（编出了 NUL）'),
                  ('\\xff', '0xFF 不是合法 UTF-8 首字节')):
    p('        assert(fold_key("%s") == "%s");  // %s' % (raw, raw, note))

# ---- 10) Options ----
sect('10) Options：每个开关都要真的生效，且能关掉')
p('        {')
p('            Options o;')
p('            o.fold_punctuation = false;')
eq('fold_key(u8(%s), o)' % lit('don’t'), lit('don’t').join(['u8(', ')']))
p('        }')
p('        {')
p('            Options o;')
p('            o.strip_invisible = false;')
eq('fold_key(u8(%s), o)' % lit('hello'), lit('hello').join(['u8(', ')']))
p('        }')
p('        {')
p('            Options o;')
p('            o.fold_case = false;')
eq('fold_key("ABC", o)', '"ABC"')
p('        }')
p('        {')
p('            Options o;')
p('            o.fold_diacritics = false;')
eq('fold_key(u8(%s), o)' % lit('é'), lit('é').join(['u8(', ')']),
   '重音不折时 é 原样；大小写折叠仍把 É 折成 é（两条路径独立）')
p('        }')
p('        {')
p('            Options o;')
p('            o.fold_ligatures = false;')
eq('fold_key(u8(%s), o)' % lit('ﬁle'), lit('ﬁle').join(['u8(', ')']))
p('        }')
p('        {')
p('            Options o;')
p('            o.strip_combining = false;')
eq('fold_key("e\\xcc\\x81", o)', '"e\\xcc\\x81"')
p('        }')
p('        {')
p('            Options o;')
p('            o.trim = false;')
eq('fold_key("  hi  ", o)', '"  hi  "')
p('        }')
p('        // 全关：只剩"全角 -> 半角"这一条无条件生效的规则')
p('        {')
p('            Options none;')
p('            none.fold_case = false;')
p('            none.fold_diacritics = false;')
p('            none.fold_punctuation = false;')
p('            none.strip_invisible = false;')
p('            none.strip_combining = false;')
p('            none.fold_ligatures = false;')
p('            none.trim = false;')
eq('fold_key("Hello\\xc2\\xa0", none)', '"Hello\\xc2\\xa0"',
   'NBSP 既不 trim 也不归一（strip_invisible 不管它）')
eq('fold_key(u8(%s), none)' % lit('Ａ'), lit('A').join(['u8(', ')']),
   '全角仍折半角：这一步不在任何开关里')
eq('fold_key("  \\xef\\xbc\\xa1  ", none)', '"  A  "')
p('        }')

# ---- 11) 空输入 ----
sect('11) kCaseOddEven 的两条分支')
p('        // 西里尔补充的成对音：U+0460（Ѡ，大写，偶数码点）走"偶数 +1"；')
p('        // U+0461（ѡ，小写，奇数码点）已经是小写，原样返回。')
p('        // 越南语 U+1E00 段现在由 kFold 接管（NFKD 推出 w 等），所以这条')
p('        // 路径的可达样例要挑一个不在重音表里的。')
eq('fold_key(u8(%s))' % lit('Ѡ'), lit('ѡ').join(['u8(', ')']),
   '偶数码点 +1')
eq('fold_key(u8(%s))' % lit('ѡ'), lit('ѡ').join(['u8(', ')']),
   '奇数码点已是小写，原样')
eq('fold_key(u8(%s))' % lit('Ѡ'), lit('ѡ').join(['u8(', ')']))

sect('12) 全角空格 U+3000')
eq('fold_key(u8(%s))' % lit('　hello　'), lit('hello').join(['u8(', ')']),
   '全角空格折成 U+0020 再被 trim 掉')
eq('fold_key(u8(%s))' % lit('a　b'), lit('a b').join(['u8(', ')']),
   '夹在中间的也要折成普通空格')

sect('13) 空输入')
p('        assert(fold_key("").empty());')
p('        assert(fold_key("   ").empty());')
p('        assert(fold_key(u8(%s)).empty());' % lit('﻿'))

p('    std::cout << "OK\\n";')
p('    return 0;')
p('}')

with io.open('tests/text_norm_std_pro_test.cpp', 'w', encoding='utf-8') as f:
    f.write('\n'.join(L))
print('generated tests/text_norm_std_pro_test.cpp (%d lines)' % len(L))
