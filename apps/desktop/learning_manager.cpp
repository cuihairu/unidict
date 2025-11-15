#include "learning_manager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QRandomGenerator>

// LearningStats 实现
QJsonObject LearningStats::toJson() const
{
    QJsonObject obj;
    obj["word"] = word;
    obj["lookupCount"] = lookupCount;
    obj["correctAnswers"] = correctAnswers;
    obj["wrongAnswers"] = wrongAnswers;
    obj["firstLookup"] = firstLookup.toString(Qt::ISODate);
    obj["lastLookup"] = lastLookup.toString(Qt::ISODate);
    obj["nextReview"] = nextReview.toString(Qt::ISODate);
    obj["masteryLevel"] = masteryLevel;
    obj["difficulty"] = difficulty;
    obj["notes"] = notes;

    QJsonArray tagsArray;
    for (const QString& tag : tags) {
        tagsArray.append(tag);
    }
    obj["tags"] = tagsArray;

    return obj;
}

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
    , m_reviewTimer(new QTimer(this))
{
    loadStats();

    // 每小时检查一次复习提醒
    m_reviewTimer->setInterval(60 * 60 * 1000); // 1小时
    m_reviewTimer->setSingleShot(false);
    connect(m_reviewTimer, &QTimer::timeout, this, &LearningManager::checkReviews);
    m_reviewTimer->start();

    // 立即检查一次
    checkReviews();
}

void LearningManager::recordLookup(const QString& word, const QString& definition)
{
    if (word.isEmpty()) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it == m_wordStats.end()) {
        // 新单词
        LearningStats stats;
        stats.word = normalizedWord;
        stats.firstLookup = QDateTime::currentDateTime();
        stats.lastLookup = stats.firstLookup;
        stats.lookupCount = 1;
        stats.difficulty = calculateDifficulty(normalizedWord);

        // 初始复习间隔：1天
        stats.nextReview = QDateTime::currentDateTime().addDays(1);

        m_wordStats[normalizedWord] = stats;
        emit newWordAdded(normalizedWord);
    } else {
        // 已存在单词
        it->lookupCount++;
        it->lastLookup = QDateTime::currentDateTime();
    }

    saveStats();
    checkAchievements();
}

void LearningManager::recordTestResult(const QString& word, bool correct)
{
    if (word.isEmpty()) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it == m_wordStats.end()) {
        recordLookup(normalizedWord); // 确保单词存在记录
        it = m_wordStats.find(normalizedWord);
    }

    if (correct) {
        it->correctAnswers++;
        if (it->masteryLevel < 5) {
            it->masteryLevel = qMin(5, it->masteryLevel + 1);
            emit masteryLevelChanged(normalizedWord, it->masteryLevel);
        }
    } else {
        it->wrongAnswers++;
        it->masteryLevel = qMax(0, it->masteryLevel - 1);
        emit masteryLevelChanged(normalizedWord, it->masteryLevel);
    }

    // 根据测试结果调整复习间隔
    scheduleReview(normalizedWord, calculateNextInterval(*it, correct));

    saveStats();
    checkAchievements();
}

void LearningManager::updateMasteryLevel(const QString& word, int level)
{
    if (word.isEmpty() || level < 0 || level > 5) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it != m_wordStats.end()) {
        int oldLevel = it->masteryLevel;
        it->masteryLevel = level;

        if (oldLevel != level) {
            emit masteryLevelChanged(normalizedWord, level);
            saveStats();
        }
    }
}

void LearningManager::addWordNote(const QString& word, const QString& note)
{
    if (word.isEmpty()) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it == m_wordStats.end()) {
        recordLookup(normalizedWord); // 确保单词存在记录
        it = m_wordStats.find(normalizedWord);
    }

    it->notes = note;
    saveStats();
}

void LearningManager::addWordTag(const QString& word, const QString& tag)
{
    if (word.isEmpty() || tag.isEmpty()) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it == m_wordStats.end()) {
        recordLookup(normalizedWord);
        it = m_wordStats.find(normalizedWord);
    }

    if (!it->tags.contains(tag)) {
        it->tags.append(tag);
        saveStats();
    }
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

void LearningManager::scheduleReview(const QString& word, int intervalDays)
{
    if (word.isEmpty()) return;

    QString normalizedWord = word.toLower().trimmed();
    auto it = m_wordStats.find(normalizedWord);

    if (it != m_wordStats.end()) {
        if (intervalDays < 0) {
            intervalDays = calculateNextInterval(*it, true);
        }

        it->nextReview = QDateTime::currentDateTime().addDays(intervalDays);
        saveStats();
    }
}

void LearningManager::completeReview(const QString& word, bool remembered)
{
    recordTestResult(word, remembered);
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

void LearningManager::setDailyTarget(int wordCount)
{
    if (wordCount > 0) {
        m_dailyTarget = wordCount;
        saveStats();
    }
}

int LearningManager::getDailyTarget() const
{
    return m_dailyTarget;
}

bool LearningManager::isDailyTargetMet() const
{
    return getDailyStats()["targetMet"].toBool();
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

void LearningManager::saveStats()
{
    QString filePath = getStatsFilePath();
    QFile file(filePath);

    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Cannot save stats to" << filePath;
        return;
    }

    QJsonObject root;
    root["dailyTarget"] = m_dailyTarget;
    root["lastSaved"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray statsArray;
    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        statsArray.append(it->toJson());
    }
    root["wordStats"] = statsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
}

QString LearningManager::getStatsFilePath() const
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    return dataPath + "/learning_stats.json";
}

int LearningManager::calculateNextInterval(const LearningStats& stats, bool remembered) const
{
    // 基于艾宾浩斯遗忘曲线的复习间隔算法
    int baseInterval = 1; // 基础间隔1天

    if (remembered) {
        // 记住了，延长间隔
        switch (stats.masteryLevel) {
        case 0: return 1;   // 1天
        case 1: return 3;   // 3天
        case 2: return 7;   // 1周
        case 3: return 14;  // 2周
        case 4: return 30;  // 1月
        case 5: return 90;  // 3月
        default: return 1;
        }
    } else {
        // 没记住，缩短间隔
        return qMax(1, baseInterval / 2);
    }
}

double LearningManager::calculateDifficulty(const QString& word) const
{
    // 简单的难度计算：基于单词长度和字符复杂度
    double lengthFactor = qMin(word.length() / 8.0, 2.0);

    // 检查是否包含特殊字符或大写字母
    int complexChars = 0;
    for (const QChar& c : word) {
        if (!c.isLower() || !c.isLetter()) {
            complexChars++;
        }
    }
    double complexityFactor = complexChars / (double)word.length();

    return qMin(10.0, 1.0 + lengthFactor + complexityFactor * 3.0);
}

void LearningManager::checkReviews()
{
    QVariantList dueReviews = getDueReviews();

    for (const QVariant& review : dueReviews) {
        QVariantMap reviewMap = review.toMap();
        QString word = reviewMap["word"].toString();
        emit reviewDue(word);
    }
}

void LearningManager::checkAchievements()
{
    // 简单的成就系统
    static QSet<QString> unlockedAchievements;

    int totalWords = m_wordStats.size();
    QVariantMap dailyStats = getDailyStats();

    // 首个单词成就
    if (totalWords >= 1 && !unlockedAchievements.contains("first_word")) {
        unlockedAchievements.insert("first_word");
        emit achievementUnlocked("学习达人：查询了第一个单词！");
    }

    // 词汇里程碑
    if (totalWords >= 100 && !unlockedAchievements.contains("100_words")) {
        unlockedAchievements.insert("100_words");
        emit achievementUnlocked("词汇大师：掌握了100个单词！");
    }

    // 每日目标达成
    if (dailyStats["targetMet"].toBool() && !unlockedAchievements.contains("daily_target")) {
        unlockedAchievements.insert("daily_target");
        emit dailyTargetMet();
    }
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

QVariantList LearningManager::getAllStats() const
{
    QVariantList result;
    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        result.append(getWordStats(it->word));
    }
    return result;
}

QVariantMap LearningManager::getWeeklyStats() const
{
    QVariantMap result;
    QDateTime weekAgo = QDateTime::currentDateTime().addDays(-7);

    int weeklyLookups = 0;
    int newWordsWeek = 0;

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->firstLookup >= weekAgo) {
            newWordsWeek++;
        }
        if (it->lastLookup >= weekAgo) {
            weeklyLookups++;
        }
    }

    result["newWords"] = newWordsWeek;
    result["lookups"] = weeklyLookups;
    result["weekStart"] = weekAgo.date();

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

QVariantList LearningManager::getReviewSchedule(int days) const
{
    QVariantList result;
    QDateTime now = QDateTime::currentDateTime();
    QDateTime endDate = now.addDays(days);

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->nextReview.isValid() && it->nextReview >= now && it->nextReview <= endDate) {
            QVariantMap item;
            item["word"] = it->word;
            item["reviewDate"] = it->nextReview.date();
            item["masteryLevel"] = it->masteryLevel;
            result.append(item);
        }
    }

    return result;
}

QVariantList LearningManager::getAchievements() const
{
    QVariantList result;
    int totalWords = m_wordStats.size();

    // 静态成就列表
    QList<QPair<QString, int>> achievements = {
        {"初学者", 1}, {"学习者", 10}, {"进步者", 50},
        {"词汇达人", 100}, {"词汇专家", 500}, {"词汇大师", 1000}
    };

    for (const auto& achievement : achievements) {
        QVariantMap item;
        item["name"] = achievement.first;
        item["required"] = achievement.second;
        item["achieved"] = totalWords >= achievement.second;
        item["progress"] = qMin(100.0, (double)totalWords / achievement.second * 100);
        result.append(item);
    }

    return result;
}

QVariantList LearningManager::getRecommendedWords(int limit) const
{
    // 简单实现：返回最近查询但掌握程度较低的单词
    QVariantList result;
    QList<QPair<QString, QDateTime>> candidates;

    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        if (it->masteryLevel <= 3 && it->lookupCount >= 2) {
            candidates.append({it->word, it->lastLookup});
        }
    }

    // 按最近查询时间排序
    std::sort(candidates.begin(), candidates.end(),
              [](const QPair<QString, QDateTime>& a, const QPair<QString, QDateTime>& b) {
                  return a.second > b.second;
              });

    for (int i = 0; i < qMin(limit, candidates.size()); ++i) {
        result.append(candidates[i].first);
    }

    return result;
}

bool LearningManager::exportStats(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QJsonObject root;
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["totalWords"] = m_wordStats.size();
    root["dailyTarget"] = m_dailyTarget;

    QJsonArray statsArray;
    for (auto it = m_wordStats.constBegin(); it != m_wordStats.constEnd(); ++it) {
        statsArray.append(it->toJson());
    }
    root["wordStats"] = statsArray;

    QJsonDocument doc(root);
    file.write(doc.toJson());
    return true;
}

bool LearningManager::importStats(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject root = doc.object();

    QJsonArray statsArray = root["wordStats"].toArray();

    // 合并导入的数据
    for (const QJsonValue& value : statsArray) {
        LearningStats stats = LearningStats::fromJson(value.toObject());
        auto existing = m_wordStats.find(stats.word);

        if (existing == m_wordStats.end()) {
            // 新单词，直接添加
            m_wordStats[stats.word] = stats;
        } else {
            // 存在的单词，合并数据
            existing->lookupCount += stats.lookupCount;
            existing->correctAnswers += stats.correctAnswers;
            existing->wrongAnswers += stats.wrongAnswers;
            existing->masteryLevel = qMax(existing->masteryLevel, stats.masteryLevel);

            if (stats.firstLookup < existing->firstLookup) {
                existing->firstLookup = stats.firstLookup;
            }
            if (stats.lastLookup > existing->lastLookup) {
                existing->lastLookup = stats.lastLookup;
            }

            // 合并标签
            for (const QString& tag : stats.tags) {
                if (!existing->tags.contains(tag)) {
                    existing->tags.append(tag);
                }
            }
        }
    }

    saveStats();
    return true;
}

void LearningManager::resetStats()
{
    m_wordStats.clear();
    m_dailyTarget = 10;
    saveStats();
}