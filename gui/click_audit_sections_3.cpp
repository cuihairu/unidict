// 离屏 GUI 真点审计场景（LAY-010 自 gui/click_audit_test.cpp 拆出，第 3/4 批）：
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

void t11_vocabDblClick(AuditCtx& c) {
    QTabWidget* tabs = c.sideTabs();
    tabClick(tabs, 1);
    QListWidget* vocab = c.list("vocabList");
    if (vocab->count() == 0) {
        audit(false, QStringLiteral("收藏项双击回查"), QStringLiteral("列表空"));
        tabClick(tabs, 0);
        return;
    }
    QListWidgetItem* item = vocab->item(0);
    const QString word = item->data(Qt::UserRole).toString();
    c.input()->setText(QStringLiteral("greeting"));
    QTest::keyClick(c.input(), Qt::Key_Return);
    flush(100);
    QListWidgetItem* target = nullptr;
    for (int i = 0; i < vocab->count(); ++i) {
        if (vocab->item(i)->data(Qt::UserRole).toString() == word) {
            target = vocab->item(i);
        }
    }
    if (!target) {
        target = vocab->item(0);
    }
    QTest::mousePress(vocab->viewport(), Qt::LeftButton, Qt::NoModifier,
                      vocab->visualItemRect(target).center());
    QTest::mouseRelease(vocab->viewport(), Qt::LeftButton, Qt::NoModifier,
                        vocab->visualItemRect(target).center());
    flush(40);
    QTest::mouseDClick(vocab->viewport(), Qt::LeftButton, Qt::NoModifier,
                       vocab->visualItemRect(target).center());
    flush(120);
    audit(c.input()->text() == target->data(Qt::UserRole).toString(),
          QStringLiteral("收藏项双击回查"),
          QStringLiteral("word=%1 input=%2")
              .arg(target->data(Qt::UserRole).toString())
              .arg(c.input()->text()));
    tabClick(tabs, 0);
}

void t12_vocabTagDialog(AuditCtx& c) {
    QTabWidget* tabs = c.sideTabs();
    tabClick(tabs, 1);
    QListWidget* vocab = c.list("vocabList");
    if (vocab->count() == 0) {
        audit(false, QStringLiteral("收藏右键设置标签"), QStringLiteral("列表空"));
        tabClick(tabs, 0);
        return;
    }
    QListWidgetItem* item = vocab->item(0);
    c.noteWord = item->data(Qt::UserRole).toString();

    modalSeen = false;
    // 菜单 exec 里真点「设置标签…」→ 输入框 exec 里代填 en + Ok
    QTimer::singleShot(200, [] {
        if (QWidget* w = QApplication::activeModalWidget()) {
            modalSeen = true;  // getText 弹层真出现（代填+Ok 走 accept 路径）
            inputAccept(w, QStringLiteral("en"));
        }
    });
    QTimer::singleShot(100, [vocab, item] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("设置标签…")) {
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(a).center());
                return;
            }
        }
        menu->close();
    });
    rightClick(vocab->viewport(), vocab->visualItemRect(item).center());
    flush(500);

    // refreshVocabulary 后按 word 重找，断言 tags 已挂上
    bool tagged = false;
    for (int i = 0; i < vocab->count(); ++i) {
        if (vocab->item(i)->data(Qt::UserRole).toString() == c.noteWord) {
            const QStringList tags =
                vocab->item(i)->data(Qt::UserRole + 1).toStringList();
            tagged = tags.contains(QStringLiteral("en"));
        }
    }
    audit(modalSeen && tagged,
          QStringLiteral("收藏右键真点「设置标签…」→ 输入框代填落库"),
          QStringLiteral("modalSeen=%1 tagged=%2")
              .arg(modalSeen ? 1 : 0)
              .arg(tagged ? 1 : 0));
    tabClick(tabs, 0);
}

void t13_vocabGroupFilter(AuditCtx& c) {
    QTabWidget* tabs = c.sideTabs();
    tabClick(tabs, 1);
    QComboBox* group = c.win->findChild<QComboBox*>("vocabGroupBox");
    const int enIndex = group->findText(QStringLiteral("en"));
    audit(enIndex > 0, QStringLiteral("收藏分组下拉聚合了 en 标签"),
          QStringLiteral("items=%1").arg(group->count()));
    if (enIndex > 0) {
        const bool picked = comboSelect(group, enIndex);
        flush(100);
        QListWidget* vocab = c.list("vocabList");
        const bool filtered =
            !vocab->count() || !vocab->item(0)
                                   ->data(Qt::UserRole + 1)
                                   .toStringList()
                                   .contains(QStringLiteral("en"))
                ? vocab->count() == 0
                : true;
        audit(picked && filtered,
              QStringLiteral("收藏分组过滤真点生效"),
              QStringLiteral("picked=%1 count=%2")
                  .arg(picked ? 1 : 0)
                  .arg(vocab->count()));
        comboSelect(group, 0);
        flush(80);
    }
    tabClick(tabs, 0);
}

void t14_vocabRemoveMenu(AuditCtx& c) {
    QTabWidget* tabs = c.sideTabs();
    tabClick(tabs, 1);
    QListWidget* vocab = c.list("vocabList");
    const int before = vocab->count();
    if (before == 0) {
        audit(false, QStringLiteral("收藏右键移除"), QStringLiteral("列表空"));
        tabClick(tabs, 0);
        return;
    }
    QListWidgetItem* item = vocab->item(0);
    const QString word = item->data(Qt::UserRole).toString();

    bool removed = false;
    QTimer::singleShot(100, [&removed] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("移除收藏")) {
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(a).center());
                removed = true;
                return;
            }
        }
        menu->close();
    });
    rightClick(vocab->viewport(), vocab->visualItemRect(item).center());
    flush(300);

    bool gone = true;
    for (int i = 0; i < vocab->count(); ++i) {
        if (vocab->item(i)->data(Qt::UserRole).toString() == word) {
            gone = false;
        }
    }
    audit(removed && gone && vocab->count() == before - 1,
          QStringLiteral("收藏右键菜单真点「移除收藏」"),
          QStringLiteral("removed=%1 count %2→%3")
              .arg(removed ? 1 : 0)
              .arg(before)
              .arg(vocab->count()));
    tabClick(tabs, 0);
}

} // namespace gui_audit
