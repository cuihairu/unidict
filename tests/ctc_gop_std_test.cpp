// CTC 强制对齐 + GOP 打分纯 std 测试：合成 log-probs 上验证
// 区间单调性、同类音素分隔、缺证低分与词级聚合、M4 变体容忍与
// M6 混淆定位门槛，不碰真模型。
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

#include "std/ctc_gop_std.h"

using namespace UnidictCoreStd;

namespace {

// 行主序构造器：每帧给各类的 log 值
std::vector<float> frames(const std::vector<std::vector<float>>& rows) {
    std::vector<float> flat;
    for (const auto& r : rows) {
        flat.insert(flat.end(), r.begin(), r.end());
    }
    return flat;
}

bool near(double a, double b, double eps = 1e-3) {
    return std::fabs(a - b) < eps;
}

// 单音素完美证据：4 帧全押在类 1 上
void test_force_align_single() {
    // blank=0, A=1, B=2
    const auto lp = frames({
        {-0.1f, -0.1f, -5.0f},
        {-0.1f, -0.1f, -5.0f},
        {-0.1f, -0.1f, -5.0f},
        {-0.1f, -0.1f, -5.0f},
    });
    const auto segs = ctc_force_align(lp.data(), 4, 3, 0, {1});
    assert(segs.size() == 1);
    assert(segs[0].start_frame == 0 && segs[0].end_frame == 4);
    // gop = exp(-0.1) ≈ 0.905
    const double gop = phone_gop_score(lp.data(), 3, segs[0], 1);
    assert(near(gop, std::exp(-0.1)));
}

// 两音素时序分离：前 3 帧 A、后 2 帧 B，区间必须单调衔接
void test_force_align_two_phones() {
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f},
        {-5.0f, -0.1f, -5.0f},
        {-5.0f, -0.1f, -5.0f},
        {-5.0f, -5.0f, -0.1f},
        {-5.0f, -5.0f, -0.1f},
    });
    const auto segs = ctc_force_align(lp.data(), 5, 3, 0, {1, 2});
    assert(segs.size() == 2);
    assert(segs[0].start_frame == 0 && segs[0].end_frame == 3);
    assert(segs[1].start_frame == 3 && segs[1].end_frame == 5);
}

// 相邻同类（bookkeeper 的 KK）：blank 状态必须把两段隔开
void test_force_align_repeated_class() {
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f},  // A
        {-5.0f, -0.1f, -5.0f},  // A
        {-0.1f, -5.0f, -5.0f},  // blank
        {-5.0f, -0.1f, -5.0f},  // A
        {-5.0f, -0.1f, -5.0f},  // A
    });
    const auto segs = ctc_force_align(lp.data(), 5, 3, 0, {1, 1});
    assert(segs.size() == 2);
    assert(segs[0].start_frame == 0 && segs[0].end_frame == 2);
    assert(segs[1].start_frame == 3 && segs[1].end_frame == 5);
    // 两段各自高分
    assert(near(phone_gop_score(lp.data(), 3, segs[0], 1), std::exp(-0.1)));
    assert(near(phone_gop_score(lp.data(), 3, segs[1], 1), std::exp(-0.1)));
}

// 缺证音素：目标 [A,B] 但音频里没有 A 的证据——A 被强排在低概率
// 帧上，gop 显著低于 B（"漏读音素得低分"的核心语义）
void test_missing_phone_scores_low() {
    const auto lp = frames({
        {-0.1f, -5.0f, -5.0f},  // blank
        {-0.1f, -5.0f, -5.0f},  // blank
        {-5.0f, -5.0f, -0.1f},  // B
        {-5.0f, -5.0f, -0.1f},  // B
    });
    const auto segs = ctc_force_align(lp.data(), 4, 3, 0, {1, 2});
    assert(segs.size() == 2);
    const double gop_a = phone_gop_score(lp.data(), 3, segs[0], 1);
    const double gop_b = phone_gop_score(lp.data(), 3, segs[1], 2);
    assert(gop_a < 0.1);
    assert(near(gop_b, std::exp(-0.1)));
}

// 边界：空目标/零帧
void test_force_align_degenerate() {
    std::vector<float> lp(3, -0.1f);
    assert(ctc_force_align(lp.data(), 0, 3, 0, {1}).empty());
    assert(ctc_force_align(lp.data(), 1, 3, 0, {}).empty());
    // 单帧单音素
    const auto one = frames({{-5.0f, -0.1f, -5.0f}});
    const auto segs = ctc_force_align(one.data(), 1, 3, 0, {1});
    assert(segs.size() == 1 && segs[0].start_frame == 0 && segs[0].end_frame == 1);
}

// score_word 端到端：butter 的词典发音 B AH T ER，合成词表 + 合成
// 证据（每音素 2 帧），验区间/分数/聚合全链。T 的类用主键 "t"；
// 闪音 ɾ 变体的行为见 test_score_word_variant_flap
void test_score_word_butter() {
    // 词表：blank=0, b=1, ʌ=2, t=3, ɚ=4（espeak 符号域）
    const std::vector<std::string> labels = {"<pad>", "b", "ʌ", "t", "ɚ"};
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},  // b
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},  // ʌ
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -0.1f, -5.0f},  // t
        {-5.0f, -5.0f, -5.0f, -0.1f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},  // ɚ
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},
    });
    const auto result =
        score_word(lp.data(), 8, 5, 0, labels, {"B", "AH", "T", "ER"});
    assert(result.has_value());
    assert(result->phones.size() == 4);
    const std::vector<std::string> expect = {"B", "AH", "T", "ER"};
    for (size_t i = 0; i < 4; ++i) {
        assert(result->phones[i].arpabet == expect[i]);
        assert(result->phones[i].start_frame == 2 * static_cast<int>(i));
        assert(result->phones[i].end_frame == 2 * static_cast<int>(i) + 2);
        assert(near(result->phones[i].score, std::exp(-0.1)));
        assert(near(result->phones[i].mean_log_prob, -0.1));
    }
    // word_score = 0.7*mean + 0.3*min，四音素同分 → 就是该分本身
    assert(near(result->word_score, std::exp(-0.1)));
}

// score_word：一个音素被读歪（T 位置证据指向别的类）→ 该音素低分
// 拖垮聚合（0.3*min 项生效）
void test_score_word_mangled_phone() {
    const std::vector<std::string> labels = {"<pad>", "b", "ʌ", "t", "ɚ"};
    // T 槽位主键保留弱证据 -2（真模型里主键从不完全无证——若全帧
    // -5，对齐落点退化为平局任意选，T 可能被排到 ʌ 帧上，混淆跟着
    // 报错对象；flap 测试先踩过这个坑）
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},  // b 高
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},  // ʌ 高
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -2.0f, -0.1f},  // T 槽位证据是 ɚ（读歪）
        {-5.0f, -5.0f, -5.0f, -2.0f, -0.1f},
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},  // ɚ 高
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},
    });
    const auto result =
        score_word(lp.data(), 8, 5, 0, labels, {"B", "AH", "T", "ER"});
    assert(result.has_value());
    assert(result->phones.size() == 4);
    assert(near(result->phones[2].score, std::exp(-2.0)));  // T 弱证据记分
    assert(result->phones[0].score > 0.8 && result->phones[1].score > 0.8);
    assert(result->phones[3].score > 0.8);
    // 混淆定位（M6）：T 区间证据明显指向 ɚ 类 → 报"发成了 ER"；
    // 其余音素主键证据自身最强，无混淆
    assert(result->phones[2].confused_with == "ER");
    assert(result->phones[0].confused_with.empty());
    assert(result->phones[1].confused_with.empty());
    assert(result->phones[3].confused_with.empty());
    // min 拖底：word_score 低于三好一坏均分的 0.7 倍再加 min 项
    const double bad = result->phones[2].score;
    const double good = result->phones[0].score;
    const double expect = 0.7 * (3 * good + bad) / 4.0 + 0.3 * bad;
    assert(near(result->word_score, expect));
    assert(result->word_score < result->phones[0].score);
}

void test_score_word_invalid_inputs() {
    const std::vector<std::string> labels = {"<pad>", "b", "ʌ", "t", "ɚ"};
    const auto lp = frames({{-5.0f, -0.1f, -5.0f, -5.0f, -5.0f}});
    // 非法 ARPAbet
    assert(!score_word(lp.data(), 1, 5, 0, labels, {"X"}).has_value());
    // 词表缺 espeak 符号（删掉 b）
    const std::vector<std::string> no_b = {"<pad>", "ʌ", "ɾ", "ɚ"};
    assert(!score_word(lp.data(), 1, 5, 0, no_b, {"B"}).has_value());
    // 空目标：空结果、0 分
    const auto empty = score_word(lp.data(), 1, 5, 0, labels, {});
    assert(empty.has_value() && empty->phones.empty());
    assert(empty->word_score == 0.0);
}

// M4 变体容忍：butter 的 t 读成闪音 ɾ——对齐仍钉在主键 t 类上，
// 打分取 max(t, ɾ)，区间内 ɾ 证据更足时 T 得高分（地道美音不扣分）
void test_score_word_variant_flap() {
    // 词表：blank=0, b=1, ʌ=2, t=3, ɚ=4, ɾ=5。主键 t 在闪音位置保留
    // 弱证据（-2，真模型里主键从不完全无证）——若全帧无证，对齐位置
    // 退化成平局任意选，变体救援无从谈起（测试先踩了这个坑）
    const std::vector<std::string> labels = {"<pad>", "b", "ʌ", "t", "ɚ",
                                             "\xC9\xBE"};
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f, -5.0f},  // b
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f, -5.0f},  // ʌ
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -2.0f, -5.0f, -0.1f},  // t 弱、ɾ（变体）强
        {-5.0f, -5.0f, -5.0f, -2.0f, -5.0f, -0.1f},
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f, -5.0f},  // ɚ
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f, -5.0f},
    });
    const auto result =
        score_word(lp.data(), 8, 6, 0, labels, {"B", "AH", "T", "ER"});
    assert(result.has_value());
    assert(result->phones.size() == 4);
    // T 的区间仍由主键类对齐出来（帧 4-5），分数取变体 ɾ 的证据
    assert(result->phones[2].start_frame == 4 &&
           result->phones[2].end_frame == 6);
    assert(near(result->phones[2].score, std::exp(-0.1)));
    assert(near(result->phones[2].mean_log_prob, -0.1));
    assert(result->phones[2].score > 0.9);
    // 变体证据也是记分证据：ɾ 是 argmax 但均值恰等于 best_mean，
    // 过不了 margin——地道闪音不报"发成了 ɾ"
    assert(result->phones[2].confused_with.empty());
    // 其余音素不受影响，词分被抬回高位
    assert(result->phones[0].score > 0.9 && result->phones[1].score > 0.9 &&
           result->phones[3].score > 0.9);
    assert(result->word_score > 0.9);

    // 对照组：同一时序、词表裁掉 ɾ（变体静默跳过）→ T 退化回主键
    // 打分，按弱证据 -2 扣（这才是"纯 t 类视角"该有的分数）
    const std::vector<std::string> no_flap = {"<pad>", "b", "ʌ", "t", "ɚ"};
    const auto lp_strict = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -0.1f, -5.0f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -2.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -2.0f, -5.0f},
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},
        {-5.0f, -5.0f, -5.0f, -5.0f, -0.1f},
    });
    const auto strict =
        score_word(lp_strict.data(), 8, 5, 0, no_flap, {"B", "AH", "T", "ER"});
    assert(strict.has_value());
    assert(strict->phones[2].start_frame == 4 &&
           strict->phones[2].end_frame == 6);
    assert(near(strict->phones[2].score, std::exp(-2.0)));
    assert(strict->phones[2].score < 0.2);
    assert(strict->word_score < result->word_score);
}

// 变体取 max 的对称性：主键证据更足时按主键记（容忍不许反向抬高
// 别的音），同时确认 mean_log_prob 记的是实际记分的那份证据
void test_score_word_variant_max_prefers_primary() {
    const std::vector<std::string> labels = {"<pad>", "ʌ", "t",
                                             "\xC9\xBE"};
    // T 区间：t 类 -1.0、ɾ 类 -0.5 → max 取 ɾ（-0.5）
    const auto lp = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f},  // ʌ
        {-5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -1.0f, -0.5f},  // ɾ 更强
        {-5.0f, -5.0f, -1.0f, -0.5f},
    });
    const auto result = score_word(lp.data(), 4, 4, 0, labels, {"AH", "T"});
    assert(result.has_value());
    assert(near(result->phones[1].mean_log_prob, -0.5));
    assert(near(result->phones[1].score, std::exp(-0.5)));

    // 反转：t 类 -0.5、ɾ 类 -1.0 → max 回到主键
    const auto lp2 = frames({
        {-5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -0.1f, -5.0f, -5.0f},
        {-5.0f, -5.0f, -0.5f, -1.0f},  // t 更强
        {-5.0f, -5.0f, -0.5f, -1.0f},
    });
    const auto result2 = score_word(lp2.data(), 4, 4, 0, labels, {"AH", "T"});
    assert(result2.has_value());
    assert(near(result2->phones[1].mean_log_prob, -0.5));
    assert(near(result2->phones[1].score, std::exp(-0.5)));
}

// M6 混淆定位的门槛矩阵：自由变体（映射回同一 ARPAbet）、合写
// 展开、词表外多语符号、margin 不足四种"不报"路径 + 合写正例。
// 单音素两帧是最小可构造场景；主键统一给 -2 弱证据（对齐可跑），
// 干扰类给 -0.1 强证据（margin 1.9 远超 0.7 门槛）。espeak 符号
// 一律原生 UTF-8 字面量（\x 转义贪婪续读的教训见 ipa 测试头注）
void test_score_word_confusion_gates() {
    // 自由变体：ASCII g 对主键 ɡ 都映射 G，且 g 不在容忍表里——
    // argmax 是 g、margin 足够，但"发成了同一个音"不是混淆
    {
        const std::vector<std::string> labels = {"<pad>", "ɡ", "g"};
        const auto lp = frames({
            {-5.0f, -2.0f, -0.1f},
            {-5.0f, -2.0f, -0.1f},
        });
        const auto r = score_word(lp.data(), 2, 3, 0, labels, {"G"});
        assert(r.has_value());
        assert(near(r->phones[0].score, std::exp(-2.0)));
        assert(r->phones[0].confused_with.empty());
    }
    // 合写正例：AA 区间证据是 ɑːɹ（r-色合写）→ 展开成 "AA R" 报出
    {
        const std::vector<std::string> labels = {"<pad>", "ɑː", "ɑːɹ"};
        const auto lp = frames({
            {-5.0f, -2.0f, -0.1f},
            {-5.0f, -2.0f, -0.1f},
        });
        const auto r = score_word(lp.data(), 2, 3, 0, labels, {"AA"});
        assert(r.has_value());
        assert(r->phones[0].confused_with == "AA R");
    }
    // 词表外多语符号（¥ 不在映射表）：argmax 赢了 margin 也映射不回
    // ARPAbet——不报（报一个用户看不懂的符号没有意义）
    {
        const std::vector<std::string> labels = {"<pad>", "ʌ", "¥"};
        const auto lp = frames({
            {-5.0f, -2.0f, -0.1f},
            {-5.0f, -2.0f, -0.1f},
        });
        const auto r = score_word(lp.data(), 2, 3, 0, labels, {"AH"});
        assert(r.has_value());
        assert(r->phones[0].confused_with.empty());
    }
    // margin 不足：ɚ 只比 t 领先 0.3（< 0.7）——高分音素的相邻类
    // 抖动就是这个量级，不该被报成混淆
    {
        const std::vector<std::string> labels = {"<pad>", "t", "ɚ"};
        const auto lp = frames({
            {-5.0f, -0.8f, -0.5f},
            {-5.0f, -0.8f, -0.5f},
        });
        const auto r = score_word(lp.data(), 2, 3, 0, labels, {"T"});
        assert(r.has_value());
        assert(r->phones[0].confused_with.empty());
    }
}

}  // namespace

int main() {
    test_force_align_single();
    test_force_align_two_phones();
    test_force_align_repeated_class();
    test_missing_phone_scores_low();
    test_force_align_degenerate();
    test_score_word_butter();
    test_score_word_mangled_phone();
    test_score_word_invalid_inputs();
    test_score_word_variant_flap();
    test_score_word_variant_max_prefers_primary();
    test_score_word_confusion_gates();
    return 0;
}
