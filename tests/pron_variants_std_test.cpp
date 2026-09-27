// 变体容忍表纯 std 测试：表内容（含 M7 位置感知行）、notable 位、
// 域归属（变体经正向映射应回到主键域，文档化的跨域例外除外）、
// 未知音素空表。
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "std/espeak_arpabet_std.h"
#include "std/pron_variants_std.h"

using namespace UnidictCoreStd;

namespace {

std::vector<std::string> syms(const std::vector<PhoneVariant>& vars) {
    std::vector<std::string> out;
    for (const auto& v : vars) {
        out.push_back(v.espeak);
    }
    return out;
}

void test_table_contents() {
    const std::string flap = "\xC9\xBE";      // ɾ
    const std::string syll_l = "l\xCC\xA9";   // l̩
    const std::string syll_n = "n\xCC\xA9";   // n̩
    const std::string inv_3 = "\xC9\x9C";     // ɜ
    const std::string a = "\xC9\x91";         // ɑ
    const std::string turned_a = "\xC9\x92";  // ɒ
    const std::string schwa = "\xC9\x99";     // ə
    const std::string ae_inv = "\xC9\x90";    // ɐ
    const std::string open_o = "\xC9\x94";    // ɔ
    const std::string schwi = "\xE1\xB5\xBB"; // ᵻ
    const std::string long_o = "o\xCB\x90";   // oː
    const std::string eng = "\xC5\x8B";       // ŋ

    // 位置无关行：word_final 两种取值都返回同一批（silent）
    for (bool final_pos : {false, true}) {
        assert((syms(arpabet_variants("T", final_pos)) ==
                std::vector<std::string>{flap}));
        assert((syms(arpabet_variants("D", final_pos)) ==
                std::vector<std::string>{flap}));
        assert((syms(arpabet_variants("L", final_pos)) ==
                std::vector<std::string>{syll_l}));
        assert((syms(arpabet_variants("N", final_pos)) ==
                std::vector<std::string>{syll_n}));
        assert((syms(arpabet_variants("ER", final_pos)) ==
                std::vector<std::string>{inv_3}));
        assert((syms(arpabet_variants("AA", final_pos)) ==
                std::vector<std::string>{a, turned_a}));
        assert((syms(arpabet_variants("AH", final_pos)) ==
                std::vector<std::string>{schwa, ae_inv}));
        assert((syms(arpabet_variants("AO", final_pos)) ==
                std::vector<std::string>{open_o, a}));
        assert((syms(arpabet_variants("EH", final_pos)) ==
                std::vector<std::string>{"e"}));
        assert((syms(arpabet_variants("IH", final_pos)) ==
                std::vector<std::string>{schwi}));
        assert((syms(arpabet_variants("IY", final_pos)) ==
                std::vector<std::string>{"i"}));
        assert((syms(arpabet_variants("OW", final_pos)) ==
                std::vector<std::string>{"o", long_o}));
        assert((syms(arpabet_variants("UW", final_pos)) ==
                std::vector<std::string>{"u"}));
    }

    // M7 位置感知行：词尾 g→ŋ 同化——词尾才有，且 notable（音素级
    // 替代，分数容忍之外还要报出）；非词尾是空表（goal 的首 g 读 ŋ
    // 是真错误，照扣）
    const auto final_g = arpabet_variants("G", true);
    assert((syms(final_g) == std::vector<std::string>{eng}));
    assert(final_g[0].notable);
    assert(arpabet_variants("G", false).empty());
}

void test_notable_flags() {
    // 既有行全是同音素实现（silent）：报出"你发了正确的音"没有意义
    for (bool final_pos : {false, true}) {
        for (const std::string& phone :
             {"T", "D", "L", "N", "ER", "AA", "AH", "AO", "EH", "IH",
              "IY", "OW", "UW"}) {
            for (const auto& v : arpabet_variants(phone, final_pos)) {
                assert(!v.notable);
            }
        }
    }
    // 表内唯一的 notable 是词尾 G 的 ŋ
    for (const auto& v : arpabet_variants("G", true)) {
        assert(v.notable == (v.espeak == "\xC5\x8B"));
    }
}

void test_unknown_phone_empty() {
    // 辅音清浊对（d 读 t 是错误）与不在表内的音素一律空表（两个
    // 位置都验——NG 词尾也不开 ŋ 之外的口子）
    for (const std::string& phone :
         {"", "XX", "B", "K", "NG", "S", "ZH", "R", "UH", "OY", "AW",
          "AY", "M", "P", "F", "V", "W", "Y", "HH", "JH", "CH", "TH",
          "DH", "SH", "Z"}) {
        assert(arpabet_variants(phone, false).empty());
        assert(arpabet_variants(phone, true).empty());
    }
    assert(arpabet_variants("G", false).empty());  // 非词尾单独再钉一次
}

void test_variants_stay_in_domain() {
    // 每个变体经 espeak_to_arpabet 正向映射应回到主键的 ARPAbet 域
    // ——保证"变体"不是把别的音素错当同类。文档化的跨域例外三条：
    //  1. AO→ɑ：cot-caught 合并里 ɑ 归 AA 域；
    //  2. D→ɾ：模型没有 D 的闪音类，latter/ladder 合流后 T、D 共享
    //     同一个 ɾ 证据类（正向映射里 ɾ 只归 T）；
    //  3. G→ŋ（词尾，M7）：同化后的实读落在 NG 域——这正是 notable
    //     的原因（音素级替代而非同域实现）。
    for (bool final_pos : {false, true}) {
        for (const std::string& phone :
             {"T", "D", "L", "N", "ER", "AA", "AH", "AO", "EH", "IH",
              "IY", "OW", "UW", "G"}) {
            for (const auto& v : arpabet_variants(phone, final_pos)) {
                const std::vector<std::string> mapped = espeak_to_arpabet(v.espeak);
                assert(!mapped.empty());
                const bool same_domain =
                    mapped.size() == 1 && mapped[0] == phone;
                if (!same_domain) {
                    assert(phone == "AO" && v.espeak == "\xC9\x91" &&  // ɑ
                               mapped.size() == 1 && mapped[0] == "AA" ||
                           phone == "D" && v.espeak == "\xC9\xBE" &&  // ɾ
                               mapped.size() == 1 && mapped[0] == "T" ||
                           phone == "G" && v.espeak == "\xC5\x8B" &&  // ŋ
                               mapped.size() == 1 && mapped[0] == "NG");
                }
            }
        }
    }
}

}  // namespace

int main() {
    test_table_contents();
    test_notable_flags();
    test_unknown_phone_empty();
    test_variants_stay_in_domain();
    std::cout << "pron_variants_std_test: all assertions passed\n";
    return 0;
}
