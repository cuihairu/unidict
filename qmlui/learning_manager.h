#ifndef LEARNINGMANAGER_H
#define LEARNINGMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QVariantMap>
#include <QVariantList>
#include <QJsonObject>
#include <QTimer>

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

    QJsonObject toJson() const;
    static LearningStats fromJson(const QJsonObject& obj);
};

// 复习提醒项
struct ReviewItem {
    QString word;
    QDateTime dueTime;
    int priority = 1;             // 优先级 1-5
    QString reason;               // 复习原因
};

class LearningManager : public QObject
{
    Q_OBJECT

public:
    explicit LearningManager(QObject *parent = nullptr);

    // 学习记录管理
    Q_INVOKABLE void recordLookup(const QString& word, const QString& definition = "");
    Q_INVOKABLE void recordTestResult(const QString& word, bool correct);
    Q_INVOKABLE void updateMasteryLevel(const QString& word, int level);
    Q_INVOKABLE void addWordNote(const QString& word, const QString& note);
    Q_INVOKABLE void addWordTag(const QString& word, const QString& tag);

    // 学习统计查询
    Q_INVOKABLE QVariantMap getWordStats(const QString& word) const;
    Q_INVOKABLE QVariantList getAllStats() const;
    Q_INVOKABLE QVariantMap getDailyStats() const;
    Q_INVOKABLE QVariantMap getWeeklyStats() const;
    Q_INVOKABLE QVariantMap getProgressStats() const;

    // 复习系统 (基于艾宾浩斯遗忘曲线)
    Q_INVOKABLE QVariantList getDueReviews() const;
    Q_INVOKABLE QVariantList getReviewSchedule(int days = 7) const;
    Q_INVOKABLE void scheduleReview(const QString& word, int intervalDays = -1);
    Q_INVOKABLE void completeReview(const QString& word, bool remembered);

    // 学习目标和成就
    Q_INVOKABLE void setDailyTarget(int wordCount);
    Q_INVOKABLE int getDailyTarget() const;
    Q_INVOKABLE bool isDailyTargetMet() const;
    Q_INVOKABLE QVariantList getAchievements() const;

    // 数据导入导出
    Q_INVOKABLE bool exportStats(const QString& filePath);
    Q_INVOKABLE bool importStats(const QString& filePath);
    Q_INVOKABLE void resetStats();

    // 智能推荐
    Q_INVOKABLE QVariantList getWeakWords(int limit = 10) const;
    Q_INVOKABLE QVariantList getRecommendedWords(int limit = 10) const;
    Q_INVOKABLE QString getMotivationalMessage() const;

signals:
    void newWordAdded(const QString& word);
    void masteryLevelChanged(const QString& word, int level);
    void dailyTargetMet();
    void reviewDue(const QString& word);
    void achievementUnlocked(const QString& achievement);

private slots:
    void checkReviews();

private:
    QMap<QString, LearningStats> m_wordStats;
    QTimer* m_reviewTimer;
    int m_dailyTarget = 10;

    void loadStats();
    void saveStats();
    int calculateNextInterval(const LearningStats& stats, bool remembered) const;
    double calculateDifficulty(const QString& word) const;
    void checkAchievements();
    QString getStatsFilePath() const;

    // 辅助函数
    int calculateReviewPriority(const LearningStats& stats) const;
    QString getReviewReason(const LearningStats& stats) const;
};

#endif // LEARNINGMANAGER_H