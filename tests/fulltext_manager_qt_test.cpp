// FullTextManagerQt（adapters/qt）全量测试。此前该文件 0% 覆盖（320 行、
// 11 个 Q_INVOKABLE）：全文索引的落盘/加载/版本协商（UDFT1/2/3）、签名校验、
// 索引升级、源差异对比与导出，全是应用"缓存加速"路径上的判据，出错会静默
// 退化成全量扫描或用错索引。
//
// 索引格式由 core/std 的 DictionaryManagerStd 负责写；本测试用真实词典 +
// 真实 UDFT 文件驱动，验证 Qt 桥接层的判据与报告。

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "fulltext_manager_qt.h"

using UnidictAdaptersQt::FullTextManagerQt;

class FullTextManagerQtTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // 构造与词典加载
    void loadDictionariesFromEnv_emptyReturnsFalse();
    void loadDictionariesFromEnv_loadsAndIndexes();
    void loadDictionariesFromEnv_partialFailureStillReportsSuccess();

    // 保存 / 读取统计
    void saveAndStatsFromFile_roundTrip();
    void statsFromFile_missingFileReportsError();
    void currentStats_emptyAndAfterLoad();

    // 签名
    void currentSignature_emptyBeforeDictionaries();
    void verifyIndexMatch_detectsMatchAndMismatch();
    void verifyIndexMatch_missingFileReportsError();

    // 加载三种兼容模式
    void loadIndex_strict_acceptsMatchingSignature();
    void loadIndex_strict_rejectsMismatch();
    void loadIndex_loose_acceptsMismatch();
    void loadIndex_auto_fallsBackForLegacy();
    void loadIndex_auto_rejectsNonLegacyMismatch();
    void loadIndex_unknownModeTreatedAsLoose();
    void loadIndex_missingFileReportsError();

    // loadIndexDetailed
    void loadIndexDetailed_reportsRealVersion();
    void loadIndexDetailed_strictFailureCarriesError();
    void loadIndexDetailed_autoAndLoose();

    // 升级
    void upgrade_rewritesIndexForCurrentDictionaries();
    void upgrade_missingInputFails();

    // 源差异诊断
    void verifyIndexDetailed_reportsSourceDiff();
    void verifyIndexDetailed_reportsChangedSource();
    void verifyIndexDetailed_missingFileReportsError();
    void verifyIndexDetailed_legacyTreatsAsMatch();
    void exportSourceDiff_writesJson();
    void exportSourceDiff_unwritablePathFails();

    // 落一份"与当前词典签名一致"的索引
    static void buildMatchingIndex(FullTextManagerQt& m, const QString& path) {
        QVERIFY(m.saveIndex(path));
    }

    // 手工写一个最小可解析的 legacy v1 索引（无签名段）：
    //   magic(5) docs(4)=0 terms(4)=0
    static void writeLegacyV1(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return;
        f.write("UDFT1", 5);
        const quint32 zero = 0;
        f.write(reinterpret_cast<const char*>(&zero), 4);   // docs
        f.write(reinterpret_cast<const char*>(&zero), 4);   // terms
    }

private:
    QTemporaryDir m_dir;
    QString m_dictA;
    QString m_dictB;
    QString m_dictC;

    // 造一个 JSON 词典
    static QString makeDict(const QString& path, const QString& name,
                            const QStringList& words) {
        QJsonArray entries;
        for (const QString& w : words) {
            QJsonObject e;
            e["word"] = w;
            e["definition"] = QStringLiteral("definition of %1 %2").arg(w, name);
            entries.append(e);
        }
        QJsonObject root;
        root["name"] = name;
        root["entries"] = entries;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return QString();
        f.write(QJsonDocument(root).toJson());
        return name;
    }
};

void FullTextManagerQtTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_dictA = m_dir.filePath("a.json");
    m_dictB = m_dir.filePath("b.json");
    m_dictC = m_dir.filePath("c.json");
    QCOMPARE(makeDict(m_dictA, "dictA",
                      {"alpha", "beta", "gamma", "delta"}), QString("dictA"));
    QCOMPARE(makeDict(m_dictB, "dictB",
                      {"epsilon", "zeta", "eta", "theta"}), QString("dictB"));
    QCOMPARE(makeDict(m_dictC, "dictC",
                      {"iota", "kappa"}), QString("dictC"));
}

// ---------------------------------------------------------------- 词典加载

void FullTextManagerQtTest::loadDictionariesFromEnv_emptyReturnsFalse()
{
    qunsetenv("UNIDICT_DICTS");
    FullTextManagerQt m;
    QVERIFY(!m.loadDictionariesFromEnv());

    // 空串同样视为未设置
    qputenv("UNIDICT_DICTS", "");
    FullTextManagerQt m2;
    QVERIFY(!m2.loadDictionariesFromEnv());
}

void FullTextManagerQtTest::loadDictionariesFromEnv_loadsAndIndexes()
{
    // 分隔符跟平台 PATH 惯例一致（见 fulltext_manager_qt.cpp 的注释）
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());

    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    // 签名应包含两个词典
    const QString sig = m.currentSignature();
    QVERIFY2(sig.contains("dictA"), qPrintable(sig));
    QVERIFY2(sig.contains("dictB"), qPrintable(sig));

    // 倒排索引是懒建的：loadDictionariesFromEnv 只建词项索引，全文索引
    // 要到第一次 save_fulltext_index 才落盘（ensure_fulltext_index_built）。
    // 所以"刚加载完"时 currentStats 仍是空的——钉住这个懒加载语义。
    const QVariantMap st = m.currentStats();
    QCOMPARE(st.value("version").toInt(), 0);
    QCOMPARE(st.value("docs").toLongLong(), 0LL);
    QVERIFY(!st.value("signature").toString().isEmpty());

    // 落盘一次之后统计才有内容
    const QString idx = m_dir.filePath("lazy.udft");
    QVERIFY(m.saveIndex(idx));
    const QVariantMap st2 = m.currentStats();
    QVERIFY2(st2.value("docs").toLongLong() > 0, "saveIndex 后应有文档统计");
    QVERIFY(st2.value("terms").toLongLong() > 0);
    QVERIFY(st2.value("postings").toLongLong() > 0);
}

void FullTextManagerQtTest::loadDictionariesFromEnv_partialFailureStillReportsSuccess()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep +
                           m_dir.filePath("missing.json").toUtf8());
    FullTextManagerQt m;
    // 只要有一个加载成功就报 true（ok |= ...）
    QVERIFY(m.loadDictionariesFromEnv());
    QVERIFY(m.currentSignature().contains("dictA"));
}

// ---------------------------------------------------------------- 保存/统计

void FullTextManagerQtTest::saveAndStatsFromFile_roundTrip()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());

    const QString idx = m_dir.filePath("index.udft");
    QVERIFY(m.saveIndex(idx));
    QVERIFY(QFile::exists(idx));

    const QVariantMap s = m.statsFromFile(idx);
    QVERIFY2(!s.contains("error"), qPrintable(s.value("error").toString()));
    // 落盘格式是 UDFT3
    QCOMPARE(s.value("version").toInt(), 3);
    QCOMPARE(s.value("signature").toString(), m.currentSignature());
    QVERIFY(s.value("docs").toLongLong() > 0);
    QVERIFY(s.value("terms").toLongLong() > 0);
    QVERIFY(s.value("postings").toLongLong() > 0);
    QVERIFY(s.contains("compressed_terms"));
    QVERIFY(s.contains("compressed_bytes"));
    QVERIFY(s.contains("pairs_decompressed"));
    QVERIFY(s.contains("avg_df"));
}

void FullTextManagerQtTest::statsFromFile_missingFileReportsError()
{
    FullTextManagerQt m;
    const QVariantMap s = m.statsFromFile(m_dir.filePath("nope.udft"));
    QVERIFY(s.contains("error"));
    QVERIFY(!s.value("error").toString().isEmpty());
    // 失败时不该带任何统计字段
    QVERIFY(!s.contains("version"));
    QVERIFY(!s.contains("docs"));

    // 垃圾文件同样是 error 而不是崩
    const QString junk = m_dir.filePath("junk.udft");
    {
        QFile f(junk);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not an index at all");
    }
    const QVariantMap s2 = m.statsFromFile(junk);
    QVERIFY(s2.contains("error"));
}

void FullTextManagerQtTest::currentStats_emptyAndAfterLoad()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    // 懒加载：刚加载完词典时还没有倒排索引
    QCOMPARE(m.currentStats().value("docs").toLongLong(), 0LL);
    QCOMPARE(m.currentStats().value("version").toInt(), 0);

    const QString idx = m_dir.filePath("cur.udft");
    QVERIFY(m.saveIndex(idx));

    // 加载一份索引后，currentStats 的 version 应反映已加载的格式版本
    FullTextManagerQt m2;
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    QVERIFY(m2.loadDictionariesFromEnv());
    QVERIFY(m2.loadIndex(idx, "strict").isEmpty());
    QCOMPARE(m2.currentStats().value("version").toInt(), 3);
    QVERIFY(m2.currentStats().value("docs").toLongLong() > 0);
}

// ---------------------------------------------------------------- 签名

void FullTextManagerQtTest::verifyIndexMatch_detectsMatchAndMismatch()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("vmatch.udft");
    buildMatchingIndex(writer, idx);

    // 同签名 → 空串（表示匹配）
    FullTextManagerQt same;
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    QVERIFY(same.loadDictionariesFromEnv());
    QVERIFY2(same.verifyIndexMatch(idx).isEmpty(),
             qPrintable(same.verifyIndexMatch(idx)));

    // 换词典 → 不匹配，且错误串可读
    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt diff;
    QVERIFY(diff.loadDictionariesFromEnv());
    const QString err = diff.verifyIndexMatch(idx);
    QVERIFY2(!err.isEmpty(), "签名不符时应有描述");
    QVERIFY(err.contains("signature mismatch"));
}

void FullTextManagerQtTest::verifyIndexMatch_missingFileReportsError()
{
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    const QString err = m.verifyIndexMatch(m_dir.filePath("absent.udft"));
    QVERIFY(!err.isEmpty());
    QVERIFY2(!err.contains("signature mismatch"),
             "缺文件应报 last_error 而不是签名不符");
}

void FullTextManagerQtTest::currentSignature_emptyBeforeDictionaries()
{
    qunsetenv("UNIDICT_DICTS");
    FullTextManagerQt m;
    // 没有词典时签名仍是一个可判定的串（不是空——空会让"签名一致"误判）
    const QString sig = m.currentSignature();
    QVERIFY2(!sig.isEmpty(), "签名不应为空，否则无法区分'无词典'与'签名相同'");
}

// ---------------------------------------------------------------- 加载模式

// 造一个"当前签名下有效"的索引

void FullTextManagerQtTest::loadIndex_strict_acceptsMatchingSignature()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("match.udft");
    buildMatchingIndex(writer, idx);

    FullTextManagerQt reader;
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    QVERIFY(reader.loadDictionariesFromEnv());
    QVERIFY2(reader.loadIndex(idx, "strict").isEmpty(),
             qPrintable(reader.loadIndex(idx, "strict")));
    QCOMPARE(reader.currentStats().value("version").toInt(), 3);
}

void FullTextManagerQtTest::loadIndex_strict_rejectsMismatch()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("mismatch.udft");
    buildMatchingIndex(writer, idx);

    // 换一套词典（多一个 dictC）→ 签名必然不同
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8() + sep +
                           m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    const QString err = reader.loadIndex(idx, "strict");
    QVERIFY2(!err.isEmpty(), "strict 模式应当拒绝签名不符的索引");
    QVERIFY(err.contains("signature mismatch") || err.contains("invalid index"));
    // 拒绝后内存里不该留下索引
    QCOMPARE(reader.currentStats().value("docs").toLongLong(), 0LL);
}

void FullTextManagerQtTest::loadIndex_loose_acceptsMismatch()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("loose.udft");
    buildMatchingIndex(writer, idx);

    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    QVERIFY2(reader.loadIndex(idx, "loose").isEmpty(),
             qPrintable(reader.loadIndex(idx, "loose")));
    QVERIFY(reader.currentStats().value("docs").toLongLong() > 0);
}

void FullTextManagerQtTest::loadIndex_auto_fallsBackForLegacy()
{
    // auto 模式只接受 legacy v1 的宽松回退
    const QString v1 = m_dir.filePath("legacy_v1.udft");
    writeLegacyV1(v1);
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep);
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    QVERIFY2(reader.loadIndex(v1, "auto").isEmpty(),
             qPrintable(reader.loadIndex(v1, "auto")));
    QCOMPARE(reader.currentStats().value("version").toInt(), 1);
}

void FullTextManagerQtTest::loadIndex_auto_rejectsNonLegacyMismatch()
{
    // 回归：auto 模式此前会把"签名不匹配但能解析"的 v2/v3 索引当成加载成功
    // 返回（out_error 只在解析失败时才写，成功路径恒为空串），于是调用方
    // 以为成功、内存里却换上了一套不属于当前词典的旧索引，全文检索开始
    // 静默返回旧结果。accept_version=1 之后必须明确报错。
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("v3_mismatch.udft");
    buildMatchingIndex(writer, idx);

    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    const QString err = reader.loadIndex(idx, "auto");
    QVERIFY2(!err.isEmpty(), "签名不符的 v3 索引在 auto 模式下必须报错");
    QVERIFY2(err.contains("signature mismatch"), qPrintable(err));
    // 且不能留在内存里
    QCOMPARE(reader.currentStats().value("version").toInt(), 0);
    QCOMPARE(reader.currentStats().value("docs").toLongLong(), 0LL);

    // loose 模式仍然放行（显式的"我知道有风险"语义）
    FullTextManagerQt loose;
    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    QVERIFY(loose.loadDictionariesFromEnv());
    QVERIFY2(loose.loadIndex(idx, "loose").isEmpty(),
             "loose 模式按设计忽略签名");
    QCOMPARE(loose.currentStats().value("version").toInt(), 3);
}

void FullTextManagerQtTest::loadIndex_unknownModeTreatedAsLoose()
{
    // 未识别的 compatMode 落到最后的 loose 分支
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("unknown_mode.udft");
    buildMatchingIndex(writer, idx);

    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    QVERIFY2(reader.loadIndex(idx, "whatever").isEmpty(),
             "未知模式应当等同 loose");
}

void FullTextManagerQtTest::loadIndex_missingFileReportsError()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep);
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    for (const char* mode : {"strict", "auto", "loose", "x"}) {
        QVERIFY2(!m.loadIndex(m_dir.filePath("absent.udft"), mode).isEmpty(),
                 mode);
    }
}

// ---------------------------------------------------------------- Detailed

void FullTextManagerQtTest::loadIndexDetailed_reportsRealVersion()
{
    // 回归：三个成功分支原先硬编码 version=3。落盘是 UDFT3 所以看不出
    // 问题，但源码级 assert：这里改成读内存索引的真实版本后，仍应是 3。
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("detailed.udft");
    buildMatchingIndex(writer, idx);

    for (const char* mode : {"strict", "auto", "loose"}) {
        FullTextManagerQt reader;
        qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
        QVERIFY(reader.loadDictionariesFromEnv());
        const QVariantMap r = reader.loadIndexDetailed(idx, mode);
        QVERIFY2(r.value("ok").toBool(),
                 qPrintable(QStringLiteral("%1: %2")
                                .arg(mode, r.value("error").toString())));
        QCOMPARE(r.value("mode").toString(), QString(mode));
        // 真实版本 = statsFromFile 读到的那个，两处必须一致
        QCOMPARE(r.value("version").toInt(), 3);
        QCOMPARE(r.value("version").toInt(),
                 writer.statsFromFile(idx).value("version").toInt());
        QVERIFY(r.value("error").toString().isEmpty());
        QVERIFY(!r.value("currentSignature").toString().isEmpty());
    }
}

void FullTextManagerQtTest::loadIndexDetailed_strictFailureCarriesError()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("det_fail.udft");
    buildMatchingIndex(writer, idx);

    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    const QVariantMap r = reader.loadIndexDetailed(idx, "strict");
    QVERIFY(!r.value("ok").toBool());
    QVERIFY2(r.value("error").toString().contains("strict mode"),
             qPrintable(r.value("error").toString()));
    QCOMPARE(r.value("version").toInt(), 0);
    QVERIFY(r.value("error").toString().contains("signature mismatch") ||
            r.value("error").toString().contains("invalid index"));
}

void FullTextManagerQtTest::loadIndexDetailed_autoAndLoose()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("det_auto.udft");
    buildMatchingIndex(writer, idx);

    // 缺文件时两种模式都带 error 且 ok=false
    const QString absent = m_dir.filePath("no_such.udft");
    for (const char* mode : {"auto", "loose", "weird"}) {
        FullTextManagerQt reader;
        qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
        QVERIFY(reader.loadDictionariesFromEnv());
        const QVariantMap r = reader.loadIndexDetailed(absent, mode);
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(!r.value("error").toString().isEmpty());
    }
}

// ---------------------------------------------------------------- 升级

void FullTextManagerQtTest::upgrade_rewritesIndexForCurrentDictionaries()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString in = m_dir.filePath("upgrade_in.udft");
    buildMatchingIndex(writer, in);

    // 换一套词典后升级：输出应绑定新签名
    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    const QString out = m_dir.filePath("upgrade_out.udft");
    QVERIFY(reader.upgrade(in, out));
    QVERIFY(QFile::exists(out));
    QCOMPARE(reader.statsFromFile(out).value("signature").toString(),
             reader.currentSignature());
    // 升级后 strict 模式应当能直接加载
    QVERIFY(reader.loadIndex(out, "strict").isEmpty());
}

void FullTextManagerQtTest::upgrade_missingInputFails()
{
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    QVERIFY(!m.upgrade(m_dir.filePath("absent.udft"),
                        m_dir.filePath("out.udft")));
}

// ---------------------------------------------------------------- 源差异

void FullTextManagerQtTest::verifyIndexDetailed_reportsSourceDiff()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    // 索引里含 dictA + dictB
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("diff.udft");
    buildMatchingIndex(writer, idx);

    // 当前只含 dictC → A/B 的源文件是"被移除"，C 的源文件是"新增"
    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());

    const QVariantMap r = reader.verifyIndexDetailed(idx);
    QVERIFY2(r.value("ok").toBool(),
             qPrintable(r.value("error").toString()));
    QCOMPARE(r.value("version").toInt(), 3);
    QVERIFY(!r.value("match").toBool());
    QCOMPARE(r.value("error").toString(), QString("signature mismatch"));
    QVERIFY(!r.value("fileSignature").toString().isEmpty());
    QVERIFY(!r.value("currentSignature").toString().isEmpty());
    // 签名前缀用于 UI 侧并排显示
    QCOMPARE(r.value("fileSigPrefix").toString().size(),
             qMin(32, r.value("fileSignature").toString().size()));
    QCOMPARE(r.value("currentSigPrefix").toString().size(),
             qMin(32, r.value("currentSignature").toString().size()));

    // 源差异。语义按实现里的注释：
    //   added   = 只在索引文件签名里（当前词典集没有）→ 索引引用了已不存在的源
    //   removed = 只在当前词典集里（索引签名没有）→ 建索引之后新加的源
    const QStringList added = r.value("addedSourcePaths").toStringList();
    const QStringList removed = r.value("removedSourcePaths").toStringList();
    QVERIFY2(added.contains(m_dictA), qPrintable(added.join(",")));
    QVERIFY2(added.contains(m_dictB), qPrintable(added.join(",")));
    QVERIFY2(removed.contains(m_dictC), qPrintable(removed.join(",")));
    QVERIFY(!added.contains(m_dictC));
    QVERIFY(!removed.contains(m_dictA));
    // 路径集合无交集
    for (const QString& p : added) QVERIFY(!removed.contains(p));
    // 两边源文件完全不相交 → 没有"同路径但 size/mtime 不同"的变更
    QVERIFY(r.value("changedSourcePaths").toStringList().isEmpty());

    // 明细里带 owner/size/mtime
    const QVariantList addedDet = r.value("addedSourcesDetailed").toList();
    QCOMPARE(addedDet.size(), 2);
    bool sawOwner = false;
    for (const QVariant& v : addedDet) {
        const QVariantMap mm = v.toMap();
        QVERIFY(mm.contains("path"));
        QVERIFY(mm.contains("sizeFile"));
        QVERIFY(mm.contains("mtimeFile"));
        if (!mm.value("ownerFile").toString().isEmpty()) sawOwner = true;
    }
    QVERIFY2(sawOwner, "新增明细应带归属词典名");

    // 当前词典清单（即使索引不匹配也会给出，便于 UI 展示）
    const QVariantList curDicts = r.value("currentDicts").toList();
    QCOMPARE(curDicts.size(), 1);
    QCOMPARE(curDicts.first().toMap().value("name").toString(),
             QString("dictC"));

    // 文件侧的词典摘要来自签名
    const QVariantList fileDicts = r.value("fileDicts").toList();
    QCOMPARE(fileDicts.size(), 2);

    // 源文件清单：签名里每个词典的源都被解析出来（回归：原实现把 '#'
    // 当成前置分隔符，filesRaw 恒空，整套源差异诊断是死的）
    const QVariantList fileSources = r.value("fileSources").toList();
    QCOMPARE(fileSources.size(), 2);
    QStringList parsedPaths;
    for (const QVariant& v : fileSources) {
        const QVariantMap mm = v.toMap();
        QVERIFY(mm.contains("name"));
        QVERIFY(mm.contains("files"));
        QVERIFY(mm.contains("filesRaw"));
        const QVariantList raws = mm.value("filesRaw").toList();
        QCOMPARE(raws.size(), 1);
        const QVariantMap fr = raws.first().toMap();
        QVERIFY(!fr.value("path").toString().isEmpty());
        QVERIFY(!fr.value("size").toString().isEmpty());
        QVERIFY(!fr.value("mtime").toString().isEmpty());
        parsedPaths << fr.value("path").toString();
        // 展示串带上 size/mtime
        const QString shown = mm.value("files").toList().first().toString();
        QVERIFY2(shown.startsWith(fr.value("path").toString()), qPrintable(shown));
        QVERIFY(shown.contains('('));
        QVERIFY(shown.contains(','));
    }
    parsedPaths.sort();
    QStringList expected = {m_dictA, m_dictB};
    expected.sort();
    QCOMPARE(parsedPaths, expected);

    // 当前侧同样能解析出 C
    const QVariantList curSources = r.value("currentSources").toList();
    QCOMPARE(curSources.size(), 1);
    QCOMPARE(curSources.first().toMap().value("filesRaw").toList().size(), 1);

    // changesByDict 汇总
    const QVariantList byDict = r.value("changesByDict").toList();
    QVERIFY(!byDict.isEmpty());
    QHash<QString, QVariantMap> byName;
    for (const QVariant& v : byDict) {
        const QVariantMap mm = v.toMap();
        QVERIFY(mm.contains("dict"));
        QVERIFY(mm.contains("added"));
        QVERIFY(mm.contains("removed"));
        QVERIFY(mm.contains("changed"));
        byName.insert(mm.value("dict").toString(), mm);
    }
    QCOMPARE(byName["dictA"].value("added").toInt(), 1);
    QCOMPARE(byName["dictA"].value("removed").toInt(), 0);
    QCOMPARE(byName["dictC"].value("removed").toInt(), 1);
}

void FullTextManagerQtTest::verifyIndexDetailed_reportsChangedSource()
{
    // 同一个词典文件被改过（size 或 mtime 变了）→ 应当报 changed 而不是
    // added/removed。这条同时覆盖 reason 的 size / mtime 两个子分支。
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep);
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("changed.udft");
    buildMatchingIndex(writer, idx);

    // 改写 A 的内容（词变了 → 签名里的 size 和 mtime 都变）
    const QString a2 = m_dir.filePath("a2.json");
    QCOMPARE(makeDict(a2, "dictA", {"alpha", "beta", "gamma", "zeta", "eta"}),
             QString("dictA"));
    qputenv("UNIDICT_DICTS", a2.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());

    const QVariantMap r = reader.verifyIndexDetailed(idx);
    QVERIFY(r.value("ok").toBool());
    QVERIFY(!r.value("match").toBool());
    // 路径不同（a.json → a2.json）所以是一条 added + 一条 removed，而不是
    // changed——changed 需要两边是**同一个路径**。
    QCOMPARE(r.value("addedSourcePaths").toStringList().size(), 1);
    QCOMPARE(r.value("removedSourcePaths").toStringList().size(), 1);

    // 同一路径、内容变化 → changed。做法：先让两边加载同一个路径，但
    // 在建索引之后改写该文件的内容。
    const QString b1 = m_dir.filePath("b1.json");
    QCOMPARE(makeDict(b1, "dictB", {"one", "two"}), QString("dictB"));
    qputenv("UNIDICT_DICTS", b1.toUtf8());
    FullTextManagerQt w2;
    QVERIFY(w2.loadDictionariesFromEnv());
    const QString idx2 = m_dir.filePath("changed2.udft");
    buildMatchingIndex(w2, idx2);
    // 建完索引后原地改写 b1.json（更多词 → size 变）
    QCOMPARE(makeDict(b1, "dictB",
                      {"one", "two", "three", "four", "five", "six"}),
             QString("dictB"));

    FullTextManagerQt r2;
    qputenv("UNIDICT_DICTS", b1.toUtf8());
    QVERIFY(r2.loadDictionariesFromEnv());
    const QVariantMap rc = r2.verifyIndexDetailed(idx2);
    QVERIFY(rc.value("ok").toBool());
    const QStringList changed = rc.value("changedSourcePaths").toStringList();
    QVERIFY2(changed.contains(b1), qPrintable(changed.join(",")));
    QVERIFY(rc.value("addedSourcePaths").toStringList().isEmpty());
    QVERIFY(rc.value("removedSourcePaths").toStringList().isEmpty());
    const QVariantList changedDet = rc.value("changedSourcesDetailed").toList();
    QCOMPARE(changedDet.size(), 1);
    const QVariantMap cd = changedDet.first().toMap();
    QVERIFY2(cd.value("reason").toString().contains("size"),
             qPrintable(cd.value("reason").toString()));
    QVERIFY(cd.value("sizeFile") != cd.value("sizeCurrent"));
    // 归属词典：两侧同名
    QCOMPARE(cd.value("ownerFile").toString(), cd.value("ownerCurrent").toString());
}

void FullTextManagerQtTest::verifyIndexDetailed_missingFileReportsError()
{
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    const QVariantMap r =
        m.verifyIndexDetailed(m_dir.filePath("absent.udft"));
    QVERIFY(!r.value("ok").toBool());
    QVERIFY(!r.value("error").toString().isEmpty());
    QCOMPARE(r.value("match").toBool(), false);
    QCOMPARE(r.value("version").toInt(), 0);
    // 即使失败也带上当前词典清单，方便 UI 直接显示
    QCOMPARE(r.value("currentDicts").toList().size(), 1);
}

void FullTextManagerQtTest::verifyIndexDetailed_legacyTreatsAsMatch()
{
    const QString v1 = m_dir.filePath("legacy1.udft");
    writeLegacyV1(v1);
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());

    const QVariantMap r = m.verifyIndexDetailed(v1);
    QVERIFY(r.value("ok").toBool());
    QCOMPARE(r.value("version").toInt(), 1);
    // legacy v1 无签名：按"可验证但视为匹配"处理
    QCOMPARE(r.value("match").toBool(), true);
    QVERIFY(r.value("error").toString().isEmpty());

    // verifyIndexMatch（简版）同样把 v1 视为 ok
    QVERIFY(m.verifyIndexMatch(v1).isEmpty());
    // loadIndexDetailed 的 auto 模式也接受 legacy
    const QVariantMap d = m.loadIndexDetailed(v1, "auto");
    QVERIFY(d.value("ok").toBool());
    QCOMPARE(d.value("version").toInt(), 1);
}

void FullTextManagerQtTest::exportSourceDiff_writesJson()
{
    const QByteArray sep = QString(QDir::listSeparator()).toUtf8();
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8() + sep + m_dictB.toUtf8());
    FullTextManagerQt writer;
    QVERIFY(writer.loadDictionariesFromEnv());
    const QString idx = m_dir.filePath("exp.udft");
    buildMatchingIndex(writer, idx);

    qputenv("UNIDICT_DICTS", m_dictC.toUtf8());
    FullTextManagerQt reader;
    QVERIFY(reader.loadDictionariesFromEnv());
    const QVariantMap verify = reader.verifyIndexDetailed(idx);

    const QString out = m_dir.filePath("diff.json");
    QVERIFY(reader.exportSourceDiff(verify, out));
    QVERIFY(QFile::exists(out));

    QFile f(out);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    f.close();
    QVERIFY(root.contains("generatedAt"));
    QVERIFY(root.contains("version"));
    QVERIFY(root.contains("match"));
    QVERIFY(root.contains("fileSignature"));
    QVERIFY(root.contains("currentSignature"));
    QVERIFY(root.contains("added"));
    QVERIFY(root.contains("removed"));
    QVERIFY(root.contains("changed"));
    QVERIFY(root.contains("dictSummary"));
    QVERIFY(root.contains("changesByDict"));
    // 差异明细确实落进了文件（语义同 verifyIndexDetailed：added=只在索引
    // 文件签名里，removed=只在当前词典集里）
    QCOMPARE(root.value("added").toArray().size(), 2);
    QCOMPARE(root.value("removed").toArray().size(), 1);
    QVERIFY(root.value("changed").toArray().isEmpty());
    // match=false 被如实记录
    QCOMPARE(root.value("match").toBool(), false);
    QCOMPARE(root.value("version").toInt(), 3);
    // changesByDict 里能看到归属词典与计数
    const QJsonArray byDict = root.value("changesByDict").toArray();
    QVERIFY(!byDict.isEmpty());
    bool sawDict = false;
    for (const QJsonValue& v : byDict) {
        const QJsonObject o = v.toObject();
        if (o.value("dict").toString() == "dictA") {
            sawDict = true;
            QCOMPARE(o.value("added").toInt(), 1);
            QCOMPARE(o.value("removed").toInt(), 0);
        }
    }
    QVERIFY2(sawDict, "changesByDict 应含 dictA");
    // 生成时间是 UTC ISO8601
    QVERIFY(root.value("generatedAt").toString().endsWith("Z"));
}

void FullTextManagerQtTest::exportSourceDiff_unwritablePathFails()
{
    qputenv("UNIDICT_DICTS", m_dictA.toUtf8());
    FullTextManagerQt m;
    QVERIFY(m.loadDictionariesFromEnv());
    const QVariantMap empty;
    // 父目录不存在 → QSaveFile 打不开 → false
    QVERIFY(!m.exportSourceDiff(empty, m_dir.filePath("no_dir/x.json")));
    // 直接覆盖一个目录 → 也应失败
    const QString asDir = m_dir.filePath("dir_target.json");
    QVERIFY(QDir().mkpath(asDir));
    QVERIFY(!m.exportSourceDiff(empty, asDir));
}

QTEST_MAIN(FullTextManagerQtTest)
#include "fulltext_manager_qt_test.moc"
