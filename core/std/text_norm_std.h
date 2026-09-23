// 查词键归一化：无 Qt、无 ICU 的轻量实现（码点查表）。

#ifndef UNIDICT_TEXT_NORM_STD_H
#define UNIDICT_TEXT_NORM_STD_H

#include <string>

namespace UnidictCoreStd::TextNorm {

// 把任意 UTF-8 查询词折叠为归一化查词键。规则按序：
//   1. 全角 ASCII（U+FF01–U+FF5E）与全角空格（U+3000）折叠为半角；
//   2. 组合变音符（U+0300–U+036F，NFD 形式）直接丢弃；
//   3. 重音/拼音声调字母按查表折叠为基础 ASCII 字母（é→e、ǐ→i、ß→ss 等）；
//   4. ASCII 大写转小写；
//   5. 去除首尾空白。
// 非拉丁文字（中文、日文等）与无规则命中的码点原样保留；
// 非法 UTF-8 字节原样透传，不做纠错。
std::string fold_key(const std::string& s);

// 归一化逻辑版本号：规则变更时递增，用于让持久化索引缓存失效。
inline constexpr int kFoldKeyVersion = 1;

} // namespace UnidictCoreStd::TextNorm

#endif // UNIDICT_TEXT_NORM_STD_H
