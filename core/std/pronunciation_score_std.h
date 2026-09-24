// Pronunciation scoring kernel (std-only): ARPAbet phoneme table,
// Needleman-Wunsch sequence alignment and word-score aggregation.
// Pure logic for docs/pronunciation-plan.md M3 — the sherpa-onnx
// adapter (M3b) feeds per-phoneme GOP scores into aggregate_word_score;
// nothing here touches audio devices or model runtimes, so it all goes
// through std tests.

#ifndef UNIDICT_PRONUNCIATION_SCORE_STD_H
#define UNIDICT_PRONUNCIATION_SCORE_STD_H

#include <cstddef>
#include <string>
#include <vector>

namespace UnidictCoreStd {

// ---- ARPAbet (CMU 39 phones) ----
// The common domain for dictionary pronunciations and ASR phoneme
// output; uppercase-only by convention (both sides emit uppercase).

// 15 vowels: AA AE AH AO AW AY EH ER EY IH IY OW OY UH UW
bool is_vowel(const std::string& phoneme);

// Uppercase ARPAbet membership over the full 39-phone inventory
bool is_valid_phoneme(const std::string& phoneme);

// ---- Alignment ----

enum class PhonemeAlignType { kMatch, kSub, kDel, kIns };

struct PhonemeAlignment {
    PhonemeAlignType type;
    int target_index = -1;  // -1 for kIns (extra phone the user added)
    int hypo_index = -1;    // -1 for kDel (phone the user dropped)
};

// Substitution cost in [0,1]: identical 0; same class (vowel/vowel or
// consonant/consonant, e.g. AH vs ER) 0.5; cross-class or invalid 1.
double phoneme_substitution_cost(const std::string& a, const std::string& b);

// Global alignment (Needleman-Wunsch) of the target (dictionary)
// pronunciation against the hypothesis (recognized) sequence.
// Gap cost 0.75: dropping a phone costs more than fumbling one within
// its class, less than replacing it across classes. The returned path
// is ordered by target position; every target phone appears exactly
// once (as kMatch/kSub/kDel).
std::vector<PhonemeAlignment> align_phoneme_sequences(
    const std::vector<std::string>& target,
    const std::vector<std::string>& hypo);

// ---- Scoring ----

// Alignment-only word similarity in [0,1]:
// (n_match + 0.5*n_sub - 0.25*n_ins) / target_len, clamped.
// Extra insertions are mildly penalized but can never drag the score
// below 0 — they pollute less than a missed phone.
double alignment_similarity(const std::vector<PhonemeAlignment>& aligns,
                            size_t target_len);

// Aggregate per-phone GOP scores (M3b adapter output, each in [0,1])
// into one word score in [0,1]: 0.7*mean + 0.3*min. The mean gives the
// overall picture, the worst phone keeps a single badly mangled sound
// from hiding behind the rest. Empty input scores 0.
double aggregate_word_score(const std::vector<double>& phoneme_scores);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_PRONUNCIATION_SCORE_STD_H
