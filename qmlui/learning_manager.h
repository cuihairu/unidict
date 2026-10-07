#ifndef LEARNINGMANAGER_H
#define LEARNINGMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QMap>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>

// 学习统计数据结构
struct LearningStats {
    QString word;
    int lookupCount = 0;          // 查询次数
    int correctAnswers = 0;       // 正确回答次数
    int wrongAnswers = 0;         // 错误回答次数
    QDateTime firstLookup;        // 首次查询时间
    QDateTime lastLookup;         // 最近查询时间
    QDateTime nextReview;         // 下次复习时间
    int masteryLevel = 0;         // 掌握程度 0-5
    double difficulty = 1.0;      // 单词难度系数
    QStringList tags;             // 标签分类
    QString notes;                // 用户笔记

    static LearningStats fromJson(const QJsonObject& obj);
};

// 学习统计读面：从 AppDataLocation 的 learning_stats.json 加载历史数据，
// 供 Main.qml 学习统计 Tab 展示（今日/进度/到期复习/弱项/激励语）。
// 写面（学习记录/答题/复习调度/成就/导入导出）在 P-7 游戏化死码清理中
// 退役——全仓零消费，四技能数据模型批次另起新写入口。
class LearningManager : public QObject
{
    Q_OBJECT

public:
    explicit LearningManager(QObject *parent = nullptr);

    // 单词明细（getWeakWords 的条目数据源）
    Q_INVOKABLE QVariantMap getWordStats(const QString& word) const;

    // 学习统计查询（学习统计 Tab 消费面）
    Q_INVOKABLE QVariantMap getDailyStats() const;
    Q_INVOKABLE QVariantMap getProgressStats() const;
    Q_INVOKABLE QVariantList getDueReviews() const;
    Q_INVOKABLE QVariantList getWeakWords(int limit = 10) const;
    Q_INVOKABLE QString getMotivationalMessage() const;

private:
    QMap<QString, LearningStats> m_wordStats;
    int m_dailyTarget = 10;

    void loadStats();
    QString getStatsFilePath() const;

    // 辅助函数（getDueReviews 的条目修饰）
    int calculateReviewPriority(const LearningStats& stats) const;
    QString getReviewReason(const LearningStats& stats) const;
};

#endif // LEARNINGMANAGER_H
