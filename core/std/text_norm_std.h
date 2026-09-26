// 专业查词的键归一化（docs/pro_dictionary_gap.md P0 ②「字符归一化不足：
// 大小写/重音/全半角/Unicode 兼容折叠、标点归一化对"专业查词"很关键
// （尤其多语种）」）。
//
// 之前 fold_key() 只做三件事：全角→半角、Latin 重音折叠、ASCII 小写。
// 漏掉的每一条在真实使用里都能变成"明明词典里有这个词却查不到"：
//
//   1) **非拉丁文字的大小写完全没折**。俄语 U+0410–U+042F（А–Я）原样保留，
//      所以 Москва 与 москва 折出不同的键 → 俄语词典是**大小写敏感**的。
//      希腊语 U+0391–U+03AB 同理；而且 ς(U+03C2) 与 σ(U+03C3) 是同一个
//      字母的词尾/词中两形，词形不同、键必须相同。
//   2) **标点/引号/破折号不归一**。从 Word、网页、PDF 里复制来的
//      "don't" 十有八九是 U+2019，"—" 是 U+2014，"…" 是 U+2026——全都查不到。
//      用户不会去手动改引号。
//   3) **不可见字符原样保留**。PDF 复制常带 U+00AD 软连字符、U+200B 零宽
//      空格、U+FEFF BOM，词条键里带了它们就永远匹配不上，而界面上完全
//      看不出来。
//   4) **连字与合成字母不折**。ﬁ(U+FB01) / ﬀ / ﬃ 这些排版连字在词典键里
//      就是普通字母连写。
//   5) **组合音标只覆盖 U+0300–U+036F**。希伯来语尼基德(U+05B0–U+05BC)、
//      阿拉伯语哈拉卡(U+064B–U+0652)、U+20D0 以上的符号组合音标都漏了，
//      带注音的希伯来/阿拉伯词条一个都匹配不上。
//   6) **只 trim ASCII 空白**。U+00A0 不换行空格、各种 U+2000–U+200A 细空格
//      都会让首尾空白残留，键对不上。
//
// 全部按表驱动实现，core/std 不引入 ICU。分四类表：
//   kPunct       —— 标点/符号 → ASCII 等价（引号、破折号、省略号、乘号…）
//   kSpace       —— Unicode 空白 → U+0020
//   kInvisible   —— 零宽/BOM/软连字符/变体选择符/组合记号：直接丢弃
//   kFold        —— 已有的小写+重音折叠表（沿用）
// 大小写另走 kCaseOffset（成对区间，统一偏移）与 kCaseOddEven（±1 交替）。

#ifndef UNIDICT_TEXT_NORM_STD_H
#define UNIDICT_TEXT_NORM_STD_H

#include <cstdint>
#include <string>

namespace UnidictCoreStd::TextNorm {

// fold_key 的可配置项。各开关独立，默认全开（即 fold_key 的行为）。
struct Options {
    // 大小写折叠，含非拉丁文字（西里尔、希腊、越南语 Latin Extended Additional）
    bool fold_case = true;
    // 重音/声调折叠：é→e、ǎ→a、ß→ss
    bool fold_diacritics = true;
    // 标点归一：’→'、—→-、…→...、×→x、全角标点→半角
    bool fold_punctuation = true;
    // 丢弃不可见字符：零宽空格/连接符、BOM、软连字符、变体选择符
    bool strip_invisible = true;
    // 组合记号（NFC 音标、希伯来尼基德、阿拉伯哈拉卡）整体丢弃
    bool strip_combining = true;
    // 连字展开：ﬁ→fi、ﬀ→ff
    bool fold_ligatures = true;
    // 首尾空白裁剪（含 Unicode 空白）
    bool trim = true;
};

// 把任意 UTF-8 查询词折叠为归一化查词键。规则按序：
//   1. 按 Options 丢弃不可见字符与组合记号；
//   2. 全角 ASCII（U+FF01–U+FF5E）与全角空格（U+3000）折叠为半角；
//   3. 标点/引号/破折号折叠为 ASCII 等价；
//   4. 重音/拼音声调字母按查表折叠为基础字母（é→e、ǐ→i、ß→ss 等）；
//   5. 大小写折叠：ASCII、Latin-1 补充、西里尔、希腊、Latin Extended Additional；
//   6. Unicode 空白归一为 U+0020；
//   7. 裁剪首尾空白。
// 非拉丁文字（中文、日文、阿拉伯文本体等）原样保留；
// 非法 UTF-8 字节原样透传，不做纠错。
//
// **注意**：折叠是有损的（é 与 e 折成同一个键）。这正是查词键要的——
// 用户敲 e / é / É / ｅ 都该命中同一条词条。展示用文本请用原文，不要用键。
std::string fold_key(const std::string& s);

// 同上，显式指定 Options。
std::string fold_key(const std::string& s, const Options& opt);

// 归一化逻辑版本号：规则变更时递增，用于让持久化索引缓存失效重建。
// v2：新增标点归一、不可见字符与组合记号丢弃、连字展开、非拉丁大小写折叠、
//      Unicode 空白归一。旧键与新键不同，缓存必须重建。
inline constexpr int kFoldKeyVersion = 2;

}  // namespace UnidictCoreStd::TextNorm

#endif  // UNIDICT_TEXT_NORM_STD_H
