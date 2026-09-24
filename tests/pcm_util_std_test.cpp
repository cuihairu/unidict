// pcm_util（波形降采样 + WAV 封装）纯 std 测试——发音练习 M1 里唯一
// 能进 CI 的纯逻辑层，Qt Multimedia 壳不许进测试（分层纪律见
// docs/pronunciation-plan.md）。
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "pcm_util.h"

using namespace PronAudio;

namespace {

void test_downsample_empty_and_zero_columns() {
    assert(downsample_wave({}, 100).empty());
    std::vector<int16_t> pcm = {1, 2, 3};
    assert(downsample_wave(pcm, 0).empty());
}

void test_downsample_basic() {
    std::vector<int16_t> pcm;
    for (int i = 0; i < 100; ++i) {
        pcm.push_back(static_cast<int16_t>(i));
    }
    const auto cols = downsample_wave(pcm, 10);  // 每列恰好 10 样本
    assert(cols.size() == 10);
    assert(cols[0].min == 0 && cols[0].max == 9);
    assert(cols[5].min == 50 && cols[5].max == 59);
    assert(cols[9].min == 90 && cols[9].max == 99);
}

void test_downsample_uneven_tail() {
    const std::vector<int16_t> pcm = {0, 1, 2, 3, 4};
    const auto cols = downsample_wave(pcm, 2);  // per=3：{0..2} {3,4}
    assert(cols.size() == 2);
    assert(cols[0].min == 0 && cols[0].max == 2);
    assert(cols[1].min == 3 && cols[1].max == 4);
}

void test_downsample_extremes() {
    const std::vector<int16_t> pcm = {-32768, 32767, 0};
    const auto cols = downsample_wave(pcm, 1);
    assert(cols.size() == 1);
    assert(cols[0].min == -32768);
    assert(cols[0].max == 32767);
}

void test_downsample_more_columns_than_samples() {
    const std::vector<int16_t> pcm = {5, -5, 0};
    const auto cols = downsample_wave(pcm, 10);  // 逐样本成列，min==max
    assert(cols.size() == 3);
    assert(cols[0].min == 5 && cols[0].max == 5);
    assert(cols[1].min == -5 && cols[1].max == -5);
    assert(cols[2].min == 0 && cols[2].max == 0);
}

void test_wav_roundtrip() {
    const std::vector<int16_t> pcm = {0, 1, -1, 32767, -32768};
    const auto bytes = build_wav_bytes(pcm);
    assert(bytes.size() == 44 + pcm.size() * 2);
    assert(std::memcmp(bytes.data(), "RIFF", 4) == 0);
    assert(std::memcmp(bytes.data() + 8, "WAVE", 4) == 0);
    assert(std::memcmp(bytes.data() + 12, "fmt ", 4) == 0);
    assert(std::memcmp(bytes.data() + 36, "data", 4) == 0);
    // RIFF 尺寸 = 36 + data；data 声明尺寸 = 10
    assert(bytes[4] == static_cast<uint8_t>(36 + 10));
    assert(bytes[40] == 10 && bytes[41] == 0 && bytes[42] == 0 && bytes[43] == 0);

    WavInfo info;
    assert(parse_wav_header(bytes.data(), bytes.size(), info));
    assert(info.channels == kChannels);
    assert(info.sample_rate == kSampleRate);
    assert(info.bits == kBitsPerSample);
    assert(info.data_offset == 44);
    assert(info.data_bytes == 10);
    assert(std::memcmp(bytes.data() + 44, pcm.data(), 10) == 0);
}

void test_wav_rejects_garbage() {
    const auto bytes = build_wav_bytes({1, 2, 3});
    WavInfo info;
    // < 44 字节、空指针、坏 magic 都拒收
    assert(!parse_wav_header(bytes.data(), 30, info));
    assert(!parse_wav_header(nullptr, 100, info));
    auto bad = bytes;
    bad[0] = 'X';
    assert(!parse_wav_header(bad.data(), bad.size(), info));
    bad = bytes;
    bad[9] = 'x';  // WAVE 魔数破坏
    assert(!parse_wav_header(bad.data(), bad.size(), info));
}

void test_wav_truncated_data_still_parses() {
    // data 声明 6 字节但文件被截到只留 4 字节 payload：以实际可用为准
    auto bytes = build_wav_bytes({1, 2, 3});
    bytes.resize(44 + 4);
    WavInfo info;
    assert(parse_wav_header(bytes.data(), bytes.size(), info));
    assert(info.data_bytes == 4);
}

void test_wav_extra_chunk_with_padding() {
    // 手工构造 fmt + LIST(奇数尺寸带 pad) + data，验证 chunk 遍历按
    // 2 字节对齐跳过额外块
    std::vector<uint8_t> wav;
    auto push = [&wav](const char* s, size_t n) {
        wav.insert(wav.end(), s, s + n);
    };
    auto push_u32 = [&wav](uint32_t x) {
        wav.push_back(x & 0xFF);
        wav.push_back((x >> 8) & 0xFF);
        wav.push_back((x >> 16) & 0xFF);
        wav.push_back(x >> 24);
    };
    auto push_u16 = [&wav](uint16_t x) {
        wav.push_back(x & 0xFF);
        wav.push_back((x >> 8) & 0xFF);
    };
    push("RIFF", 4);
    push_u32(0);  // 占位，尾部回填
    push("WAVE", 4);
    push("fmt ", 4);
    push_u32(16);
    push_u16(1);        // PCM
    push_u16(kChannels);
    push_u32(kSampleRate);
    push_u32(kSampleRate * kChannels * kBitsPerSample / 8);
    push_u16(kChannels * kBitsPerSample / 8);
    push_u16(kBitsPerSample);
    push("LIST", 4);
    push_u32(5);  // 奇数：后面补 1 字节 pad
    push("abcde", 5);
    wav.push_back(0);  // pad
    push("data", 4);
    push_u32(2);
    wav.push_back(0x34);
    wav.push_back(0x12);
    const uint32_t riff_size = static_cast<uint32_t>(wav.size() - 8);
    wav[4] = riff_size & 0xFF;
    wav[5] = (riff_size >> 8) & 0xFF;
    wav[6] = (riff_size >> 16) & 0xFF;
    wav[7] = riff_size >> 24;

    WavInfo info;
    assert(parse_wav_header(wav.data(), wav.size(), info));
    assert(info.channels == kChannels);
    assert(info.sample_rate == kSampleRate);
    assert(info.bits == kBitsPerSample);
    assert(info.data_offset == wav.size() - 2);
    assert(info.data_bytes == 2);
}

}  // namespace

int main() {
    test_downsample_empty_and_zero_columns();
    test_downsample_basic();
    test_downsample_uneven_tail();
    test_downsample_extremes();
    test_downsample_more_columns_than_samples();
    test_wav_roundtrip();
    test_wav_rejects_garbage();
    test_wav_truncated_data_still_parses();
    test_wav_extra_chunk_with_padding();
    return 0;
}
