#include "std/ctc_gop_std.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "std/pron_variants_std.h"

namespace UnidictCoreStd {
namespace {

// log 域的"负无穷"：不能直接用 -inf（-inf + 有限数仍是 -inf 没问题，
// 但有的编译器/优化下 max 归并路径里出现 NaN 不好查），用大负数
constexpr double kNegInf = -1e30;

// 混淆定位门槛（log 域）：最强非容忍类须压过记分证据这么多才报
// "发成了那个音"。≈×2 概率——高分音素的相邻类抖动（真模型里相邻
// 音素类差常在 0.1-0.3）过不了，读歪时替代类通常领先 2 个 nat 以上
constexpr double kConfusionMarginLog = 0.7;

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
    // GCOVR_EXCL_START：段区间来自 CTC Viterbi 回溯，路径必须访问每个音素
    // 状态，所以每个音素至少分到 1 帧，n 恒 >= 1。空区间无法构造。
    if (n <= 0) {  // GCOVR_EXCL_LINE
        return 0.0;  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_STOP
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
        // GCOVR_EXCL_START：is_valid_phoneme 认可的 39 个音素在
        // arpabet_to_espeak 里都有映射（test_espeak_arpabet_std 对全表做了
        // 正向/反向核对），走到这里 espeak 不会为空。留作映射表日后出现
        // 空洞时的兜底。
        if (espeak.empty()) {  // GCOVR_EXCL_LINE
            return std::nullopt;  // GCOVR_EXCL_LINE
        }  // GCOVR_EXCL_STOP
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
        // GOP 取 max(主键, 容忍变体)（M4 变体容忍）：对齐仍钉在主键
        // 类上（定位语义不变），打分时若区间内变体证据更足则按变体记
        // ——butter 的 t 读成闪音 ɾ 不该被扣分。变体符号不在词表里时
        // 静默跳过（词表裁剪场景），全缺则退化回纯主键 GOP。
        std::vector<int> candidates = {classes[k]};
        for (const std::string& v : arpabet_variants(p.arpabet)) {
            const auto it = std::find(class_labels.begin(), class_labels.end(), v);
            if (it != class_labels.end()) {
                candidates.push_back(static_cast<int>(it - class_labels.begin()));
            }
        }
        double best_mean = kNegInf;
        if (n > 0) {
            for (int c : candidates) {
                double sum = 0.0;
                for (int t = seg.start_frame; t < seg.end_frame; ++t) {
                    sum += logp_at(frame_log_probs, num_classes, t, c);
                }
                best_mean = std::max(best_mean, sum / n);
            }
            // 混淆定位（M6）：同一区间逐类平均证据取 argmax（排除
            // blank——静音不是"发成了什么"）。主键/容忍变体的均值
            // ≤ best_mean，天然过不了 margin 门槛，无需显式排除
            int argmax_class = -1;
            double best_other = kNegInf;
            for (int c = 0; c < num_classes; ++c) {
                if (c == blank_index) {
                    continue;
                }
                double sum = 0.0;
                for (int t = seg.start_frame; t < seg.end_frame; ++t) {
                    sum += logp_at(frame_log_probs, num_classes, t, c);
                }
                const double mean = sum / n;
                if (mean > best_other) {
                    best_other = mean;
                    argmax_class = c;
                }
            }
            // argmax_class == -1 只在词表除 blank 外无类时发生，此时
            // best_other 仍是 kNegInf，门槛必不通过；显式判空防未初始化
            if (argmax_class >= 0 &&
                best_other > best_mean + kConfusionMarginLog) {
                // 映射回 ARPAbet 才对用户有意义：词表里的多语符号
                // （法/德元音等）映射为空不报；合写（ɑːɹ）展开后
                // 空格连接。映射回目标自身的自由变体（ASCII g 对
                // G 的主键 ɡ）不是混淆
                std::string joined;
                for (const std::string& a :
                     espeak_to_arpabet(class_labels[static_cast<size_t>(
                                           argmax_class)])) {
                    if (!joined.empty()) {
                        joined += ' ';
                    }
                    joined += a;
                }
                if (!joined.empty() && joined != p.arpabet) {
                    p.confused_with = joined;
                }
            }
        }
        p.mean_log_prob = n > 0 ? best_mean : kNegInf;
        p.score = n > 0 ? std::clamp(std::exp(p.mean_log_prob), 0.0, 1.0) : 0.0;
        scores.push_back(p.score);
        result.phones.push_back(std::move(p));
    }
    result.word_score = aggregate_word_score(scores);
    return result;
}

}  // namespace UnidictCoreStd
