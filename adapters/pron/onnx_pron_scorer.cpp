#include "onnx_pron_scorer.h"

#include <cstdint>
#include <fstream>
#include <utility>

#include <onnxruntime_cxx_api.h>

#include "std/ctc_logits_std.h"
#include "std/pron_wave_std.h"

namespace UnidictPron {
namespace {

// 读完整个文件（模型 635MB 级；会话创建期间持有，之后即释放）。
// 用内存加载而不是路径 ctor：ORT 的路径参数在 Windows 是 wchar_t，
// 内存加载顺带绕开跨平台路径编码这摊事
bool read_file(const std::string& path, std::string& error,
               std::vector<char>& out) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "cannot open model file: " + path;
        return false;
    }
    const std::streamsize size = file.tellg();
    if (size <= 0) {
        error = "empty model file: " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    file.read(out.data(), size);
    if (!file) {
        error = "short read on model file: " + path;
        return false;
    }
    return true;
}

std::string read_text_file(const std::string& path, bool& ok,
                           std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        ok = false;
        error = "cannot open vocab file: " + path;
        return {};
    }
    ok = true;
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

}  // namespace

struct PronScorerOnnx::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "unidict_pron"};
    Ort::SessionOptions options;
    Ort::Session session{nullptr};
    UnidictCoreStd::PronVocab vocab;

    // 会话的输入输出名（导出名不可控，加载时查出来存住）
    std::string input_name;   // 空串 = 无输入图（测试 fixture）
    std::string output_name;
};

PronScorerOnnx::PronScorerOnnx() = default;
PronScorerOnnx::~PronScorerOnnx() = default;

std::unique_ptr<PronScorerOnnx> PronScorerOnnx::load(const Config& cfg,
                                                     std::string& error) {
    std::vector<char> model_bytes;
    if (!read_file(cfg.model_path, error, model_bytes)) {
        return nullptr;
    }
    bool ok = false;
    const std::string vocab_text = read_text_file(cfg.vocab_path, ok, error);
    if (!ok) {
        return nullptr;
    }
    auto vocab = UnidictCoreStd::parse_vocab_json(vocab_text);
    if (!vocab) {
        error = "invalid vocab json: " + cfg.vocab_path;
        return nullptr;
    }

    auto scorer = std::unique_ptr<PronScorerOnnx>(new PronScorerOnnx());
    scorer->impl_ = std::make_unique<Impl>();
    Impl& impl = *scorer->impl_;
    try {
        impl.options.SetIntraOpNumThreads(cfg.intra_op_threads > 0
                                              ? cfg.intra_op_threads
                                              : 1);
        impl.options.SetInterOpNumThreads(1);
        impl.options.SetExecutionMode(ORT_SEQUENTIAL);
        impl.session = Ort::Session(
            impl.env, reinterpret_cast<const void*>(model_bytes.data()),
            model_bytes.size(), impl.options);

        Ort::AllocatorWithDefaultOptions allocator;
        if (impl.session.GetInputCount() > 0) {
            impl.input_name =
                impl.session.GetInputNameAllocated(0, allocator).get();
        }
        if (impl.session.GetOutputCount() < 1) {
            error = "model has no outputs: " + cfg.model_path;
            return nullptr;
        }
        impl.output_name =
            impl.session.GetOutputNameAllocated(0, allocator).get();
    } catch (const Ort::Exception& e) {
        error = std::string("onnxruntime session error: ") + e.what();
        return nullptr;
    }
    impl.vocab = std::move(*vocab);
    return scorer;
}

const UnidictCoreStd::PronVocab& PronScorerOnnx::vocab() const {
    return impl_->vocab;
}

std::optional<UnidictCoreStd::WordGopResult> PronScorerOnnx::score(
    const std::vector<int16_t>& pcm,
    const std::vector<std::string>& target_arpabet, std::string& error) {
    if (pcm.empty()) {
        error = "empty pcm";
        return std::nullopt;
    }
    if (target_arpabet.empty()) {
        error = "empty target phones";
        return std::nullopt;
    }

    // 采集域 (int16 PCM) → 模型域（零均值/单位方差 float 波形）
    const std::vector<float> wave = UnidictCoreStd::normalize_waveform(
        UnidictCoreStd::pcm_to_float(pcm));

    Impl& impl = *impl_;
    std::vector<Ort::Value> outputs;
    const char* output_names[] = {impl.output_name.c_str()};
    try {
        if (impl.input_name.empty()) {
            // 无输入图（Constant fixture）：无 feeds 直接跑
            outputs = impl.session.Run(Ort::RunOptions{nullptr}, nullptr,
                                       nullptr, 0, output_names, 1);
        } else {
            const std::int64_t n = static_cast<std::int64_t>(wave.size());
            const std::int64_t input_shape[2] = {1, n};
            Ort::MemoryInfo mem_info = Ort::MemoryInfo::CreateCpu(
                OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value input = Ort::Value::CreateTensor<float>(
                mem_info, const_cast<float*>(wave.data()), wave.size(),
                input_shape, 2);
            const char* input_names[] = {impl.input_name.c_str()};
            outputs = impl.session.Run(Ort::RunOptions{nullptr}, input_names,
                                       &input, 1, output_names, 1);
        }
    } catch (const Ort::Exception& e) {
        error = std::string("onnxruntime inference error: ") + e.what();
        return std::nullopt;
    }
    if (outputs.empty()) {
        error = "inference produced no output tensor";
        return std::nullopt;
    }

    // (1,T,C) logits → fp32 → 每帧 log_softmax
    const Ort::Value& out = outputs[0];
    std::vector<std::int64_t> shape;
    size_t element_count = 0;
    try {
        const auto info = out.GetTensorTypeAndShapeInfo();
        shape = info.GetShape();
        element_count = static_cast<size_t>(info.GetElementCount());
        const ONNXTensorElementDataType type = info.GetElementType();
        if (type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT &&
            type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
            error = "unexpected output tensor type (need float/fp16)";
            return std::nullopt;
        }
        if (shape.size() != 3 || shape[0] != 1 || shape[1] <= 0 ||
            shape[2] <= 0 ||
            element_count != static_cast<size_t>(shape[1] * shape[2])) {
            error = "unexpected output shape (need (1,T,C) CTC logits)";
            return std::nullopt;
        }
        const int num_frames = static_cast<int>(shape[1]);
        const int num_classes = static_cast<int>(shape[2]);

        std::vector<float> log_probs(element_count);
        if (type == ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16) {
            // fp16 张量 API 只给原始比特，位级转换自己做
            const uint16_t* half = out.GetTensorData<uint16_t>();
            for (size_t i = 0; i < element_count; ++i) {
                log_probs[i] = UnidictCoreStd::fp16_to_fp32(half[i]);
            }
        } else {
            const float* f = out.GetTensorData<float>();
            log_probs.assign(f, f + element_count);
        }
        for (int t = 0; t < num_frames; ++t) {
            UnidictCoreStd::log_softmax_row(
                log_probs.data() + static_cast<size_t>(t) * num_classes,
                num_classes,
                log_probs.data() + static_cast<size_t>(t) * num_classes);
        }
        return UnidictCoreStd::score_word(
            log_probs.data(), num_frames, num_classes, impl.vocab.blank_index,
            impl.vocab.labels, target_arpabet);
    } catch (const Ort::Exception& e) {
        error = std::string("onnxruntime output error: ") + e.what();
        return std::nullopt;
    }
}

}  // namespace UnidictPron
