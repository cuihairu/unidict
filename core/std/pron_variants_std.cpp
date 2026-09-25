#include "std/pron_variants_std.h"

namespace UnidictCoreStd {

std::vector<std::string> arpabet_variants(const std::string& arpabet) {
    // 只收"读得地道但不是教科书音"的自然语音过程/口音变体，每条都有
    // 明确的语音学依据；符号必须存在于模型词表（espeak_arpabet_std 的
    // 正向映射按词表 392 类逐一核对过，表里查得到的就在词表里）。
    //
    // 明确不收的（宁缺毋滥）：
    //  - G→ŋ（dog 尾音同化，M3b 实证）：真实过程，但只发生在词尾；
    //    位置盲表会连"goal 读成 ŋ"这种真错误一起原谅，等位置感知
    //    变体（is_final 之类）进 M4 实测再收。
    //  - 清浊/调音部位混淆（d↔t、s↔ʃ 等）：那是发音错误，不是变体，
    //    容忍了评分就失去纠正意义。
    static const struct {
        const char* arpabet;
        std::vector<std::string> variants;
    } kTable[] = {
        // 美式闪音：butter/water 的 t、ladder 与 latter 合流
        {"T", {"\xC9\xBE"}},  // ɾ
        {"D", {"\xC9\xBE"}},  // ɾ
        // 音节辅音：button 的 n̩、little 的 l̩（espeak 实际会发这类符号）
        {"L", {"l\xCC\xA9"}},  // l̩
        {"N", {"n\xCC\xA9"}},  // n̩
        // 非儿化：英音/美音弱读 -er 的 ɜ（ɚ 是主键）
        {"ER", {"\xC9\x9C"}},  // ɜ
        // 元音松紧/长短变体（espeak 在弱读与连读里发短式）
        {"AA", {"\xC9\x91", "\xC9\x92"}},    // ɑ, ɒ
        {"AH", {"\xC9\x99", "\xC9\x90"}},    // ə, ɐ（schwa 化/央化）
        {"AO", {"\xC9\x94", "\xC9\x91"}},    // ɔ, ɑ（cot-caught 合并；ɑ
                                             //  跨域——正向映射里 ɑ→AA，
                                             //  这里是计划明示的例外）
        {"EH", {"e"}},                       // DRESS 元音高化
        {"IH", {"\xE1\xB5\xBB"}},            // ᵻ（schwi 松弛）
        {"IY", {"i"}},                       // happy 词尾短 i
        {"OW", {"o", "o\xCB\x90"}},          // o, oː（单化）
        {"UW", {"u"}},                       // GOOSE 松弛
    };
    for (const auto& row : kTable) {
        if (arpabet == row.arpabet) {
            return row.variants;
        }
    }
    return {};
}

}  // namespace UnidictCoreStd
