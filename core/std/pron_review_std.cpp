#include "std/pron_review_std.h"

#include <algorithm>

namespace UnidictCoreStd {
namespace {

bool has_review_tag(const std::vector<std::string>& tags) {
    for (const std::string& t : tags) {
        if (t == kPronReviewTag) {
            return true;
        }
    }
    return false;
}

}  // namespace

bool word_score_valid(double word_score) {
    return word_score >= 0.0 && word_score <= 1.0;
}

bool word_score_unstable(double word_score) {
    // [0,1) 才算不稳：0（最差）也算；域外/NaN 走 word_score_valid 拦下
    return word_score_valid(word_score) && word_score < kUnstableWordScore;
}

std::optional<std::vector<std::string>> next_pron_review_tags(
    const std::vector<std::string>& tags, double word_score) {
    // 域外（含 NaN——比较恒为假）是坏数据/哨兵值：不动标签，宁可不改也
    // 不拿坏分数猜状态
    if (!word_score_valid(word_score)) {
        return std::nullopt;
    }
    const bool has = has_review_tag(tags);
    const bool unstable = word_score_unstable(word_score);  // 此处必在域内
    if (has == unstable) {
        return std::nullopt;  // 状态没变：不值得一次落盘
    }
    std::vector<std::string> out = tags;
    if (unstable) {
        out.push_back(kPronReviewTag);
    } else {
        // 只摘我们自己这一个标签（可能重复出现也一并清干净），他标签
        // 与相对顺序不动
        out.erase(std::remove(out.begin(), out.end(),
                              std::string(kPronReviewTag)),
                  out.end());
    }
    return out;
}

}  // namespace UnidictCoreStd
