// F13b macOS 焦点窗选区读取（AX API）+ TCC 授权交付面单测。
// 三种口径：
//   macOS  —— 已授权时自建 QPlainTextEdit 选中文本经 AX 读回原文（真回环）；
//             未授权 QSKIP——CI runner 无人工 TCC 授权，诚实跳过不假装；
//   其余平台 —— 恒空串 / 恒未授权 / 触发提示为 no-op 契约（不假装工作）；
//   全平台 —— 「辅助功能」系统设置深链常量（QML 侧据此开系统设置）。
// CI 面：macos-latest 有真会话可跑回环；Linux/Windows offscreen 只验契约。
#include "mac_selection_reader.h"

#include <QPlainTextEdit>
#include <QtTest/QTest>

class MacSelectionReaderTest : public QObject {
    Q_OBJECT

private slots:
    void unsupportedPlatformReturnsEmpty();
    void unsupportedPlatformNeverTrusted();
    void promptIsSafeOnEveryPlatform();
    void settingsUrlIsAccessibilityPane();
    void roundtripOwnWindowSelection();
};

// 非 macOS 只钉契约（不支持平台恒空串，不假装工作）
void MacSelectionReaderTest::unsupportedPlatformReturnsEmpty() {
#ifdef Q_OS_MACOS
    QSKIP("only meaningful off macOS");
#endif
    QCOMPARE(MacSelectionReader::readFocusedSelection(), QString());
}

// 非 macOS 恒未授权（不假装已获辅助功能权限）
void MacSelectionReaderTest::unsupportedPlatformNeverTrusted() {
#ifdef Q_OS_MACOS
    QSKIP("trust state depends on the host grant");
#endif
    QCOMPARE(MacSelectionReader::isAccessibilityTrusted(), false);
}

// 触发系统提示在任何平台都不得崩：非 macOS 为 no-op；macOS 上会弹系统
// 授权框，为避免 CI 里弹窗，本平台跳过真调用（授权面由 QML 流程验收）
void MacSelectionReaderTest::promptIsSafeOnEveryPlatform() {
#ifdef Q_OS_MACOS
    QSKIP("avoid popping the TCC dialog on CI");
#endif
    MacSelectionReader::requestAccessibilityPrompt();
    QVERIFY(true);  // no-op 不崩即通过
}

// 系统设置深链：各平台同值，须指向「辅助功能」隐私面板
void MacSelectionReaderTest::settingsUrlIsAccessibilityPane() {
    const QString url = MacSelectionReader::accessibilitySettingsUrl();
    QVERIFY(url.startsWith(QStringLiteral("x-apple.systempreferences:")));
    QVERIFY(url.contains(QStringLiteral("Privacy_Accessibility")));
}

// macOS：自建窗口选中文本 → AX 读回原文（roundtrip 真链路；未授权跳过）
void MacSelectionReaderTest::roundtripOwnWindowSelection() {
#ifndef Q_OS_MACOS
    QSKIP("macOS AX face only");
#endif
    if (!MacSelectionReader::isAccessibilityTrusted()) {
        QSKIP("needs manual TCC accessibility grant; not available on CI");
    }
    const char* expected = "hello focused selection";

    QPlainTextEdit* editor = new QPlainTextEdit;
    editor->setPlainText(QString::fromLatin1(expected));
    editor->resize(600, 200);
    editor->show();
    editor->activateWindow();
    editor->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(editor));

    editor->selectAll();
    QTest::qWait(200);  // 让焦点落到编辑器、AX 可读

    const QString got = MacSelectionReader::readFocusedSelection();
    // AX 可能按控件换行符回传，断言前归一空白
    QCOMPARE(got.simplified(), QString::fromLatin1(expected));
    editor->deleteLater();
}

QTEST_MAIN(MacSelectionReaderTest)
#include "mac_selection_reader_test.moc"
