// M9 发音练习历史纯 std 测试：一次练习的累计（首次/再次/坏数据/未知
// 时钟）、进步趋势的判定死区、练习清单的过滤与次序（低分 → 久未练 →
// 词名定序，时刻未知当最久未练）。不碰数据落盘（那是 store 的活，见
// data_store_std_pron_test）与平台壳。
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "std/pron_history_std.h"

using namespace UnidictCoreStd;

namespace {

bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) < eps;
}

constexpr long long kDay = 86400;

PronRecordStd rec(const std::string& word, double last, double best, int attempts,
                  long long at) {
    PronRecordStd r;
    r.word = word;
    r.last_score = last;
    r.best_score = best;
    r.attempts = attempts;
    r.last_at = at;
    return r;
}

// 首次练习：没有可比对象，记录从零建起，趋势是"首次"
void test_first_attempt() {
    const auto a = next_pron_attempt("cat", nullptr, 0.72, 1000);
    assert(a.has_value());
    assert(!a->has_prev);
    assert(near(a->delta, 0.0));
    assert(a->record.word == "cat");
    assert(near(a->record.last_score, 0.72));
    assert(near(a->record.best_score, 0.72));  // 首次：最好 = 本次
    assert(a->record.attempts == 1);
    assert(a->record.last_at == 1000);
    assert(pron_trend(*a) == PronTrend::kFirstTime);
}

// 连续练习：次数累加、best 只涨、delta 是与上次的差
void test_cumulative_attempts() {
    const auto first = next_pron_attempt("cat", nullptr, 0.5, 1000);
    assert(first.has_value());
    // 回升：last 0.7、best 0.7、attempts 2、delta +0.2
    const auto second = next_pron_attempt("cat", &first->record, 0.7, 2000);
    assert(second.has_value());
    assert(second->has_prev);
    assert(near(second->delta, 0.2));
    assert(near(second->record.last_score, 0.7));
    assert(near(second->record.best_score, 0.7));
    assert(second->record.attempts == 2);
    assert(second->record.last_at == 2000);
    assert(pron_trend(*second) == PronTrend::kUp);
    // 回落：best 保住 0.7（只涨不跌），last 0.4、delta -0.3
    const auto third = next_pron_attempt("cat", &second->record, 0.4, 3000);
    assert(third.has_value());
    assert(near(third->delta, -0.3));
    assert(near(third->record.last_score, 0.4));
    assert(near(third->record.best_score, 0.7));
    assert(third->record.attempts == 3);
    assert(pron_trend(*third) == PronTrend::kDown);
}

// 趋势死区：|Δ| <= 0.02 算持平（单次 GOP 的抖动量级），刚过线才算进步
void test_trend_flat_zone() {
    auto at = [](double score) {
        const PronRecordStd prev = rec("cat", 0.5, 0.5, 1, 1000);
        return next_pron_attempt("cat", &prev, score, 2000);
    };
    assert(pron_trend(*at(0.51)) == PronTrend::kFlat);   // Δ +0.01
    assert(pron_trend(*at(0.49)) == PronTrend::kFlat);   // Δ -0.01
    assert(pron_trend(*at(0.519)) == PronTrend::kFlat);  // Δ +0.019（贴边仍在区内）
    assert(pron_trend(*at(0.481)) == PronTrend::kFlat);  // Δ -0.019
    assert(pron_trend(*at(0.53)) == PronTrend::kUp);     // Δ +0.03
    assert(pron_trend(*at(0.47)) == PronTrend::kDown);   // Δ -0.03
    assert(near(at(0.53)->delta, 0.03));
    // 死区边界（Δ 恰为 0.02）刻意不断言：0.52-0.5 在二进制浮点下是
    // 0.020000000000000018，判哪边由舍入噪声决定——这正是死区存在的
    // 理由（0.02 宽的带子本来就该压住这种量级），不是缺陷
    // 持平也照样累计：练了就是练了，历史要如实记
    const auto flat = at(0.51);
    assert(flat->record.attempts == 2);
    assert(near(flat->record.last_score, 0.51));
}

// 坏数据不进历史：空词名、域外分数、NaN
void test_invalid_inputs_rejected() {
    const PronRecordStd prev = rec("cat", 0.5, 0.5, 1, 1000);
    assert(!next_pron_attempt("", nullptr, 0.7, 2000).has_value());
    assert(!next_pron_attempt("", &prev, 0.7, 2000).has_value());
    assert(!next_pron_attempt("cat", nullptr, -1.0, 2000).has_value());
    assert(!next_pron_attempt("cat", nullptr, 1.5, 2000).has_value());
    assert(!next_pron_attempt(
               "cat", &prev, std::numeric_limits<double>::quiet_NaN(), 2000)
                .has_value());
    // 边界值仍算有效（0 与 1 都是合法词分）
    assert(next_pron_attempt("cat", nullptr, 0.0, 2000).has_value());
    assert(next_pron_attempt("cat", nullptr, 1.0, 2000).has_value());
}

// 磁盘被改过的记录（best < last）由本次拉齐，不让坏 best 一直挂着
void test_repairs_inconsistent_record() {
    const PronRecordStd bad = rec("cat", 0.8, 0.2, 2, 1000);  // best < last
    const auto a = next_pron_attempt("cat", &bad, 0.3, 2000);
    assert(a.has_value());
    assert(near(a->record.best_score, 0.3));  // max(0.3, 坏 best 0.2)
    assert(near(a->record.last_score, 0.3));
    assert(near(a->delta, -0.5));             // delta 仍以 last 为准
    assert(a->record.attempts == 3);
}

// 未知时钟：now <= 0 保留旧时间戳（不抹掉已知时刻），首次则留空
void test_unknown_now_keeps_timestamp() {
    const auto first = next_pron_attempt("cat", nullptr, 0.5, 0);
    assert(first.has_value());
    assert(first->record.last_at == 0);
    const PronRecordStd prev = rec("dog", 0.5, 0.5, 3, 1234);
    const auto a = next_pron_attempt("dog", &prev, 0.6, 0);
    assert(a.has_value());
    assert(a->record.last_at == 1234);  // 旧时刻保留
    assert(a->record.attempts == 4);
    const auto b = next_pron_attempt("dog", &prev, 0.6, 5000);
    assert(b.has_value());
    assert(b->record.last_at == 5000);
}

// 练习清单：只收不稳词（<0.6，与 M8 标签同门槛），低分 → 久未练 → 词名
void test_practice_queue_filter_and_order() {
    const long long now = 100 * kDay;
    const std::vector<PronRecordStd> records = {
        rec("eagle", 0.9, 0.9, 5, now - kDay),      // 稳了 → 不在清单
        rec("cat", 0.42, 0.7, 3, now - 3 * kDay),   // 0.42，3 天前
        rec("dog", 0.55, 0.55, 1, now - 10),        // 0.55，刚练过
        rec("bird", 0.42, 0.42, 1, now - 10 * kDay),// 0.42，10 天前
        rec("zebra", 0.42, 0.42, 1, 0),             // 0.42，时刻未知
        rec("broken", -1.0, 0.9, 9, now),           // 坏分数 → 不在清单
    };
    const auto q = pron_practice_queue(records, now, 0);
    assert(q.size() == 4);
    // 0.42 组内：时刻未知（视作最久未练）→ 10 天 → 3 天
    assert(q[0].word == "zebra" && q[1].word == "bird" && q[2].word == "cat");
    assert(q[3].word == "dog");  // 0.55 垫底
    assert(near(q[0].last_score, 0.42));

    // 截断：limit 2 只给最该练的两个；limit 0 = 全给
    const auto top2 = pron_practice_queue(records, now, 2);
    assert(top2.size() == 2 && top2[0].word == "zebra" && top2[1].word == "bird");
    // limit 超过实际条数：全给
    assert(pron_practice_queue(records, now, 99).size() == 4);
    // 空输入 / 全稳 → 空清单
    assert(pron_practice_queue({}, now, 5).empty());
    assert(pron_practice_queue({rec("eagle", 0.9, 0.9, 1, now)}, now, 5).empty());
}

// 同分同天数时按词名定序（界面不因磁盘顺序跳动）
void test_practice_queue_tiebreak_by_word() {
    const long long now = 50 * kDay;
    const std::vector<PronRecordStd> records = {
        rec("zulu", 0.5, 0.5, 1, now - kDay),
        rec("alpha", 0.5, 0.5, 1, now - kDay),
        rec("mike", 0.5, 0.5, 1, now - kDay),
    };
    const auto q = pron_practice_queue(records, now, 0);
    assert(q.size() == 3);
    assert(q[0].word == "alpha" && q[1].word == "mike" && q[2].word == "zulu");
    // now 未知时"多久没练"算不出来：全按最久未练处理，退回词名定序
    const auto unknown_now = pron_practice_queue(records, 0, 0);
    assert(unknown_now.size() == 3);
    assert(unknown_now[0].word == "alpha" && unknown_now[2].word == "zulu");
}

}  // namespace

int main() {
    test_first_attempt();
    test_cumulative_attempts();
    test_trend_flat_zone();
    test_invalid_inputs_rejected();
    test_repairs_inconsistent_record();
    test_unknown_now_keeps_timestamp();
    test_practice_queue_filter_and_order();
    test_practice_queue_tiebreak_by_word();
    std::cout << "pron_history_std_test: all assertions passed\n";
    return 0;
}
