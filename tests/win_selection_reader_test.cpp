// F13a 焦点窗选区读取单测（quick_lookup 热键文本源）。
// 两种口径：
//   Windows —— 真窗口回环：自建 QPlainTextEdit 选中文本，经 UIA 读回原文；
//   其余平台 —— 恒空串契约（诚实不假装，QML 侧据此回落剪贴板路径）。
// CI 面：windows-latest qwindows 有真桌面会话跑回环；Linux/macOS offscreen
// 跑契约与编译面（COM 代码只有 Windows 分支才编译，本地 Linux 只验契约）。
#include "win_selection_reader.h"

#include <QPlainTextEdit>
#include <QtTest/QTest>

class WinSelectionReaderTest : public QObject {
    Q_OBJECT

private slots:
    void unsupportedPlatformReturnsEmpty();
    void roundtripOwnWindowSelection();
    void noSelectionReturnsEmpty();
};

// 非 Windows 只钉契约（不支持平台恒空串，不假装工作）
void WinSelectionReaderTest::unsupportedPlatformReturnsEmpty() {
#ifdef Q_OS_WIN
    QSKIP("only meaningful off Windows");
#endif
    QCOMPARE(WinSelectionReader::readFocusedSelection(), QString());
}

// Windows：自建窗口选中文本 → UIA 读回原文（roundtrip 真链路）
void WinSelectionReaderTest::roundtripOwnWindowSelection() {
#ifndef Q_OS_WIN
    QSKIP("Windows UIA face only");
#endif
    const char* expected = "hello focused selection";

    QPlainTextEdit* editor = new QPlainTextEdit;
    editor->setPlainText(QString::fromLatin1(expected));
    editor->resize(600, 200);
    editor->show();
    editor->activateWindow();
    editor->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(editor));

    editor->selectAll();
    QTest::qWait(150);  // 让无障碍树激活、焦点落到编辑器

    const QString got = WinSelectionReader::readFocusedSelection();
    // UIA 可能按控件换行符回传，断言前归一空白
    QCOMPARE(got.simplified(), QString::fromLatin1(expected));
    editor->deleteLater();
}

// Windows：选区清空后回空串（触发式读取不得残留上一次结果）
void WinSelectionReaderTest::noSelectionReturnsEmpty() {
#ifndef Q_OS_WIN
    QSKIP("Windows UIA face only");
#endif
    QPlainTextEdit* editor = new QPlainTextEdit;
    editor->setPlainText("some text to read");
    editor->resize(600, 200);
    editor->show();
    editor->activateWindow();
    editor->setFocus();
    QVERIFY(QTest::qWaitForWindowExposed(editor));

    editor->textCursor().setPosition(0);  // 折叠选区
    QTest::qWait(150);

    QCOMPARE(WinSelectionReader::readFocusedSelection().trimmed(), QString());
    editor->deleteLater();
}

QTEST_MAIN(WinSelectionReaderTest)
#include "win_selection_reader_test.moc"
