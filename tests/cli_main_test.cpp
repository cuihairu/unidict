// cli/main.cpp（Qt 版 CLI）行为测试——todo Q-9（此前 0%）。
// main() 不能链进 QTest 二进制（符号冲突），按子进程驱动：QProcess 跑
// unidict_cli，断言退出码与 stdout。coverage 构建里子进程同样带插桩，
// .gcda 路径编译期烧死在构建树（二进制拷去临时目录也一样写回），gcovr
// 照常归并——被测的就是真 CLI 本身。
//
// 状态隔离：DictionaryManager 把 dictionary_state.json 读写进
// AppDataLocation（Linux=$XDG_DATA_HOME，Windows=%LOCALAPPDATA%）。每
// 个用例给子进程一对独立的临时目录 env，既不动用户真实词典状态，也让
// "空状态"断言与用例顺序无关。macOS 的 AppDataLocation 没有 env 重定向
// 口子——本仓库开发与 CI 均在 Linux，假设注明于此。
#include <QtTest>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>

class CliMainTest : public QObject
{
    Q_OBJECT

private slots:
    // 参数解析与路由
    void noArgs_printsUsageAndExit1();
    void help_exitsZeroWithOptionList();
    void version_printsAppVersion();

    // --list 的三种形态：空状态 / 空状态+错误 / 有词典
    void list_emptyState_saysNoDictionariesExit1();
    void list_validEmptyState_printsOnlyNoDictionaries();
    void list_missingDir_printsLastError();
    void list_singleDict_printsNameAndFormat();
    void list_dictDir_printsContainedDict();

    // 查词路径：命中 0 / 未命中 2
    void lookup_knownWord_exit0AndPrintsDefinition();
    void lookup_unknownWord_exit2();

    // loadDefaultDictionaryLocations 的两条自动加载路
    void envDictDir_isLoaded();
    void localDictionariesDir_nextToBinary_isLoaded();

private:
    struct Run {
        int code = -1;
        QString out;
        QString err;
    };
    // 跑 CLI。binary 默认 UNIDICT_CLI_PATH；localDir 用例传拷贝到临时
    // 目录的副本（applicationDirPath 随之搬家）。isoDir 承接
    // XDG_DATA_HOME/LOCALAPPDATA；extraEnv 追加单条 env。
    Run run(const QTemporaryDir& isoDir, const QStringList& args,
            const QString& binary = QString(),
            const QString& extraEnvName = QString(),
            const QString& extraEnvValue = QString());

    // 造一个只含 dict.json 副本的词典目录（examples/dict.json 的 name
    // 是 "Unidict Sample"，--list 断言靠它）。失败返回空串——QTest 宏
    // 展开是 `return;`，非 void 函数里用不了，断言留在调用方
    static QString makeDictDirIn(const QString& parent,
                                 const QString& name = QStringLiteral("dicts"));
};

QString CliMainTest::makeDictDirIn(const QString& parent, const QString& name)
{
    const QString dir = QDir(parent).filePath(name);
    QDir().mkpath(dir);
    const QString src = QString::fromUtf8(UNIDICT_DICT_JSON);
    if (!QFile::copy(src, QDir(dir).filePath(QFileInfo(src).fileName()))) {
        return QString();
    }
    return dir;
}

CliMainTest::Run CliMainTest::run(const QTemporaryDir& isoDir,
                                  const QStringList& args,
                                  const QString& binary,
                                  const QString& extraEnvName,
                                  const QString& extraEnvValue)
{
    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // DictionaryManager 状态的落点重定向到用例私有目录（见文件头注释）
    env.insert(QStringLiteral("XDG_DATA_HOME"), isoDir.path());
    env.insert(QStringLiteral("LOCALAPPDATA"), isoDir.path());
    if (!extraEnvName.isEmpty()) {
        env.insert(extraEnvName, extraEnvValue);
    }
    proc.setProcessEnvironment(env);
    proc.setProcessChannelMode(QProcess::SeparateChannels);

    const QString prog = binary.isEmpty()
                             ? QString::fromUtf8(UNIDICT_CLI_PATH)
                             : binary;
    proc.start(prog, args);
    Run r;
    if (!proc.waitForStarted(10000)) {
        r.err = QStringLiteral("start failed: ") + prog;
        return r;   // code 留 -1，调用方的 code>=0 断言会带上 err 炸出来
    }
    if (!proc.waitForFinished(30000)) {
        r.err = QStringLiteral("timeout: ") + prog + QStringLiteral(" ") +
                args.join(QLatin1Char(' '));
        return r;
    }
    r.code = proc.exitCode();
    r.out = QString::fromUtf8(proc.readAllStandardOutput());
    r.err = QString::fromUtf8(proc.readAllStandardError());
    return r;
}

void CliMainTest::noArgs_printsUsageAndExit1()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 1);
    QVERIFY2(r.out.contains(QStringLiteral("Usage: unidict_cli")),
             qPrintable(r.out));
    // 三条 usage 行都打出来（--dict / --dict-dir / --list）
    QVERIFY(r.out.contains(QStringLiteral("--dict-dir <directory>")));
    QVERIFY(r.out.contains(QStringLiteral("--list")));
}

void CliMainTest::help_exitsZeroWithOptionList()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {QStringLiteral("--help")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    // QCommandLineParser 的 help 面：自述 + 全部选项
    QVERIFY2(r.out.contains(QStringLiteral("Unidict command line lookup")),
             qPrintable(r.out));
    QVERIFY(r.out.contains(QStringLiteral("--dict-dir")));
    QVERIFY(r.out.contains(QStringLiteral("-d, --dict <file>")));
}

void CliMainTest::version_printsAppVersion()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {QStringLiteral("--version")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    QVERIFY2(r.out.contains(QStringLiteral("unidict_cli 1.0")),
             qPrintable(r.out));
}

void CliMainTest::list_emptyState_saysNoDictionariesExit1()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    // 全新隔离环境：loadState 找不到状态文件会置 lastError，--list 的
    // 空列表分支除了"No dictionaries loaded."还要多打一行错误
    const Run r = run(iso, {QStringLiteral("--list")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 1);
    QVERIFY2(r.out.contains(QStringLiteral("No dictionaries loaded.")),
             qPrintable(r.out));
    QVERIFY2(r.out.contains(QStringLiteral("State file does not exist")),
             qPrintable(r.out));   // lastError 非空 → 多打一行
}

void CliMainTest::list_validEmptyState_printsOnlyNoDictionaries()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    // 预置合法的空状态文件（schema：root.dictionaries 数组，可为空）：
    // loadState 成功、lastError 保持空 → 空列表分支只打一句话。
    // AppDataLocation=$XDG_DATA_HOME/unidict_cli（org 名未设，取 app 名）
    const QString stateDir = QDir(iso.path()).filePath(QStringLiteral("unidict_cli"));
    QDir().mkpath(stateDir);
    {
        QFile f(QDir(stateDir).filePath(QStringLiteral("dictionary_state.json")));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"dictionaries\": []}");
    }
    const Run r = run(iso, {QStringLiteral("--list")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 1);
    QVERIFY2(r.out == QStringLiteral("No dictionaries loaded.\n"),
             qPrintable(r.out));   // 干净空态：就这一行，没有错误行
}

void CliMainTest::list_missingDir_printsLastError()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const QString nope = QDir(iso.path()).filePath(QStringLiteral("no-such-dir"));
    const Run r = run(iso, {QStringLiteral("--list"),
                            QStringLiteral("-D"), nope});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 1);
    QVERIFY2(r.out.contains(QStringLiteral("No dictionaries loaded.")),
             qPrintable(r.out));
    // 词典列表空但 lastError 有值 → 额外打一行错误（-D 目录不存在）
    QVERIFY2(r.out.contains(QStringLiteral("Dictionary directory does not exist")),
             qPrintable(r.out));
    QVERIFY(r.out.contains(nope));
}

void CliMainTest::list_singleDict_printsNameAndFormat()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {QStringLiteral("--list"),
                            QStringLiteral("-d"),
                            QString::fromUtf8(UNIDICT_DICT_JSON)});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    // info.name [format] path 一行
    QVERIFY2(r.out.contains(QStringLiteral("Unidict Sample [Json]")),
             qPrintable(r.out));
    QVERIFY(r.out.contains(QStringLiteral("dict.json")));
}

void CliMainTest::list_dictDir_printsContainedDict()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const QString dir = makeDictDirIn(iso.path());
    QVERIFY(!dir.isEmpty());
    const Run r = run(iso, {QStringLiteral("--list"),
                            QStringLiteral("-D"), dir});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    QVERIFY2(r.out.contains(QStringLiteral("Unidict Sample")),
             qPrintable(r.out));
}

void CliMainTest::lookup_knownWord_exit0AndPrintsDefinition()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {QStringLiteral("-d"),
                            QString::fromUtf8(UNIDICT_DICT_JSON),
                            QStringLiteral("hello")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    QVERIFY2(r.out.contains(QStringLiteral("greeting")), qPrintable(r.out));
}

void CliMainTest::lookup_unknownWord_exit2()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const Run r = run(iso, {QStringLiteral("-d"),
                            QString::fromUtf8(UNIDICT_DICT_JSON),
                            QStringLiteral("q9-no-such-word")});
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 2);   // 查无此词：非零但不与 usage(1) 混
}

void CliMainTest::envDictDir_isLoaded()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    const QString dir = makeDictDirIn(iso.path());
    QVERIFY(!dir.isEmpty());
    // UNIDICT_DICT_DIR 走 loadDefaultDictionaryLocations 的 env 路
    const Run r = run(iso, {QStringLiteral("--list")}, QString(),
                      QStringLiteral("UNIDICT_DICT_DIR"), dir);
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    QVERIFY2(r.out.contains(QStringLiteral("Unidict Sample")),
             qPrintable(r.out));
}

void CliMainTest::localDictionariesDir_nextToBinary_isLoaded()
{
    QTemporaryDir iso;
    QVERIFY(iso.isValid());
    // 把 CLI 拷进临时目录跑：applicationDirPath 搬家，dictionaries/
    // 子目录才被 localDir 路捡到（构建目录里造这目录会毒化别的用例与
    // 并发会话，必须走副本）
    const QString binDir = QDir(iso.path()).filePath(QStringLiteral("bin"));
    QDir().mkpath(binDir);
    const QString src = QString::fromUtf8(UNIDICT_CLI_PATH);
    const QString exe = QDir(binDir).filePath(QFileInfo(src).fileName());
    QVERIFY2(QFile::copy(src, exe), qPrintable(src + " -> " + exe));
    // 构建树的 rpath 是绝对路径，副本在 Linux 下直接可跑（.gcda 仍写回
    // 构建树——覆盖归因不受拷贝位置影响）
    // 目录名必须是 "dictionaries"——loadDefaultDictionaryLocations 找的就是它
    QVERIFY(!makeDictDirIn(binDir, QStringLiteral("dictionaries")).isEmpty());

    const Run r = run(iso, {QStringLiteral("--list")}, exe);
    QVERIFY2(r.code >= 0, qPrintable(r.err));
    QCOMPARE(r.code, 0);
    QVERIFY2(r.out.contains(QStringLiteral("Unidict Sample")),
             qPrintable(r.out));
}

QTEST_MAIN(CliMainTest)
#include "cli_main_test.moc"
