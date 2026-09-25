// fp16 转换与 log_softmax 纯 std 测试。
// GOP = exp(mean log p)，这两步任何一个位错都会系统性污染全部分数。
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <vector>

#include "std/ctc_logits_std.h"

using UnidictCoreStd::fp16_to_fp32;
using UnidictCoreStd::log_softmax_row;

namespace {

uint16_t f32_to_f16_round(float v) {
    // 测试自用的就近舍入转换（IEEE 754 half），断言 fp16_to_fp32 时反推
    const uint32_t bits = [v] {
        uint32_t b;
        static_assert(sizeof(b) == sizeof(v), "");
        __builtin_memcpy(&b, &v, sizeof(b));
        return b;
    }();
    const uint32_t sign = (bits >> 16) & 0x8000;
    const int32_t exp = static_cast<int32_t>((bits >> 23) & 0xFF) - 127;
    const uint32_t frac = bits & 0x7FFFFF;
    if (((bits >> 23) & 0xFF) == 0xFF) {  // Inf/NaN
        return static_cast<uint16_t>(sign | 0x7C00 | (frac ? 0x200 : 0));
    }
    if (exp > 15) return static_cast<uint16_t>(sign | 0x7C00);   // 溢出→Inf
    if (exp >= -14) {  // 规格数
        const uint32_t hfrac = frac >> 13 +
                               ((frac >> 12) & 1);  // 就近舍入
        return static_cast<uint16_t>(
            sign | ((exp + 15) << 10) | (hfrac & 0x3FF));
    }
    if (exp >= -25) {  // 次正规（够用即可，测试不追下界）
        const uint32_t hfrac = (frac | 0x800000) >> (14 - exp - 1);
        return static_cast<uint16_t>(sign | (hfrac & 0x3FF));
    }
    return static_cast<uint16_t>(sign);  // 下溢→±0
}

void test_fp16_specials() {
    assert(fp16_to_fp32(0x0000) == 0.0f);
    assert(fp16_to_fp32(0x8000) == -0.0f);
    const float pinf = fp16_to_fp32(0x7C00);
    const float ninf = fp16_to_fp32(0xFC00);
    assert(std::isinf(pinf) && pinf > 0);
    assert(std::isinf(ninf) && ninf < 0);
    assert(std::isnan(fp16_to_fp32(0x7E00)));
}

void test_fp16_values() {
    // 位模式逐一对照（torch.float16 语义）
    struct Case { uint16_t h; float f; };
    for (const Case& c : {
             Case{0x3C00, 1.0f},
             Case{0xBC00, -1.0f},
             Case{0x4000, 2.0f},
             Case{0xC400, -4.0f},
             Case{0xD000, -32.0f},
             Case{0x3555, 0.333251953125f},    // 最近的 fp16 到 1/3
             Case{0x0400, 6.103515625e-5f},    // 最小规格数 2^-14
             Case{0x03FF, 6.097555179782743e-5f},  // 最大次正规 (1023/1024)·2^-14
             Case{0x0001, 5.960464477539063e-8f},  // 最小次正规 2^-24
             Case{0x7BFF, 65504.0f},           // 最大 fp16
         }) {
        assert(fp16_to_fp32(c.h) == c.f);
    }
    // 往返：常规 logits 幅度（±30）内 fp32→fp16→fp32 无损
    for (float v = -30.0f; v <= 30.0f; v += 0.25f) {
        const float back = fp16_to_fp32(f32_to_f16_round(v));
        assert(std::abs(back - v) < 5e-3f * (v == 0 ? 1 : std::abs(v) + 1) + 1e-6);
    }
}

void test_log_softmax_known() {
    // [0,0] → [-ln2, -ln2]
    float out2[2];
    log_softmax_row(std::vector<float>{0.f, 0.f}.data(), 2, out2);
    assert(std::abs(out2[0] - (-std::log(2.0))) < 1e-6);
    assert(std::abs(out2[1] - (-std::log(2.0))) < 1e-6);
    // [ln4, 0, 0] → [ln4−ln6, −ln6, −ln6]（e^ln4=4，分母 4+1+1=6）
    const float in3[] = {std::log(4.0f), 0.0f, 0.0f};
    float out3[3];
    log_softmax_row(in3, 3, out3);
    assert(std::abs(out3[0] - (std::log(4.0) - std::log(6.0))) < 1e-6);
    assert(std::abs(out3[1] - (-std::log(6.0))) < 1e-6);
    assert(std::abs(out3[2] - (-std::log(6.0))) < 1e-6);
}

void test_log_softmax_stability_and_inplace() {
    // 大值：不减 max 会溢出成 NaN
    const float big[] = {1000.0f, 999.0f, 998.0f};
    float out[3];
    log_softmax_row(big, 3, out);
    for (const float v : out) assert(std::isfinite(v));
    // 原地等价于异地
    float a[3] = {1000.0f, 999.0f, 998.0f};
    float b[3] = {1000.0f, 999.0f, 998.0f};
    log_softmax_row(a, 3, a);
    log_softmax_row(b, 3, out);
    for (int i = 0; i < 3; ++i) assert(a[i] == out[i]);
    // 行和的 exp 恒等于 1（log_softmax 定义式）
    double s = 0;
    for (const float v : a) s += std::exp(static_cast<double>(v));
    assert(std::abs(s - 1.0) < 1e-6);
}

}  // namespace

int main() {
    test_fp16_specials();
    test_fp16_values();
    test_log_softmax_known();
    test_log_softmax_stability_and_inplace();
    return 0;
}
