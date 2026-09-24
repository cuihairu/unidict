// GlobalHotkeys：平台无关层（注册簿记/开关/幂等）的行为测试。
// native 层仅 Windows 实现（RegisterHotKey + WM_HOTKEY），Linux/macOS
// 是 stub——涉及"注册是否真成功"的断言全部用状态一致性写法，三平台
// CI（Windows runner 无顶层窗口时注册也会失败）都必须绿。

#include <QCoreApplication>
#include <QtTest>

#include "global_hotkeys.h"

class GlobalHotkeysTest : public QObject {
    Q_OBJECT

private slots:
    void platform_expectation();

    void empty_action_and_sequence_rejected();

    void disable_blocks_registration();

    void unregister_unknown_is_noop();

    void state_consistency_register_unregister();
};

void GlobalHotkeysTest::platform_expectation() {
    // Windows 已实现 native 层，其余平台保持 stub
#ifdef Q_OS_WIN
    QVERIFY(GlobalHotkeys::isPlatformSupported());
#else
    QVERIFY(!GlobalHotkeys::isPlatformSupported());
#endif
}

void GlobalHotkeysTest::empty_action_and_sequence_rejected() {
    GlobalHotkeys hotkeys;
    QVERIFY(!hotkeys.registerHotkey(QString(), QStringLiteral("Ctrl+Alt+U")));
    QVERIFY(!hotkeys.registerHotkey(QStringLiteral("act"), QString()));
    QVERIFY(!hotkeys.registerHotkey(QStringLiteral("act"), QStringLiteral("")));

    // 无修饰键的序列在 native 层就该被拒（会抢死系统单键）
    QVERIFY(!hotkeys.registerHotkey(QStringLiteral("act"), QStringLiteral("U")));
    QVERIFY(hotkeys.registeredActions().isEmpty());
}

void GlobalHotkeysTest::disable_blocks_registration() {
    GlobalHotkeys hotkeys;
    hotkeys.setEnabled(false);
    QVERIFY(!hotkeys.isEnabled());
    QVERIFY(!hotkeys.registerHotkey(QStringLiteral("act"), QStringLiteral("Ctrl+Alt+U")));
    QVERIFY(hotkeys.registeredActions().isEmpty());

    hotkeys.setEnabled(true);
    QVERIFY(hotkeys.isEnabled());
}

void GlobalHotkeysTest::unregister_unknown_is_noop() {
    GlobalHotkeys hotkeys;
    hotkeys.unregisterHotkey(QStringLiteral("never-registered"));
    hotkeys.unregisterAllHotkeys(); // 空表上也必须安全
    QVERIFY(hotkeys.registeredActions().isEmpty());
}

void GlobalHotkeysTest::state_consistency_register_unregister() {
    GlobalHotkeys hotkeys;
    // 返回值与注册簿记必须一致（不硬断言 native 成败——随平台与窗口环境而异）
    const bool ok = hotkeys.registerHotkey(QStringLiteral("quickLookup"),
                                           QStringLiteral("Ctrl+Alt+U"));
    QCOMPARE(hotkeys.registeredActions().contains(QStringLiteral("quickLookup")), ok);
    QCOMPARE(hotkeys.getHotkeyForAction(QStringLiteral("quickLookup")),
             ok ? QStringLiteral("Ctrl+Alt+U") : QString());

    hotkeys.unregisterHotkey(QStringLiteral("quickLookup"));
    QVERIFY(hotkeys.registeredActions().isEmpty());
    QVERIFY(hotkeys.getHotkeyForAction(QStringLiteral("quickLookup")).isEmpty());

    // 重复注销幂等
    hotkeys.unregisterHotkey(QStringLiteral("quickLookup"));
}

// 不用 QTEST_MAIN：target 链了 Qt6::Gui（global_hotkeys.cpp 需要 QWindow），
// 宏会展开成 QGuiApplication，在无显示的 Linux CI 上加载平台插件即挂。
// QCoreApplication 不触碰平台插件，三平台都能裸跑。
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    GlobalHotkeysTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "global_hotkeys_test.moc"
