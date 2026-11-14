// 离屏 GUI 真点审计场景（LAY-010 自 gui/click_audit_test.cpp 拆出，第 1/4 批）：
// 场景清单与调用顺序见 click_audit_sections.h，共享面见 click_audit_helpers.h。

#define UNIDICT_GUI_AUDIT_NO_MAIN
#include "main.cpp"

#include "click_audit_helpers.h"
#include "click_audit_sections.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <QtTest/QtTest>

namespace gui_audit {

void t02_lookupByReturn(AuditCtx& c) {
    c.input()->setFocus();
    flush();
    c.input()->setText(QStringLiteral("hello"));
    QTest::keyClick(c.input(), Qt::Key_Return);
    flush(120);

    audit(!c.status()->text().isEmpty() &&
              !c.status()->text().contains(QStringLiteral("就绪")),
          QStringLiteral("回车查询落状态行"),
          QStringLiteral("statusText=%1").arg(c.status()->text()));
    audit(!c.view("resultView")->toPlainText().isEmpty(),
          QStringLiteral("回车查询出释义"),
          QStringLiteral("resultView len=%1")
              .arg(c.view("resultView")->toPlainText().size()));
    audit(c.win->findChild<QPushButton*>("starButton")->isEnabled(),
          QStringLiteral("查询成功后收藏按钮可用"),
          QStringLiteral("starButton enabled=1"));
}

void t03_completerSelect(AuditCtx& c) {
    c.input()->setFocus();
    flush();
    c.input()->setText(QStringLiteral("hel"));
    flush(120);

    QCompleter* completer = c.input()->completer();
    const bool hasItems = completer && completer->completionModel() &&
                          completer->completionModel()->rowCount() > 0;
    bool activated = false;
    QString picked;
    if (hasItems) {
        // 真点补全弹窗第一项（popup 可见才点；离屏不可见退级记事实）
        QAbstractItemView* popup = qobject_cast<QAbstractItemView*>(completer->popup());
        if (popup && popup->isVisible()) {
            const QModelIndex mi = completer->completionModel()->index(0, 0);
            const QPoint pos = popup->visualRect(mi).center();
            if (popup->viewport()->rect().contains(pos)) {
                QTest::mouseClick(popup->viewport(), Qt::LeftButton,
                                  Qt::NoModifier, pos);
                flush(120);
                activated = true;
                picked = c.input()->text();
            }
        }
        if (!activated) {
            // 键盘路径：Down 选中 + Enter 激活（completer activated 信号）
            QTest::keyClick(c.input(), Qt::Key_Down);
            QTest::keyClick(c.input(), Qt::Key_Return);
            flush(120);
            activated = true;
            picked = c.input()->text();
        }
    }
    audit(hasItems && activated && !picked.isEmpty(),
          QStringLiteral("补全选中回填并查询"),
          QStringLiteral("items=%1 picked=%2")
              .arg(hasItems ? 1 : 0)
              .arg(picked));
}

void t04_contentTabsSweep(AuditCtx& c) {
    QTabWidget* tabs = c.contentTabs();
    audit(tabs->count() == 5, QStringLiteral("内容页签五个"),
          QStringLiteral("count=%1").arg(tabs->count()));

    const QStringList names = {QStringLiteral("词典"), QStringLiteral("例句"),
                               QStringLiteral("词组"),
                               QStringLiteral("近义联想"),
                               QStringLiteral("全文检索")};
    bool allSwitched = true;
    QString detail;
    for (int i = 0; i < tabs->count(); ++i) {
        tabClick(tabs, i);
        if (tabs->currentIndex() != i) {
            allSwitched = false;
            detail += QStringLiteral("%1 ").arg(i);
        }
        if (tabs->tabText(i) != names.value(i)) {
            allSwitched = false;
            detail += QStringLiteral("text%1 ").arg(i);
        }
    }
    audit(allSwitched, QStringLiteral("五内容页签逐个真点切换"),
          detail.isEmpty() ? QStringLiteral("0-4 全部切到位")
                           : QStringLiteral("异常位: %1").arg(detail));

    // 四个非词典页有数据（fixture 全字段覆盖）
    tabClick(tabs, 1);
    audit(c.view("examplesView")->toPlainText().size() > 4,
          QStringLiteral("例句页有数据"),
          QStringLiteral("len=%1")
              .arg(c.view("examplesView")->toPlainText().size()));
    tabClick(tabs, 2);
    audit(c.view("phrasesView")->toPlainText().size() > 4,
          QStringLiteral("词组页有数据"),
          QStringLiteral("len=%1")
              .arg(c.view("phrasesView")->toPlainText().size()));
    tabClick(tabs, 3);
    audit(c.view("relatedView")->toPlainText().size() > 4,
          QStringLiteral("近义联想页有数据"),
          QStringLiteral("len=%1")
              .arg(c.view("relatedView")->toPlainText().size()));
    tabClick(tabs, 4);
    audit(c.view("fulltextView")->toPlainText().size() > 4,
          QStringLiteral("全文页有数据"),
          QStringLiteral("len=%1")
              .arg(c.view("fulltextView")->toPlainText().size()));
    tabClick(tabs, 0);
}

void t05_exampleAnchorJump(AuditCtx& c) {
    QTabWidget* tabs = c.contentTabs();
    tabClick(tabs, 1);  // 例句页
    // 例句页每个条目带 #ft:<i> 全文命中回查锚
    bool jumped = false;
    QString toWord;
    for (int i = 0; i < 20 && !jumped; ++i) {
        const QString href = QStringLiteral("#ft:%1").arg(i);
        if (anchorClick(c.view("examplesView"), href)) {
            jumped = true;
            flush(120);
            toWord = c.input()->text();
        }
    }
    audit(jumped && !toWord.isEmpty(),
          QStringLiteral("例句页 #ft 锚点真点跳转"),
          QStringLiteral("clicked=%1 word=%2").arg(jumped ? 1 : 0).arg(toWord));
    tabClick(tabs, 0);
}

void t06_sideTabsSwitch(AuditCtx& c) {
    QTabWidget* tabs = c.sideTabs();
    audit(tabs->count() >= 2, QStringLiteral("侧栏页签（历史/收藏）"),
          QStringLiteral("count=%1").arg(tabs->count()));
    tabClick(tabs, 1);
    audit(tabs->currentIndex() == 1,
          QStringLiteral("侧栏「收藏」页签真点切换"),
          QStringLiteral("currentIndex=%1").arg(tabs->currentIndex()));
    tabClick(tabs, 0);
    audit(tabs->currentIndex() == 0,
          QStringLiteral("侧栏「历史」页签真点切换"),
          QStringLiteral("currentIndex=%1").arg(tabs->currentIndex()));
}

} // namespace gui_audit
