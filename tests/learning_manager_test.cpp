// LearningManager（qmlui）读面测试。P-7 游戏化死码清理后 LearningManager
// 只剩统计读面（学习统计 Tab 消费）：getWordStats/getDailyStats/
// getProgressStats/getDueReviews/getWeakWords/getMotivationalMessage，
// 数据从 AppDataLocation 的 learning_stats.json 加载。写面（学习记录/
// 答题/复习调度/成就/导入导出）已随零消费退役，本文件改用 fixture
// 手摆统计文件驱动读面——读语义不再依赖写链。
//
// 统计文件落在 QStandardPaths::AppDataLocation，用 test mode 把它导到
// 临时目录，避免污染真实用户数据。

#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "learning_manager.h"

class LearningManagerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 加载
    void loadStats_missingFileStartsFresh();
    void loadStats_corruptFileDoesNotCrash();
    void fixture_roundTripsIntoWordStats();

    // 复习到期读面
    void getDueReviews_onlyPastDue();
    void reviewPriorityAndReason_buckets();

    // 统计面板
    void dailyStats_countsNewLookupsAndReviews();
    void dailyStats_reportsTargetFromFile();
    void progressStats_classifiesMastery();
    void getWeakWords_ranksByWeakScore();
    void emptyManager_reportsZerosNotNaN();

    void motivationalMessage_fromKnownPool();

private:
    QTemporaryDir m_dir;
};

// ---------------------------------------------------------------- fixture

static QString statsFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/learning_stats.json";
}

// 每个用例都从干净的统计文件开始：LearningManager 在构造时 loadStats()
static void resetStatsFile()
{
    QDir().mkpath(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation));
    const QString path = statsFilePath();
    QFile::remove(path);
    QDir(path).removeRecursively();
}

// 手摆统计文件（读面测试的唯一数据通道）
static void writeStatsFile(const QJsonObject& root)
{
    QDir().mkpath(QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation));
    QFile f(statsFilePath());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QJsonDocument(root).toJson());
}

static QJsonObject makeWord(const QString& word)
{
    QJsonObject w;
    w["word"] = word;
    w["lookupCount"] = 1;
    w["correctAnswers"] = 0;
    w["wrongAnswers"] = 0;
    w["firstLookup"] = QString();
    w["lastLookup"] = QString();
    w["nextReview"] = QString();
    w["masteryLevel"] = 0;
    w["difficulty"] = 1.0;
    w["notes"] = QString();
    w["tags"] = QJsonArray();
    return w;
}

void LearningManagerTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    // 把 AppDataLocation 导到临时目录：测试不该写进真实用户目录
    QStandardPaths::setTestModeEnabled(true);
    resetStatsFile();
}

void LearningManagerTest::cleanupTestCase()
{
    QFile::remove(statsFilePath());
}

// ---------------------------------------------------------------- 加载

void LearningManagerTest::loadStats_missingFileStartsFresh()
{
    resetStatsFile();
    LearningManager m;
    QVERIFY(m.getWordStats("hello").isEmpty());
    QVERIFY(m.getDueReviews().isEmpty());
}

void LearningManagerTest::loadStats_corruptFileDoesNotCrash()
{
    resetStatsFile();
    QFile f(statsFilePath());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("not json {{{");
    f.close();

    LearningManager m;   // 坏文件按空态容错，不崩
    QVERIFY(m.getWordStats("hello").isEmpty());
    QCOMPARE(m.getDailyStats().value("target").toInt(), 10);
}

void LearningManagerTest::fixture_roundTripsIntoWordStats()
{
    resetStatsFile();
    // 键形态 = 历史写入器（recordLookup）落盘的归一化小写；fromJson 原样加载
    QJsonObject w = makeWord("serene");
    w["lookupCount"] = 4;
    w["correctAnswers"] = 3;
    w["wrongAnswers"] = 1;
    w["firstLookup"] = QDateTime::currentDateTime().addDays(-9).toString(Qt::ISODate);
    w["lastLookup"] = QDateTime::currentDateTime().addDays(-1).toString(Qt::ISODate);
    w["nextReview"] = QDateTime::currentDateTime().addDays(3).toString(Qt::ISODate);
    w["masteryLevel"] = 3;
    w["difficulty"] = 2.5;
    w["notes"] = "calm word";
    w["tags"] = QJsonArray{"gre", "mood"};

    QJsonObject root;
    root["dailyTarget"] = 7;
    root["wordStats"] = QJsonArray{w};
    writeStatsFile(root);

    LearningManager m;
    // 查询侧归一化：去空白 + 小写
    QVariantMap s = m.getWordStats("  SERENE ");
    QCOMPARE(s.value("word").toString(), QString("serene"));
    QCOMPARE(s.value("lookupCount").toInt(), 4);
    QCOMPARE(s.value("correctAnswers").toInt(), 3);
    QCOMPARE(s.value("wrongAnswers").toInt(), 1);
    QCOMPARE(s.value("masteryLevel").toInt(), 3);
    QCOMPARE(s.value("difficulty").toDouble(), 2.5);
    QCOMPARE(s.value("notes").toString(), QString("calm word"));
    QCOMPARE(s.value("firstLookup").toDateTime().date(),
             QDate::currentDate().addDays(-9));
    QCOMPARE(s.value("lastLookup").toDateTime().date(),
             QDate::currentDate().addDays(-1));
    QCOMPARE(s.value("nextReview").toDateTime().date(),
             QDate::currentDate().addDays(3));
    // accuracy = 3/4
    QVERIFY(qFuzzyCompare(s.value("accuracy").toDouble(), 75.0));
    QCOMPARE(s.value("tags").toList().size(), 2);
    QVERIFY(s.value("tags").toList().contains(QLatin1String("gre")));

    // 未记录词返回空 map
    QVERIFY(m.getWordStats("unknown").isEmpty());
}

// ---------------------------------------------------------------- 复习到期

void LearningManagerTest::getDueReviews_onlyPastDue()
{
    resetStatsFile();
    QJsonObject overdue = makeWord("overdue");
    overdue["nextReview"] = QDateTime::currentDateTime().addSecs(-3600)
                                .toString(Qt::ISODate);
    QJsonObject future = makeWord("future");
    future["nextReview"] = QDateTime::currentDateTime().addDays(2)
                               .toString(Qt::ISODate);
    QJsonObject noDate = makeWord("nodate");   // 无效时间不算到期

    QJsonObject root;
    root["wordStats"] = QJsonArray{overdue, future, noDate};
    writeStatsFile(root);

    LearningManager m;
    const QVariantList due = m.getDueReviews();
    QCOMPARE(due.size(), 1);
    QCOMPARE(due.first().toMap().value("word").toString(), QString("overdue"));
    QVERIFY(due.first().toMap().value("dueTime").toDateTime()
            <= QDateTime::currentDateTime());
}

void LearningManagerTest::reviewPriorityAndReason_buckets()
{
    resetStatsFile();
    QJsonObject longOverdue = makeWord("long_overdue");   // 超期 200h，低掌握 → p5
    longOverdue["nextReview"] = QDateTime::currentDateTime().addSecs(-200 * 3600)
                                    .toString(Qt::ISODate);
    QJsonObject dayOverdue = makeWord("day_overdue");     // 超期 48h，低掌握 → p4
    dayOverdue["nextReview"] = QDateTime::currentDateTime().addSecs(-48 * 3600)
                                   .toString(Qt::ISODate);
    QJsonObject freshLow = makeWord("fresh_low");         // 刚过期 1h，低掌握 → p2
    freshLow["nextReview"] = QDateTime::currentDateTime().addSecs(-3600)
                                 .toString(Qt::ISODate);
    QJsonObject freshHigh = makeWord("fresh_high");       // 刚过期，高掌握 → p1
    freshHigh["masteryLevel"] = 4;
    freshHigh["nextReview"] = QDateTime::currentDateTime().addSecs(-3600)
                                  .toString(Qt::ISODate);

    QJsonObject root;
    root["wordStats"] = QJsonArray{longOverdue, dayOverdue, freshLow, freshHigh};
    writeStatsFile(root);

    LearningManager m;
    QVariantMap byWord;
    for (const QVariant& r : m.getDueReviews())
        byWord[r.toMap().value("word").toString()] = r;

    QCOMPARE(byWord.size(), 4);
    QCOMPARE(byWord["long_overdue"].toMap().value("priority").toInt(), 5);
    QCOMPARE(byWord["long_overdue"].toMap().value("reason").toString(),
             QString("长期未复习"));
    QCOMPARE(byWord["day_overdue"].toMap().value("priority").toInt(), 4);
    QCOMPARE(byWord["day_overdue"].toMap().value("reason").toString(),
             QString("昨日遗留"));
    QCOMPARE(byWord["fresh_low"].toMap().value("priority").toInt(), 2);
    QCOMPARE(byWord["fresh_low"].toMap().value("reason").toString(),
             QString("掌握程度较低"));
    QCOMPARE(byWord["fresh_high"].toMap().value("priority").toInt(), 1);
    QCOMPARE(byWord["fresh_high"].toMap().value("reason").toString(),
             QString("定期复习"));
}

// ---------------------------------------------------------------- 统计面板

void LearningManagerTest::dailyStats_countsNewLookupsAndReviews()
{
    resetStatsFile();
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime todayStart = now.date().startOfDay();

    QJsonObject todayWord = makeWord("today_word");   // 今日新查 + 今日到期
    todayWord["firstLookup"] = now.toString(Qt::ISODate);
    todayWord["lastLookup"] = now.toString(Qt::ISODate);
    todayWord["nextReview"] = now.addSecs(-7200).toString(Qt::ISODate);
    QJsonObject oldWord = makeWord("old_word");       // 首查 9 天前，未到期
    oldWord["firstLookup"] = todayStart.addDays(-9).toString(Qt::ISODate);
    oldWord["lastLookup"] = todayStart.addDays(-9).toString(Qt::ISODate);
    oldWord["nextReview"] = now.addDays(5).toString(Qt::ISODate);

    QJsonObject root;
    root["wordStats"] = QJsonArray{todayWord, oldWord};
    writeStatsFile(root);

    LearningManager m;
    const QVariantMap s = m.getDailyStats();
    QCOMPARE(s.value("newWords").toInt(), 1);
    QCOMPARE(s.value("lookups").toInt(), 1);
    QCOMPARE(s.value("reviews").toInt(), 1);   // nextReview 落在今日窗口
    QCOMPARE(s.value("date").toDate(), QDate::currentDate());
}

void LearningManagerTest::dailyStats_reportsTargetFromFile()
{
    resetStatsFile();
    const QDateTime now = QDateTime::currentDateTime();
    QJsonObject a = makeWord("a");
    a["firstLookup"] = now.toString(Qt::ISODate);
    a["lastLookup"] = now.toString(Qt::ISODate);
    QJsonObject b = makeWord("b");
    b["firstLookup"] = now.toString(Qt::ISODate);
    b["lastLookup"] = now.toString(Qt::ISODate);
    QJsonObject c = makeWord("c");
    c["firstLookup"] = now.addSecs(-86400 * 3).toString(Qt::ISODate);
    c["lastLookup"] = now.addSecs(-86400 * 3).toString(Qt::ISODate);

    QJsonObject root;
    root["dailyTarget"] = 2;   // 今日新词 2 个 → 恰好达标
    root["wordStats"] = QJsonArray{a, b, c};
    writeStatsFile(root);

    LearningManager m;
    const QVariantMap s = m.getDailyStats();
    QCOMPARE(s.value("target").toInt(), 2);
    QCOMPARE(s.value("targetMet").toBool(), true);
}

void LearningManagerTest::progressStats_classifiesMastery()
{
    resetStatsFile();
    QJsonObject mastered1 = makeWord("m1");
    mastered1["masteryLevel"] = 4;
    QJsonObject mastered2 = makeWord("m2");
    mastered2["masteryLevel"] = 5;
    QJsonObject weak1 = makeWord("w1");
    weak1["masteryLevel"] = 2;
    QJsonObject weak2 = makeWord("w2");
    weak2["masteryLevel"] = 0;
    QJsonObject mid = makeWord("mid");
    mid["masteryLevel"] = 3;

    QJsonObject root;
    root["wordStats"] = QJsonArray{mastered1, mastered2, weak1, weak2, mid};
    writeStatsFile(root);

    LearningManager m;
    const QVariantMap s = m.getProgressStats();
    QCOMPARE(s.value("totalWords").toInt(), 5);
    QCOMPARE(s.value("masteredWords").toInt(), 2);   // >= 4
    QCOMPARE(s.value("weakWords").toInt(), 2);       // <= 2
    QVERIFY(qFuzzyCompare(s.value("masteryRate").toDouble(), 40.0));
}

void LearningManagerTest::getWeakWords_ranksByWeakScore()
{
    resetStatsFile();
    // 强词：高掌握、低难度、全对 → 得分 0.02 不入围
    QJsonObject strong = makeWord("strong");
    strong["masteryLevel"] = 5;
    strong["correctAnswers"] = 4;
    // 弱词 A：全错 + 零掌握 + 高难度 → 得分 1.0
    QJsonObject weakA = makeWord("weak_a");
    weakA["wrongAnswers"] = 4;
    weakA["difficulty"] = 10.0;
    // 弱词 B：中掌握 → 得分约 0.47
    QJsonObject weakB = makeWord("weak_b");
    weakB["masteryLevel"] = 3;
    weakB["difficulty"] = 5.0;

    QJsonObject root;
    root["wordStats"] = QJsonArray{strong, weakA, weakB};
    writeStatsFile(root);

    LearningManager m;
    const QVariantList weak = m.getWeakWords(1);   // limit 收口
    QCOMPARE(weak.size(), 1);
    QCOMPARE(weak.first().toMap().value("word").toString(), QString("weak_a"));
    QVERIFY(weak.first().toMap().value("weakness").toDouble() > 0.9);
    // 条目携带明细
    QCOMPARE(weak.first().toMap().value("stats").toMap()
                 .value("wrongAnswers").toInt(), 4);

    const QVariantList all = m.getWeakWords(10);
    QCOMPARE(all.size(), 2);   // strong 不入围
    QCOMPARE(all.at(1).toMap().value("word").toString(), QString("weak_b"));
}

void LearningManagerTest::emptyManager_reportsZerosNotNaN()
{
    resetStatsFile();
    LearningManager m;
    const QVariantMap daily = m.getDailyStats();
    QCOMPARE(daily.value("newWords").toInt(), 0);
    QCOMPARE(daily.value("lookups").toInt(), 0);
    QCOMPARE(daily.value("reviews").toInt(), 0);
    QCOMPARE(daily.value("target").toInt(), 10);
    QCOMPARE(daily.value("targetMet").toBool(), false);

    const QVariantMap progress = m.getProgressStats();
    QCOMPARE(progress.value("totalWords").toInt(), 0);
    QCOMPARE(progress.value("masteredWords").toInt(), 0);
    QCOMPARE(progress.value("weakWords").toInt(), 0);
    QVERIFY(qFuzzyCompare(progress.value("masteryRate").toDouble(), 0.0));

    QVERIFY(m.getDueReviews().isEmpty());
    QVERIFY(m.getWeakWords(10).isEmpty());
}

void LearningManagerTest::motivationalMessage_fromKnownPool()
{
    resetStatsFile();
    LearningManager m;
    const QStringList pool = {
        QStringLiteral("坚持学习，每天进步一点点！"),
        QStringLiteral("今天又掌握了新单词，继续保持！"),
        QStringLiteral("复习是巩固记忆的关键，加油！"),
        QStringLiteral("词汇量正在稳步提升，很棒！"),
        QStringLiteral("学习无止境，知识改变命运！")
    };
    for (int i = 0; i < 20; ++i)
        QVERIFY2(pool.contains(m.getMotivationalMessage()),
                 "message must come from the known pool");
}

QTEST_MAIN(LearningManagerTest)
#include "learning_manager_test.moc"
