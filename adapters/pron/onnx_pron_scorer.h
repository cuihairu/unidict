// 发音评分 onnxruntime 推理壳（M3b 适配器层）。
// 分层铁律（docs/pronunciation-plan.md）：core/ 只见纯逻辑（ctc_gop_std
// 吃帧级 log 概率），重依赖 onnxruntime 锁在 adapters/pron。Pimpl 把
// ORT 头挡在 .cpp 里，消费方（gui/、工具）只跟 PCM 与 ARPAbet 打交道。
// 模型资产（model.onnx 635MB fp16 + vocab.json）不进 git，运行时由
// 调用方给路径；模型卡锁定的约定：输入 (1,N) 16kHz 零均值/单位方差
// float 波形，输出 (1,T,392) CTC logits、50 帧/秒、blank=<pad>=0。

#ifndef UNIDICT_ONNX_PRON_SCORER_H
#define UNIDICT_ONNX_PRON_SCORER_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "std/ctc_gop_std.h"
#include "std/pron_vocab_std.h"

namespace UnidictPron {

class PronScorerOnnx {
public:
    struct Config {
        std::string model_path;  // model.onnx
        std::string vocab_path;  // vocab.json（392 类 espeak IPA）
        int intra_op_threads = 1;  // 打分是单次短推理，默认不吃多核
    };

    // 加载模型与词表。失败返回 nullptr 并把原因写进 error（文件缺失、
    // 词表非法、会话创建失败）——GUI 侧据此回退到"无评分跟读"（M2）
    static std::unique_ptr<PronScorerOnnx> load(const Config& cfg,
                                                std::string& error);
    // 构造/析构都在 .cpp（Pimpl：unique_ptr<Impl> 的隐式析构要求
    // Impl 在调用点完整）
    PronScorerOnnx();
    ~PronScorerOnnx();

    PronScorerOnnx(const PronScorerOnnx&) = delete;
    PronScorerOnnx& operator=(const PronScorerOnnx&) = delete;

    // 16kHz/单声道/16bit PCM（PronAudio 采集规格）+ 目标 ARPAbet 序列
    // → 每音素 GOP + 词分。音素不在 ARPAbet 域或词表缺对应 espeak
    // 符号返回 nullopt（语义同 UnidictCoreStd::score_word）；推理失败
    // 返回 nullopt 并写 error
    std::optional<UnidictCoreStd::WordGopResult> score(
        const std::vector<int16_t>& pcm,
        const std::vector<std::string>& target_arpabet, std::string& error);

    // 帧级诊断（M3b 调优/M4 定位用）：每帧 top-1 类与 log p。与 score
    // 共用推理路径；失败返回空并写 error
    struct FrameDiag {
        int frame = 0;
        std::string best_label;  // 词表符号（越界帧给 "?"）
        double best_logp = 0;    // log_softmax 后
    };
    std::vector<FrameDiag> diagnose(const std::vector<int16_t>& pcm,
                                    std::string& error);

    const UnidictCoreStd::PronVocab& vocab() const;

    // 模型输出帧率（模型卡：CNN 前端 320 倍降采样 @16kHz = 50fps）。
    // M4 音素定位的帧→毫秒换算用
    static constexpr int kFramesPerSecond = 50;

private:
    // 推理 + 后处理共用路径：PCM → 模型域 → (T,C) log_softmax。失败
    // 返回 false 并写 error；成功时 frames/classes/log_probs 就绪
    // （行主序，每帧 C 类）
    bool run_inference(const std::vector<int16_t>& pcm, int& frames,
                       int& classes, std::vector<float>& log_probs,
                       std::string& error);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace UnidictPron

#endif  // UNIDICT_ONNX_PRON_SCORER_H
