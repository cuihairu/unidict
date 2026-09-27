// M8 生词本联动纯 std 测试：词分 →「发音不稳」标签的增删规则（打上/
// 摘下/状态没变免落盘/幂等）、门槛边界、有效域外分数不动标签、既有
// 标签与顺序不受影响。生词本读写在 GUI 壳（不进测试），这里只测规则。
#include <cassert>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "std/pron_review_std.h"

using namespace UnidictCoreStd;

namespace {

void test_unstable_predicate() {
    assert(word_score_unstable(0.0));            // 最差（0 分）也算不稳
    assert(word_score_unstable(0.042));          // dog 真模型实测分
    assert(word_score_unstable(0.599));
    assert(!word_score_unstable(kUnstableWordScore));  // 门槛边界：>= 不算
    assert(!word_score_unstable(0.795));         // cat 正常发音：不误伤
    assert(!word_score_unstable(1.0));
    // 有效域外不判（NaN 比较恒假、负数哨兵、超上界坏数据）
    assert(!word_score_unstable(std::numeric_limits<double>::quiet_NaN()));
    assert(!word_score_unstable(-1.0));
    assert(!word_score_unstable(5.0));
}

void test_add_tag() {
    // 空标签组 + 不稳 → 打上
    const auto r1 = next_pron_review_tags({}, 0.4);
    assert(r1.has_value() && *r1 == std::vector<std::string>{kPronReviewTag});
    // 既有标签：保序、追加在尾部
    const auto r2 = next_pron_review_tags({"收藏", "难词"}, 0.4);
    assert(r2.has_value() &&
           *r2 == (std::vector<std::string>{"收藏", "难词", kPronReviewTag}));
    // 已有标签 + 仍不稳 → 状态没变，nullopt（免落盘、幂等）
    assert(!next_pron_review_tags({kPronReviewTag}, 0.4).has_value());
    assert(!next_pron_review_tags({"x", kPronReviewTag}, 0.4).has_value());
}

void test_remove_tag() {
    // 稳了 → 摘掉标签：返回已变更的空组（有值的空 vector ≠ nullopt）
    const auto r1 = next_pron_review_tags({kPronReviewTag}, 0.9);
    assert(r1.has_value() && r1->empty());
    // 只摘我们这一个，他标签保序保留
    const auto r2 = next_pron_review_tags({"x", kPronReviewTag, "y"}, 0.9);
    assert(r2.has_value() && *r2 == (std::vector<std::string>{"x", "y"}));
    // 本来就没有标签 + 稳 → 状态没变，nullopt
    assert(!next_pron_review_tags({}, 0.9).has_value());
    assert(!next_pron_review_tags({"x"}, 0.9).has_value());
}

void test_invalid_score_untouched() {
    // 域外/NaN 分数：谓词不判（false），变更函数整个不动标签
    const std::vector<std::string> tagged = {kPronReviewTag};
    assert(!next_pron_review_tags(
               tagged, std::numeric_limits<double>::quiet_NaN())
               .has_value());
    assert(!next_pron_review_tags(tagged, -1.0).has_value());  // 哨兵值
    assert(!next_pron_review_tags(tagged, 5.0).has_value());
    assert(!next_pron_review_tags({}, 5.0).has_value());
}

}  // namespace

int main() {
    test_unstable_predicate();
    test_add_tag();
    test_remove_tag();
    test_invalid_score_untouched();
    return 0;
}
