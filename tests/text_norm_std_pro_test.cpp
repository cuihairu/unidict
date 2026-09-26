// 查词键归一化的专业场景回归（docs/pro_dictionary_gap.md P0 ②「字符归一化
// 不足：大小写/重音/全半角/Unicode 兼容折叠、标点归一化对"专业查词"很关键
// （尤其多语种）」）。
//
// 本文件由 scripts/gen_text_norm_test.py 生成：测试里全是 UTF-8 字节字面量，
// 手写必错（C++ 的 \x 转义贪婪；凭记忆敲 UTF-8/GBK 字节）。用 Python 里的
// 真实字符串当源，字节与期望值都不可能错。改测试请改生成脚本。
//
// 表本身的正确性（升序/不重叠/与 Python str.lower() 逐码点一致）由
// scripts/check_text_norm_tables.py 离线核；这里核的是**端到端行为**。
//
// 每条断言都对应一个真实场景："用户从某个来源复制/输入这个词，词典里有，
// 但查不到"。断言刻意写成**成对的输入 → 同一个键**，因为归一化的契约就是
// "不同写法必须折成同一个键"，只测单边的话折成什么都算过。

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "std/text_norm_std.h"

using namespace UnidictCoreStd::TextNorm;

// C++ 的十六进制转义是贪婪的（"\x94b" 会被当成一个越界的数），
// 所以多字节 UTF-8 必须每字节一个独立字面量。
static std::string u8(const std::string& s) { return s; }

// 把一个码点编成 UTF-8，供"逐码点核大小写表"用。
static void enc(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

int main() {
    // =========================================================================
    // 1) 大小写：非拉丁文字此前完全没折，俄语/希腊语词典是大小写敏感的
    // =========================================================================
        // Москва 与 москва 折成同一个键
        assert(fold_key(u8("\xd0\x9c" "\xd0\xbe" "\xd1\x81" "\xd0\xba" "\xd0\xb2" "\xd0\xb0")) == fold_key(u8("\xd0\xbc" "\xd0\xbe" "\xd1\x81" "\xd0\xba" "\xd0\xb2" "\xd0\xb0")));
        // 全部西里尔大写逐个核：А-Я 是 +0x20，Ѐ-Џ 是 +0x50
        for (uint32_t c = 0x0410; c <= 0x042F; ++c) {
            std::string src;
            enc(src, c);
            const std::string folded = fold_key(src);
            assert(folded.size() == 2);
            const unsigned char c0 = static_cast<unsigned char>(folded[0]);
            const uint32_t got = ((c0 & 0x1Fu) << 6) |
                                (static_cast<unsigned char>(folded[1]) & 0x3Fu);
            assert(got == c + 0x20);
        }
        for (uint32_t c = 0x0400; c <= 0x040F; ++c) {
            std::string src;
            enc(src, c);
            const std::string folded = fold_key(src);
            assert(folded.size() == 2);
            const unsigned char c0 = static_cast<unsigned char>(folded[0]);
            const uint32_t got = ((c0 & 0x1Fu) << 6) |
                                (static_cast<unsigned char>(folded[1]) & 0x3Fu);
            assert(got == c + 0x50);
        }
        assert(fold_key(u8("\xce\x91" "\xce\xb8" "\xce\xae" "\xce\xbd" "\xce\xb1")) == u8("\xce\xb1" "\xce\xb8" "\xce\xae" "\xce\xbd" "\xce\xb1"));
        // 希腊 final sigma ς（词尾形 U+03C2）与 σ（词中形 U+03C3）是同一字母的两种写法，查的是同一条词条，键必须相同
        assert(fold_key(u8("\xcf\x83" "\xce\xbf" "\xcf\x83" "\xce\xbf" "\xcf\x83")) == fold_key(u8("\xcf\x83" "\xce\xbf" "\xcf\x83" "\xce\xbf" "\xcf\x82")));
        // 词尾形直接折成 σ
        assert(fold_key(u8("\xcf\x83" "\xce\xbf" "\xcf\x83" "\xce\xbf" "\xcf\x82")) == u8("\xcf\x83" "\xce\xbf" "\xcf\x83" "\xce\xbf" "\xcf\x83"));
        // 越南语 Ế/ế：重音折叠的意义所在
        assert(fold_key(u8("\x54" "\x69" "\xe1\xbb\x87" "\x6e" "\x67")) == fold_key(u8("\x54" "\x69" "\xe1\xba\xbf" "\x6e" "\x67")));
        // 乔治亚文现代小写映射跨了 0x2000，只有生成表折得对
        assert(fold_key(u8("\xe1\x82\xa0" "\xe1\x82\xa1" "\xe1\x82\xa2")) == u8("\xe2\xb4\x80" "\xe2\xb4\x81" "\xe2\xb4\x82"));
        // Ẁ U+1E00 在重音折叠表里（NFKD = w + 组合符），所以先折成 w；重音折叠优先于大小写折叠
        assert(fold_key(u8("\xe1\xba\x80")) == u8("\x77"));
        // Ÿ U+0178 的小写是 y U+0079，偏移是**负**的：生成表必须用 int32 delta
        assert(fold_key(u8("\xc5\xb8")) == u8("\x79"));
        assert(fold_key(u8("\xc5\x92")) == fold_key(u8("\xc5\x93")));
        // Đ 走重音折叠表（0x010E-0x0111 -> "d"），不是大小写表
        assert(fold_key(u8("\xc4\x90")) == u8("\x64"));
    // =========================================================================
    // 2) 标点归一：从 Word / 网页 / PDF 复制来的文本
    // =========================================================================
        // 左单引号 U+2018
        assert(fold_key(u8("\x64" "\x6f" "\x6e" "\xe2\x80\x98" "\x74")) == u8("\x64" "\x6f" "\x6e" "\x27" "\x74"));
        // 右单引号 U+2019
        assert(fold_key(u8("\x64" "\x6f" "\x6e" "\xe2\x80\x99" "\x74")) == u8("\x64" "\x6f" "\x6e" "\x27" "\x74"));
        assert(fold_key(u8("\xe2\x80\x9c" "\x74" "\x65" "\x73" "\x74" "\xe2\x80\x9d")) == u8("\"" "\x74" "\x65" "\x73" "\x74" "\""));
        // 破折号族全部折成 ASCII 连字符
        assert(fold_key(u8("\x61" "\xe2\x80\x90" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\x91" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\x92" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\x93" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\x94" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\x95" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x88\x92" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\x61" "\xe2\x80\xa6" "\x62")) == u8("\x61" "\x2e" "\x2e" "\x2e" "\x62"));
        assert(fold_key(u8("\x33" "\xc3\x97" "\x34")) == u8("\x33" "\x78" "\x34"));
        assert(fold_key(u8("\x61" "\xe2\x80\xa2" "\x62")) == u8("\x61" "\x2d" "\x62"));
        assert(fold_key(u8("\xc2\xa9" "\x32" "\x30" "\x32" "\x34")) == u8("\x28" "\x63" "\x29" "\x32" "\x30" "\x32" "\x34"));
        // 折成 (tm) 而不是 tm：与字母 tm 区分开
        assert(fold_key(u8("\xe2\x84\xa2")) == u8("\x28" "\x74" "\x6d" "\x29"));
        assert(fold_key(u8("\xc2\xbd")) == u8("\x31" "\x2f" "\x32"));
        assert(fold_key(u8("\xc2\xbc")) == u8("\x31" "\x2f" "\x34"));
        assert(fold_key(u8("\xc2\xbe")) == u8("\x33" "\x2f" "\x34"));
        // 全角字母与全角逗号
        assert(fold_key(u8("\xef\xbc\xa1" "\xef\xbc\x8c" "\xef\xbc\xa2")) == u8("\x61" "\x2c" "\x62"));
    // =========================================================================
    // 3) 不可见字符：PDF/网页复制带进来的"看不见的脏东西"
    // =========================================================================
        // 零宽空格 U+200B
        assert(fold_key(u8("\x68" "\x65" "\x6c" "\x6c" "\x6f")) == u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"));
        // 零宽非连接符 U+200C
        assert(fold_key(u8("\x61" "\xe2\x80\x8c" "\x62")) == u8("\x61" "\x62"));
        // 零宽连接符 U+200D
        assert(fold_key(u8("\x61" "\xe2\x80\x8d" "\x62")) == u8("\x61" "\x62"));
        // BOM U+FEFF
        assert(fold_key(u8("\xef\xbb\xbf" "\x68" "\x65" "\x6c" "\x6c" "\x6f")) == u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"));
        // 软连字符 U+00AD（PDF 最常见）
        assert(fold_key(u8("\x6d" "\x61" "\x6e" "\xc2\xad" "\x75" "\x61" "\x6c")) == u8("\x6d" "\x61" "\x6e" "\x75" "\x61" "\x6c"));
        // 变体选择符 VS16
        assert(fold_key(u8("\x61" "\xef\xb8\x8f" "\x62")) == u8("\x61" "\x62"));
        // 标签字符 U+E0000
        assert(fold_key(u8("\x61" "\xf3\xa0\x80\x80" "\x62")) == u8("\x61" "\x62"));
    // =========================================================================
    // 4) 组合记号：希伯来语尼基德 / 阿拉伯语哈拉卡 / 印度系元音符号
    // =========================================================================
        // 带尼基德与不带尼基德折成同一个键
        assert(fold_key(u8("\xd7\xa9" "\xd6\xb8" "\xd7\x81" "\xd7\x9c" "\xd7\x95" "\xd6\xbc" "\xd7\x9d")) == fold_key(u8("\xd7\xa9" "\xd7\x9c" "\xd7\x95" "\xd7\x9d")));
        // 阿拉伯语带哈拉卡
        assert(fold_key(u8("\xd8\xa7" "\xd9\x84" "\xd8\xb9" "\xd9\x8e" "\xd8\xb1" "\xd9\x8e" "\xd8\xa8" "\xd9\x90" "\xd9\x8a" "\xd9\x8e" "\xd9\x91" "\xd8\xa9")) == fold_key(u8("\xd8\xa7" "\xd9\x84" "\xd8\xb9" "\xd8\xb1" "\xd8\xa8" "\xd9\x8a" "\xd8\xa9")));
        // 天城文的元音符号（matra）是独立码点，不折就折不出正确的键
        assert(fold_key(u8("\xe0\xa4\x95" "\xe0\xa4\xbe")) == u8("\xe0\xa4\x95"));
        assert(fold_key(u8("\xe0\xa4\x95" "\xe0\xa4\xbf" "\xe0\xa4\xa8")) == u8("\xe0\xa4\x95" "\xe0\xa4\xa8"));
    // =========================================================================
    // 5) 连字：排版连字在词典键里就是普通字母连写
    // =========================================================================
        assert(fold_key(u8("\xef\xac\x81" "\x6c" "\x65")) == u8("\x66" "\x69" "\x6c" "\x65"));
        assert(fold_key(u8("\xef\xac\x80" "\x78")) == u8("\x66" "\x66" "\x78"));
        assert(fold_key(u8("\xef\xac\x83" "\x79")) == u8("\x66" "\x66" "\x69" "\x79"));
        assert(fold_key(u8("\xef\xac\x84" "\x79")) == u8("\x66" "\x66" "\x6c" "\x79"));
        assert(fold_key(u8("\xef\xac\x85")) == u8("\x73" "\x74"));
    // =========================================================================
    // 6) Unicode 空白：首尾 trim 与中间归一
    // =========================================================================
        // 不换行空格 NBSP
        assert(fold_key(u8("\xc2\xa0" "\x68" "\x65" "\x6c" "\x6c" "\x6f" "\xc2\xa0")) == u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"));
        // em space / thin space
        assert(fold_key(u8("\xe2\x80\x83" "\x68" "\x69" "\xe2\x80\x83")) == u8("\x68" "\x69"));
        assert(fold_key(u8("\x20" "\x20" "\x68" "\x69" "\x20" "\x20")) == u8("\x68" "\x69"));
        assert(fold_key(u8("\x20" "\x20" "\x20" "\x09" "\x0a" "\x20" "\x68" "\x69" "\x20" "\x0d" "\x0a" "\x20" "\x20")) == u8("\x68" "\x69"));
    // =========================================================================
    // 7) 重音折叠：原有能力，别回退
    // =========================================================================
        assert(fold_key(u8("\x63" "\x61" "\x66" "\xc3\xa9")) == u8("\x63" "\x61" "\x66" "\x65"));
        assert(fold_key(u8("\x43" "\x41" "\x46" "\xc3\x89")) == u8("\x63" "\x61" "\x66" "\x65"));
        // ß -> ss
        assert(fold_key(u8("\x73" "\x74" "\x72" "\x61" "\xc3\x9f" "\x65")) == u8("\x73" "\x74" "\x72" "\x61" "\x73" "\x73" "\x65"));
        // 拼音 ǐ -> yi
        assert(fold_key(u8("\x79" "\xc4\xab")) == u8("\x79" "\x69"));
        assert(fold_key(u8("\xc8\x99")) == u8("\x73"));
    // =========================================================================
    // 8) 非拉丁文字本体原样保留（不能被误折）
    // =========================================================================
        // 中文
        assert(fold_key(u8("\xe4\xb8\xad" "\xe6\x96\x87")) == u8("\xe4\xb8\xad" "\xe6\x96\x87"));
        // あい
        assert(fold_key(u8("\xe3\x81\x82" "\xe3\x81\x84")) == u8("\xe3\x81\x82" "\xe3\x81\x84"));
        // العربية
        assert(fold_key(u8("\xd8\xa7" "\xd9\x84" "\xd8\xb9" "\xd8\xb1" "\xd8\xa8" "\xd9\x8a" "\xd8\xa9")) == u8("\xd8\xa7" "\xd9\x84" "\xd8\xb9" "\xd8\xb1" "\xd8\xa8" "\xd9\x8a" "\xd8\xa9"));
    // =========================================================================
    // 9) 非法 UTF-8：原样透传，不纠错、不丢字节
    // =========================================================================
        // 字节层面手写：这些不是合法 UTF-8，用 u8() 反而会被改写
        assert(fold_key("a\xffz") == "a\xffz");  // 孤立续字节
        assert(fold_key("\xc3") == "\xc3");  // 截断的 2 字节序列
        assert(fold_key("\xed\xa0\x80") == "\xed\xa0\x80");  // UTF-8 代理区
        assert(fold_key("\xc0\x80") == "\xc0\x80");  // 过长编码（编出了 NUL）
        assert(fold_key("\xff") == "\xff");  // 0xFF 不是合法 UTF-8 首字节
    // =========================================================================
    // 10) Options：每个开关都要真的生效，且能关掉
    // =========================================================================
        {
            Options o;
            o.fold_punctuation = false;
        assert(fold_key(u8("\x64" "\x6f" "\x6e" "\xe2\x80\x99" "\x74"), o) == u8("\x64" "\x6f" "\x6e" "\xe2\x80\x99" "\x74"));
        }
        {
            Options o;
            o.strip_invisible = false;
        assert(fold_key(u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"), o) == u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"));
        }
        {
            Options o;
            o.fold_case = false;
        assert(fold_key("ABC", o) == "ABC");
        }
        {
            Options o;
            o.fold_diacritics = false;
        // 重音不折时 é 原样；大小写折叠仍把 É 折成 é（两条路径独立）
        assert(fold_key(u8("\xc3\xa9"), o) == u8("\xc3\xa9"));
        }
        {
            Options o;
            o.fold_ligatures = false;
        assert(fold_key(u8("\xef\xac\x81" "\x6c" "\x65"), o) == u8("\xef\xac\x81" "\x6c" "\x65"));
        }
        {
            Options o;
            o.strip_combining = false;
        assert(fold_key("e\xcc\x81", o) == "e\xcc\x81");
        }
        {
            Options o;
            o.trim = false;
        assert(fold_key("  hi  ", o) == "  hi  ");
        }
        // 全关：只剩"全角 -> 半角"这一条无条件生效的规则
        {
            Options none;
            none.fold_case = false;
            none.fold_diacritics = false;
            none.fold_punctuation = false;
            none.strip_invisible = false;
            none.strip_combining = false;
            none.fold_ligatures = false;
            none.trim = false;
        // NBSP 既不 trim 也不归一（strip_invisible 不管它）
        assert(fold_key("Hello\xc2\xa0", none) == "Hello\xc2\xa0");
        // 全角仍折半角：这一步不在任何开关里
        assert(fold_key(u8("\xef\xbc\xa1"), none) == u8("\x41"));
        assert(fold_key("  \xef\xbc\xa1  ", none) == "  A  ");
        }
    // =========================================================================
    // 11) kCaseOddEven 的两条分支
    // =========================================================================
        // 西里尔补充的成对音：U+0460（Ѡ，大写，偶数码点）走"偶数 +1"；
        // U+0461（ѡ，小写，奇数码点）已经是小写，原样返回。
        // 越南语 U+1E00 段现在由 kFold 接管（NFKD 推出 w 等），所以这条
        // 路径的可达样例要挑一个不在重音表里的。
        // 偶数码点 +1
        assert(fold_key(u8("\xd1\xa0")) == u8("\xd1\xa1"));
        // 奇数码点已是小写，原样
        assert(fold_key(u8("\xd1\xa1")) == u8("\xd1\xa1"));
        assert(fold_key(u8("\xd1\xa0")) == u8("\xd1\xa1"));
    // =========================================================================
    // 12) 全角空格 U+3000
    // =========================================================================
        // 全角空格折成 U+0020 再被 trim 掉
        assert(fold_key(u8("\xe3\x80\x80" "\x68" "\x65" "\x6c" "\x6c" "\x6f" "\xe3\x80\x80")) == u8("\x68" "\x65" "\x6c" "\x6c" "\x6f"));
        // 夹在中间的也要折成普通空格
        assert(fold_key(u8("\x61" "\xe3\x80\x80" "\x62")) == u8("\x61" "\x20" "\x62"));
    // =========================================================================
    // 13) 空输入
    // =========================================================================
        assert(fold_key("").empty());
        assert(fold_key("   ").empty());
        assert(fold_key(u8("\xef\xbb\xbf")).empty());
    std::cout << "OK\n";
    return 0;
}