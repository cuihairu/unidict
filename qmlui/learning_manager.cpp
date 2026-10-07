#include "learning_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QRandomGenerator>

// LearningStats 实现
LearningStats LearningStats::fromJson(const QJsonObject& obj)
{
    LearningStats stats;
    stats.word = obj["word"].toString();
    stats.lookupCount = obj["lookupCount"].toInt();
    stats.correctAnswers = obj["correctAnswers"].toInt();
    stats.wrongAnswers = obj["wrongAnswers"].toInt();
    stats.firstLookup = QDateTime::fromString(obj["firstLookup"].toString(), Qt::ISODate);
    stats.lastLookup = QDateTime::fromString(obj["lastLookup"].toString(), Qt::ISODate);
    stats.nextReview = QDateTime::fromString(obj["nextReview"].toString(), Qt::ISODate);
    stats.masteryLevel = obj["masteryLevel"].toInt();
    stats.difficulty = obj["difficulty"].toDouble(1.0);
    stats.notes = obj["notes"].toString();

    const QJsonArray tagsArray = obj["tags"].toArray();
    for (const QJsonValue& tag : tagsArray) {
        stats.tags.append(tag.toString());
    }

    return stats;
}

// LearningManager 实现
LearningManager::LearningManager(QObject *parent)
    : QObject(parent)
{
    loadStats();
}

QVariantMap LearningManager::getWordStats(const QString& word) const
{
    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.constFind(normalizedWord);

    QVariantMap result;
    if (it != m_wordStats.constEnd()) {
        result["word"] = it->word;
        result["lookupCount"] = it->lookupCount;
        result["correctAnswers"] = it->correctAnswers;
        result["wrongAnswers"] = it->wrongAnswers;
        result["masteryLevel"] = it->masteryLevel;
        result["difficulty"] = it->difficulty;
        result["firstLookup"] = it->firstLookup;
        result["lastLookup"] = it->lastLookup;
        result["nextReview"] = it->nextReview;
        result["notes"] = it->notes;
        result["tags"] = QVariantList(it->tags.begin(), it->tags.end());

        // 计算准确率
        int total = it->correctAnswers + it->wrongAnswers;
        result["accuracy"] = total > 0 ? (double)it->correctAnswers / total * 100 : 0.0;
    }

    return result;
}

QVariantList LearningManager::getDueReviews() const
{
    QVariantList result;
    QDateTime now = QDateTime::currentDateTime();

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->nextReview.isValid() && it->nextReview <= now) {
            QVariantMap item;
            item["word"] = it->word;
            item["dueTime"] = it->nextReview;
            item["priority"] = calculateReviewPriority(*it);
            item["reason"] = getReviewReason(*it);
            result.append(item);
        }
    }

    return result;
}

QVariantMap LearningManager::getDailyStats() const
{
    QVariantMap result;
    QDateTime today = QDateTime::currentDateTime().date().startOfDay();
    QDateTime tomorrow = today.addDays(1);

    int todayLookups = 0;
    int newWordsToday = 0;
    int reviewsToday = 0;

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->firstLookup >= today && it->firstLookup < tomorrow) {
            newWordsToday++;
        }
        if (it->lastLookup >= today && it->lastLookup < tomorrow) {
            todayLookups++;
        }
    }

    // 计算今日到期复习
    QVariantList dueReviews = getDueReviews();
    for (const QVariant& review : dueReviews) {
        QVariantMap reviewMap = review.toMap();
        QDateTime dueTime = reviewMap["dueTime"].toDateTime();
        if (dueTime >= today && dueTime < tomorrow) {
            reviewsToday++;
        }
    }

    result["newWords"] = newWordsToday;
    result["lookups"] = todayLookups;
    result["reviews"] = reviewsToday;
    result["target"] = m_dailyTarget;
    result["targetMet"] = newWordsToday >= m_dailyTarget;
    result["date"] = today.date();

    return result;
}

QVariantMap LearningManager::getProgressStats() const
{
    QVariantMap result;

    int totalWords = m_wordStats.size();
    int masteredWords = 0; // 掌握程度 >= 4
    int weakWords = 0;     // 掌握程度 <= 2

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->masteryLevel >= 4) masteredWords++;
        if (it->masteryLevel <= 2) weakWords++;
    }

    result["totalWords"] = totalWords;
    result["masteredWords"] = masteredWords;
    result["weakWords"] = weakWords;
    result["masteryRate"] = totalWords > 0 ? (double)masteredWords / totalWords * 100 : 0.0;

    return result;
}

QVariantList LearningManager::getWeakWords(int limit) const
{
    QVariantList result;
    QList<QPair<QString, double>> weakWords;

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        // 计算弱项得分：错误率 + 低掌握程度 + 高难度
        int total = it->correctAnswers + it->wrongAnswers;
        double errorRate = total > 0 ? (double)it->wrongAnswers / total : 0.5;
        double masteryFactor = 1.0 - (it->masteryLevel / 5.0);
        double difficultyFactor = it->difficulty / 10.0;

        double weakScore = errorRate * 0.5 + masteryFactor * 0.3 + difficultyFactor * 0.2;

        if (weakScore > 0.3) { // 只考虑弱项得分较高的单词
            weakWords.append({it->word, weakScore});
        }
    }

    // 按弱项得分排序
    std::sort(weakWords.begin(), weakWords.end(),
              [](const QPair<QString, double>& a, const QPair<QString, double>& b) {
                  return a.second > b.second;
              });

    // 返回前N个
    for (int i = 0; i < qMin(limit, weakWords.size()); ++i) {
        QVariantMap item;
        item["word"] = weakWords[i].first;
        item["weakness"] = weakWords[i].second;
        item["stats"] = getWordStats(weakWords[i].first);
        result.append(item);
    }

    return result;
}

QString LearningManager::getMotivationalMessage() const
{
    QStringList messages = {
        "坚持学习，每天进步一点点！",
        "今天又掌握了新单词，继续保持！",
        "复习是巩固记忆的关键，加油！",
        "词汇量正在稳步提升，很棒！",
        "学习无止境，知识改变命运！"
    };

    int index = QRandomGenerator::global()->bounded(messages.size());
    return messages[index];
}

void LearningManager::loadStats()
{
    QString filePath = getStatsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No existing stats file, starting fresh";
        return;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    m_dailyTarget = root["dailyTarget"].toInt(10);

    QJsonArray statsArray = root["wordStats"].toArray();
    m_wordStats.clear();

    for (const QJsonValue& value : statsArray) {
        LearningStats stats = LearningStats::fromJson(value.toObject());
        m_wordStats[stats.word] = stats;
    }

    qDebug() << "Loaded stats for" << m_wordStats.size() << "words";
}

QString LearningManager::getStatsFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/learning_stats.json";
}

// 辅助函数实现

int LearningManager::calculateReviewPriority(const LearningStats& stats) const
{
    // 基于遗忘时间和掌握程度计算优先级
    QDateTime now = QDateTime::currentDateTime();
    int hoursOverdue = stats.nextReview.secsTo(now) / 3600;

    int priority = 1; // 默认优先级
    if (hoursOverdue > 24) priority = 3;      // 超期1天+
    if (hoursOverdue > 168) priority = 4;     // 超期1周+
    if (stats.masteryLevel <= 2) priority += 1; // 低掌握程度

    return qMin(5, priority);
}

QString LearningManager::getReviewReason(const LearningStats& stats) const
{
    QDateTime now = QDateTime::currentDateTime();
    int hoursOverdue = stats.nextReview.secsTo(now) / 3600;

    if (hoursOverdue > 168) {
        return "长期未复习";
    } else if (hoursOverdue > 24) {
        return "昨日遗留";
    } else if (stats.masteryLevel <= 2) {
        return "掌握程度较低";
    } else {
        return "定期复习";
    }
}
