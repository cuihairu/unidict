#include "std/pron_history_std.h"

#include <algorithm>
#include <limits>

#include "std/pron_review_std.h"

namespace UnidictCoreStd {
namespace {

// 距上次练习的秒数。时刻未知（记录缺 last_at，或调用方给的 now 未知）
// 取最大值：清单里"不知道多久没练"的词就该排在最该练的位置，而不是
// 悄悄当成刚练过排到末尾
long long staleness(const PronRecordStd& rec, long long now) {
    if (now <= 0 || rec.last_at <= 0) {
        return std::numeric_limits<long long>::max();
    }
    return now - rec.last_at;
}

}  // namespace

std::optional<PronAttempt> next_pron_attempt(const std::string& word,
                                            const PronRecordStd* prev,
                                            double word_score, long long now) {
    // 键为空没有可归属的词；分数无效（NaN/域外哨兵值）是坏数据——两者
    // 都不进历史：记一条假记录比不记更糟，它会污染基线与练习清单
    if (word.empty() || !word_score_valid(word_score)) {
        return std::nullopt;
    }
    PronAttempt a;
    a.has_prev = prev != nullptr;
    a.delta = a.has_prev ? word_score - prev->last_score : 0.0;
    a.record.word = word;
    a.record.last_score = word_score;
    // best 是历史最好：坏数据（best < last，磁盘被改过）也由本次拉齐
    a.record.best_score = std::max(word_score, prev ? prev->best_score : 0.0);
    a.record.attempts = (prev ? prev->attempts : 0) + 1;
    // 未知时刻不该抹掉已知时刻（now <= 0 只在调用方拿不到时钟时出现）
    a.record.last_at = now > 0 ? now : (prev ? prev->last_at : 0);
    return a;
}

PronTrend pron_trend(const PronAttempt& attempt) {
    if (!attempt.has_prev) {
        return PronTrend::kFirstTime;
    }
    if (attempt.delta > kPronFlatDelta) {
        return PronTrend::kUp;
    }
    if (attempt.delta < -kPronFlatDelta) {
        return PronTrend::kDown;
    }
    return PronTrend::kFlat;
}

std::vector<PronRecordStd> pron_practice_queue(
    const std::vector<PronRecordStd>& records, long long now, size_t limit) {
    std::vector<PronRecordStd> out;
    for (const PronRecordStd& rec : records) {
        // 门槛复用 M8 的"不稳"：域外分数（磁盘坏值）判不稳返回 false，
        // 天然被挡在清单外
        if (word_score_unstable(rec.last_score)) {
            out.push_back(rec);
        }
    }
    std::sort(out.begin(), out.end(),
              [now](const PronRecordStd& a, const PronRecordStd& b) {
                  if (a.last_score != b.last_score) {
                      return a.last_score < b.last_score;  // 分低的先练
                  }
                  const long long sa = staleness(a, now);
                  const long long sb = staleness(b, now);
                  if (sa != sb) {
                      return sa > sb;  // 同分：久未练的更该练
                  }
                  return a.word < b.word;  // 词名定序，界面不跳动
              });
    if (limit > 0 && out.size() > limit) {
        out.resize(limit);
    }
    return out;
}

}  // namespace UnidictCoreStd
