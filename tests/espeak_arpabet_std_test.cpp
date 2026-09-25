// espeak IPA ↔ ARPAbet 映射纯 std 测试。
// 词表覆盖面按模型 vocab.json（392 类）核对：英语域必须全通，
// 多语符号必须走空结果兜底（不许半吊子映射）。
#include <cassert>
#include <string>
#include <vector>

#include "std/espeak_arpabet_std.h"
#include "std/pronunciation_score_std.h"

using namespace UnidictCoreStd;

namespace {

// 39 音素正反映射 roundtrip：ARPAbet 域的每个音素都必须有 espeak
// 主键并映射回来（这是 score_word 的可行域，缺一个词典音就断桥）
void test_roundtrip_all_arpabet() {
    for (const char* p : {"AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER",
                          "EY", "IH", "IY", "OW", "OY", "UH", "UW",
                          "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K",
                          "L", "M", "N", "NG", "P", "R", "S", "SH", "T",
                          "TH", "V", "W", "Y", "Z", "ZH"}) {
        const std::string espeak = arpabet_to_espeak(p);
        assert(!espeak.empty());
        const auto back = espeak_to_arpabet(espeak);
        assert(back.size() == 1);
        assert(back[0] == p);
    }
    assert(arpabet_to_espeak("XX").empty());
}

void test_single_mapping() {
    // 元音代表：schwa 就近 AH、r 色元音 ER、双元音/长元音逐一对
    struct Case {
        const char* espeak;
        const char* arpabet;
    };
    for (const Case& c : {Case{"ə", "AH"},   Case{"ɐ", "AH"},  Case{"ʌ", "AH"},
                          Case{"ɑː", "AA"},  Case{"ɒ", "AA"},  Case{"æ", "AE"},
                          Case{"ɔː", "AO"},  Case{"aʊ", "AW"}, Case{"aɪ", "AY"},
                          Case{"ɛ", "EH"},   Case{"ɜː", "ER"}, Case{"ɚ", "ER"},
                          Case{"eɪ", "EY"},  Case{"ɪ", "IH"},  Case{"ᵻ", "IH"},
                          Case{"iː", "IY"},  Case{"oʊ", "OW"}, Case{"ɔɪ", "OY"},
                          Case{"ʊ", "UH"},   Case{"uː", "UW"},
                          Case{"ð", "DH"},   Case{"θ", "TH"},  Case{"ʃ", "SH"},
                          Case{"ʒ", "ZH"},   Case{"ŋ", "NG"},  Case{"ɡ", "G"},
                          Case{"dʒ", "JH"},  Case{"tʃ", "CH"}, Case{"ɹ", "R"},
                          Case{"j", "Y"}}) {
        const auto got = espeak_to_arpabet(c.espeak);
        assert(got.size() == 1);
        assert(got[0] == c.arpabet);
    }
    // 美式 flap：butter 的 T，CMU 域里就是 T
    const auto flap = espeak_to_arpabet("ɾ");
    assert(flap.size() == 1 && flap[0] == "T");
}

void test_expansion_mapping() {
    // 合写 1→2 展开：音节 l、r-色元音+r
    const auto al = espeak_to_arpabet("əl");
    assert((al == std::vector<std::string>{"AH", "L"}));
    const auto ar = espeak_to_arpabet("ɑːɹ");
    assert((ar == std::vector<std::string>{"AA", "R"}));
    const auto ir = espeak_to_arpabet("ɪɹ");
    assert((ir == std::vector<std::string>{"IH", "R"}));
    const auto ur = espeak_to_arpabet("ʊɹ");
    assert((ur == std::vector<std::string>{"UH", "R"}));
    // 音节辅音预组合键（词表里就是 n+U+0329 一个键）
    const auto syll_n = espeak_to_arpabet("n̩");
    assert(syll_n.size() == 1 && syll_n[0] == "N");
    const auto syll_l = espeak_to_arpabet("l̩");
    assert(syll_l.size() == 1 && syll_l[0] == "L");
}

void test_unmapped_fallthrough() {
    // 多语符号（模型词表里的法/德/中音素）必须空结果，由上游按
    // 非法音素 cost 1 兜底——不许瞎映射污染分数
    assert(espeak_to_arpabet("ɑ̃").empty());   // 法语鼻化
    assert(espeak_to_arpabet("tɕ").empty());  // 普通话
    assert(espeak_to_arpabet("ç").empty());   // 德语
    assert(espeak_to_arpabet("X").empty());
    assert(espeak_to_arpabet("").empty());
}

}  // namespace

int main() {
    test_roundtrip_all_arpabet();
    test_single_mapping();
    test_expansion_mapping();
    test_unmapped_fallthrough();
    return 0;
}
