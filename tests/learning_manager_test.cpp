// LearningManager（qmlui）全量测试。此前该文件 0% 覆盖：
// 学习记录、答题统计、掌握度、标签/笔记、日/周/进度统计、艾宾浩斯复习
// 调度、成就、导入导出——24 个 Q_INVOKABLE 一个测试都没有，任何静默劣化
// 都不会被发现。
//
// 统计文件落在 QStandardPaths::AppDataLocation，用 test mode 把它导到
// 临时目录，避免污染真实用户数据。

#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

#include "learning_manager.h"

class LearningManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 记录
    void recordLookup_createsAndUpdates();
    void recordLookup_normalizesAndEmitsNewWord();
    void recordLookup_emptyWordIgnored();

    void recordTestResult_createsWordIfMissing();
    void recordTestResult_correctRaisesMasteryAndCapsAt5();
    void recordTestResult_wrongLowersMasteryAndFloorsAt0();
    void completeReview_delegatesToRecordTestResult();

    void updateMasteryLevel_validatesRangeAndEmits();
    void addWordNote_createsWordIfMissing();
    void addWordTag_dedupesAndValidates();

    // 查询
    void getWordStats_reportsAccuracyAndEmptyForUnknown();
    void getAllStats_coversEveryWord();
    void getWordStats_ignoresCaseAndWhitespace();

    // 复习调度
    void scheduleReview_usesEbbinghausTableWhenIntervalNegative();
    void scheduleReview_explicitIntervalWins();
    void scheduleReview_unknownWordIsNoop();
    void recordTestResult_schedulesWithCalculatedInterval();
    void getDueReviews_onlyPastDue();
    void getReviewSchedule_onlyUpcomingWithinWindow();
    void reviewPriorityAndReason_buckets();

    // 统计面板
    void dailyStats_countsNewLookupsAndReviews();
    void dailyStats_targetMetAndTargetSetter();
    void isDailyTargetMet_matchesDailyStats();
    void weeklyStats_countsRecentActivity();
    void progressStats_classifiesMastery();
    void emptyManager_reportsZerosNotNaN();

    // 弱项 / 推荐
    void weakWords_ranksAndTruncates();
    void recommendedWords_filtersAndSorts();

    // 成就
    void achievement_firstWordFiresOnce();
    void achievement_100WordsFires();
    void achievement_dailyTargetFires();
    void achievement_resetStatsClearsUnlockState();
    void achievement_perInstanceNotProcessGlobal();
    void achievements_listProgressSaturates();

    // 持久化
    void statsPersistAcrossInstances();
    void loadStats_missingFileStartsFresh();
    void loadStats_corruptFileDoesNotCrash();
    void saveStats_unwritablePathWarns();

    // 导入导出
    void exportImport_roundTrip();
    void importStats_mergesCountersAndTimestamps();
    void importStats_addsNewWordsAndMergesTags();
    void importStats_missingFileFails();

    void motivationalMessage_fromKnownPool();

private:
    QTemporaryDir m_dir;
};

void LearningManagerTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    // 把 AppDataLocation 导到临时目录：测试不该写进真实用户目录
    QStandardPaths::setTestModeEnabled(true);
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    QFile::remove(dataDir + "/learning_stats.json");
}

void LearningManagerTest::cleanupTestCase()
{
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QFile::remove(dataDir + "/learning_stats.json");
}

// 每个用例都从干净的统计文件开始：LearningManager 在构造时 loadStats()
static void resetStatsFile()
{
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString path = dataDir + "/learning_stats.json";
    QFile::remove(path);
    // saveStats_unwritablePathWarns 用例会把它换成目录来触发写失败；
    // 残留的目录会让后续用例的 saveStats 全部告警，所以一并清掉。
    QDir(path).removeRecursively();
}

// ---------------------------------------------------------------- 记录

void LearningManagerTest::recordLookup_createsAndUpdates()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("hello");
    QVariantMap s = m.getWordStats("hello");
    QCOMPARE(s.value("word").toString(), QString("hello"));
    QCOMPARE(s.value("lookupCount").toInt(), 1);
    QVERIFY(s.value("firstLookup").toDateTime().isValid());
    QCOMPARE(s.value("firstLookup").toDateTime(), s.value("lastLookup").toDateTime());
    // 新词默认安排在 1 天后复习
    QVERIFY(s.value("nextReview").toDateTime() > QDateTime::currentDateTime());

    // 第二次查：计数 +1，firstLookup 不动
    const QDateTime first = s.value("firstLookup").toDateTime();
    m.recordLookup("hello");
    s = m.getWordStats("hello");
    QCOMPARE(s.value("lookupCount").toInt(), 2);
    QCOMPARE(s.value("firstLookup").toDateTime(), first);
}

void LearningManagerTest::recordLookup_normalizesAndEmitsNewWord()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::newWordAdded);

    m.recordLookup("  HeLLo  ");
    QCOMPARE(spy.count(), 1);
    // 归一化：去空白 + 小写
    QCOMPARE(spy.first().first().toString(), QString("hello"));
    QVERIFY(m.getWordStats("hello").contains("word"));
    // 归一化后大小写不同的查询命中同一条
    QCOMPARE(m.getWordStats("HELLO").value("lookupCount").toInt(), 1);
    m.recordLookup("HeLLo");
    QCOMPARE(m.getWordStats("hello").value("lookupCount").toInt(), 2);
    // 再次记录不重复发 newWordAdded
    QCOMPARE(spy.count(), 1);
}

void LearningManagerTest::recordLookup_emptyWordIgnored()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::newWordAdded);
    m.recordLookup("");
    QCOMPARE(spy.count(), 0);
    QVERIFY(m.getWordStats("").isEmpty());
    QVERIFY(m.getAllStats().isEmpty());
}

// ---------------------------------------------------------------- 答题

void LearningManagerTest::recordTestResult_createsWordIfMissing()
{
    resetStatsFile();
    LearningManager m;
    m.recordTestResult("brandnew", true);
    const QVariantMap s = m.getWordStats("brandnew");
    QCOMPARE(s.value("correctAnswers").toInt(), 1);
    QCOMPARE(s.value("lookupCount").toInt(), 1);   // 内部补了一次 recordLookup
    QCOMPARE(s.value("masteryLevel").toInt(), 1);
}

void LearningManagerTest::recordTestResult_correctRaisesMasteryAndCapsAt5()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::masteryLevelChanged);
    for (int i = 0; i < 8; ++i) {
        m.recordTestResult("word", true);
    }
    QCOMPARE(m.getWordStats("word").value("masteryLevel").toInt(), 5);
    QCOMPARE(m.getWordStats("word").value("correctAnswers").toInt(), 8);
    QVERIFY(spy.count() >= 1);
    // 到顶之后不再发信号（没有变化）
    const int before = spy.count();
    m.recordTestResult("word", true);
    QCOMPARE(spy.count(), before);
}

void LearningManagerTest::recordTestResult_wrongLowersMasteryAndFloorsAt0()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::masteryLevelChanged);
    for (int i = 0; i < 8; ++i) {
        m.recordTestResult("word", false);
    }
    QCOMPARE(m.getWordStats("word").value("masteryLevel").toInt(), 0);
    QCOMPARE(m.getWordStats("word").value("wrongAnswers").toInt(), 8);
    QVERIFY(spy.count() >= 1);

    // 准确率
    QCOMPARE(m.getWordStats("word").value("accuracy").toDouble(), 0.0);
    m.recordTestResult("word", true);
    const QVariantMap s = m.getWordStats("word");
    QCOMPARE(s.value("correctAnswers").toInt(), 1);
    QCOMPARE(s.value("wrongAnswers").toInt(), 8);
    QVERIFY(s.value("accuracy").toDouble() > 10.0);
    QVERIFY(s.value("accuracy").toDouble() < 12.0);
}

void LearningManagerTest::completeReview_delegatesToRecordTestResult()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("rec");
    m.completeReview("rec", true);
    QCOMPARE(m.getWordStats("rec").value("correctAnswers").toInt(), 1);
    m.completeReview("rec", false);
    QCOMPARE(m.getWordStats("rec").value("wrongAnswers").toInt(), 1);
}

void LearningManagerTest::updateMasteryLevel_validatesRangeAndEmits()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("w");
    QSignalSpy spy(&m, &LearningManager::masteryLevelChanged);

    m.updateMasteryLevel("w", 3);
    QCOMPARE(m.getWordStats("w").value("masteryLevel").toInt(), 3);
    QCOMPARE(spy.count(), 1);

    // 相同等级不发信号也不落盘
    m.updateMasteryLevel("w", 3);
    QCOMPARE(spy.count(), 1);

    // 越界被拒
    m.updateMasteryLevel("w", -1);
    m.updateMasteryLevel("w", 6);
    m.updateMasteryLevel("", 2);
    QCOMPARE(m.getWordStats("w").value("masteryLevel").toInt(), 3);
    QCOMPARE(spy.count(), 1);

    // 未知单词：静默无操作
    m.updateMasteryLevel("ghost", 2);
    QCOMPARE(spy.count(), 1);
}

void LearningManagerTest::addWordNote_createsWordIfMissing()
{
    resetStatsFile();
    LearningManager m;
    m.addWordNote("noted", "first note");
    QCOMPARE(m.getWordStats("noted").value("notes").toString(),
             QString("first note"));
    m.addWordNote("noted", "second note");
    QCOMPARE(m.getWordStats("noted").value("notes").toString(),
             QString("second note"));
    // 空单词被拒
    m.addWordNote("", "x");
}

void LearningManagerTest::addWordTag_dedupesAndValidates()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::newWordAdded);

    m.addWordTag("tagged", "verb");
    QCOMPARE(spy.count(), 1);  // 内部补的 recordLookup
    QVariantMap s = m.getWordStats("tagged");
    QCOMPARE(s.value("tags").toStringList(), QStringList{"verb"});

    // 同标签不重复加、不重复落盘
    m.addWordTag("tagged", "verb");
    s = m.getWordStats("tagged");
    QCOMPARE(s.value("tags").toStringList().size(), 1);

    // 不同标签追加；大小写按原样存（标签不做归一）
    m.addWordTag("tagged", "Noun");
    s = m.getWordStats("tagged");
    QCOMPARE(s.value("tags").toStringList().size(), 2);
    QVERIFY(s.value("tags").toStringList().contains("Noun"));

    // 空标签 / 空单词被拒
    m.addWordTag("tagged", "");
    m.addWordTag("", "x");
    QCOMPARE(m.getWordStats("tagged").value("tags").toStringList().size(), 2);
    QCOMPARE(spy.count(), 1);
}

// ---------------------------------------------------------------- 查询

void LearningManagerTest::getWordStats_reportsAccuracyAndEmptyForUnknown()
{
    resetStatsFile();
    LearningManager m;
    QVERIFY(m.getWordStats("nothing").isEmpty());

    m.recordLookup("x");
    // 没有答题 → accuracy 0（不是 NaN 也不是除零）
    QCOMPARE(m.getWordStats("x").value("accuracy").toDouble(), 0.0);
    QVERIFY(m.getWordStats("x").contains("difficulty"));
    QVERIFY(m.getWordStats("x").contains("firstLookup"));
    QVERIFY(m.getWordStats("x").contains("nextReview"));
}

void LearningManagerTest::getAllStats_coversEveryWord()
{
    resetStatsFile();
    LearningManager m;
    QVERIFY(m.getAllStats().isEmpty());
    m.recordLookup("a");
    m.recordLookup("b");
    m.recordLookup("c");
    const QVariantList all = m.getAllStats();
    QCOMPARE(all.size(), 3);
    QStringList words;
    for (const QVariant& v : all) words << v.toMap().value("word").toString();
    words.sort();
    QCOMPARE(words, QStringList({"a", "b", "c"}));
}

void LearningManagerTest::getWordStats_ignoresCaseAndWhitespace()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("Mixed");
    QCOMPARE(m.getWordStats("  MIXED  ").value("word").toString(),
             QString("mixed"));
}

// ---------------------------------------------------------------- 复习调度

void LearningManagerTest::scheduleReview_usesEbbinghausTableWhenIntervalNegative()
{
    resetStatsFile();
    LearningManager m;
    // intervalDays 默认 -1 → 走 calculateNextInterval(mastery, true)
    // 艾宾浩斯表：mastery 0→1天 1→3 2→7 3→14 4→30 5→90
    const int expected[6] = {1, 3, 7, 14, 30, 90};
    for (int level = 0; level <= 5; ++level) {
        m.recordLookup("w");
        m.updateMasteryLevel("w", level);
        m.scheduleReview("w", -1);
        const QDateTime due = m.getWordStats("w").value("nextReview").toDateTime();
        const int days = QDateTime::currentDateTime().daysTo(due);
        // 天数在跨夏令时/边界时可能差 1，放宽到 ±1
        QVERIFY2(std::abs(days - expected[level]) <= 1,
                 qPrintable(QStringLiteral("level=%1 got=%2 want=%3")
                                .arg(level).arg(days).arg(expected[level])));
    }
}

void LearningManagerTest::scheduleReview_explicitIntervalWins()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("w");
    m.updateMasteryLevel("w", 5);   // 记住的话表里是 90 天
    m.scheduleReview("w", 2);        // 显式 2 天应当胜出
    const int days = QDateTime::currentDateTime()
                         .daysTo(m.getWordStats("w").value("nextReview").toDateTime());
    QVERIFY(std::abs(days - 2) <= 1);
}

void LearningManagerTest::scheduleReview_unknownWordIsNoop()
{
    resetStatsFile();
    LearningManager m;
    m.scheduleReview("ghost", 1);
    m.scheduleReview("", 1);
    QVERIFY(m.getWordStats("ghost").isEmpty());
    QVERIFY(m.getAllStats().isEmpty());
}

void LearningManagerTest::recordTestResult_schedulesWithCalculatedInterval()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("w");
    // recordTestResult 先涨掌握度、再按**涨完之后**的等级排下次复习：
    // mastery 1 → 答对变 2 → 表里 7 天。
    m.updateMasteryLevel("w", 1);
    m.recordTestResult("w", true);
    QCOMPARE(m.getWordStats("w").value("masteryLevel").toInt(), 2);
    const int days = QDateTime::currentDateTime()
                         .daysTo(m.getWordStats("w").value("nextReview").toDateTime());
    QVERIFY2(std::abs(days - 7) <= 1, qPrintable(QStringLiteral("days=%1").arg(days)));

    // 答错：掌握度先降再排期，间隔不因"没记住"而变（艾宾浩斯表只对
    // remembered=true 分档，false 走 qMax(1, 1/2) = 1 天）
    m.recordTestResult("w", false);
    QCOMPARE(m.getWordStats("w").value("masteryLevel").toInt(), 1);
    const int days2 = QDateTime::currentDateTime()
                          .daysTo(m.getWordStats("w").value("nextReview").toDateTime());
    QVERIFY2(std::abs(days2 - 1) <= 1, qPrintable(QStringLiteral("days2=%1").arg(days2)));
}

void LearningManagerTest::getDueReviews_onlyPastDue()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("fresh");
    m.recordLookup("due");
    m.scheduleReview("due", 0);   // 立刻到期
    QVERIFY(std::abs(QDateTime::currentDateTime()
                         .daysTo(m.getWordStats("due").value("nextReview").toDateTime())) <= 1);

    const QVariantList due = m.getDueReviews();
    QCOMPARE(due.size(), 1);
    QCOMPARE(due.first().toMap().value("word").toString(), QString("due"));
    QVERIFY(due.first().toMap().contains("dueTime"));
    QVERIFY(due.first().toMap().contains("priority"));
    QVERIFY(!due.first().toMap().value("reason").toString().isEmpty());
}

void LearningManagerTest::getReviewSchedule_onlyUpcomingWithinWindow()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("soon");
    m.recordLookup("later");
    m.recordLookup("far");
    m.scheduleReview("soon", 2);
    m.scheduleReview("later", 10);
    m.scheduleReview("far", 100);

    // "已到期"的条目用 importStats 注入：scheduleReview(w, 0) 落在
    // "当下"，而 getReviewSchedule 内部重新取 now，同一毫秒内 nextReview
    // >= now 可能成立——那样这条用例会变成时序相关的假绿/假红。
    const QJsonObject expired = [] {
        QJsonObject w;
        w["word"] = "expired";
        w["lookupCount"] = 2;
        w["masteryLevel"] = 1;
        w["firstLookup"] = QDateTime::currentDateTime().addDays(-5)
                              .toString(Qt::ISODate);
        w["lastLookup"] = QDateTime::currentDateTime().addDays(-1)
                             .toString(Qt::ISODate);
        w["nextReview"] = QDateTime::currentDateTime().addDays(-1)
                              .toString(Qt::ISODate);
        w["difficulty"] = 1.0;
        return w;
    }();
    QJsonArray arr;
    arr.append(expired);
    const QString seed = m_dir.filePath("schedule_seed.json");
    {
        QJsonObject root;
        root["wordStats"] = arr;
        QFile f(seed);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
    }
    QVERIFY(m.importStats(seed));

    const QVariantList sched = m.getReviewSchedule(7);
    QStringList words;
    for (const QVariant& v : sched) words << v.toMap().value("word").toString();
    words.sort();
    // 2 天内的 soon 进 7 天窗口；10 天/100 天的不进；已到期的也不进
    QCOMPARE(words, QStringList({"soon"}));
    QVERIFY(sched.first().toMap().contains("reviewDate"));
    QVERIFY(sched.first().toMap().contains("masteryLevel"));
    // 窗口放到 30 天：later(+10) 进来，far(+100) 仍不进，expired 仍不进
    const QVariantList wide = m.getReviewSchedule(30);
    QStringList wideWords;
    for (const QVariant& v : wide) wideWords << v.toMap().value("word").toString();
    wideWords.sort();
    QCOMPARE(wideWords, QStringList({"later", "soon"}));
    // 窗口放到 120 天：far 也进来
    QCOMPARE(m.getReviewSchedule(120).size(), 3);
    // 窗口为负 → 空
    QVERIFY(m.getReviewSchedule(0).isEmpty());
    QVERIFY(m.getReviewSchedule(-5).isEmpty());
}

void LearningManagerTest::reviewPriorityAndReason_buckets()
{
    resetStatsFile();
    LearningManager m;
    // nextReview 是私有状态，公开 API 里 scheduleReview 的负数会被重算成
    // 艾宾浩斯表值（永远是未来）。要构造"超期 N 天"的条目，用 importStats
    // 注入带过去时间戳的记录——fromJson 会恢复 nextReview/masteryLevel。
    const QDateTime now = QDateTime::currentDateTime();
    struct Case {
        const char* word;
        int mastery;
        int overdueDays;
    };
    const Case cases[] = {
        {"long_ago_low",  0, 200},  // 超期>1周  + 掌握低 → 优先级 3+1+1 / 长期未复习
        {"yesterday_low", 0, 2},    // 超期>1天  + 掌握低 → 优先级 3+1   / 昨日遗留
        {"yesterday_high",5, 2},    // 超期>1天  + 掌握高 → 优先级 3     / 昨日遗留
        {"today_low",    0, 0},    // 未超期    + 掌握低 → 优先级 1+1   / 掌握程度较低
        {"today_high",   5, 0},    // 未超期    + 掌握高 → 优先级 1     / 定期复习
    };

    QJsonArray arr;
    for (const auto& c : cases) {
        QJsonObject w;
        w["word"] = QString::fromLatin1(c.word);
        w["lookupCount"] = 3;
        w["masteryLevel"] = c.mastery;
        w["firstLookup"] = now.addDays(-300).toString(Qt::ISODate);
        w["lastLookup"] = now.addDays(-1).toString(Qt::ISODate);
        w["nextReview"] = now.addDays(-c.overdueDays).toString(Qt::ISODate);
        w["difficulty"] = 1.0;
        arr.append(w);
    }
    const QString path = m_dir.filePath("due_seed.json");
    {
        QJsonObject root;
        root["wordStats"] = arr;
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
    }
    QVERIFY(m.importStats(path));

    QHash<QString, QVariantMap> byWord;
    for (const QVariant& v : m.getDueReviews()) {
        const QVariantMap mm = v.toMap();
        byWord.insert(mm.value("word").toString(), mm);
    }
    QCOMPARE(byWord.size(), 5);

    QCOMPARE(byWord["long_ago_low"].value("priority").toInt(), 5);
    QCOMPARE(byWord["long_ago_low"].value("reason").toString(),
             QString("长期未复习"));
    QCOMPARE(byWord["yesterday_low"].value("priority").toInt(), 4);
    QCOMPARE(byWord["yesterday_low"].value("reason").toString(),
             QString("昨日遗留"));
    QCOMPARE(byWord["yesterday_high"].value("priority").toInt(), 3);
    QCOMPARE(byWord["yesterday_high"].value("reason").toString(),
             QString("昨日遗留"));
    QCOMPARE(byWord["today_low"].value("priority").toInt(), 2);
    QCOMPARE(byWord["today_low"].value("reason").toString(),
             QString("掌握程度较低"));
    QCOMPARE(byWord["today_high"].value("priority").toInt(), 1);
    QCOMPARE(byWord["today_high"].value("reason").toString(),
             QString("定期复习"));
}

// ---------------------------------------------------------------- 统计面板

void LearningManagerTest::dailyStats_countsNewLookupsAndReviews()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("one");
    m.recordLookup("two");
    m.recordLookup("three");
    m.recordLookup("one");  // 第二次查询不增加 newWords

    const QVariantMap d = m.getDailyStats();
    QCOMPARE(d.value("newWords").toInt(), 3);
    QCOMPARE(d.value("lookups").toInt(), 3);
    QCOMPARE(d.value("reviews").toInt(), 0);   // 还没到期
    QCOMPARE(d.value("target").toInt(), 10);
    QCOMPARE(d.value("targetMet").toBool(), false);
    QVERIFY(d.value("date").isValid());

    // 让一个词立刻到期 → 今日 reviews 计入
    m.scheduleReview("one", 0);
    const QVariantMap d2 = m.getDailyStats();
    QVERIFY(d2.value("reviews").toInt() >= 1);
}

void LearningManagerTest::dailyStats_targetMetAndTargetSetter()
{
    resetStatsFile();
    LearningManager m;
    QCOMPARE(m.getDailyTarget(), 10);
    m.setDailyTarget(2);
    QCOMPARE(m.getDailyTarget(), 2);
    QVERIFY(!m.isDailyTargetMet());

    m.recordLookup("a");
    QVERIFY(!m.isDailyTargetMet());
    m.recordLookup("b");
    QVERIFY(m.isDailyTargetMet());
    QCOMPARE(m.getDailyStats().value("targetMet").toBool(), true);

    // 非正数被拒（不覆盖已有目标）
    m.setDailyTarget(0);
    m.setDailyTarget(-5);
    QCOMPARE(m.getDailyTarget(), 2);
}

void LearningManagerTest::isDailyTargetMet_matchesDailyStats()
{
    resetStatsFile();
    LearningManager m;
    m.setDailyTarget(1);
    QCOMPARE(m.isDailyTargetMet(),
             m.getDailyStats().value("targetMet").toBool());
    m.recordLookup("x");
    QCOMPARE(m.isDailyTargetMet(), true);
}

void LearningManagerTest::weeklyStats_countsRecentActivity()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("a");
    m.recordLookup("b");
    m.recordLookup("a");
    const QVariantMap w = m.getWeeklyStats();
    QCOMPARE(w.value("newWords").toInt(), 2);
    QCOMPARE(w.value("lookups").toInt(), 2);
    QVERIFY(w.value("weekStart").isValid());
}

void LearningManagerTest::progressStats_classifiesMastery()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("mastered");
    m.updateMasteryLevel("mastered", 4);
    m.recordLookup("mid");
    m.updateMasteryLevel("mid", 3);
    m.recordLookup("weak");
    m.updateMasteryLevel("weak", 1);

    const QVariantMap p = m.getProgressStats();
    QCOMPARE(p.value("totalWords").toInt(), 3);
    QCOMPARE(p.value("masteredWords").toInt(), 1);   // >=4
    QCOMPARE(p.value("weakWords").toInt(), 1);       // <=2
    QVERIFY(std::abs(p.value("masteryRate").toDouble() - 100.0 / 3.0) < 0.01);
}

void LearningManagerTest::emptyManager_reportsZerosNotNaN()
{
    resetStatsFile();
    LearningManager m;
    const QVariantMap p = m.getProgressStats();
    QCOMPARE(p.value("totalWords").toInt(), 0);
    // 空数据时 masteryRate 必须是 0 而不是 NaN
    QCOMPARE(p.value("masteryRate").toDouble(), 0.0);
    QVERIFY(m.getDailyStats().contains("targetMet"));
    QVERIFY(m.getWeakWords().isEmpty());
    QVERIFY(m.getRecommendedWords().isEmpty());
    QVERIFY(m.getDueReviews().isEmpty());
    QVERIFY(m.getReviewSchedule().isEmpty());
}

// ---------------------------------------------------------------- 弱项/推荐

void LearningManagerTest::weakWords_ranksAndTruncates()
{
    resetStatsFile();
    LearningManager m;
    // 强项：掌握 5、无错题 → 弱项得分低，被 0.3 门槛滤掉
    m.recordLookup("strong");
    m.updateMasteryLevel("strong", 5);
    m.recordTestResult("strong", true);

    // 弱项：错得多、掌握低
    m.recordLookup("veryweak");
    for (int i = 0; i < 4; ++i) m.recordTestResult("veryweak", false);

    // 中等
    m.recordLookup("mid");
    m.recordTestResult("mid", true);

    const QVariantList weak = m.getWeakWords(10);
    QVERIFY(!weak.isEmpty());
    // 降序
    double prev = 1e9;
    for (const QVariant& v : weak) {
        const QVariantMap mm = v.toMap();
        QVERIFY(mm.contains("word"));
        QVERIFY(mm.contains("weakness"));
        QVERIFY(mm.contains("stats"));
        QVERIFY(mm.value("weakness").toDouble() <= prev + 1e-9);
        prev = mm.value("weakness").toDouble();
    }
    // 最弱的排第一，且不包含 strong
    QCOMPARE(weak.first().toMap().value("word").toString(), QString("veryweak"));
    for (const QVariant& v : weak) {
        QVERIFY(v.toMap().value("word").toString() != QString("strong"));
    }
    // 截断
    QVERIFY(m.getWeakWords(1).size() <= 1);
    QVERIFY(m.getWeakWords(0).isEmpty());
    QVERIFY(m.getWeakWords(-3).isEmpty());
}

void LearningManagerTest::recommendedWords_filtersAndSorts()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("once");            // lookupCount 1 → 不该进推荐
    m.recordLookup("twice");
    m.recordLookup("twice");
    m.recordLookup("mastered");
    m.recordLookup("mastered");
    m.recordLookup("mastered");
    m.updateMasteryLevel("mastered", 5);  // mastery 5 → 不该进推荐

    const QVariantList rec = m.getRecommendedWords();
    QStringList words;
    for (const QVariant& v : rec) words << v.toString();
    QCOMPARE(words, QStringList({"twice"}));
    // limit 截断与 <=0
    QVERIFY(m.getRecommendedWords(0).isEmpty());
    QVERIFY(m.getRecommendedWords(-1).isEmpty());
    QCOMPARE(m.getRecommendedWords(5).size(), 1);
}

// ---------------------------------------------------------------- 成就

void LearningManagerTest::achievement_firstWordFiresOnce()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::achievementUnlocked);
    m.recordLookup("a");
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.first().first().toString().contains("第一个单词"));
    m.recordLookup("b");
    m.recordLookup("c");
    QCOMPARE(spy.count(), 1);   // 不重复解锁
}

void LearningManagerTest::achievement_100WordsFires()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::achievementUnlocked);
    for (int i = 0; i < 100; ++i) {
        m.recordLookup(QStringLiteral("w%1").arg(i));
    }
    QStringList all;
    for (const QList<QVariant>& args : spy) all << args.first().toString();
    QVERIFY2(all.join("|").contains("100个单词"),
             qPrintable(QStringLiteral("成就信号: %1").arg(all.join("|"))));
}

void LearningManagerTest::achievement_dailyTargetFires()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::dailyTargetMet);
    m.setDailyTarget(1);
    QVERIFY(!m.isDailyTargetMet());
    m.recordLookup("a");
    QCOMPARE(spy.count(), 1);
    m.recordLookup("b");
    QCOMPARE(spy.count(), 1);   // 不重复
}

void LearningManagerTest::achievement_resetStatsClearsUnlockState()
{
    resetStatsFile();
    LearningManager m;
    QSignalSpy spy(&m, &LearningManager::achievementUnlocked);
    m.recordLookup("a");
    QCOMPARE(spy.count(), 1);

    m.resetStats();
    QVERIFY(m.getAllStats().isEmpty());
    QCOMPARE(m.getDailyTarget(), 10);

    // 重置后重新开始：成就应当能再次解锁
    m.recordLookup("a");
    QCOMPARE(spy.count(), 2);
}

void LearningManagerTest::achievement_perInstanceNotProcessGlobal()
{
    resetStatsFile();
    // 回归：unlockedAchievements 原先是函数内 static（进程级），
    // 第二个实例会看不到任何成就信号。
    LearningManager m1;
    QSignalSpy spy1(&m1, &LearningManager::achievementUnlocked);
    m1.recordLookup("a");
    QCOMPARE(spy1.count(), 1);

    LearningManager m2;
    QSignalSpy spy2(&m2, &LearningManager::achievementUnlocked);
    m2.recordLookup("b");
    QCOMPARE(spy2.count(), 1);   // 各实例独立解锁
}

void LearningManagerTest::achievements_listProgressSaturates()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("a");
    const QVariantList ach = m.getAchievements();
    QCOMPARE(ach.size(), 6);
    QCOMPARE(ach.first().toMap().value("name").toString(), QString("初学者"));
    QCOMPARE(ach.first().toMap().value("required").toInt(), 1);
    QCOMPARE(ach.first().toMap().value("achieved").toBool(), true);
    // 1 个单词：后面的都未达成，progress 很小但不超过 100
    for (int i = 1; i < ach.size(); ++i) {
        const QVariantMap mm = ach[i].toMap();
        QCOMPARE(mm.value("achieved").toBool(), false);
        QVERIFY(mm.value("progress").toDouble() > 0.0);
        QVERIFY(mm.value("progress").toDouble() < 100.0);
    }
    // 远超门槛时 progress 饱和在 100
    LearningManager big;
    for (int i = 0; i < 10; ++i) big.recordLookup(QStringLiteral("w%1").arg(i));
    for (const QVariant& v : big.getAchievements()) {
        QVERIFY(v.toMap().value("progress").toDouble() <= 100.0);
    }
}

// ---------------------------------------------------------------- 持久化

void LearningManagerTest::statsPersistAcrossInstances()
{
    resetStatsFile();
    {
        LearningManager m;
        m.setDailyTarget(7);
        m.recordLookup("keepme");
        m.addWordNote("keepme", "note text");
        m.addWordTag("keepme", "tagA");
        m.recordTestResult("keepme", true);
    }
    // 新实例构造时 loadStats() 读回磁盘
    LearningManager m2;
    const QVariantMap s = m2.getWordStats("keepme");
    QCOMPARE(s.value("word").toString(), QString("keepme"));
    QCOMPARE(s.value("notes").toString(), QString("note text"));
    QCOMPARE(s.value("tags").toStringList(), QStringList{"tagA"});
    QCOMPARE(s.value("correctAnswers").toInt(), 1);
    QCOMPARE(s.value("masteryLevel").toInt(), 1);
    QCOMPARE(m2.getDailyTarget(), 7);
}

void LearningManagerTest::loadStats_missingFileStartsFresh()
{
    resetStatsFile();
    LearningManager m;
    QVERIFY(m.getAllStats().isEmpty());
    QCOMPARE(m.getDailyTarget(), 10);
}

void LearningManagerTest::loadStats_corruptFileDoesNotCrash()
{
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    const QString path = dataDir + "/learning_stats.json";
    QFile::remove(path);
    QDir(path).removeRecursively();
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ this is not valid json");
    }
    LearningManager m;
    QVERIFY(m.getAllStats().isEmpty());
    // 仍可正常写入
    m.recordLookup("after");
    QCOMPARE(m.getWordStats("after").value("lookupCount").toInt(), 1);
    // 空文件
    QFile::remove(path);
    { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); }
    LearningManager m2;
    QVERIFY(m2.getAllStats().isEmpty());

    // 合法 JSON 但结构不符（wordStats 不是数组）
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"dailyTarget\": 3, \"wordStats\": \"not-an-array\"}");
    }
    LearningManager m3;
    QVERIFY(m3.getAllStats().isEmpty());
    QCOMPARE(m3.getDailyTarget(), 3);
}

void LearningManagerTest::saveStats_unwritablePathWarns()
{
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    // 把 learning_stats.json 变成一个目录 → ofstream 打开必败 → saveStats
    // 走告警分支而不是崩
    const QString path = dataDir + "/learning_stats.json";
    QFile::remove(path);
    QVERIFY(QDir().mkpath(path));
    {
        LearningManager m;
        m.recordLookup("stillworks");   // 不崩、内存态仍然更新
        QCOMPARE(m.getWordStats("stillworks").value("lookupCount").toInt(), 1);
    }
    // 恢复成正常文件，别影响后续用例
    QDir(path).removeRecursively();
}

// ---------------------------------------------------------------- 导入导出

void LearningManagerTest::exportImport_roundTrip()
{
    resetStatsFile();
    LearningManager m;
    m.setDailyTarget(4);
    m.recordLookup("a");
    m.addWordNote("a", "n");
    m.addWordTag("a", "t");
    m.recordTestResult("a", true);

    const QString out = m_dir.filePath("export.json");
    QVERIFY(m.exportStats(out));
    QVERIFY(QFile::exists(out));

    // 导出文件是合法 JSON 且含关键字段
    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    QCOMPARE(root.value("totalWords").toInt(), 1);
    QCOMPARE(root.value("dailyTarget").toInt(), 4);
    QVERIFY(!root.value("exportDate").toString().isEmpty());
    QCOMPARE(root.value("wordStats").toArray().size(), 1);
}

void LearningManagerTest::importStats_mergesCountersAndTimestamps()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("shared");
    m.recordTestResult("shared", true);      // 1 次正确, mastery 1
    const QDateTime origFirst = m.getWordStats("shared")
                                    .value("firstLookup").toDateTime();

    // 造一份同词、计数更高、首查更早的导入数据
    const QString path = m_dir.filePath("import_merge.json");
    {
        QJsonObject w;
        w["word"] = "shared";
        w["lookupCount"] = 5;
        w["correctAnswers"] = 3;
        w["wrongAnswers"] = 2;
        w["firstLookup"] = origFirst.addDays(-3).toString(Qt::ISODate);
        w["lastLookup"] = origFirst.addDays(3).toString(Qt::ISODate);
        w["masteryLevel"] = 4;
        w["difficulty"] = 2.5;
        w["notes"] = "";
        w["tags"] = QJsonArray{"fromImport"};
        QJsonArray arr;
        arr.append(w);
        QJsonObject root;
        root["wordStats"] = arr;
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
    }
    QVERIFY(m.importStats(path));

    const QVariantMap s = m.getWordStats("shared");
    QCOMPARE(s.value("lookupCount").toInt(), 6);       // 1 + 5
    QCOMPARE(s.value("correctAnswers").toInt(), 4);   // 1 + 3
    QCOMPARE(s.value("wrongAnswers").toInt(), 2);     // 0 + 2
    QCOMPARE(s.value("masteryLevel").toInt(), 4);     // 取较大者
    // 时间戳经 JSON（ISODate，秒级精度）往返，毫秒被截掉，按秒比较
    QCOMPARE(s.value("firstLookup").toDateTime().toSecsSinceEpoch(),
             origFirst.addDays(-3).toSecsSinceEpoch());
    QVERIFY(s.value("lastLookup").toDateTime() > QDateTime::currentDateTime());
    QVERIFY(s.value("tags").toStringList().contains("fromImport"));
}

void LearningManagerTest::importStats_addsNewWordsAndMergesTags()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("existing");
    m.addWordTag("existing", "keep");

    const QString path = m_dir.filePath("import_add.json");
    {
        QJsonObject w;
        w["word"] = "brandnew";
        w["lookupCount"] = 2;
        w["masteryLevel"] = 2;
        w["tags"] = QJsonArray{"x", "y"};
        QJsonArray arr;
        arr.append(w);
        QJsonObject root;
        root["wordStats"] = arr;
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QJsonDocument(root).toJson());
    }
    QVERIFY(m.importStats(path));
    QCOMPARE(m.getAllStats().size(), 2);
    QCOMPARE(m.getWordStats("brandnew").value("lookupCount").toInt(), 2);
    QCOMPARE(m.getWordStats("brandnew").value("tags").toStringList().size(), 2);
    // 原有词的标签不受影响
    QCOMPARE(m.getWordStats("existing").value("tags").toStringList(),
             QStringList{"keep"});
}

void LearningManagerTest::importStats_missingFileFails()
{
    resetStatsFile();
    LearningManager m;
    m.recordLookup("a");
    QVERIFY(!m.importStats(m_dir.filePath("nope.json")));
    // 失败不影响内存态
    QCOMPARE(m.getAllStats().size(), 1);
    // 不可写路径
    QVERIFY(!m.exportStats(m_dir.filePath("no_such_dir/x.json")));
}

// ---------------------------------------------------------------- 杂项

void LearningManagerTest::motivationalMessage_fromKnownPool()
{
    LearningManager m;
    const QStringList pool = {
        "坚持学习，每天进步一点点！",
        "今天又掌握了新单词，继续保持！",
        "复习是巩固记忆的关键，加油！",
        "词汇量正在稳步提升，很棒！",
        "学习无止境，知识改变命运！"
    };
    // 随机取值，多次调用都应落在池子里
    for (int i = 0; i < 50; ++i) {
        QVERIFY2(pool.contains(m.getMotivationalMessage()),
                 qPrintable(m.getMotivationalMessage()));
    }
}

QTEST_MAIN(LearningManagerTest)
#include "learning_manager_test.moc"
