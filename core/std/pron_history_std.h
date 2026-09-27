#pragma once

// 发音练习历史（M9，纯逻辑）：跟自己的进步比——GOP 绝对分对重口音用户
// 天然偏严（docs/pronunciation-plan.md「已知风险」一节），单次分说明
// 不了进步：得记住同一个词练过几次、最近多少、最好多少；练习清单再按
// "没练稳的先练"排序，把评过分的词变成一份可练的清单，M8 的「发音不稳」
// 标签由此有了去处（不只标给生词本，也告诉用户接下来练哪个）。
//
// 分层同 M8：这里只算"记录该怎么变、清单怎么排"，落盘在 DataStoreStd
// （PronRecordStd 随生词/笔记存在同一个 store 文件里），界面与文案在
// gui/pronunciation_panel 平台壳（不进测试）。
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "std/data_store_std.h"  // PronRecordStd（持久记录形态）

namespace UnidictCoreStd {

// 进步/退步的判定死区：|Δ| 不超过这个值算持平。单次 GOP 词分本身就有
// ±0.02 量级的抖动（录音噪声、口音落在模型边缘），把这个量级说成
// "进步了"是自欺——用户会开始信一个并不存在的信号。（边界上判哪边由
// 浮点舍入决定，这是死区而非刀刃的应有之义。）
constexpr double kPronFlatDelta = 0.02;

// 一次练习之后的历史状态
struct PronAttempt {
    PronRecordStd record;   // 累计后的新记录（可直接落盘）
    bool has_prev = false;  // 之前练过（delta 才有意义）
    double delta = 0.0;     // 本次 − 上次（正 = 进步）
};

enum class PronTrend { kFirstTime, kUp, kFlat, kDown };

// 累计一次练习：prev 是该词的历史记录（nullptr = 首次），now 为 epoch
// 秒（<= 0 视为未知时刻，此时保留旧时间戳而不是抹掉它——已知的时刻比
// 空值有用）。词名为空或词分无效（域外/NaN 哨兵值，见 pron_review_std
// 的 word_score_valid）返回 nullopt：坏数据不进历史，宁可不记也不拿
// 哨兵值污染基线。
std::optional<PronAttempt> next_pron_attempt(const std::string& word,
                                            const PronRecordStd* prev,
                                            double word_score, long long now);

// 相对化趋势：给"比上次 +0.12 / 持平 / 首次记录"这类展示用的定性判断
PronTrend pron_trend(const PronAttempt& attempt);

// 练习清单：只收"最近一次仍未稳"的记录（词分 < kUnstableWordScore，
// 与 M8 的标签同一门槛——标签说"还没稳"，清单就该练还没稳的）。次序：
// 词分低优先 → 距上次练习久优先 → 词名字典序（定序：可测、界面稳定）。
// 时刻未知（last_at = 0，或 now 未知）当"最久未练"排最前——不知道多久
// 没练的，就当最该练。limit 为 0 时不截断。
std::vector<PronRecordStd> pron_practice_queue(
    const std::vector<PronRecordStd>& records, long long now, size_t limit);

}  // namespace UnidictCoreStd
