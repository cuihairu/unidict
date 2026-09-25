#pragma once

// 纯 std 音频小工具：波形降采样与 WAV 封装/解析。刻意不碰 Qt、不碰
// 麦克风——发音练习（docs/pronunciation-plan.md M1）里能进 CI 的纯逻辑层，
// Qt Multimedia 采集/回放壳（gui/audio_recorder、gui/pcm_playback）不进
// 任何测试。M3b 起发音评分适配器（adapters/pron）也从这里消费 WAV 解析。
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace PronAudio {

// 语音模型的标准输入规格：16kHz / 单声道 / 16bit。采集与封装都锁死
// 这套参数，评测层（M3+）按同一规格消费。
inline constexpr uint32_t kSampleRate = 16000;
inline constexpr uint16_t kChannels = 1;
inline constexpr uint16_t kBitsPerSample = 16;
// 单次录音上限 30 秒：跟读一段词句足够，也防忘点停止把内存吃穿
//（960KB int16 封顶）。
inline constexpr size_t kMaxSamples = static_cast<size_t>(kSampleRate) * 30;

// 一列波形的包络：该时间片内 PCM 样本的最小/最大值
struct WaveColumn {
    int16_t min = 0;
    int16_t max = 0;
};

// PCM 样本 → 波形包络列（每列取 min/max，供 Widget 一笔一列画竖线）。
// 列数多于样本数时按样本逐个成列；空输入或 0 列返回空。
inline std::vector<WaveColumn> downsample_wave(const std::vector<int16_t>& pcm,
                                               size_t columns) {
    std::vector<WaveColumn> out;
    if (columns == 0 || pcm.empty()) {
        return out;
    }
    out.reserve(columns);
    const size_t per = (pcm.size() + columns - 1) / columns;
    for (size_t c = 0; c < columns; ++c) {
        const size_t begin = c * per;
        if (begin >= pcm.size()) {
            break;
        }
        const size_t end = std::min(begin + per, pcm.size());
        int16_t lo = pcm[begin];
        int16_t hi = pcm[begin];
        for (size_t i = begin + 1; i < end; ++i) {
            lo = std::min(lo, pcm[i]);
            hi = std::max(hi, pcm[i]);
        }
        out.push_back({lo, hi});
    }
    return out;
}

inline void put_u16(std::vector<uint8_t>& v, size_t at, uint16_t x) {
    v[at] = static_cast<uint8_t>(x & 0xFF);
    v[at + 1] = static_cast<uint8_t>(x >> 8);
}

inline void put_u32(std::vector<uint8_t>& v, size_t at, uint32_t x) {
    v[at] = static_cast<uint8_t>(x & 0xFF);
    v[at + 1] = static_cast<uint8_t>((x >> 8) & 0xFF);
    v[at + 2] = static_cast<uint8_t>((x >> 16) & 0xFF);
    v[at + 3] = static_cast<uint8_t>(x >> 24);
}

inline uint16_t get_u16(const uint8_t* d) {
    return static_cast<uint16_t>(d[0] | (d[1] << 8));
}

inline uint32_t get_u32(const uint8_t* d) {
    return static_cast<uint32_t>(d[0]) | (static_cast<uint32_t>(d[1]) << 8) |
           (static_cast<uint32_t>(d[2]) << 16) |
           (static_cast<uint32_t>(d[3]) << 24);
}

// PCM 样本封装为 44 字节头 + little-endian 数据的 WAV 文件字节
inline std::vector<uint8_t> build_wav_bytes(const std::vector<int16_t>& pcm) {
    const size_t data_bytes = pcm.size() * sizeof(int16_t);
    std::vector<uint8_t> out(44 + data_bytes);
    std::memcpy(out.data(), "RIFF", 4);
    put_u32(out, 4, static_cast<uint32_t>(36 + data_bytes));
    std::memcpy(out.data() + 8, "WAVE", 4);
    std::memcpy(out.data() + 12, "fmt ", 4);
    put_u32(out, 16, 16);  // fmt 块尺寸（PCM 固定 16）
    put_u16(out, 20, 1);   // audio format: PCM
    put_u16(out, 22, kChannels);
    put_u32(out, 24, kSampleRate);
    put_u32(out, 28, kSampleRate * kChannels * kBitsPerSample / 8);  // byte rate
    put_u16(out, 32, static_cast<uint16_t>(kChannels * kBitsPerSample / 8));  // block align
    put_u16(out, 34, kBitsPerSample);
    std::memcpy(out.data() + 36, "data", 4);
    put_u32(out, 40, static_cast<uint32_t>(data_bytes));
    if (!pcm.empty()) {
        std::memcpy(out.data() + 44, pcm.data(), data_bytes);
    }
    return out;
}

// 宽容解析 WAV 头：遍历 chunk（含 2 字节对齐 padding），fmt 必须在 data
// 之前（规范如此）。data 声明尺寸被文件截断时以实际可用为准——评测层
// 宁可少读一段，不要直接拒收。
struct WavInfo {
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits = 0;
    size_t data_offset = 0;  // data 块 payload 起点
    size_t data_bytes = 0;   // 实际可读字节数（可能小于声明值）
};

inline bool parse_wav_header(const uint8_t* data, size_t size, WavInfo& out) {
    if (!data || size < 44) {
        return false;
    }
    if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) {
        return false;
    }
    bool have_fmt = false;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint32_t declared = get_u32(data + pos + 4);
        const size_t body = pos + 8;
        const size_t take = std::min<size_t>(declared, size - body);
        if (std::memcmp(data + pos, "fmt ", 4) == 0 && take >= 16) {
            out.channels = get_u16(data + body + 2);
            out.sample_rate = get_u32(data + body + 4);
            out.bits = get_u16(data + body + 14);
            have_fmt = true;
        } else if (std::memcmp(data + pos, "data", 4) == 0) {
            out.data_offset = body;
            out.data_bytes = take;
            if (!have_fmt) {
                return false;
            }
            return out.channels > 0 && out.sample_rate > 0 && out.bits > 0;
        }
        // chunk 按 2 字节对齐：奇数尺寸补 1 字节 pad
        pos = body + take + (take & 1);
    }
    return false;
}

}  // namespace PronAudio
