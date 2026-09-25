// ARPAbet 音素的发音变体表（M4 变体容忍，纯逻辑层）。
// GOP 按模型主键打分对"读得地道但不是教科书音"天然偏严（M3b 真模型
// 实证：dog 的尾音 g→ŋ 同化被狠扣）。本表收录自然语音过程/口音变体
// 的 espeak 符号，评分时对目标音素取 max(主键, 容忍变体)——只放宽
// 同类自然变体，不放宽清浊/调音部位的真实错误（d 读成 t 照扣）。
//
// 保守纪律：只收模型词表（392 类）里确实存在的符号；不做位置感知
//（首/尾音的变体不同），位置相关变体等 M4 实测再评估——宁缺毋滥，
// 容忍过宽会把真错误也洗成高分。

#ifndef UNIDICT_PRON_VARIANTS_STD_H
#define UNIDICT_PRON_VARIANTS_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

// ARPAbet 音素 → 容忍的 espeak 变体符号（不含主键本身；无变体返回
// 空表）。全部条目经 espeak_to_arpabet 正向映射核对过域归属
std::vector<std::string> arpabet_variants(const std::string& arpabet);

}  // namespace UnidictCoreStd

#endif  // UNIDICT_PRON_VARIANTS_STD_H
