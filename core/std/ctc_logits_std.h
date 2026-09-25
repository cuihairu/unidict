// CTC logits 后处理（M3b 纯逻辑层）。
// 评分声学模型输出帧级 logits（wav2vec2-espeak-ctc：(1,T,392)，fp16
// 导出），ctc_gop_std 吃的是 log_softmax 后的帧级 log 概率。这里补上
// 中间这一步，顺带处理 fp16：onnxruntime C++ API 对 float16 张量只给
// 原始比特，转 fp32 自己做。数值稳定性（减 max）与 fp16 逐位正确性
// 都进单测——GOP 是 exp(mean log p)，后处理错一位全盘皆错。

#ifndef UNIDICT_CTC_LOGITS_STD_H
#define UNIDICT_CTC_LOGITS_STD_H

#include <cmath>
#include <cstdint>
#include <cstring>

namespace UnidictCoreStd {

// IEEE 754 half → float。指数/尾数移位拼装，含次正规数（denormal，
// fp16 下界 6e-8 一带的 logits 编码）与 Inf/NaN 透传
inline float fp16_to_fp32(uint16_t h) {
    const uint32_t sign = static_cast<uint32_t>(h & 0x8000) << 16;
    const uint32_t exp = (h >> 10) & 0x1F;
    const uint32_t frac = h & 0x3FF;
    uint32_t bits = 0;
    if (exp == 0) {
        if (frac == 0) {
            bits = sign;  // ±0
        } else {
            // 次正规：fp16 2^-24 量级 → 规格化到 fp32 指数域
            // fp32 指数 = -14 - leading_zeros(frac 的 10bit 内) + 127
            int e = -1;
            uint32_t f = frac;
            do {
                ++e;
                f <<= 1;
            } while ((f & 0x400) == 0);
            bits = sign | (static_cast<uint32_t>(127 - 15 - e) << 23) |
                   ((f & 0x3FF) << 13);
        }
    } else if (exp == 0x1F) {
        bits = sign | 0x7F800000 | (frac << 13);  // Inf / NaN
    } else {
        bits = sign | ((exp + 112) << 23) | (frac << 13);  // 127-15=112
    }
    float out;
    static_assert(sizeof(out) == sizeof(bits), "float must be 32-bit");
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

// 一帧 logits（C 类）→ log_softmax，数值稳定实现（减行最大值）。
// x_i - max - log(Σ e^(x_j - max))；第一遍先写 x_i - max，求和后再
// 统一减 log_sum。in == out（原地）允许：每个元素先读后写，互不干扰
inline void log_softmax_row(const float* in, int n, float* out) {
    float maxv = in[0];
    for (int i = 1; i < n; ++i) {
        if (in[i] > maxv) {
            maxv = in[i];
        }
    }
    for (int i = 0; i < n; ++i) {
        out[i] = in[i] - maxv;
    }
    double sum = 0.0;  // 392 项 e^x 求和放 double，避免大 C 值精度塌陷
    for (int i = 0; i < n; ++i) {
        sum += std::exp(static_cast<double>(out[i]));
    }
    const float log_sum = static_cast<float>(std::log(sum));
    for (int i = 0; i < n; ++i) {
        out[i] -= log_sum;
    }
}

}  // namespace UnidictCoreStd

#endif  // UNIDICT_CTC_LOGITS_STD_H
