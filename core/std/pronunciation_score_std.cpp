#include "std/pronunciation_score_std.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace UnidictCoreStd {
namespace {

constexpr double kGapCost = 0.75;
constexpr double kSameClassCost = 0.5;

// clang-format off
constexpr std::array<const char*, 15> kVowels = {
    "AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER",
    "EY", "IH", "IY", "OW", "OY", "UH", "UW",
};
constexpr std::array<const char*, 24> kConsonants = {
    "B", "CH", "D", "DH", "F", "G", "HH", "JH", "K", "L", "M", "N",
    "NG", "P", "R", "S", "SH", "T", "TH", "V", "W", "Y", "Z", "ZH",
};
// clang-format on

template <size_t N>
bool in_table(const std::array<const char*, N>& table, const std::string& p) {
    return std::find_if(table.begin(), table.end(),
                        [&p](const char* e) { return p == e; }) != table.end();
}

}  // namespace

bool is_vowel(const std::string& phoneme) {
    return in_table(kVowels, phoneme);
}

bool is_valid_phoneme(const std::string& phoneme) {
    return in_table(kVowels, phoneme) || in_table(kConsonants, phoneme);
}

double phoneme_substitution_cost(const std::string& a, const std::string& b) {
    if (a == b) {
        return 0.0;
    }
    if (!is_valid_phoneme(a) || !is_valid_phoneme(b)) {
        return 1.0;
    }
    if (is_vowel(a) == is_vowel(b)) {
        return kSameClassCost;
    }
    return 1.0;
}

std::vector<PhonemeAlignment> align_phoneme_sequences(
    const std::vector<std::string>& target,
    const std::vector<std::string>& hypo) {
    const size_t m = target.size();
    const size_t n = hypo.size();
    // D[i][j]: cheapest alignment cost of target[0..i) vs hypo[0..j)
    std::vector<std::vector<double>> d(m + 1, std::vector<double>(n + 1, 0.0));
    for (size_t i = 1; i <= m; ++i) {
        d[i][0] = d[i - 1][0] + kGapCost;  // target phones dropped
    }
    for (size_t j = 1; j <= n; ++j) {
        d[0][j] = d[0][j - 1] + kGapCost;  // extra phones inserted
    }
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            const double sub = d[i - 1][j - 1] +
                               phoneme_substitution_cost(target[i - 1], hypo[j - 1]);
            const double del = d[i - 1][j] + kGapCost;
            const double ins = d[i][j - 1] + kGapCost;
            d[i][j] = std::min({sub, del, ins});
        }
    }

    // Backtrack preferring substitution/match, then deletion, so the
    // path consumes target phones as early as possible.
    std::vector<PhonemeAlignment> path;
    path.reserve(m + n);
    size_t i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 &&
            d[i][j] == d[i - 1][j - 1] + phoneme_substitution_cost(target[i - 1], hypo[j - 1])) {
            const bool same = target[i - 1] == hypo[j - 1];
            path.push_back({same ? PhonemeAlignType::kMatch : PhonemeAlignType::kSub,
                            static_cast<int>(i - 1), static_cast<int>(j - 1)});
            --i;
            --j;
        } else if (i > 0 && d[i][j] == d[i - 1][j] + kGapCost) {
            path.push_back({PhonemeAlignType::kDel, static_cast<int>(i - 1), -1});
            --i;
        } else {
            path.push_back({PhonemeAlignType::kIns, -1, static_cast<int>(j - 1)});
            --j;
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}

double alignment_similarity(const std::vector<PhonemeAlignment>& aligns,
                            size_t target_len) {
    if (target_len == 0) {
        return 0.0;
    }
    double score = 0.0;
    for (const auto& a : aligns) {
        switch (a.type) {
            case PhonemeAlignType::kMatch: score += 1.0; break;
            case PhonemeAlignType::kSub: score += 0.5; break;
            case PhonemeAlignType::kIns: score -= 0.25; break;
            case PhonemeAlignType::kDel: break;  // missed phone: no credit
        }
    }
    return std::clamp(score / static_cast<double>(target_len), 0.0, 1.0);
}

double aggregate_word_score(const std::vector<double>& phoneme_scores) {
    if (phoneme_scores.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    double worst = 1.0;
    for (double s : phoneme_scores) {
        sum += s;
        worst = std::min(worst, s);
    }
    const double mean = sum / static_cast<double>(phoneme_scores.size());
    return std::clamp(0.7 * mean + 0.3 * worst, 0.0, 1.0);
}

}  // namespace UnidictCoreStd
