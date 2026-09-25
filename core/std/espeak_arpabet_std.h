// espeak IPA → ARPAbet 映射（M3b）。
// 评分声学模型（sadda-speech/wav2vec2-espeak-ctc）的输出域是 espeak
// IPA 音素，词典发音的域是 CMU ARPAbet——M3a 内核和 GOP 打分全在
// ARPAbet 域工作，所以映射是两个世界之间唯一的桥。两边都是英语音素
// 全集，一一对应基本成立；个别 espeak 合写符号展开成两个 ARPAbet
// （r-色元音 ɑːɹ → AA R）。映射表只覆盖 espeak 英语实际会输出的
// 符号（覆盖依据模型词表 392 类逐一核对），未收录的（多语模型词表
// 里的法/德/中等音素）返回空结果，由调用方按非法音素处理（M3a
// substitution cost 1 兜底）。键按 UTF-8 字节串整体比较。

#ifndef UNIDICT_ESPEAK_ARPABET_STD_H
#define UNIDICT_ESPEAK_ARPABET_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

// espeak IPA 音素 → ARPAbet（大写 39 音素域，顺序与词典发音一致）；
// 未收录返回空 vector。合写符号（əl/ɑːɹ…）展开成多项
std::vector<std::string> espeak_to_arpabet(const std::string& espeak_phone);

// ARPAbet → espeak 主音素（反向，用于构造测试与调试工具）；
// 同一 ARPAbet 有多个 espeak 变体时返回最常见的一个；未收录返回空串
std::string arpabet_to_espeak(const std::string& arpabet);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_ESPEAK_ARPABET_STD_H
