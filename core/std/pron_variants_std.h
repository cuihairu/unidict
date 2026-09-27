// ARPAbet 音素的发音变体表（M4 变体容忍 + M7 位置感知，纯逻辑层）。
// GOP 按模型主键打分对"读得地道但不是教科书音"天然偏严（M3b 真模型
// 实证：dog 的尾音 g→ŋ 同化被狠扣）。本表收录自然语音过程/口音变体
// 的 espeak 符号，评分时对目标音素取 max(主键, 容忍变体)——只放宽
// 同类自然变体，不放宽清浊/调音部位的真实错误（d 读成 t 照扣）。
//
// 位置感知（M7）：词尾 g→ŋ 同化是真过程，但只发生在词尾；goal 的
// 首 g 读成 ŋ 是真错误。位置盲表分不开两者，所以变体带 word_final
// 门槛——只有目标音素在词尾时词尾限定变体才进候选。
//
// 保守纪律：只收模型词表（392 类）里确实存在的符号；不做位置枚举
// （首/中/尾）——现在只有"词尾"一类真实需求，等新变位出现再扩。

#ifndef UNIDICT_PRON_VARIANTS_STD_H
#define UNIDICT_PRON_VARIANTS_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

// 一条容忍变体。notable 区分两类语义：
//  - silent（默认）：同音素的语音学实现（闪音 ɾ 之于 t、音节 l̩ 之于
//    l）——只参与记分 max，永不向用户报出，报它等于报"你发了正确
//    的音"；
//  - notable：不同音素级的替代，因自然语音过程被策略容忍（词尾
//    g→ŋ）——记分 max 之外，区间证据明显压过主键时写进
//    PhoneGopResult::realized_as：分数不扣，但"你发的是它"要透明
//    给用户（地道与鼻音化错误由用户对照示范自行判断）。
struct PhoneVariant {
    std::string espeak;   // 模型词表域符号
    bool notable = false;
};

// ARPAbet 音素 → 容忍的变体（不含主键本身；无变体返回空表）。
// word_final：目标音素是否在词尾（词尾限定变体只在 true 时返回）。
// 全部条目经 espeak_to_arpabet 正向映射核对过域归属
std::vector<PhoneVariant> arpabet_variants(const std::string& arpabet,
                                           bool word_final);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_PRON_VARIANTS_STD_H
