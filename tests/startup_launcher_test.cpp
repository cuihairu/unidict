// StartupLauncher：开机自启开关的平台语义测试。
// Windows 走真注册表（HKCU Run 键，CI runner 一次性环境可安全往返）；
// Linux/macOS（F9 已实装）经 UNIDICT_AUTOSTART_DIR 钉临时目录往返并断言
// 落盘物内容（.desktop Exec / plist ProgramArguments 含应用路径），不碰
// 真实 ~/.config 或 ~/Library；移动/web 等仍断言 stub 语义。
// 与 global_hotkeys_test 同款：QCoreApplication 裸跑，不触碰平台插件。

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "startup_launcher.h"

class StartupLauncherTest : public QObject {
    Q_OBJECT

private slots:
    void platform_expectation();

    void stub_semantics();

    void toggle_roundtrip();
};

void StartupLauncherTest::platform_expectation() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    QVERIFY(StartupLauncher::isPlatformSupported());
#else
    QVERIFY(!StartupLauncher::isPlatformSupported());
#endif
}

void StartupLauncherTest::stub_semantics() {
    StartupLauncher launcher;
    if (StartupLauncher::isPlatformSupported()) {
        QSKIP("桌面平台（Win/Linux/macOS）走 toggle_roundtrip 真往返");
    }
    QVERIFY(!launcher.isEnabled());
    QVERIFY(!launcher.setEnabled(true));  // 开启不可实现
    QVERIFY(!launcher.isEnabled());
    QVERIFY(launcher.setEnabled(false));  // 关闭是成功 no-op
    QVERIFY(!launcher.isEnabled());
}

void StartupLauncherTest::toggle_roundtrip() {
    if (!StartupLauncher::isPlatformSupported()) {
        QSKIP("移动/web 无 native 层");
    }
    StartupLauncher launcher;
#ifndef Q_OS_WIN
    // 落盘密封：目录钉临时目录，测试不写真实 ~/.config / ~/Library
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(qputenv("UNIDICT_AUTOSTART_DIR", dir.path().toUtf8()));
#endif
    // 前置清理：确保从关闭态开始（上次异常退出可能残留）
    QVERIFY(launcher.setEnabled(false));
    QVERIFY(!launcher.isEnabled());

    QVERIFY(launcher.setEnabled(true));
    QVERIFY(launcher.isEnabled());

#ifndef Q_OS_WIN
    // 落盘物断言：文件存在且 Exec/ProgramArguments 含本测试可执行路径
    const QString fPath = dir.path() + "/"
                          + StartupLauncher::entryFileName();
    QFile f(fPath);
    QVERIFY(f.exists());
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString body = QString::fromUtf8(f.readAll());
    f.close();
    QVERIFY(body.contains(QCoreApplication::applicationFilePath()));
#endif

    QVERIFY(launcher.setEnabled(false));
    QVERIFY(!launcher.isEnabled());

#ifndef Q_OS_WIN
    QVERIFY(!QFile::exists(fPath));
#endif

    // 重复关闭幂等
    QVERIFY(launcher.setEnabled(false));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    StartupLauncherTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "startup_launcher_test.moc"
