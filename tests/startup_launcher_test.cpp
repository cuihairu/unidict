// StartupLauncher：开机自启开关的平台语义测试。
// Windows 走真注册表（HKCU Run 键，CI runner 一次性环境可安全往返）；
// 其余平台断言 stub 语义（isEnabled 恒假、开启被拒、关闭是成功 no-op）。
// 与 global_hotkeys_test 同款：QCoreApplication 裸跑，不触碰平台插件。

#include <QCoreApplication>
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
#ifdef Q_OS_WIN
    QVERIFY(StartupLauncher::isPlatformSupported());
#else
    QVERIFY(!StartupLauncher::isPlatformSupported());
#endif
}

void StartupLauncherTest::stub_semantics() {
    StartupLauncher launcher;
    if (StartupLauncher::isPlatformSupported()) {
        QSKIP("Windows 走 toggle_roundtrip 的真注册表往返");
    }
    QVERIFY(!launcher.isEnabled());
    QVERIFY(!launcher.setEnabled(true));  // 开启不可实现
    QVERIFY(!launcher.isEnabled());
    QVERIFY(launcher.setEnabled(false));  // 关闭是成功 no-op
    QVERIFY(!launcher.isEnabled());
}

void StartupLauncherTest::toggle_roundtrip() {
    if (!StartupLauncher::isPlatformSupported()) {
        QSKIP("仅 Windows 有 native 层");
    }
    StartupLauncher launcher;
    // 前置清理：确保从关闭态开始（上次异常退出可能残留）
    QVERIFY(launcher.setEnabled(false));
    QVERIFY(!launcher.isEnabled());

    QVERIFY(launcher.setEnabled(true));
    QVERIFY(launcher.isEnabled());

    QVERIFY(launcher.setEnabled(false));
    QVERIFY(!launcher.isEnabled());

    // 重复关闭幂等
    QVERIFY(launcher.setEnabled(false));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    StartupLauncherTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "startup_launcher_test.moc"
