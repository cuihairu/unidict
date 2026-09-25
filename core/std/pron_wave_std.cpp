#include "std/pron_wave_std.h"

#include <cmath>
#include <cstring>
#include <fstream>

namespace UnidictCoreStd {

std::vector<float> pcm_to_float(const std::vector<int16_t>& pcm) {
    std::vector<float> out;
    out.reserve(pcm.size());
    for (const int16_t s : pcm) {
        out.push_back(static_cast<float>(s) / 32768.0f);
    }
    return out;
}

std::vector<float> normalize_waveform(const std::vector<float>& x) {
    if (x.empty()) {
        return {};
    }
    double sum = 0.0;
    for (const float v : x) {
        sum += v;
    }
    const double mean = sum / static_cast<double>(x.size());
    double var = 0.0;
    for (const float v : x) {
        const double d = v - mean;
        var += d * d;
    }
    var /= static_cast<double>(x.size());
    const double std_dev = std::sqrt(var);
    // 静音护栏：除以趋零 std 会把量化底噪放大几个数量级
    const double scale = std_dev < 1e-5 ? 1.0 : 1.0 / std_dev;
    std::vector<float> out;
    out.reserve(x.size());
    for (const float v : x) {
        out.push_back(static_cast<float>((v - mean) * scale));
    }
    return out;
}

bool load_wav_16k_mono(const std::string& path, std::vector<int16_t>& pcm,
                       std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open wav: " + path;
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    PronAudio::WavInfo info;
    if (!PronAudio::parse_wav_header(bytes.data(), bytes.size(), info)) {
        error = "not a parseable wav: " + path;
        return false;
    }
    if (info.sample_rate != PronAudio::kSampleRate ||
        info.channels != PronAudio::kChannels ||
        info.bits != PronAudio::kBitsPerSample) {
        error = "wav must be 16kHz/mono/16bit (got " +
                std::to_string(info.sample_rate) + "Hz/" +
                std::to_string(info.channels) + "ch/" +
                std::to_string(info.bits) + "bit): " + path;
        return false;
    }
    const size_t count = info.data_bytes / sizeof(int16_t);
    pcm.resize(count);
    if (count > 0) {
        // WAV data 块 little-endian；目标平台 x86/ARM 小端直接 memcpy，
        // 大端主机不在支持矩阵里（Qt 官方构建同样假设小端）
        std::memcpy(pcm.data(), bytes.data() + info.data_offset,
                    count * sizeof(int16_t));
    }
    return true;
}

}  // namespace UnidictCoreStd
