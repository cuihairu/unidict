// Q-7 覆盖收口：adapters/qt 里此前 0% 的 std↔Qt 薄桥接——AI 服务
// （外部命令 + 无命令回落启发式）、剪贴板、设置项（QSettings 走 test
// mode 落盘临时目录）。ai_service/clipboard/settings 没有库目标（只有
// qmlui 直接编译），照 test_sync_service_qt 的做法把源码挂进本测试 target。

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "ai_service_qt.h"
#include "clipboard_qt.h"
#include "settings_qt.h"

class QtAdaptersTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    // AI 服务：env 命令入口、外部命令三种结局（stdout/stderr/启动失败/崩溃）、
    // 无命令时的回落启发式
    void q7_ai_service();
    // L99 LLM integration（provider auth/streaming/caching/prompts 面）：
    // 缓存 memo、prompt 模板、streaming 能力面（auth-gated 真 LLM 不入仓）。
    void q7_ai_l99();
    // 剪贴板读写往返（offscreen 平台插件提供内存剪贴板）
    void q7_clipboard();
    // 设置项：bool/string/int 往返 + 缺省值 + 落盘后跨实例可读
    void q7_settings();
};

void QtAdaptersTest::initTestCase() {
    // QSettings(UserScope) 与剪贴板都不碰真实用户目录
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("unidict-test"));
    QCoreApplication::setApplicationName(QStringLiteral("qt-adapters-test"));
}

void QtAdaptersTest::cleanupTestCase() {
    QStandardPaths::setTestModeEnabled(false);
}

void QtAdaptersTest::q7_ai_service() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // runExternal 是私有的：用临时目录里的脚本当"外部命令"，从
    // translate/grammarCheck 公共入口驱动它的全部出口。
    // （lambda 里不能用 QVERIFY——它展开出裸 return;——失败时返回空串，
    // 由调用侧 QVERIFY(!path.isEmpty()) 接住）
    auto writeScript = [&dir](const QString& name,
                              const QByteArray& body) -> QString {
        const QString path = QDir(dir.path()).filePath(name);
        QFile script(path);
        if (!script.open(QIODevice::WriteOnly)) {
            return {};
        }
        script.write(body);
        script.close();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                        QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                        QFileDevice::ExeOther);
        return path;
    };

    // 外部命令不能用 /bin/cat：本机 coreutils 是 uutils 实现，对 translate
    // 固定附加的 --to 参数直接报错退出——改用忽略全部参数的 cat 脚本
    const QString catSh = writeScript(QStringLiteral("cat.sh"),
                                      QByteArray("#!/bin/sh\ncat\n"));
    QVERIFY(!catSh.isEmpty());

    // 构造期 env 入口：有配置 → 直接作为外部命令
    qputenv("UNIDICT_AI_CMD", catSh.toUtf8());
    UnidictAdaptersQt::AiServiceQt viaEnv;
    QCOMPARE(viaEnv.command(), catSh);
    // P-9 provider 可替换面：有命令 = command + heuristic 兜底链
    QCOMPARE(viaEnv.providerNames(), (QStringList{"command", "heuristic"}));
    qunsetenv("UNIDICT_AI_CMD");

    // 无配置 → 空命令
    UnidictAdaptersQt::AiServiceQt plain;
    QVERIFY(plain.command().isEmpty());
    // 无命令 = 仅 heuristic 一家
    QCOMPARE(plain.providerNames(), (QStringList{"heuristic"}));

    // setCommand/command 读写
    plain.setCommand(catSh);
    QCOMPARE(plain.command(), catSh);
    QCOMPARE(plain.providerNames(), (QStringList{"command", "heuristic"}));

    // 清空命令 → 摘除 command provider（可替换双向成立）
    plain.setCommand(QString());
    QCOMPARE(plain.providerNames(), (QStringList{"heuristic"}));
    QCOMPARE(plain.command(), QString());
    plain.setCommand(catSh);

    // 外部命令三结局（stdout 命中 / stderr 兜底 / 信号击杀）都靠
    // shebang 脚本——QProcess 在 Windows 上经 CreateProcess 拉不起
    // .sh（无关联执行器 → FailedToStart），这三条只由 POSIX 作业覆盖；
    // Windows 侧断言该环境下实际发生的形态：命令起不来 → 回落启发式
#ifndef Q_OS_WIN
    // 外部命令命中：cat 把 stdin 原样回显 → translate/grammarCheck 直接采用
    QCOMPARE(plain.translate("hello", "zh"), QString("hello"));
    QCOMPARE(plain.grammarCheck("some text"), QString("some text"));

    // 只有 stderr 输出 → 读 stderr 兜底
    const QString stderrSh = writeScript(QStringLiteral("stderr.sh"),
                                         QByteArray("#!/bin/sh\necho boom >&2\n"));
    QVERIFY(!stderrSh.isEmpty());
    plain.setCommand(stderrSh);
    QCOMPARE(plain.translate("hello", "zh"), QString("boom"));

    // 非零退出码 → 失败命令的 stderr 不当答案（深审 P2：runExternal
    // 曾无视 exitCode，把报错文本当释义返回），回落启发式
    const QString failSh = writeScript(QStringLiteral("fail.sh"),
                                       QByteArray("#!/bin/sh\necho boom >&2\nexit 1\n"));
    QVERIFY(!failSh.isEmpty());
    plain.setCommand(failSh);
    QCOMPARE(plain.translate("hello", "zh"), QString("[Mock Translation to Chinese]\nhello"));

    // 进程被信号杀死 → 非 NormalExit → 外部输出作废，走回落启发式
    const QString crashSh = writeScript(QStringLiteral("crash.sh"),
                                        QByteArray("#!/bin/sh\nkill -9 $$\n"));
    QVERIFY(!crashSh.isEmpty());
    plain.setCommand(crashSh);
    QCOMPARE(plain.translate("hello", "zh"), QString("[Mock Translation to Chinese]\nhello"));
#else
    QCOMPARE(plain.translate("hello", "zh"),
             QString("[Mock Translation to Chinese]\nhello"));
#endif

    // 启动失败 → 外部输出为空 → 回落启发式
    plain.setCommand(QStringLiteral("unidict_no_such_cmd_xyz"));
    QCOMPARE(plain.translate("hi", "zh"), QString("[Mock Translation to Chinese]\nhi"));
    // 失败无感：坏命令仍在链上占位（providerNames 反映配置态），
    // 但对上层零报错——静默让位 heuristic
    QCOMPARE(plain.providerNames(), (QStringList{"command", "heuristic"}));

    // 无命令：runExternal 空返回，translate/grammarCheck 走回落启发式
    UnidictAdaptersQt::AiServiceQt none;
    QCOMPARE(none.translate("hi", "zh"), QString("[Mock Translation to Chinese]\nhi"));
    QCOMPARE(none.translate("hi", "en"), QString("[Mock Translation to English]\nhi"));
    QCOMPARE(none.translate("hi", "fr"), QString("[Mock Translation to fr]\nhi"));
    QCOMPARE(none.grammarCheck(QStringLiteral("   ")), QString("Input is empty."));
    QCOMPARE(none.grammarCheck(QStringLiteral("hello world")),
             QString("Suggestions:\n- Consider ending the sentence with punctuation.\n"
                     "- Sentence may start with a capital letter."));
    QCOMPARE(none.grammarCheck(QStringLiteral("Hello world")),
             QString("Suggestions:\n- Consider ending the sentence with punctuation."));
    QCOMPARE(none.grammarCheck(QStringLiteral("hello world.")),
             QString("Suggestions:\n- Sentence may start with a capital letter."));
    QCOMPARE(none.grammarCheck(QStringLiteral("Hello world.")),
             QString("No obvious issues (mock)."));

    // AI 语境造句（roadmap AI sentence generation）：无命令 → 启发式
    // mock 逐字断言；空白词 → 空串（链上无输出）；有命令 → sentences
    // 子命令与 translate/grammar 同管道（cat 回显即命中）
    QCOMPARE(none.generateSentences(QStringLiteral("hello")),
             QString("[Mock sentences]\n"
                     "1. She said \"hello\" when she opened the door.\n"
                     "2. He greeted everyone with a friendly \"hello\".\n"
                     "3. Try using \"hello\" in your next conversation."));
    QCOMPARE(none.generateSentences(QStringLiteral("  ")), QString());
    plain.setCommand(catSh);
#ifndef Q_OS_WIN
    QCOMPARE(plain.generateSentences(QStringLiteral("hello")), QString("hello"));
#else
    QCOMPARE(plain.generateSentences(QStringLiteral("hello")),
             QString("[Mock sentences]\n"
                     "1. She said \"hello\" when she opened the door.\n"
                     "2. He greeted everyone with a friendly \"hello\".\n"
                     "3. Try using \"hello\" in your next conversation."));
#endif
    plain.setCommand(QString());
}

// L99 LLM integration 面（provider auth 属用户凭据/计费，BLOCKED 不入仓；
// 此处覆盖可做部分：caching / prompts / streaming 能力面）。
void QtAdaptersTest::q7_ai_l99() {
    UnidictAdaptersQt::AiServiceQt svc;
    // 无命令 → 仅启发式；链上无 streaming（今日无真 LLM/SSE provider）
    QCOMPARE(svc.providerNames(), (QStringList{"heuristic"}));
    QVERIFY(!svc.streamingSupported());

    // L99 prompts：默认模板按操作稳定可查（纯展示面，不依赖命令）
    QVERIFY(!svc.promptFor(QStringLiteral("translate")).isEmpty());
    QVERIFY(!svc.promptFor(QStringLiteral("grammar")).isEmpty());
    QVERIFY(!svc.promptFor(QStringLiteral("sentences")).isEmpty());
    // 覆盖优先：setPrompt 后 promptFor 返回覆盖值
    svc.setPrompt(QStringLiteral("translate"), QStringLiteral("Custom translate prompt"));
    QCOMPARE(svc.promptFor(QStringLiteral("translate")),
             QString("Custom translate prompt"));
    // 空串清除覆盖 → 回落默认
    svc.setPrompt(QStringLiteral("translate"), QString());
    QVERIFY(!svc.promptFor(QStringLiteral("translate")).isEmpty());

    // L99 caching：重复调用命中 memo（启发式输出确定，可逐字断言）
    QVERIFY(svc.cacheEnabled());
    QCOMPARE(svc.cacheSize(), 0);
    const QString first = svc.translate(QStringLiteral("hello"), QStringLiteral("zh"));
    QCOMPARE(first, QString("[Mock Translation to Chinese]\nhello"));
    QCOMPARE(svc.cacheSize(), 1);
    // 覆盖变更使模板影响 command provider 输出（转发开启时）→ 旧结果作废，
    // 清缓存后重算仍得到启发式输出（无命令路径输出与模板无关，仅断言缓存清空）
    svc.setPrompt(QStringLiteral("translate"), QStringLiteral("Custom translate prompt"));
    QCOMPARE(svc.cacheSize(), 0);
    QCOMPARE(svc.translate(QStringLiteral("hello"), QStringLiteral("zh")), first);
    QCOMPARE(svc.cacheSize(), 1);
    // 转发开关变更链上模板生效状态 → 同样作废缓存
    svc.setPromptForwarding(true);
    QCOMPARE(svc.cacheSize(), 0);
    QCOMPARE(svc.translate(QStringLiteral("hello"), QStringLiteral("zh")), first);
    QCOMPARE(svc.cacheSize(), 1);
    svc.setPromptForwarding(false);
    QCOMPARE(svc.cacheSize(), 0);
    // 同输入同语言 → 缓存命中（输出不变）
    QCOMPARE(svc.translate(QStringLiteral("hello"), QStringLiteral("zh")), first);
    QCOMPARE(svc.cacheSize(), 1);
    // 不同语言 → 不同缓存键
    svc.translate(QStringLiteral("hello"), QStringLiteral("en"));
    QCOMPARE(svc.cacheSize(), 2);
    // 空输入短路：不落缓存
    svc.translate(QString(), QStringLiteral("zh"));
    QCOMPARE(svc.cacheSize(), 2);
    // clearCache 清空
    svc.clearCache();
    QCOMPARE(svc.cacheSize(), 0);
    // 关缓存：调用后不再 memo（size 保持 0）
    svc.setCacheEnabled(false);
    QVERIFY(!svc.cacheEnabled());
    svc.translate(QStringLiteral("hello"), QStringLiteral("zh"));
    QCOMPARE(svc.cacheSize(), 0);
    svc.setCacheEnabled(true);

    // L99 prompt 转发开关：默认关（P-9 cat 回显契约不变）
    QVERIFY(!svc.promptForwardingEnabled());
    svc.setPromptForwarding(true);
    QVERIFY(svc.promptForwardingEnabled());
    svc.setPromptForwarding(false);
    QVERIFY(!svc.promptForwardingEnabled());
}

void QtAdaptersTest::q7_clipboard() {
    UnidictAdaptersQt::ClipboardQt cb;
    cb.setText(QStringLiteral("unidict-q7"));
    QCOMPARE(cb.text(), QString("unidict-q7"));
    cb.setText(QString());
    QCOMPARE(cb.text(), QString());
}

void QtAdaptersTest::q7_settings() {
    UnidictAdaptersQt::SettingsQt settings;
    settings.setBool("q7/flag", true);
    QCOMPARE(settings.getBool("q7/flag"), true);
    QCOMPARE(settings.getBool("q7/absent", true), true);
    QCOMPARE(settings.getBool("q7/absent"), false);

    settings.setString("q7/word", QStringLiteral("hello"));
    QCOMPARE(settings.getString("q7/word"), QString("hello"));
    QCOMPARE(settings.getString("q7/absent", QStringLiteral("dflt")), QString("dflt"));
    QVERIFY(settings.getString("q7/absent").isEmpty());

    settings.setInt("q7/num", 42);
    QCOMPARE(settings.getInt("q7/num"), 42);
    QCOMPARE(settings.getInt("q7/absent", 7), 7);
    QCOMPARE(settings.getInt("q7/absent"), 0);

    // set* 都已 sync 落盘：新实例（重新打开 ini）仍能读到
    UnidictAdaptersQt::SettingsQt reopened;
    QCOMPARE(reopened.getInt("q7/num"), 42);
    QCOMPARE(reopened.getBool("q7/flag"), true);
}

QTEST_MAIN(QtAdaptersTest)
#include "qt_adapters_test.moc"
