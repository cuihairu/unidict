#include "std/ctc_gop_std.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace UnidictCoreStd {
namespace {

// log 域的"负无穷"：不能直接用 -inf（-inf + 有限数仍是 -inf 没问题，
// 但有的编译器/优化下 max 归并路径里出现 NaN 不好查），用大负数
constexpr double kNegInf = -1e30;

inline double logp_at(const float* frame_log_probs, int num_classes, int frame,
                      int class_index) {
    return static_cast<double>(frame_log_probs[frame * num_classes + class_index]);
}

}  // namespace

std::vector<ForcedPhone> ctc_force_align(const float* frame_log_probs,
                                         int num_frames, int num_classes,
                                         int blank_index,
                                         const std::vector<int>& target) {
    if (num_frames <= 0 || target.empty()) {
        return {};
    }
    // CTC 拓扑：扩展序列 [blank, t0, blank, t1, ..., tL-1, blank]
    const int len = static_cast<int>(target.size());
    const int states = 2 * len + 1;
    std::vector<int> ext(states);
    for (int s = 0; s < states; ++s) {
        ext[s] = (s % 2 == 0) ? blank_index : target[s / 2];
    }

    // dp[t][s]：前 t+1 帧、停在扩展状态 s 的最优 log 概率。
    // 全表保留用于回溯；1500 帧 × 几十状态量级，内存无压力。
    std::vector<std::vector<double>> dp(num_frames,
                                        std::vector<double>(states, kNegInf));
    dp[0][0] = logp_at(frame_log_probs, num_classes, 0, ext[0]);
    if (states > 1) {
        dp[0][1] = logp_at(frame_log_probs, num_classes, 0, ext[1]);
    }
    for (int t = 1; t < num_frames; ++t) {
        for (int s = 0; s < states; ++s) {
            double best = dp[t - 1][s];  // 驻留
            if (s >= 1) {
                best = std::max(best, dp[t - 1][s - 1]);  // 前进一步
            }
            // 跳两步：仅当越过的是 blank（CTC 重复音素必须隔 blank）
            if (s >= 2 && ext[s] != ext[s - 2]) {
                best = std::max(best, dp[t - 1][s - 2]);
            }
            if (best > kNegInf / 2) {
                dp[t][s] = best + logp_at(frame_log_probs, num_classes, t, ext[s]);
            }
        }
    }

    // 路径必须结束在最后 blank 或最后音素状态；并列时偏向最后
    // 音素——证据均匀（合成/静音边界）时区间应吃满帧而不是让
    // 尾帧飘进 blank
    int s = (dp[num_frames - 1][states - 2] >= dp[num_frames - 1][states - 1])
                ? states - 2
                : states - 1;

    // 回溯：每帧所处状态。blank 状态不占 target 音素。
    std::vector<int> state_per_frame(num_frames);
    state_per_frame[num_frames - 1] = s;
    for (int t = num_frames - 2; t >= 0; --t) {
        int prev = s;  // 驻留
        if (s >= 1 && dp[t][s - 1] > dp[t][prev] + 1e-9) {
            prev = s - 1;
        }
        if (s >= 2 && ext[s] != ext[s - 2] && dp[t][s - 2] > dp[t][prev] + 1e-9) {
            prev = s - 2;
        }
        s = prev;
        state_per_frame[t] = s;
    }

    // 每个音素状态 s=2k+1 的激活帧是连续区间（拓扑单调，离开不回头）
    std::vector<ForcedPhone> out(len);
    for (int k = 0; k < len; ++k) {
        out[k].start_frame = -1;
        out[k].end_frame = -1;
    }
    for (int t = 0; t < num_frames; ++t) {
        const int st = state_per_frame[t];
        if (st % 2 == 1) {
            const int k = (st - 1) / 2;
            if (out[k].start_frame < 0) {
                out[k].start_frame = t;
            }
            out[k].end_frame = t + 1;
        }
    }
    return out;
}

double phone_gop_score(const float* frame_log_probs, int num_classes,
                       const ForcedPhone& seg, int target_class_index) {
    const int n = seg.end_frame - seg.start_frame;
    if (n <= 0) {
        return 0.0;
    }
    double sum = 0.0;
    for (int t = seg.start_frame; t < seg.end_frame; ++t) {
        sum += logp_at(frame_log_probs, num_classes, t, target_class_index);
    }
    const double mean = sum / n;
    return std::clamp(std::exp(mean), 0.0, 1.0);
}

std::optional<WordGopResult> score_word(const float* frame_log_probs,
                                        int num_frames, int num_classes,
                                        int blank_index,
                                        const std::vector<std::string>& class_labels,
                                        const std::vector<std::string>& target_arpabet) {
    if (target_arpabet.empty()) {
        return WordGopResult{};
    }
    // 词典域 → 模型类索引。相邻同类音素（KK）保持一一对应，
    // CTC 扩展序列里的 blank 状态自然把它们隔开
    std::vector<int> classes;
    classes.reserve(target_arpabet.size());
    for (const auto& phone : target_arpabet) {
        if (!is_valid_phoneme(phone)) {
            return std::nullopt;
        }
        const std::string espeak = arpabet_to_espeak(phone);
        if (espeak.empty()) {
            return std::nullopt;
        }
        const auto it = std::find(class_labels.begin(), class_labels.end(), espeak);
        if (it == class_labels.end()) {
            return std::nullopt;
        }
        classes.push_back(static_cast<int>(it - class_labels.begin()));
    }

    const auto segs =
        ctc_force_align(frame_log_probs, num_frames, num_classes, blank_index, classes);

    WordGopResult result;
    result.phones.reserve(classes.size());
    std::vector<double> scores;
    scores.reserve(classes.size());
    for (size_t k = 0; k < classes.size(); ++k) {
        const ForcedPhone& seg = segs.empty() ? ForcedPhone{} : segs[k];
        PhoneGopResult p;
        p.arpabet = target_arpabet[k];
        p.start_frame = seg.start_frame;
        p.end_frame = seg.end_frame;
        const int n = seg.end_frame - seg.start_frame;
        double sum = 0.0;
        for (int t = seg.start_frame; t < seg.end_frame; ++t) {
            sum += logp_at(frame_log_probs, num_classes, t, classes[k]);
        }
        p.mean_log_prob = n > 0 ? sum / n : kNegInf;
        p.score = n > 0 ? std::clamp(std::exp(p.mean_log_prob), 0.0, 1.0) : 0.0;
        scores.push_back(p.score);
        result.phones.push_back(std::move(p));
    }
    result.word_score = aggregate_word_score(scores);
    return result;
}

}  // namespace UnidictCoreStd
