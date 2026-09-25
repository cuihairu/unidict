// 波形预处理纯 std 测试。
// 归一化配方必须与模型导出时逐位同族（零均值/总体方差单位化），
// GOP 是 exp(mean log p)，预处理错一点全体分数系统性偏移——断言给
// 的是数值不是"大致对"。
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "std/pcm_util_std.h"
#include "std/pron_wave_std.h"

using UnidictCoreStd::load_wav_16k_mono;
using UnidictCoreStd::normalize_waveform;
using UnidictCoreStd::pcm_to_float;

namespace {

constexpr double kEps = 1e-6;

void test_pcm_to_float() {
    // 32768 归一：满负幅 -32768 → -1.0，正满幅 32767 → 0.99997
    const auto f = pcm_to_float({-32768, -16384, 0, 16384, 32767});
    assert(std::abs(f[0] - (-1.0)) < kEps);
    assert(std::abs(f[1] - (-0.5)) < kEps);
    assert(std::abs(f[2] - 0.0) < kEps);
    assert(std::abs(f[3] - 0.5) < kEps);
    assert(f[4] < 1.0 && f[4] > 0.9999);
    assert(pcm_to_float({}).empty());
}

void test_normalize_zero_mean_unit_var() {
    const std::vector<float> x = {1.f, 2.f, 3.f, 4.f, 5.f};
    const auto y = normalize_waveform(x);
    // 均值恰为 0
    double mean = 0;
    for (const float v : y) mean += v;
    mean /= y.size();
    assert(std::abs(mean) < 1e-6);
    // 总体方差恰为 1
    double var = 0;
    for (const float v : y) var += (v - mean) * (v - mean);
    var /= y.size();
    assert(std::abs(var - 1.0) < 1e-5);
    // 线性变换保序：y[0] < y[4]
    assert(y[0] < y[4]);
}

void test_normalize_silence_guard() {
    // 全零输入：std=0，不除零、不产生 NaN/Inf
    const auto y = normalize_waveform(std::vector<float>(100, 0.0f));
    for (const float v : y) {
        assert(std::isfinite(v));
        assert(v == 0.0f);
    }
    // 底噪（std < 1e-5）：去均值但保留原始幅度，不放大
    const auto z = normalize_waveform(std::vector<float>(50, 1e-7f));
    for (const float v : z) {
        assert(std::abs(v) < 1e-5);
    }
    assert(normalize_waveform({}).empty());
}

void build_wav(std::vector<uint8_t>& out, uint32_t rate, uint16_t ch,
               uint16_t bits, const std::vector<int16_t>& samples) {
    out.assign(44, 0);
    std::memcpy(out.data(), "RIFF", 4);
    const uint32_t data_bytes = samples.size() * 2;
    PronAudio::put_u32(out, 4, 36 + data_bytes);
    std::memcpy(out.data() + 8, "WAVE", 4);
    std::memcpy(out.data() + 12, "fmt ", 4);
    PronAudio::put_u32(out, 16, 16);
    PronAudio::put_u16(out, 20, 1);
    PronAudio::put_u16(out, 22, ch);
    PronAudio::put_u32(out, 24, rate);
    PronAudio::put_u32(out, 28, rate * ch * bits / 8);
    PronAudio::put_u16(out, 32, ch * bits / 8);
    PronAudio::put_u16(out, 34, bits);
    std::memcpy(out.data() + 36, "data", 4);
    PronAudio::put_u32(out, 40, data_bytes);
    out.insert(out.end(), reinterpret_cast<const uint8_t*>(samples.data()),
               reinterpret_cast<const uint8_t*>(samples.data()) +
                   data_bytes);
}

void write_tmp(const char* name, const std::vector<uint8_t>& bytes) {
    const std::string path = std::string("/tmp/") + name;
    FILE* f = fopen(path.c_str(), "wb");
    assert(f);
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);
}

void test_load_wav_16k_mono() {
    std::vector<uint8_t> wav;
    build_wav(wav, 16000, 1, 16, {100, -200, 300});
    write_tmp("pron_ok.wav", wav);
    std::vector<int16_t> pcm;
    std::string err;
    assert(load_wav_16k_mono("/tmp/pron_ok.wav", pcm, err));
    assert((pcm == std::vector<int16_t>{100, -200, 300}));

    // 采样率/声道/位深不符：拒收并给出可读错误
    build_wav(wav, 44100, 1, 16, {1});
    write_tmp("pron_rate.wav", wav);
    assert(!load_wav_16k_mono("/tmp/pron_rate.wav", pcm, err) &&
           err.find("16kHz") != std::string::npos);
    build_wav(wav, 16000, 2, 16, {1, 2});
    write_tmp("pron_ch.wav", wav);
    assert(!load_wav_16k_mono("/tmp/pron_ch.wav", pcm, err));
    build_wav(wav, 16000, 1, 8, {1});
    write_tmp("pron_bits.wav", wav);
    assert(!load_wav_16k_mono("/tmp/pron_bits.wav", pcm, err));
    // 非 WAV 文件 / 不存在的路径
    write_tmp("pron_junk.wav", {'n', 'o', 'p', 'e'});
    assert(!load_wav_16k_mono("/tmp/pron_junk.wav", pcm, err));
    assert(!load_wav_16k_mono("/tmp/pron_missing.wav", pcm, err));
}

}  // namespace

int main() {
    test_pcm_to_float();
    test_normalize_zero_mean_unit_var();
    test_normalize_silence_guard();
    test_load_wav_16k_mono();
    return 0;
}
