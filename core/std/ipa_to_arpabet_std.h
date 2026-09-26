// 词典 IPA 文本 → ARPAbet（M4 跟读整合的前置，纯逻辑层）。
// 词典词条里的发音是 IPA 文本（"ˈkæt"、espeak 风格 "k æ t"），评分
// 内核吃 ARPAbet 序列——这层转换让 GUI/CLI 直接拿词条发音去评分，
// 不必手抄音标。双模式：
//  1. 空白分词后全是合法 ARPAbet（容忍重音数字 "AE1"）→ 直接放行
//     （CMU 词典域）；
//  2. 否则按 UTF-8 IPA 解析：espeak 音素表最长前缀匹配（含 əl/ɑ˷ɹ
//     等合写展开、l̩ 音节符），标注符号（重音/切分/连读tie bar）跳过，
//     未收录码点整串拒收——宁可不评也不拿错误音素序列去打分。
// 不做的事：非英语音素（表里没有，多语模型词表也不收）；Merriam-
// Webster 式 \ˈkat\ 自家记法；CMU 以外的数字重音（"AE12" 拒）。

#ifndef UNIDICT_IPA_TO_ARPABET_STD_H
#define UNIDICT_IPA_TO_ARPABET_STD_H

#include <optional>
#include <string>
#include <vector>

namespace UnidictCoreStd {

// 解析失败（未收录码点/空结果/明显不是发音字段）返回 nullopt；
// 成功返回大写 ARPAbet 序列（可能比输入符号多，合写已展开）
std::optional<std::vector<std::string>> phonetic_text_to_arpabet(
    const std::string& text);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_IPA_TO_ARPABET_STD_H
