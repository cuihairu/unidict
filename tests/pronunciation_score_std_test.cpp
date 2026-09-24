// 发音评分内核（ARPAbet 表 / NW 对齐 / 词分聚合）纯 std 测试——
// docs/pronunciation-plan.md M3a；sherpa-onnx 适配器（M3b）按这里的
// 接口喂数据，所以接口语义必须先被钉死。
#include <cassert>
#include <string>
#include <vector>

#include "std/pronunciation_score_std.h"

using namespace UnidictCoreStd;

namespace {

std::vector<std::string> P(std::initializer_list<const char*> phones) {
    return std::vector<std::string>(phones.begin(), phones.end());
}

void test_phoneme_tables() {
    assert(is_valid_phoneme("AA") && is_valid_phoneme("B") && is_valid_phoneme("NG"));
    assert(!is_valid_phoneme("aa"));  // 大写约定，小写不算
    assert(!is_valid_phoneme("X") && !is_valid_phoneme(""));

    for (const char* v : {"AA", "AE", "AH", "AO", "AW", "AY", "EH", "ER",
                          "EY", "IH", "IY", "OW", "OY", "UH", "UW"}) {
        assert(is_vowel(v));
    }
    for (const char* c : {"B", "CH", "D", "DH", "F", "G", "HH", "JH", "K",
                          "L", "M", "N", "NG", "P", "R", "S", "SH", "T",
                          "TH", "V", "W", "Y", "Z", "ZH"}) {
        assert(!is_vowel(c));
    }
    assert(!is_vowel("XX"));
}

void test_substitution_cost() {
    assert(phoneme_substitution_cost("AA", "AA") == 0.0);
    assert(phoneme_substitution_cost("AH", "ER") == 0.5);  // 元音-元音
    assert(phoneme_substitution_cost("B", "P") == 0.5);    // 辅音-辅音
    assert(phoneme_substitution_cost("AA", "B") == 1.0);   // 跨类
    assert(phoneme_substitution_cost("AA", "XX") == 1.0);  // 非法
}

void test_align_identical() {
    const auto t = P({"HH", "AH", "L", "OW"});  // hello
    const auto aligns = align_phoneme_sequences(t, t);
    assert(aligns.size() == 4);
    for (size_t i = 0; i < aligns.size(); ++i) {
        assert(aligns[i].type == PhonemeAlignType::kMatch);
        assert(aligns[i].target_index == static_cast<int>(i));
        assert(aligns[i].hypo_index == static_cast<int>(i));
    }
    assert(alignment_similarity(aligns, t.size()) == 1.0);
}

void test_align_substitution() {
    // hello 的 AH 读成 ER（元音间混淆，真实场景高频）
    const auto aligns = align_phoneme_sequences(P({"HH", "AH", "L", "OW"}),
                                                P({"HH", "ER", "L", "OW"}));
    assert(aligns.size() == 4);
    assert(aligns[1].type == PhonemeAlignType::kSub);
    assert(aligns[1].target_index == 1 && aligns[1].hypo_index == 1);
    // (3 match + 0.5 sub) / 4
    const double sim = alignment_similarity(aligns, 4);
    assert(sim > 0.87 && sim < 0.88);
}

void test_align_deletion() {
    const auto aligns = align_phoneme_sequences(P({"AA", "B", "K"}), P({"AA", "K"}));
    // B 被吞掉：match(AA) del(B) match(K)
    int n_match = 0, n_del = 0;
    for (const auto& a : aligns) {
        if (a.type == PhonemeAlignType::kMatch) ++n_match;
        if (a.type == PhonemeAlignType::kDel) ++n_del;
    }
    assert(n_match == 2 && n_del == 1);
    assert(n_del > 0 && aligns.size() == 3);
    // 2/3
    const double sim = alignment_similarity(aligns, 3);
    assert(sim > 0.66 && sim < 0.67);
}

void test_align_insertion() {
    const auto aligns = align_phoneme_sequences(P({"B"}), P({"B", "AA"}));
    assert(aligns.size() == 2);
    assert(aligns[0].type == PhonemeAlignType::kMatch);
    assert(aligns[1].type == PhonemeAlignType::kIns);
    assert(aligns[1].target_index == -1 && aligns[1].hypo_index == 1);
    // (1 - 0.25) / 1 = 0.75
    const double sim = alignment_similarity(aligns, 1);
    assert(sim > 0.74 && sim < 0.76);
}

void test_align_empty_and_paths_monotonic() {
    const auto dels = align_phoneme_sequences(P({"AA", "B"}), {});
    assert(dels.size() == 2);
    for (const auto& a : dels) {
        assert(a.type == PhonemeAlignType::kDel && a.hypo_index == -1);
    }
    assert(alignment_similarity(dels, 2) == 0.0);
    // 空 target：全插入，clamp 到 0
    const auto ins = align_phoneme_sequences({}, P({"AA"}));
    assert(alignment_similarity(ins, 0) == 0.0);
    // 双方皆空 → 空路径
    assert(align_phoneme_sequences({}, {}).empty());

    // 路径按 target 单调（乱序输入也能对齐出正确结构）
    const auto mixed = align_phoneme_sequences(P({"K", "AE", "T"}), P({"K", "T"}));
    int prev = -1;
    for (const auto& a : mixed) {
        if (a.type != PhonemeAlignType::kIns) {
            assert(a.target_index > prev);
            prev = a.target_index;
        }
    }
}

void test_alignment_similarity_clamp() {
    // 巨量插入不能把分数拖成负
    const auto aligns = align_phoneme_sequences(P({"B"}), P({"B", "AA", "AE", "AH"}));
    assert(alignment_similarity(aligns, 1) >= 0.0);
}

void test_aggregate_word_score() {
    assert(aggregate_word_score({}) == 0.0);
    assert(aggregate_word_score({1.0, 1.0, 1.0}) == 1.0);
    assert(aggregate_word_score({0.5}) == 0.5);
    // 0.7*mean + 0.3*min：{1, 0, 1} → 0.7*(2/3) + 0.3*0 = 0.7*(2/3)
    const double s = aggregate_word_score({1.0, 0.0, 1.0});
    assert(s > 0.46 && s < 0.47);
    // 最差音素压分：同样的均值，含一个坏分的词必须低于全好词
    const double with_bad = aggregate_word_score({0.9, 0.9, 0.9, 0.2});
    const double all_good = aggregate_word_score({0.9, 0.9, 0.9, 0.9});
    assert(with_bad < all_good);
    // 0.7*(2.9/4) + 0.3*0.2 = 0.5675：坏分既压 mean 也直接进 min 项
    assert(with_bad > 0.5674 && with_bad < 0.5676);
}

}  // namespace

int main() {
    test_phoneme_tables();
    test_substitution_cost();
    test_align_identical();
    test_align_substitution();
    test_align_deletion();
    test_align_insertion();
    test_align_empty_and_paths_monotonic();
    test_alignment_similarity_clamp();
    test_aggregate_word_score();
    return 0;
}
