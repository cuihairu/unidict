// M4 变体容忍表纯 std 测试：表内容、域归属（变体经正向映射应回到
// 主键域，文档化的 AO→ɑ 例外除外）、未知音素空表。
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "std/espeak_arpabet_std.h"
#include "std/pron_variants_std.h"

using namespace UnidictCoreStd;

namespace {

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

    assert((arpabet_variants("T") == std::vector<std::string>{flap}));
    assert((arpabet_variants("D") == std::vector<std::string>{flap}));
    assert((arpabet_variants("L") == std::vector<std::string>{syll_l}));
    assert((arpabet_variants("N") == std::vector<std::string>{syll_n}));
    assert((arpabet_variants("ER") == std::vector<std::string>{inv_3}));
    assert((arpabet_variants("AA") ==
            std::vector<std::string>{a, turned_a}));
    assert((arpabet_variants("AH") ==
            std::vector<std::string>{schwa, ae_inv}));
    assert((arpabet_variants("AO") ==
            std::vector<std::string>{open_o, a}));
    assert((arpabet_variants("EH") == std::vector<std::string>{"e"}));
    assert((arpabet_variants("IH") == std::vector<std::string>{schwi}));
    assert((arpabet_variants("IY") == std::vector<std::string>{"i"}));
    assert((arpabet_variants("OW") ==
            std::vector<std::string>{"o", long_o}));
    assert((arpabet_variants("UW") == std::vector<std::string>{"u"}));
}

void test_unknown_phone_empty() {
    // 辅音清浊对（d 读 t 是错误）与不在表内的音素一律空表
    for (const std::string& phone :
         {"", "XX", "B", "K", "G", "NG", "S", "ZH", "R", "UH", "OY", "AW",
          "AY", "M", "P", "F", "V", "W", "Y", "HH", "JH", "CH", "TH", "DH",
          "SH", "Z"}) {
        assert(arpabet_variants(phone).empty());
    }
}

void test_variants_stay_in_domain() {
    // 每个变体经 espeak_to_arpabet 正向映射应回到主键的 ARPAbet 域
    // ——保证"变体"不是把别的音素错当同类。文档化的跨域例外两条：
    //  1. AO→ɑ：cot-caught 合并里 ɑ 归 AA 域；
    //  2. D→ɾ：模型没有 D 的闪音类，latter/ladder 合流后 T、D 共享
    //     同一个 ɾ 证据类（正向映射里 ɾ 只归 T）。
    for (const std::string& phone :
         {"T", "D", "L", "N", "ER", "AA", "AH", "AO", "EH", "IH", "IY",
          "OW", "UW"}) {
        for (const std::string& v : arpabet_variants(phone)) {
            const std::vector<std::string> mapped = espeak_to_arpabet(v);
            assert(!mapped.empty());
            const bool same_domain =
                mapped.size() == 1 && mapped[0] == phone;
            if (!same_domain) {
                assert(phone == "AO" && v == "\xC9\x91" &&  // ɑ
                           mapped.size() == 1 && mapped[0] == "AA" ||
                       phone == "D" && v == "\xC9\xBE" &&   // ɾ
                           mapped.size() == 1 && mapped[0] == "T");
            }
        }
    }
}

}  // namespace

int main() {
    test_table_contents();
    test_unknown_phone_empty();
    test_variants_stay_in_domain();
    std::cout << "pron_variants_std_test: all assertions passed\n";
    return 0;
}
