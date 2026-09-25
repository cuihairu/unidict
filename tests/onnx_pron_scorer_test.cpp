// onnxruntime 推理壳测试——用 1.5KB 的 Constant 假模型跑通全链路
// （加载→推理→张量解析→fp16/log_softmax→CTC 对齐→GOP），不需要
// 635MB 真模型（CI 纪律：适配器做编译+行为验证，不跑声学模型）。
// 假模型语义见 scripts/gen_fake_ctc_fixture.py：16 帧 × 8 类，帧 0-1
// blank、2-6 k、7-11 æ、12-15 t，favored logit 12 其余 0。
//
// 环境变量：
//   UNIDICT_PRON_FAKE_DIR  fixture 目录（默认 <repo>/tests/fixtures，
//                          供脚本/打包环境改路径）
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "onnx_pron_scorer.h"

using UnidictCoreStd::WordGopResult;
using UnidictPron::PronScorerOnnx;

namespace {

// fixture 路径解析：CMake 传 UNIDICT_PRON_FAKE_DIR 宏，运行时可被
// 环境变量覆盖（重定位测试/打包场景）
std::string fixture_path(const char* name) {
    const char* env = std::getenv("UNIDICT_PRON_FAKE_DIR");
    std::string base = env ? env : UNIDICT_PRON_FAKE_DIR;
    if (!base.empty() && base.back() != '/') {
        base += '/';
    }
    return base + name;
}

// 构造满足 fixture 时序的 PCM：每帧 320 样本（50fps @16kHz），
// 幅度交替让 std 正常（非静音护栏路径）
std::vector<int16_t> make_pcm(int frames) {
    std::vector<int16_t> pcm(frames * 320);
    for (size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = static_cast<int16_t>((i % 2 == 0) ? 4000 : -4000);
    }
    return pcm;
}

void test_load_rejects_garbage() {
    std::string err;
    // 词表缺失/损坏：模型在也拒收
    assert(!PronScorerOnnx::load(
        {"nonexistent.onnx", "nonexistent.json"}, err));
    assert(!PronScorerOnnx::load(
        {fixture_path("fake_ctc.onnx"), "nonexistent.json"}, err));
    // 坏词表（写进临时文件）
    const std::string bad = fixture_path("bad_vocab_test.json");
    { std::ofstream f(bad); f << "{\"a\":1}"; }  // 无 blank
    assert(!PronScorerOnnx::load(
        {fixture_path("fake_ctc.onnx"), bad}, err));
    std::remove(bad.c_str());
    // 坏模型字节（词表好的）
    const std::string bad_model = fixture_path("bad_model_test.onnx");
    { std::ofstream f(bad_model, std::ios::binary); f << "not onnx"; }
    assert(!PronScorerOnnx::load({bad_model, fixture_path("fake_vocab.json")},
                                 err));
    std::remove(bad_model.c_str());
    assert(!err.empty());
}

void test_score_end_to_end() {
    std::string err;
    const auto scorer = PronScorerOnnx::load(
        {fixture_path("fake_ctc.onnx"), fixture_path("fake_vocab.json")},
        err);
    assert(scorer);
    assert(scorer->vocab().blank_index == 0);
    assert(scorer->vocab().labels.size() == 8);
    assert(scorer->vocab().labels[1] == "k");
    assert(scorer->vocab().labels[2] == "\xC3\xA6");  // æ

    // 目标 k æ t：帧段 2-6/7-11/12-15 全部落在 favored 类上，
    // 每帧 log p(favored) ≈ 0 → GOP 分数接近 1
    const auto result = scorer->score(
        make_pcm(16), {"K", "AE", "T"}, err);
    assert(result.has_value());
    assert(result->phones.size() == 3);
    for (size_t i = 0; i < 3; ++i) {
        const auto& p = result->phones[i];
        assert(p.start_frame >= 0 && p.end_frame > p.start_frame);
        assert(p.score > 0.95);
    }
    assert(result->word_score > 0.95);
}

void test_score_semantics() {
    std::string err;
    const auto scorer = PronScorerOnnx::load(
        {fixture_path("fake_ctc.onnx"), fixture_path("fake_vocab.json")},
        err);
    assert(scorer);
    const auto pcm = make_pcm(16);

    // 词表里没有的 ARPAbet（无 espeak 主键）→ nullopt
    assert(!scorer->score(pcm, {"K", "XX"}, err).has_value());
    // 空输入
    assert(!scorer->score({}, {"K"}, err).has_value());
    assert(!scorer->score(pcm, {}, err).has_value());
    // 词表里存在但音频证据不支持的音素（ɪ → fixture 偏 k/æ/t）：
    // 词分应明显低（区分度冒烟）
    const auto off = scorer->score(pcm, {"IH"}, err);
    assert(off.has_value());
    assert(off->word_score < 0.5);
    // 注：Constant 假模型的输出帧数与输入长度无关，"音素多于帧→空
    // 区间 0 分"的行为在此 fixture 上无法构造（真模型才暴露），不写
}

}  // namespace

int main() {
    test_load_rejects_garbage();
    test_score_end_to_end();
    test_score_semantics();
    std::cout << "onnx_pron_scorer_test: all assertions passed\n";
    return 0;
}
