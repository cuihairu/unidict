// 发音评分波形预处理（M3b 纯逻辑层）。
// 评分声学模型（wav2vec2 ONNX 导出）的输入约定：16kHz 单声道、
// 零均值/单位方差归一化的 float 波形 (1, N)。采集层给的是 16bit PCM
//（pcm_util_std 的 kSampleRate/kBitsPerSample 锁的同一规格），这里做
// 最后一跳：int16 → float → 归一化。归一化必须与模型导出时的预处理
// 逐位一致，否则声学证据整体偏移、GOP 全线失真——所以它进 core 被
// 单测钉死，不散落在适配器里。

#ifndef UNIDICT_PRON_WAVE_STD_H
#define UNIDICT_PRON_WAVE_STD_H

#include <cstdint>
#include <string>
#include <vector>

#include "std/pcm_util_std.h"

namespace UnidictCoreStd {

// int16 PCM → [-1, 1) float（除以 32768；负数范围天然对称，
// -32768/32768 = -1 也在模型输入域内）
std::vector<float> pcm_to_float(const std::vector<int16_t>& pcm);

// 零均值/单位方差归一化（总体方差 ÷N，模型导出配方如此）。静音护栏：
// 标准差 < 1e-5 时不除（避免把底噪放大成巨值），仅保留去均值——
// 静音段模型自己会给出低置信度，评分层不需要这里造假
std::vector<float> normalize_waveform(const std::vector<float>& x);

// 从 WAV 文件读 16kHz/单声道/16bit PCM。格式不符（采样率/声道/位深
// 不是评分规格、解析失败、文件不可读）返回 false 并给 error——适配器
// 与调参工具的唯一入口，别的采样率宁可拒收也不悄悄重采样
bool load_wav_16k_mono(const std::string& path, std::vector<int16_t>& pcm,
                       std::string& error);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_PRON_WAVE_STD_H
