#include "std/espeak_arpabet_std.h"

#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace UnidictCoreStd {
namespace {

struct Entry {
    const char* espeak;
    const char* arpabet;  // 空格分隔：一个 espeak 符号可展开成多个 ARPAbet
};

// clang-format off
// espeak en 输出域 → CMU ARPAbet。覆盖依据模型词表（sadda-speech
// wav2vec2-espeak-ctc vocab.json 392 类）里英语实际用到的符号：
// - 美式 flap ɾ → T（"butter" espeak=b ʌ ɾ ɚ / CMU=B AH1 T ER0）
// - schwa ə/ɐ → AH、弱化 i/iː → IY、弱化 u → UW（CMU 惯例就近）
// - r-色合写 1→2 展开（ɑːɹ → AA R，对应 CMU "car"=K AA1 R）
// - 音节辅音预组合键 n̩/l̩/r̩（词表里就是 n+U+0329 一个键）
// - əl（音节 l 的合写）→ AH L
// - 词表里法/德/中 X 对（ã œ ç tɕ…）一律不收录，走空结果兜底
constexpr Entry kTable[] = {
    {"ə", "AH"},   {"ɐ", "AH"},   {"ʌ", "AH"},
    {"ɑː", "AA"},  {"ɑ", "AA"},   {"ɒ", "AA"},
    {"æ", "AE"},
    {"ɔː", "AO"},  {"ɔ", "AO"},
    {"aʊ", "AW"},  {"aɪ", "AY"},
    {"ɛ", "EH"},   {"e", "EH"},
    {"ɜː", "ER"},  {"ɜ", "ER"},   {"ɚ", "ER"},
    {"eɪ", "EY"},
    {"ɪ", "IH"},   {"ᵻ", "IH"},
    {"i", "IY"},   {"iː", "IY"},
    {"oʊ", "OW"},  {"o", "OW"},   {"oː", "OW"},
    {"ɔɪ", "OY"},
    {"ʊ", "UH"},   {"u", "UW"},   {"uː", "UW"},
    {"b", "B"},    {"tʃ", "CH"},  {"d", "D"},    {"ð", "DH"},
    {"f", "F"},    {"ɡ", "G"},    {"g", "G"},    {"h", "HH"},
    {"dʒ", "JH"},  {"k", "K"},
    {"l", "L"},    {"l̩", "L"},
    {"m", "M"},
    {"n", "N"},    {"n̩", "N"},    {"ŋ", "NG"},
    {"p", "P"},    {"ɹ", "R"},    {"r", "R"},    {"r̩", "R"},
    {"ɾ", "T"},
    {"s", "S"},    {"ʃ", "SH"},   {"t", "T"},    {"θ", "TH"},
    {"v", "V"},    {"w", "W"},    {"j", "Y"},
    {"z", "Z"},    {"ʒ", "ZH"},
    {"əl", "AH L"},
    {"ɑːɹ", "AA R"}, {"ɔːɹ", "AO R"}, {"oːɹ", "OW R"},
    {"ɛɹ", "EH R"},  {"ɪɹ", "IH R"},  {"ʊɹ", "UH R"},
    {"iə", "IY R"},
};
// clang-format on

std::vector<std::string> split_arpabet(const std::string& joined) {
    std::vector<std::string> out;
    for (size_t pos = 0; pos < joined.size();) {
        const size_t next = joined.find(' ', pos);
        if (next == std::string::npos) {
            out.emplace_back(joined.substr(pos));
            break;
        }
        out.emplace_back(joined.substr(pos, next - pos));
        pos = next + 1;
    }
    return out;
}

}  // namespace

std::vector<std::string> espeak_to_arpabet(const std::string& espeak_phone) {
    for (const auto& e : kTable) {
        if (espeak_phone == e.espeak) {
            return split_arpabet(e.arpabet);
        }
    }
    return {};
}

std::string arpabet_to_espeak(const std::string& arpabet) {
    // 反向主键：同一 ARPAbet 的多个 espeak 变体里取最典型的
    static constexpr std::array<std::pair<const char*, const char*>, 39> kReverse = {{
        {"AA", "ɑː"},  {"AE", "æ"},  {"AH", "ʌ"},  {"AO", "ɔː"}, {"AW", "aʊ"},
        {"AY", "aɪ"},  {"EH", "ɛ"},  {"ER", "ɚ"},  {"EY", "eɪ"}, {"IH", "ɪ"},
        {"IY", "iː"},  {"OW", "oʊ"}, {"OY", "ɔɪ"}, {"UH", "ʊ"},  {"UW", "uː"},
        {"B", "b"},    {"CH", "tʃ"}, {"D", "d"},   {"DH", "ð"},  {"F", "f"},
        {"G", "ɡ"},    {"HH", "h"},  {"JH", "dʒ"}, {"K", "k"},   {"L", "l"},
        {"M", "m"},    {"N", "n"},   {"NG", "ŋ"},  {"P", "p"},   {"R", "ɹ"},
        {"S", "s"},    {"SH", "ʃ"},  {"T", "t"},   {"TH", "θ"},  {"V", "v"},
        {"W", "w"},    {"Y", "j"},   {"Z", "z"},   {"ZH", "ʒ"},
    }};
    for (const auto& e : kReverse) {
        if (arpabet == e.first) {
            return e.second;
        }
    }
    return {};
}

std::optional<std::vector<std::string>> match_espeak_prefix(
    const std::string& text, const size_t pos, size_t& consumed) {
    // 表只有 ~50 个键，最长的 ɑːɹ 6 字节——线性扫一遍取最长命中，
    // 不必预排序（同名前缀如 ə 与 əl、ɔː 与 ɔːɹ 天然按长度取胜）
    size_t best_len = 0;
    std::vector<std::string> best;
    for (const auto& e : kTable) {
        const size_t len = std::strlen(e.espeak);
        if (len <= best_len || pos + len > text.size()) {
            continue;
        }
        if (text.compare(pos, len, e.espeak) == 0) {
            best_len = len;
            best = split_arpabet(e.arpabet);
        }
    }
    if (best_len == 0) {
        return std::nullopt;
    }
    consumed = best_len;
    return best;
}

}  // namespace UnidictCoreStd
