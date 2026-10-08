// 英文词形还原（lemmatization）——规则 + 不规则表，纯 C++17 无第三方。
//
// 动机（P-11 查词能力线批二）：用户拿到的词常是屈折形（running/studies/
// wolves/went），词典词头却是原形（run/study/wolf/go）——不加词形还原，
// 这些查询只能滑到前缀/模糊层拿一堆"像的词"，而不是直接命中释义。
// 成熟词典（GoldenDict/欧路）都在精确层后接形态还原。
//
// 口径：
//   - 只做「候选生成」不做词性判定：lemma_candidates() 返回按置信序排列
//     的原形候选（不含输入词自身——调用方在精确层已经查过），逐个走精确
//     查询，词典里存在哪个就是哪个。英语形态歧义（stocks=stock/stock 股
//     票复数 vs 动词三单）靠"候选 ∩ 词典词头"裁决，零数据表也能用。
//   - 不规则变化走静态表（went→go、wolves→wolf、better→good…），规则变
//     化走后缀规则（-ies/-es/-s、-ing、-ied/-ed 含双写辅音回退与 e 复原）。
//   - 只对纯 ASCII 小写化后生效；含非 ASCII 字节（CJK/重音）直接空手——
//     字节级后缀规则对多字节字符是无意义且危险的。
//   - 短词闸门：长度 <3 不产候选（"is/as" 这类去 s 全是噪声）。
//
// 消费方：DictionaryManagerStd::search_grouped 层 1（精确未中 → 原形层）；
// CLI/桌面壳经聚合链自动受益。

#ifndef UNIDICT_LEMMA_STD_H
#define UNIDICT_LEMMA_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd::Lemma {

// 返回 word 的原形候选（小写、去重、按置信序，不含 word 自身；至多 4 条）。
// word 大小写不敏感；无规则可套或含非 ASCII 字节时返回空。
std::vector<std::string> lemma_candidates(const std::string& word);

} // namespace UnidictCoreStd::Lemma

#endif // UNIDICT_LEMMA_STD_H
