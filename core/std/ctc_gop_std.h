// CTC 强制对齐 + GOP 评分内核（M3b 纯逻辑层）。
// 输入是声学模型输出的帧级 CTC log 概率（sadda-speech
// wav2vec2-espeak-ctc：(1,T,392) logits 经 log_softmax），输出
// 目标发音每个音素的强制区间与 [0,1] 分数。对齐在模型词表域
// （espeak IPA 类索引）上做，打分接口收 ARPAbet（词典域）并在
// 内部经 espeak_arpabet_std 换算——调用方只跟 ARPAbet 打交道。
// 纯 std、零音频/推理依赖，onnxruntime 推理壳按此接口喂数据。

#ifndef UNIDICT_CTC_GOP_STD_H
#define UNIDICT_CTC_GOP_STD_H

#include <optional>
#include <string>
#include <vector>

#include "std/espeak_arpabet_std.h"
#include "std/pronunciation_score_std.h"

namespace UnidictCoreStd {

struct ForcedPhone {
    int start_frame = 0;  // 含
    int end_frame = 0;    // 不含
};

// CTC Viterbi 强制对齐：target（类索引，不含 blank）扩展成
// blank,t0,blank,t1,…,blank 的 CTC 拓扑，在 T×N 帧 log 概率上
// 找最优路径。返回每个 target 音素的激活帧区间（按 target 顺序、
// 单调不重叠；CTC 拓扑保证每个音素至少激活一帧）。
// frame_log_probs：T×N 行主序（log_softmax 后，典型值 [-30,0]）。
std::vector<ForcedPhone> ctc_force_align(
    const float* frame_log_probs, int num_frames, int num_classes,
    int blank_index, const std::vector<int>& target);

// 单音素 GOP：强制区间内 log p(target_class) 的均值取 exp 得
// [0,1]。空区间返回 0。
double phone_gop_score(const float* frame_log_probs, int num_classes,
                       const ForcedPhone& seg, int target_class_index);

struct PhoneGopResult {
    std::string arpabet;       // 词典域音素
    int start_frame = 0;
    int end_frame = 0;
    double mean_log_prob = 0;  // 区间内原始平均 log p（诊断/相对化用）
    double score = 0;          // [0,1] = exp(mean_log_prob)
};

struct WordGopResult {
    std::vector<PhoneGopResult> phones;
    double word_score = 0;  // aggregate_word_score(phones 各 score)
};

// 词级管道：帧级 log 概率 + 目标 ARPAbet 序列（词典发音）→ 每音素
// GOP + M3a 聚合。class_labels 是模型词表（索引→espeak 符号）。
// 词典音素经 arpabet_to_espeak 换算类索引；目标音素不在 ARPAbet 域
// 或词表里缺该符号时返回 nullopt（上游应先经 is_valid_phoneme
// 校验）。相邻同类音素（bookkeeper 的 KK）由 CTC 拓扑的 blank
// 状态自然分隔，各自持有独立区间与分数。
std::optional<WordGopResult> score_word(
    const float* frame_log_probs, int num_frames, int num_classes,
    int blank_index, const std::vector<std::string>& class_labels,
    const std::vector<std::string>& target_arpabet);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_CTC_GOP_STD_H
