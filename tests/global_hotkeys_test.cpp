// GlobalHotkeys：平台无关层（注册簿记/开关/幂等）的行为测试。
// native 层三桌面平台都已实现（Windows=RegisterHotKey+WM_HOTKEY、
// Linux=XGrabKey 需 UNIDICT_HAS_X11 编译期开关、macOS=Carbon 桥），但
// "注册是否真成功"随会话环境而异（本测试用 QCoreApplication 裸跑：
// Windows 无顶层窗口可挂、Linux platformName 必非 xcb），涉及真注册的
// 断言全部用状态一致性/诚实门控写法，三平台 CI 都必须绿。

#include <QCoreApplication>
#include <QGuiApplication>
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

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    void linux_headless_gate_fails_gracefully();
#endif
};

void GlobalHotkeysTest::platform_expectation() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    QVERIFY(GlobalHotkeys::isPlatformSupported());
#elif defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    // 编译期找到 X11 才暴露支持面
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

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
void GlobalHotkeysTest::linux_headless_gate_fails_gracefully() {
    // 本测试 main() 只起 QCoreApplication（不初始化平台插件），此时
    // QGuiApplication::platformName() 必非 "xcb"——诚实门控必须走失败
    // 路径：返回 false、不留簿记、可连续重试不崩。真 X 会话的抓键/
    // 按键链路无显示环境不可测，CI 只守门控接线（与审计 S33 同口径）。
    QVERIFY(QGuiApplication::platformName() != QLatin1String("xcb"));
    GlobalHotkeys hotkeys;
    for (int i = 0; i < 3; ++i) {
        QVERIFY(!hotkeys.registerHotkey(QStringLiteral("quickLookup"),
                                        QStringLiteral("Ctrl+Alt+U")));
        QVERIFY(hotkeys.registeredActions().isEmpty());
        QVERIFY(hotkeys.getHotkeyForAction(QStringLiteral("quickLookup")).isEmpty());
    }
    // 失败后注销/开关路径仍安全
    hotkeys.unregisterHotkey(QStringLiteral("quickLookup"));
    hotkeys.unregisterAllHotkeys();
    hotkeys.setEnabled(false);
    QVERIFY(!hotkeys.isEnabled());
}
#endif

// 不用 QTEST_MAIN：target 链了 Qt6::Gui（global_hotkeys.cpp 需要 QWindow），
// 宏会展开成 QGuiApplication，在无显示的 Linux CI 上加载平台插件即挂。
// QCoreApplication 不触碰平台插件，三平台都能裸跑。
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    GlobalHotkeysTest tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "global_hotkeys_test.moc"
