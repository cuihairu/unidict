// 词典 IPA 文本 → ARPAbet 纯 std 测试：ARPAbet 分词模式、IPA 最长
// 前缀匹配（含合写展开/tie bar/音节符/标注符号）、拒收语义。
// 音标一律写原生 UTF-8 字面量（与 espeak_arpabet_std 表一致）——
// 别用 \x 十六进制转义：\x 后跟 hex 字符（0-9a-fA-F）会被贪婪续读，
// "e"/"d" 这类常见字母恰在其中，错码极难肉眼发现。
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "std/ipa_to_arpabet_std.h"

using UnidictCoreStd::phonetic_text_to_arpabet;

namespace {

bool eq(const std::optional<std::vector<std::string>>& got,
        const std::vector<std::string>& want) {
    return got.has_value() && *got == want;
}

void test_arpabet_token_mode() {
    // 纯 ARPAbet（词典导出常见形态）
    assert(eq(phonetic_text_to_arpabet("K AE T"), {"K", "AE", "T"}));
    // 重音数字剥除（CMU 风格）
    assert(eq(phonetic_text_to_arpabet("B AH1 T ER0"), {"B", "AH", "T", "ER"}));
    // 小写也认（剥离数字 + 大小写归一）
    assert(eq(phonetic_text_to_arpabet("k ae1 t"), {"K", "AE", "T"}));
    // 多余空白
    assert(eq(phonetic_text_to_arpabet("  K  AE\tT\n"), {"K", "AE", "T"}));
    // 数字重音只认词尾："1AE" 剥不动 → 非法
    assert(!phonetic_text_to_arpabet("1AE").has_value());
    // 混入非法 token → 不是 ARPAbet 域，转 IPA 路径后 ASCII 无从
    // 解释 → 整串拒收
    assert(!phonetic_text_to_arpabet("K AE T foo").has_value());
}

void test_ipa_basic() {
    // 词典最常见形态：斜线括重音
    assert(eq(phonetic_text_to_arpabet("/ˈkæt/"), {"K", "AE", "T"}));
    // espeak 风格空格分隔
    assert(eq(phonetic_text_to_arpabet("k æ t"), {"K", "AE", "T"}));
    // flap + 儿化（地道美音 butter）
    assert(eq(phonetic_text_to_arpabet("ˈbʌɾɚ"), {"B", "AH", "T", "ER"}));
    // 合写展开：əl → AH L
    assert(eq(phonetic_text_to_arpabet("ˈbʌtəl"),
              {"B", "AH", "T", "AH", "L"}));
    // r-色合写：ɑːɹ → AA R（car）
    assert(eq(phonetic_text_to_arpabet("kɑːɹd"), {"K", "AA", "R", "D"}));
    // 双元音
    assert(eq(phonetic_text_to_arpabet("oʊ"), {"OW"}));
    assert(eq(phonetic_text_to_arpabet("eɪ"), {"EY"}));
}

void test_ipa_tie_bar_and_marks() {
    // tie bar：t͡ʃ → CH 单音素（不拆成 T SH）
    assert(eq(phonetic_text_to_arpabet("t͡ʃeɪndʒ"),
              {"CH", "EY", "N", "JH"}));
    // 重音符在词中：əˈlaɪv
    assert(eq(phonetic_text_to_arpabet("əˈlaɪv"), {"AH", "L", "AY", "V"}));
    // 音节符 l̩ 走表键：ˈlɪtl̩ → L IH T L
    assert(eq(phonetic_text_to_arpabet("ˈlɪtl̩"), {"L", "IH", "T", "L"}));
    // 孤立长音符（表键没吃掉的）：kæːt → K AE T
    assert(eq(phonetic_text_to_arpabet("kæːt"), {"K", "AE", "T"}));
    // 音节点分节：ˈdɪkʃ.ənɛɹ.i
    assert(eq(phonetic_text_to_arpabet("ˈdɪkʃ.ənɛɹ.i"),
              {"D", "IH", "K", "SH", "AH", "N", "EH", "R", "IY"}));
}

void test_rejections() {
    // 未收录码点（非英语音素）：整串拒收，绝不输出半截序列
    assert(!phonetic_text_to_arpabet("k¥t").has_value());
    // 纯标注符号 → 空结果拒收
    assert(!phonetic_text_to_arpabet("/ˈ/").has_value());
    assert(!phonetic_text_to_arpabet("").has_value());
    assert(!phonetic_text_to_arpabet("   ").has_value());
    // 坏 UTF-8（截断的多字节序列）
    assert(!phonetic_text_to_arpabet("k\xC3").has_value());
    // overlong 编码（C0 AF = overlong 的 '/'）不得被当标注跳过
    assert(!phonetic_text_to_arpabet("k\xC0\xAFt").has_value());
    // ASCII 字母不在 IPA 域（分词模式失败后 'x' 无从解释）
    assert(!phonetic_text_to_arpabet("xyz").has_value());
}

void test_utf8_decode_edges() {
    // 3 字节合法标注（‖ U+2016 韵律切分）被跳过
    assert(eq(phonetic_text_to_arpabet("kæt‖"), {"K", "AE", "T"}));
    // 3 字节非标注（汉字）→ 拒收
    assert(!phonetic_text_to_arpabet("k中t").has_value());
    // 4 字节码点（emoji）→ 拒收
    assert(!phonetic_text_to_arpabet("k😀t").has_value());
    // 4 字节截断 → 拒收
    assert(!phonetic_text_to_arpabet("k\xF0\x9F\x98").has_value());
    // 非法首字节（0xF8 不属于任何 UTF-8 前导）→ 拒收
    assert(!phonetic_text_to_arpabet("k\xF8t").has_value());
    // 非法续字节（E2 后跟 0x28）→ 拒收
    assert(!phonetic_text_to_arpabet("k\xE2\x28t").has_value());
    // overlong 3 字节（E0 80 AF = overlong 的 '/'）→ 拒收
    assert(!phonetic_text_to_arpabet("k\xE0\x80\xAFt").has_value());
    // overlong 4 字节（F0 80 80 AF）→ 拒收
    assert(!phonetic_text_to_arpabet("k\xF0\x80\x80\xAFt").has_value());
}

void test_extract_phonetic_text() {
    using UnidictCoreStd::extract_phonetic_text;
    // 词典最常见形态：斜线字段在释义开头
    assert(extract_phonetic_text("/ˈkæt/ the sound a cat makes")
               .value_or("") == "ˈkæt");
    // 方括号风格
    assert(extract_phonetic_text("hello [həˈləʊ] greeting")
               .value_or("") == "həˈləʊ");
    // HTML 释义：标签剥掉后字段才露出来
    assert(extract_phonetic_text("<b>cat</b><br/>/kæt/ animal")
               .value_or("") == "kæt");
    // 纯 ASCII 斜线字段：只认空格分隔的合法 ARPAbet（重音数字容忍）
    assert(extract_phonetic_text("/K AE1 T/ noun").value_or("") == "K AE1 T");
    // 纯 ASCII 普通词即使"恰好能当音素解析"也不收（hello = h,e,l,l,o
    // 全是单字母音素，但没有 IPA 符号佐证）
    assert(!extract_phonetic_text("see hello/hi world").has_value());
    assert(!extract_phonetic_text("and/or fruit").has_value());
    // URL：空字段与域名都不收
    assert(!extract_phonetic_text("see https://example.com/cat here")
               .has_value());
    // 含非 ASCII 但不是英语 IPA（é 不在表）→ 不收
    assert(!extract_phonetic_text("/café/ word").has_value());
    // 无闭合分隔符 → 不收
    assert(!extract_phonetic_text("/ˈkæt no closing slash").has_value());
    // 候选内部再出现分隔符：括住的正文不是发音字段
    assert(!extract_phonetic_text("[see /ə/]").has_value());
    // 空字段跳过
    assert(!extract_phonetic_text("12//34").has_value());
    // 首个通过解析的候选胜出
    assert(extract_phonetic_text("/ˈkæt/ and /ˈdɒɡ/").value_or("")
               == "ˈkæt");
    // 扫描窗只在前 256 字节：窗外的字段不进候选
    assert(!extract_phonetic_text(std::string(260, 'a') + "/ˈkæt/")
               .has_value());
    // 字段跨出扫描窗（开在窗内、闭在窗外）→ 不收
    assert(!extract_phonetic_text(std::string(250, 'a') + "/" +
                                  std::string(20, 'b') + "/ x")
               .has_value());
    // 超长候选（>128 字节）→ 不收：发音字段没这么长
    assert(!extract_phonetic_text("/" + std::string(150, 'b') + "/ tail")
               .has_value());
    // 候选内出现 [ 或 ]：同样按嵌套拒收
    assert(!extract_phonetic_text("/a[b/ note").has_value());
    assert(!extract_phonetic_text("/a]b/ note").has_value());
    // 非法 UTF-8 出现在候选里 → 该候选拒收，继续扫
    assert(extract_phonetic_text("/k\xF8t/ /ˈkæt/").value_or("") == "ˈkæt");
}

}  // namespace

int main() {
    test_arpabet_token_mode();
    test_ipa_basic();
    test_ipa_tie_bar_and_marks();
    test_rejections();
    test_utf8_decode_edges();
    test_extract_phonetic_text();
    std::cout << "ipa_to_arpabet_std_test: all assertions passed\n";
    return 0;
}
