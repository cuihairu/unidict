#pragma once

// 发音评分 → 生词本标签联动（M8，纯逻辑）：词分低于门槛给词条打
// 「发音不稳」标签，回升后自动摘掉——标签表达**当前状态**，不是一次
// 性记录。本文件只算"标签该怎么变"，生词本读写与状态栏文案在上层
// （gui/pronunciation_panel，平台壳不进测试），与评分内核同一分层纪律。
//
// 动机（docs/pronunciation-plan.md M8）：跟读评分与生词本此前互不相
// 通——评完分就沉底，没有"哪些词还得练"的入口。打上标签后，收藏面
// 板的分组过滤下拉（聚合全部生词标签）就是练习清单入口，不依赖复习
// 队列（widgets GUI 没有队列，范围假设见计划文档）。
#include <optional>
#include <string>
#include <vector>

namespace UnidictCoreStd {

// 生词本分组标签：收藏面板"分组过滤下拉"聚合的词条级标签之一。
// UTF-8 字面量——Qt 侧展示须 QString::fromUtf8，不能走 Latin-1
constexpr const char* kPronReviewTag = "发音不稳";

// 判"不稳"的词分门槛（词分 = 0.7·mean + 0.3·min，见
// pronunciation_score_std）。真模型正例：cat 正常发音 0.795，留余量
// 不误伤；单音素明显崩（约 0.2 而其余 0.9）词分掉到 0.5 上下必进。
// 糊区 0.6-0.8 不折腾——标签只收确定的"还没稳"
constexpr double kUnstableWordScore = 0.6;

// 词分是否判"发音不稳"：有效域 [0,1]（词分由 clamp(exp) 聚合而来），
// 域外值（NaN/负数/哨兵值）比较式恒不满足，一律返回 false 不判断
bool word_score_unstable(double word_score);

// 按词分给标签组做增删：需要变更时返回新标签组（打上/摘下
// kPronReviewTag，既有标签与其顺序不动、只动我们这一个）；状态没变
// 或分数无效（域外/NaN）返回 nullopt——上层据此免去一次落盘。
std::optional<std::vector<std::string>> next_pron_review_tags(
    const std::vector<std::string>& tags, double word_score);

}  // namespace UnidictCoreStd
