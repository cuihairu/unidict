// SyncServiceQt（adapters/qt）全量测试。此前该文件 0% 覆盖（todo Q-2）：
// 文件级同步 MVP 的全部判据——双向 syncNow 合并、previewDiff 四类差异、
// applyPreview 按类别整体应用、applySelection 按条目勾选应用、冲突预览
// 的 last_changes 记账、选择集导出/导入。这些路径出错会静默丢用户数据
// （合并写回前先 clearVocabulary/clearHistory，任何判据写错都直接反映到
// 落盘结果），所以断言全部对着"最终本地库 + 最终同步文件"两侧实打实核。
//
// 隔离：DataStore 是进程内单例（core/data_store.h → DataStoreQt →
// DataStoreStd），initTestCase 里把存储路径重定向到 QTemporaryDir，每个
// 用例开头 resetStore() 清历史/生词再播种。所有失败分支都挑的不依赖文件
// 权限的构造（root 下 chmod 无效，会假绿）：
//   - 读打开失败   → 同步文件路径指向一个目录（QFile ReadOnly 打不开目录）
//   - 写打开失败   → 同步文件路径的父目录是一枚普通文件（ENOTDIR）
//   - JSON 坏文件  → 直接写字节

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>

#include "sync_service_qt.h"
#include "core/data_store.h"

using UnidictAdaptersQt::SyncServiceQt;
using UnidictCore::DataStore;

namespace {

struct Seed {
    const char* word;
    const char* def;
    qlonglong ts;
};

// 清库并播种。ts 必须 >0：DataStoreStd::add_vocabulary_item 会把 added_at==0
// 的新词填成当前时间，播种语义会被改掉。
void resetStore(const QStringList& hist, std::initializer_list<Seed> vocab) {
    // 历史按给定序原样重建（与同步回写同一原语），读回序=播种序
    DataStore::instance().restoreSearchHistory(hist);
    DataStore::instance().clearVocabulary();
    for (const Seed& v : vocab)
        DataStore::instance().addVocabularyItemWithTime(v.word, v.def, v.ts);
}

// 本地库里找某词（大小写不敏感，与同步层的键口径一致）；未命中返回空 map
QVariantMap localMetaOf(const QString& lowerWord) {
    const QVariantList meta = DataStore::instance().getVocabularyMeta();
    for (const QVariant& v : meta) {
        const QVariantMap m = v.toMap();
        if (m.value("word").toString().compare(lowerWord, Qt::CaseInsensitive) == 0)
            return m;
    }
    return {};
}

// 注意：这里不能用 Q_ASSERT_X——Release 下 QT_NO_DEBUG 让断言整体空转
// （连条件表达式都不求值），open() 会被静默跳过。返回类型非 void 的
// 辅助函数里也放不了 QVERIFY，坏文件一律回空对象，让调用点的 QCOMPARE
// 拿着空值自己红。
QJsonObject readJsonFile(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return {};
    return doc.object();
}

// 同步文件 vocab 数组的 word 列表
QStringList remoteWords(const QJsonObject& obj) {
    QStringList out;
    const QJsonArray va = obj.value("vocab").toArray();
    for (const QJsonValue& v : va) out.append(v.toObject().value("word").toString());
    return out;
}

QJsonObject remoteEntry(const QJsonObject& obj, const QString& lowerWord) {
    const QJsonArray va = obj.value("vocab").toArray();
    for (const QJsonValue& v : va) {
        const QJsonObject o = v.toObject();
        if (o.value("word").toString().compare(lowerWord, Qt::CaseInsensitive) == 0)
            return o;
    }
    return {};
}

QJsonArray strArr(std::initializer_list<const char*> items) {
    QJsonArray a;
    for (const char* s : items) a.append(QString::fromUtf8(s));
    return a;
}

QJsonObject vocabObj(const QString& word, const QString& def, bool withTs = true,
                     qlonglong ts = 0) {
    QJsonObject o;
    o["word"] = word;
    o["definition"] = def;
    if (withTs) o["added_at"] = ts;
    return o;
}

void writeBytes(const QString& path, const QByteArray& data) {
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(f.write(data), data.size());
}

void writeRemote(const QString& path, const QJsonArray& history, const QJsonArray& vocab) {
    QJsonObject o;
    o["history"] = history;
    o["vocab"] = vocab;
    writeBytes(path, QJsonDocument(o).toJson(QJsonDocument::Indented));
}

} // namespace

class SyncServiceQtTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(temp_.isValid());
        // 单例默认写 ./data/unidict.json（CWD 相对）——测试必须整个改道
        DataStore::instance().setStoragePath(temp_.filePath(QStringLiteral("data.json")));
    }

    void cleanupTestCase() {
        DataStore::instance().clearHistory();
        DataStore::instance().clearVocabulary();
    }

    // ---- 路径未设置：每个入口的早退 + setter/getter 本身 ----------------
    void unsetPath_allEntriesFail() {
        SyncServiceQt svc;
        QVERIFY(svc.syncFile().isEmpty());
        QVERIFY(svc.lastError().isEmpty());

        QVERIFY(!svc.syncNow());
        QCOMPARE(svc.lastError(), QStringLiteral("sync file not set"));

        QVariantMap pd = svc.previewDiff();
        QVERIFY(!pd.value("ok").toBool());
        QCOMPARE(pd.value("error").toString(), QStringLiteral("sync file not set"));

        QVERIFY(!svc.applyPreview(true, true, true, true));
        QCOMPARE(svc.lastError(), QStringLiteral("sync file not set"));

        QVariantMap lc = svc.lastChanges();
        QVERIFY(!lc.value("ok").toBool());
        QCOMPARE(lc.value("error").toString(), QStringLiteral("sync file not set"));

        QVERIFY(!svc.applySelection(QVariantMap{}));

        // setter 往返
        svc.setSyncFile(temp_.filePath(QStringLiteral("set_me.json")));
        QCOMPARE(svc.syncFile(), temp_.filePath(QStringLiteral("set_me.json")));
    }

    // ---- 读打开失败：同步文件路径是个目录 --------------------------------
    void directoryAsSyncFile_readFails() {
        resetStore({"q1"}, {});
        SyncServiceQt svc;
        svc.setSyncFile(temp_.filePath(QStringLiteral("remote_dir")));
        QDir().mkpath(svc.syncFile());

        QVERIFY(!svc.syncNow());
        QCOMPARE(svc.lastError(), QStringLiteral("cannot open sync file"));

        QVariantMap pd = svc.previewDiff();
        QVERIFY(!pd.value("ok").toBool());
        QCOMPARE(pd.value("error").toString(), QStringLiteral("cannot open sync file"));

        QVERIFY(!svc.applyPreview(true, true, true, true));
        QCOMPARE(svc.lastError(), QStringLiteral("cannot open sync file"));

        // applySelection 不设 lastError，只回 false
        QVERIFY(!svc.applySelection({{"localOnly", QStringList{"x"}}}));

        QVariantMap lc = svc.lastChanges();
        QVERIFY(!lc.value("ok").toBool());
        QCOMPARE(lc.value("error").toString(), QStringLiteral("cannot open sync file"));
    }

    // ---- 坏 JSON：读回来必须报错，不许静默当空远端 ------------------------
    void invalidJson_readFails() {
        const QString path = temp_.filePath(QStringLiteral("garbage.json"));
        writeBytes(path, "this is { not json");
        resetStore({"q1"}, {});
        SyncServiceQt svc;
        svc.setSyncFile(path);

        QVERIFY(!svc.syncNow());
        QCOMPARE(svc.lastError(), QStringLiteral("invalid JSON"));
        // 本地数据不许被动过
        QCOMPARE(DataStore::instance().getSearchHistory(100), QStringList{"q1"});

        QVariantMap lc = svc.lastChanges();
        QVERIFY(!lc.value("ok").toBool());
        QCOMPARE(lc.value("error").toString(), QStringLiteral("invalid JSON"));
    }

    // ---- syncNow：远端不存在 → 当空远端合并并落盘 -------------------------
    void syncNow_createsMissingRemote() {
        const QString path = temp_.filePath(QStringLiteral("fresh.json"));
        resetStore({"h1", "h2"}, {{"Keep", "keep-def", 400}});
        SyncServiceQt svc;
        svc.setSyncFile(path);

        QVERIFY(svc.syncNow());
        QVERIFY(QFile::exists(path));
        const QJsonObject obj = readJsonFile(path);
        QCOMPARE(obj.value("version").toInt(), 1);
        QVERIFY(obj.value("synced_at").toVariant().toLongLong() > 0);
        QCOMPARE(obj.value("history").toArray(), (strArr({"h1", "h2"})));
        QCOMPARE(remoteWords(obj), QStringList{"Keep"});
        // added_at>0 的词必须带时间戳落盘（syncNow 序列化判据）
        QCOMPARE(remoteEntry(obj, "keep").value("added_at").toVariant().toLongLong(), qlonglong(400));
        // 没有 last_changes（那是 apply* 的记账），lastChanges 走"无记账"早退
        QVariantMap lc = svc.lastChanges();
        QVERIFY(lc.value("ok").toBool());
        QVERIFY(lc.value("changes").toMap().isEmpty());
    }

    // ---- syncNow：全量合并（含脏元素与三类词） ----------------------------
    // 远端 vocab 混入非对象元素（数字）、空 word 对象、无 added_at 的对象，
    // 逐条核 read_remote 的过滤与 latest-wins 合并结果。
    void syncNow_fullMerge() {
        const QString path = temp_.filePath(QStringLiteral("merge.json"));
        resetStore({"loc1", "loc2"}, {
            {"Alpha",     "L-alpha", 100}, // 远端更新 → 远端胜出
            {"LocalKeep", "keep",    400}, // 仅本地
        });
        QJsonArray vocab;
        vocab.append(42);                                          // 非对象 → 跳过
        vocab.append(vocabObj("alpha", "R-alpha", true, 200));     // 远端更新
        vocab.append(vocabObj("gamma", "R-only", false));          // 仅远端，无 added_at
        vocab.append(vocabObj("", "no-word", true, 7));            // 空词 → 跳过
        writeRemote(path, strArr({"loc2", "rem1", "rem2"}), vocab);
        // 历史里混入非字符串元素也要被 read_remote 滤掉
        {
            QJsonObject o = readJsonFile(path);
            QJsonArray h = o.value("history").toArray();
            h.append(99);
            o["history"] = h;
            writeBytes(path, QJsonDocument(o).toJson());
        }

        SyncServiceQt svc;
        svc.setSyncFile(path);
        QVERIFY(svc.syncNow());

        // 本地库：alpha 取远端释义；LocalKeep 保留；gamma 拉入；空词不进
        QVariantMap alpha = localMetaOf("alpha");
        QCOMPARE(alpha.value("definition").toString(), QStringLiteral("R-alpha"));
        QCOMPARE(alpha.value("word").toString(), QStringLiteral("alpha")); // 取远端大小写
        QCOMPARE(localMetaOf("localkeep").value("definition").toString(),
                 QStringLiteral("keep"));
        QVERIFY(!localMetaOf("gamma").isEmpty());
        QVERIFY(localMetaOf("no-word").isEmpty());
        QCOMPARE(DataStore::instance().getSearchHistory(100),
                 (QStringList{"loc1", "loc2", "rem1", "rem2"}));

        // 远端文件：三个词；gamma 没有 added_at（0 不许写成假时间戳）
        const QJsonObject obj = readJsonFile(path);
        QCOMPARE(obj.value("history").toArray(), (strArr({"loc1", "loc2", "rem1", "rem2"})));
        QCOMPARE(remoteWords(obj), (QStringList{"alpha", "gamma", "LocalKeep"}));
        QCOMPARE(remoteEntry(obj, "alpha").value("added_at").toVariant().toLongLong(),
                 qlonglong(200));
        QVERIFY(!remoteEntry(obj, "gamma").contains("added_at"));
    }

    // ---- syncNow：写打开失败（父目录是普通文件） ---------------------------
    void syncNow_writeFails() {
        writeBytes(temp_.filePath(QStringLiteral("blocker1.txt")), "x");
        const QString path = temp_.filePath(QStringLiteral("blocker1.txt/sub/sync.json"));
        resetStore({"h1"}, {{"W", "d", 10}});
        SyncServiceQt svc;
        svc.setSyncFile(path);

        QVERIFY(!svc.syncNow());
        QCOMPARE(svc.lastError(), QStringLiteral("cannot open sync file for write"));
        QVERIFY(!QFileInfo(path).exists());
    }

    // ---- previewDiff：四类差异各给一条，同 ts 的不许进任何清单 -------------
    void previewDiff_classifiesAllCategories() {
        const QString path = temp_.filePath(QStringLiteral("diff.json"));
        resetStore({"h1"}, {
            {"LocalOnly",   "lo",  10},
            {"NewerRemote", "nr-old", 200},
            {"NewerLocal",  "nl",  400},
            {"Same",        "sm",  77},
        });
        QJsonArray vocab;
        vocab.append(vocabObj("remoteonly", "ro", true, 5));
        vocab.append(vocabObj("newerremote", "nr-new", true, 900));
        vocab.append(vocabObj("newerlocal", "nl-old", true, 100));
        vocab.append(vocabObj("same", "sm", true, 77));
        writeRemote(path, strArr({"h1"}), vocab);

        QFile rf(path);
        QVERIFY(rf.open(QIODevice::ReadOnly));
        const QByteArray remoteBefore = rf.readAll();
        rf.close();

        SyncServiceQt svc;
        svc.setSyncFile(path);
        const QVariantMap pd = svc.previewDiff();
        QVERIFY(pd.value("ok").toBool());
        QCOMPARE(pd.value("error").toString(), QString());
        // 键一律小写（合并按小写键，预览必须同口径，否则用户点不中）
        QCOMPARE(pd.value("localOnly").toStringList(), QStringList{"localonly"});
        QCOMPARE(pd.value("remoteOnly").toStringList(), QStringList{"remoteonly"});

        const QVariantList rn = pd.value("remoteNewer").toList();
        QCOMPARE(rn.size(), 1);
        QCOMPARE(rn[0].toMap().value("word").toString(), QStringLiteral("newerremote"));
        QCOMPARE(rn[0].toMap().value("local_ts").toLongLong(), qlonglong(200));
        QCOMPARE(rn[0].toMap().value("remote_ts").toLongLong(), qlonglong(900));

        const QVariantList ln = pd.value("localNewer").toList();
        QCOMPARE(ln.size(), 1);
        QCOMPARE(ln[0].toMap().value("word").toString(), QStringLiteral("newerlocal"));
        QCOMPARE(ln[0].toMap().value("local_ts").toLongLong(), qlonglong(400));
        QCOMPARE(ln[0].toMap().value("remote_ts").toLongLong(), qlonglong(100));

        // 预览不许改任何东西
        QCOMPARE(DataStore::instance().getSearchHistory(100), QStringList{"h1"});
        QCOMPARE(localMetaOf("newerremote").value("definition").toString(),
                 QStringLiteral("nr-old"));
        QVERIFY(rf.open(QIODevice::ReadOnly));
        QCOMPARE(rf.readAll(), remoteBefore); // 远端文件一个字节都不许动
    }

    // ---- applyPreview：四个开关全开，两侧各自应得的内容逐项核 -------------
    void applyPreview_allFlags_mergesBothSides() {
        const QString path = temp_.filePath(QStringLiteral("apply.json"));
        resetStore({"h1"}, {
            {"LocalOnly",   "LO",  10},
            {"NewerRemote", "old", 200},
            {"NewerLocal",  "NL-new", 400},
            {"Same",        "sm",  77},
        });
        QJsonArray vocab;
        vocab.append(vocabObj("newerremote", "new", true, 900));
        vocab.append(vocabObj("newerlocal", "oldr", true, 100));
        vocab.append(vocabObj("same", "sm", true, 77));
        vocab.append(vocabObj("RemoteOnly", "RO", true, 55));
        vocab.append(vocabObj("zero_ts", "Z", false)); // added_at 缺失 → 0
        writeRemote(path, strArr({"h1", "h2"}), vocab);

        SyncServiceQt svc;
        svc.setSyncFile(path);
        QVERIFY(svc.applyPreview(true, true, true, true));
        QVERIFY(svc.lastError().isEmpty());

        // 本地：拉入 remote-only；remote-newer 更新本地释义
        QCOMPARE(localMetaOf("remoteonly").value("definition").toString(),
                 QStringLiteral("RO"));
        QCOMPARE(localMetaOf("newerremote").value("definition").toString(),
                 QStringLiteral("new"));
        // local-only 不许被留在远端算进本地（本来就只在本地）
        QVERIFY(!localMetaOf("localonly").isEmpty());

        // 远端：local-only 推上去、local-newer 覆盖远端旧值、remote-newer
        // 的词在远端侧保持原样（更新的是本地，不是把远端改回旧值）
        const QJsonObject obj = readJsonFile(path);
        QCOMPARE(obj.value("history").toArray(), (strArr({"h1", "h2"})));
        QCOMPARE(remoteEntry(obj, "localonly").value("definition").toString(),
                 QStringLiteral("LO"));
        QCOMPARE(remoteEntry(obj, "newerlocal").value("definition").toString(),
                 QStringLiteral("NL-new"));
        QCOMPARE(remoteEntry(obj, "newerlocal").value("added_at").toVariant().toLongLong(),
                 qlonglong(400));
        QCOMPARE(remoteEntry(obj, "newerremote").value("definition").toString(),
                 QStringLiteral("new"));
        // added_at==0 的远端词回写时不许被编一个时间戳
        QVERIFY(!remoteEntry(obj, "zero_ts").contains("added_at"));

        // last_changes 记账 + 开关回写
        const QVariantMap lc = svc.lastChanges();
        QVERIFY(lc.value("ok").toBool());
        const QVariantMap ch = lc.value("changes").toMap();
        QCOMPARE(ch.value("takeRemoteNewer").toBool(), true);
        QCOMPARE(ch.value("takeLocalNewer").toBool(), true);
        QCOMPARE(ch.value("includeRemoteOnly").toBool(), true);
        QCOMPARE(ch.value("includeLocalOnly").toBool(), true);
        // zero_ts 词同样是 remote-only，被拉入时不许被编时间戳
        QCOMPARE(ch.value("pulled_remote_only").toStringList(),
                 QStringList({"RemoteOnly", "zero_ts"}));
        QCOMPARE(ch.value("pushed_local_only").toStringList(), QStringList{"LocalOnly"});
        QCOMPARE(ch.value("updated_local_from_remote").toStringList(), QStringList{"newerremote"});
        QCOMPARE(ch.value("updated_remote_from_local").toStringList(), QStringList{"NewerLocal"});
    }

    // ---- applyPreview：开关全关时只回写记账，词表不动 ----------------------
    void applyPreview_allFlagsOff_touchesNothingSemantic() {
        const QString path = temp_.filePath(QStringLiteral("apply_off.json"));
        resetStore({"h1"}, {{"LocalOnly", "LO", 10}});
        writeRemote(path, strArr({"h1", "h2"}),
                    {vocabObj("RemoteOnly", "RO", true, 55)});

        SyncServiceQt svc;
        svc.setSyncFile(path);
        QVERIFY(svc.applyPreview(false, false, false, false));

        // 本地既没拉远端词，也没丢自己的词
        QCOMPARE(localMetaOf("localonly").value("definition").toString(),
                 QStringLiteral("LO"));
        QVERIFY(localMetaOf("remoteonly").isEmpty());
        // 远端既没被推本地词，也没被覆盖
        const QJsonObject obj = readJsonFile(path);
        QCOMPARE(remoteWords(obj), QStringList{"RemoteOnly"});
        // 历史仍要并集（与 syncNow 同口径）
        QCOMPARE(obj.value("history").toArray(), (strArr({"h1", "h2"})));
        const QVariantMap ch = svc.lastChanges().value("changes").toMap();
        QCOMPARE(ch.value("takeRemoteNewer").toBool(), false);
        QVERIFY(ch.value("pulled_remote_only").toStringList().isEmpty());
    }

    // ---- applyPreview：写打开失败 -----------------------------------------
    void applyPreview_writeFails() {
        writeBytes(temp_.filePath(QStringLiteral("blocker2.txt")), "x");
        const QString path = temp_.filePath(QStringLiteral("blocker2.txt/sub/sync.json"));
        resetStore({"h1"}, {{"W", "d", 10}});
        SyncServiceQt svc;
        svc.setSyncFile(path);

        QVERIFY(!svc.applyPreview(true, true, true, true));
        QCOMPARE(svc.lastError(), QStringLiteral("cannot open sync file for write"));
    }

    // ---- applySelection：按条目勾选，大小写不敏感、幽灵条目要能容错 --------
    void applySelection_happyPath() {
        const QString path = temp_.filePath(QStringLiteral("sel.json"));
        resetStore({"g1"}, {
            {"LocalOnly", "LO",  10},
            {"beta",      "B-new", 800},
        });
        QJsonArray vocab;
        vocab.append(vocabObj("gamma", "G", true, 60));      // remoteOnly 勾选 → 拉入
        vocab.append(vocabObj("alpha", "A-new", true, 900)); // remoteNewer 勾选 → 强制覆盖本地
        vocab.append(vocabObj("zeta", "Z", false));          // 没人勾，ts=0 回写照常缺 added_at
        writeRemote(path, strArr({"g1", "hh"}), vocab);
        // 本地已有 alpha（旧值），remoteNewer 是无条件覆盖语义
        DataStore::instance().addVocabularyItemWithTime("alpha", "A-old", 100);

        SyncServiceQt svc;
        svc.setSyncFile(path);
        QVariantMap sel;
        sel["remoteOnly"]  = QStringList{"GAMMA", "ghost"};  // 大写键 + 不存在的勾选
        sel["localOnly"]   = QStringList{"localonly", "ghost2"};
        sel["remoteNewer"] = QStringList{"alpha"};
        sel["localNewer"]  = QStringList{"BETA"};            // 大写命中
        QVERIFY(svc.applySelection(sel));

        // 本地：gamma 拉入；alpha 被远端强制覆盖
        QCOMPARE(localMetaOf("gamma").value("definition").toString(), QStringLiteral("G"));
        QCOMPARE(localMetaOf("alpha").value("definition").toString(), QStringLiteral("A-new"));
        // 远端：local-only 推上去；beta 用本地新值覆盖远端；zeta 原样且无 added_at
        const QJsonObject obj = readJsonFile(path);
        QCOMPARE(remoteEntry(obj, "localonly").value("definition").toString(),
                 QStringLiteral("LO"));
        QCOMPARE(remoteEntry(obj, "beta").value("definition").toString(),
                 QStringLiteral("B-new"));
        QCOMPARE(remoteEntry(obj, "beta").value("added_at").toVariant().toLongLong(),
                 qlonglong(800));
        QVERIFY(!remoteEntry(obj, "zeta").contains("added_at"));
        QCOMPARE(obj.value("history").toArray(), (strArr({"g1", "hh"})));

        // 记账：selection_* 存用户原始勾选（含幽灵词），applied_* 只存命中的
        const QVariantMap ch = svc.lastChanges().value("changes").toMap();
        QCOMPARE(ch.value("selection_remote_only").toStringList(), QStringList({"GAMMA", "ghost"}));
        QCOMPARE(ch.value("selection_local_newer").toStringList(), QStringList{"BETA"});
        QCOMPARE(ch.value("pulled_remote_only").toStringList(), QStringList{"gamma"});
        QCOMPARE(ch.value("pushed_local_only").toStringList(), QStringList{"LocalOnly"});
        QCOMPARE(ch.value("updated_local_from_remote").toStringList(), QStringList{"alpha"});
        QCOMPARE(ch.value("updated_remote_from_local").toStringList(), QStringList{"beta"});
    }

    // ---- applySelection：读失败 / 写失败 -----------------------------------
    void applySelection_failures() {
        // 读：坏 JSON → 直接 false（该入口不设 lastError）
        const QString bad = temp_.filePath(QStringLiteral("sel_bad.json"));
        writeBytes(bad, "{oops");
        resetStore({"h"}, {});
        SyncServiceQt svc1;
        svc1.setSyncFile(bad);
        QVERIFY(!svc1.applySelection({{"remoteOnly", QStringList{"x"}}}));

        // 写：路径父级是普通文件
        writeBytes(temp_.filePath(QStringLiteral("blocker3.txt")), "x");
        const QString blocked = temp_.filePath(QStringLiteral("blocker3.txt/sub/sync.json"));
        SyncServiceQt svc2;
        svc2.setSyncFile(blocked);
        QVERIFY(!svc2.applySelection({{"remoteOnly", QStringList{"x"}}}));
    }

    // ---- lastChanges：文件不存在 → ok + 空记账 ------------------------------
    void lastChanges_missingFileIsOkEmpty() {
        resetStore({}, {});
        SyncServiceQt svc;
        svc.setSyncFile(temp_.filePath(QStringLiteral("nope.json")));
        const QVariantMap lc = svc.lastChanges();
        QVERIFY(lc.value("ok").toBool());
        QCOMPARE(lc.value("error").toString(), QString());
        QVERIFY(lc.value("changes").toMap().isEmpty());
    }

    // ---- 导出/导入选择集：嵌套路径自动建目录，往返保真 ----------------------
    void exportImportSelection_roundTrip() {
        const QString out = temp_.filePath(QStringLiteral("exports/deep/sel.json"));
        QVariantMap sel;
        sel["remoteOnly"]  = QStringList{"a", "B"};
        sel["localOnly"]   = QStringList{"c"};
        sel["remoteNewer"] = QStringList{};
        sel["localNewer"]  = QStringList{"d"};

        SyncServiceQt svc;
        QVERIFY(svc.exportSelection(sel, out));
        const QJsonObject obj = readJsonFile(out);
        QCOMPARE(obj.value("remoteOnly").toArray(), (strArr({"a", "B"})));
        QCOMPARE(obj.value("localOnly").toArray(), strArr({"c"}));
        QVERIFY(obj.value("remoteNewer").toArray().isEmpty());
        QVERIFY(obj.value("exported_at").toVariant().toLongLong() > 0);

        const QVariantMap imp = svc.importSelection(out);
        QVERIFY(imp.value("ok").toBool());
        const QVariantMap got = imp.value("selection").toMap();
        QCOMPARE(got.value("remoteOnly").toStringList(), QStringList({"a", "B"}));
        QCOMPARE(got.value("localOnly").toStringList(), QStringList{"c"});
        QCOMPARE(got.value("remoteNewer").toStringList(), QStringList{});
        QCOMPARE(got.value("localNewer").toStringList(), QStringList{"d"});
    }

    // ---- 导出/导入的失败面：目录路径 / 缺文件 / 坏 JSON / 非对象 ------------
    void exportImportSelection_failures() {
        SyncServiceQt svc;
        const QString dir = temp_.filePath(QStringLiteral("sel_dir"));
        QDir().mkpath(dir);
        QVariantMap sel;
        sel["remoteOnly"] = QStringList{"x"};
        QVERIFY(!svc.exportSelection(sel, dir)); // 打不开目录 → false

        QVariantMap imp = svc.importSelection(temp_.filePath(QStringLiteral("missing.json")));
        QVERIFY(!imp.value("ok").toBool());
        QCOMPARE(imp.value("error").toString(), QStringLiteral("cannot open file"));

        const QString bad = temp_.filePath(QStringLiteral("imp_bad.json"));
        writeBytes(bad, "{{{");
        imp = svc.importSelection(bad);
        QVERIFY(!imp.value("ok").toBool());
        QCOMPARE(imp.value("error").toString(), QStringLiteral("invalid JSON"));

        // 合法 JSON 但不是对象 → 同样 invalid JSON
        const QString arr = temp_.filePath(QStringLiteral("imp_arr.json"));
        writeBytes(arr, "[1,2,3]");
        imp = svc.importSelection(arr);
        QVERIFY(!imp.value("ok").toBool());
        QCOMPARE(imp.value("error").toString(), QStringLiteral("invalid JSON"));

        // 对象但没有那四个键 → ok，四个清单全空
        const QString emptyObj = temp_.filePath(QStringLiteral("imp_empty.json"));
        writeBytes(emptyObj, "{}");
        imp = svc.importSelection(emptyObj);
        QVERIFY(imp.value("ok").toBool());
        const QVariantMap got = imp.value("selection").toMap();
        QCOMPARE(got.value("remoteOnly").toStringList(), QStringList{});
        QCOMPARE(got.value("localNewer").toStringList(), QStringList{});
    }

    // ---- 幂等：syncNow 连跑两次结果收敛（合并判据没有次序依赖残留） ---------
    void syncNow_secondRunIsStable() {
        const QString path = temp_.filePath(QStringLiteral("stable.json"));
        resetStore({"a"}, {{"X", "x1", 50}});
        SyncServiceQt svc;
        svc.setSyncFile(path);
        QVERIFY(svc.syncNow());
        QJsonObject first = readJsonFile(path);
        QVERIFY(svc.syncNow());
        QJsonObject second = readJsonFile(path);
        first.remove("synced_at");
        second.remove("synced_at");
        QCOMPARE(second, first); // 除时间戳外必须逐字段同构
    }

private:
    QTemporaryDir temp_;
};

QTEST_MAIN(SyncServiceQtTest)
#include "sync_service_qt_test.moc"
