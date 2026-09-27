#include "std/pron_variants_std.h"

namespace UnidictCoreStd {

std::vector<PhoneVariant> arpabet_variants(const std::string& arpabet,
                                           bool word_final) {
    // 只收"读得地道但不是教科书音"的自然语音过程/口音变体，每条都有
    // 明确的语音学依据；符号必须存在于模型词表（espeak_arpabet_std 的
    // 正向映射按词表 392 类逐一核对过，表里查得到的就在词表里）。
    //
    // 明确不收的（宁缺毋滥）：
    //  - 清浊/调音部位混淆（d↔t、s↔ʃ 等）：那是发音错误，不是变体，
    //    容忍了评分就失去纠正意义。
    //  - 词中/词首的 g→ŋ：同化只发生在词尾（final_only 锁住），
    //    "goal 读成 ŋ"必须照扣并报混淆。
    static const struct Row {
        const char* arpabet;
        bool final_only;
        std::vector<PhoneVariant> variants;
    } kTable[] = {
        // 美式闪音：butter/water 的 t、ladder 与 latter 合流（同音素
        // 实现，silent——永不报出）
        {"T", false, {{"\xC9\xBE", false}}},   // ɾ
        {"D", false, {{"\xC9\xBE", false}}},   // ɾ
        // 音节辅音：button 的 n̩、little 的 l̩（espeak 实际会发这类符号）
        {"L", false, {{"l\xCC\xA9", false}}},  // l̩
        {"N", false, {{"n\xCC\xA9", false}}},  // n̩
        // 非儿化：英音/美音弱读 -er 的 ɜ（ɚ 是主键）
        {"ER", false, {{"\xC9\x9C", false}}},  // ɜ
        // 元音松紧/长短变体（espeak 在弱读与连读里发短式）
        {"AA", false, {{"\xC9\x91", false},   // ɑ
                       {"\xC9\x92", false}}},  // ɒ
        {"AH", false, {{"\xC9\x99", false},   // ə
                       {"\xC9\x90", false}}},  // ɐ（schwa 化/央化）
        {"AO", false, {{"\xC9\x94", false},   // ɔ
                       {"\xC9\x91", false}}},  // ɑ（cot-caught 合并；ɑ
                                               //  跨域——正向映射里 ɑ→AA，
                                               //  这里是计划明示的例外）
        {"EH", false, {{"e", false}}},        // DRESS 元音高化
        {"IH", false, {{"\xE1\xB5\xBB", false}}},  // ᵻ（schwi 松弛）
        {"IY", false, {{"i", false}}},        // happy 词尾短 i
        {"OW", false, {{"o", false},          // o
                       {"o\xCB\x90", false}}},  // oː（单化）
        {"UW", false, {{"u", false}}},        // GOOSE 松弛
        // 位置感知（M7）：词尾 g→ŋ 同化（dog 的尾音，M3b 真模型实证）
        // ——final_only 锁词尾；ŋ 是 NG 域的音素级替代而非同音素实现，
        // notable：分数按 max 容忍之外还要 realized_as 报出
        {"G", true, {{"\xC5\x8B", true}}},    // ŋ
    };
    for (const auto& row : kTable) {
        if (arpabet == row.arpabet) {
            if (row.final_only && !word_final) {
                return {};
            }
            return row.variants;
        }
    }
    return {};
}

}  // namespace UnidictCoreStd
