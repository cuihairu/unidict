// 发音评分模型词表解析（M3b 纯逻辑层）。
// 评分声学模型 sadda-speech/wav2vec2-espeak-ctc 的词表是 HF 风格的
// vocab.json——扁平 "符号": id 映射（实测 {"<s>":1, "<pad>":0, "n":4, …}），
// 392 类、CTC blank = <pad> = 0。解析产出两样东西：id→espeak 符号表
// （ctc_gop_std::score_word 的 class_labels）与 blank 类索引。词表来自
// 下载的模型资产，格式不可控，解析必须严苛——坏词表宁可拒收，不许
// 让错位的音素表进评分管道（每个分数都会静默错一位）。

#ifndef UNIDICT_PRON_VOCAB_STD_H
#define UNIDICT_PRON_VOCAB_STD_H

#include <optional>
#include <string>
#include <vector>

namespace UnidictCoreStd {

struct PronVocab {
    // id → espeak IPA 符号。id 稀疏时空洞留空串（score_word 按值
    // find，空串永不匹配）；blank 符号原样保留
    std::vector<std::string> labels;
    int blank_index = -1;  // CTC blank 类索引，未识别出 blank 符号为 -1

    bool valid() const {
        return blank_index >= 0 &&
               static_cast<int>(labels.size()) > blank_index &&
               !labels[blank_index].empty();
    }
};

// 解析扁平 {"符号": id, …} JSON 文本。成功要求：至少一项、id 非负
// 整数、无重复 id、且能在常见 blank 命名（<pad>/[PAD]/<blk>/…）里
// 找到 blank。结构不符/转义非法/尾部垃圾一律 nullopt
std::optional<PronVocab> parse_vocab_json(const std::string& text);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_PRON_VOCAB_STD_H
