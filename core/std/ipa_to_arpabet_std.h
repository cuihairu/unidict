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

// 从释义文本提取音标字段（M4 GUI 接线的前置）：词典通常没有独立的
// 发音字段，音标按惯例写在释义开头——/ˈkæt/ 或 [kæt]。在去 HTML
// 标签后的前 256 字节里扫成对分隔符，取第一个严格通过解析的候选：
//  - 纯 ASCII 候选只认空格分隔的合法 ARPAbet（/K AE T/）——像
//    "hello" 这种恰好全由单字母音素组成的普通词没有 IPA 符号佐证，
//    不能因为"能解析"就收（URL/文本斜杠误收防线）；
//  - 含非 ASCII 的候选须整体通过 IPA 严格解析。
// 候选内部再出现 / [ ] 一律拒收（"[see /ə/]" 这种括住的正文不是
// 发音字段）。已知局限：HTML 实体编码的音标（&#x0259;）不识别——
// 只剥标签不解实体，宁缺毋滥。找不到返回 nullopt（调用方退回
// 无评分跟读）。
std::optional<std::string> extract_phonetic_text(const std::string& text);

// 词条口音字段（M5 全球口音）：词典惯例英音在前、美音在后
// （"英 [kæt] 美 [kæt]"、"UK /…/ US /…/"）。british 是第一个通过
// 解析的字段；american 是其后第一个与 british 不同的字段——双解
// 词典英美同音时只记 british（口音切换无意义），不硬凑。字段仍受
// extract_phonetic_text 的全部护栏约束。
struct PhoneticFields {
    std::optional<std::string> british;
    std::optional<std::string> american;
};

// 提取释义里的英美两个音标字段；同一套扫描/护栏，扫到第二个不同
// 字段即停。单字段词典 american 为空，调用方按"口音切换不改变
// 评测参考"处理
PhoneticFields extract_phonetic_variants(const std::string& text);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_IPA_TO_ARPABET_STD_H
