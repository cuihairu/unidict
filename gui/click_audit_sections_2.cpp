// 离屏 GUI 真点审计场景（LAY-010 自 gui/click_audit_test.cpp 拆出，第 2/4 批）：
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

void t07_historyDblClick(AuditCtx& c) {
    QListWidget* hist = c.list("historyList");
    audit(hist->count() > 0, QStringLiteral("历史有条目（前序查询产生）"),
          QStringLiteral("count=%1").arg(hist->count()));
    if (hist->count() == 0) {
        audit(false, QStringLiteral("历史项双击回查"), QStringLiteral("无历史"));
        return;
    }
    QListWidgetItem* item = hist->item(0);
    const QString word = item->data(Qt::UserRole).toString();
    // 先查另一个 fixture 词制造差异，再双击历史第 0 条看回填
    c.input()->setText(QStringLiteral("greeting"));
    QTest::keyClick(c.input(), Qt::Key_Return);
    flush(100);
    QListWidgetItem* target = nullptr;
    for (int i = 0; i < hist->count(); ++i) {
        if (hist->item(i)->data(Qt::UserRole).toString() == word) {
            target = hist->item(i);
            break;
        }
    }
    if (!target) {
        target = hist->item(0);
    }
    // 双击成功会走 runLookup → refreshHistory 重建列表（旧 item 已删），
    // 断言用双击前存下的 QString，不碰失效指针
    const QString expected = target->data(Qt::UserRole).toString();
    const QPoint pos = hist->visualItemRect(target).center();
    QTest::mousePress(hist->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    QTest::mouseRelease(hist->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    flush(40);
    QTest::mouseDClick(hist->viewport(), Qt::LeftButton, Qt::NoModifier, pos);
    flush(120);
    audit(c.input()->text() == expected,
          QStringLiteral("历史项双击回查"),
          QStringLiteral("word=%1 input=%2")
              .arg(expected)
              .arg(c.input()->text()));
}

void t08_historyPinMenu(AuditCtx& c) {
    QListWidget* hist = c.list("historyList");
    if (hist->count() == 0) {
        audit(false, QStringLiteral("历史右键菜单置顶"), QStringLiteral("无历史"));
        return;
    }
    QListWidgetItem* item = hist->item(0);
    const QString word = item->data(Qt::UserRole).toString();
    const bool wasPinned = item->data(Qt::UserRole + 1).toBool();

    // 排 timer：菜单 exec 事件循环里真点「置顶/取消置顶」
    const QString kAction = QStringLiteral("置顶/取消置顶");
    bool menuPicked = false;
    QTimer::singleShot(100, [&menuPicked, wasPinned, kAction] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        QAction* act = nullptr;
        for (QAction* a : menu->actions()) {
            if (a->text() == kAction) {
                act = a;
            }
        }
        if (act) {
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(act).center());
            menuPicked = true;
        } else {
            menu->close();
        }
    });
    rightClick(hist->viewport(), hist->visualItemRect(item).center());
    flush(300);

    bool nowPinned = false;
    for (int i = 0; i < hist->count(); ++i) {
        if (hist->item(i)->data(Qt::UserRole).toString() == word) {
            nowPinned = hist->item(i)->data(Qt::UserRole + 1).toBool();
        }
    }
    audit(menuPicked && nowPinned != wasPinned,
          QStringLiteral("历史右键菜单真点「置顶/取消置顶」"),
          QStringLiteral("menuPicked=%1 pinned %2→%3")
              .arg(menuPicked ? 1 : 0)
              .arg(wasPinned ? 1 : 0)
              .arg(nowPinned ? 1 : 0));
    // 复原为未置顶，避免 📌 前缀干扰后续断言
    if (nowPinned) {
        QTimer::singleShot(100, [kAction] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                return;
            }
            for (QAction* a : menu->actions()) {
                if (a->text() == kAction) {
                    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                      menu->actionGeometry(a).center());
                    return;
                }
            }
            menu->close();
        });
        // 第 0 条可能已被 📌 换位——按 word 找
        for (int i = 0; i < hist->count(); ++i) {
            if (hist->item(i)->data(Qt::UserRole).toString() == word) {
                item = hist->item(i);
                break;
            }
        }
        rightClick(hist->viewport(), hist->visualItemRect(item).center());
        flush(300);
    }
}

void t09_historyRemoveMenu(AuditCtx& c) {
    QListWidget* hist = c.list("historyList");
    const int before = hist->count();
    if (before == 0) {
        audit(false, QStringLiteral("历史右键删除"), QStringLiteral("无历史"));
        return;
    }
    QListWidgetItem* item = hist->item(0);
    const QString word = item->data(Qt::UserRole).toString();

    bool removed = false;
    QTimer::singleShot(100, [&removed] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        if (!menu) {
            return;
        }
        for (QAction* a : menu->actions()) {
            if (a->text() == QStringLiteral("删除该条")) {
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(a).center());
                removed = true;
                return;
            }
        }
        menu->close();
    });
    rightClick(hist->viewport(), hist->visualItemRect(item).center());
    flush(300);

    bool gone = true;
    for (int i = 0; i < hist->count(); ++i) {
        if (hist->item(i)->data(Qt::UserRole).toString() == word) {
            gone = false;
        }
    }
    audit(removed && gone && hist->count() == before - 1,
          QStringLiteral("历史右键菜单真点「删除该条」"),
          QStringLiteral("removed=%1 count %2→%3")
              .arg(removed ? 1 : 0)
              .arg(before)
              .arg(hist->count()));
}

void t10_starButton(AuditCtx& c) {
    QPushButton* star = c.win->findChild<QPushButton*>("starButton");
    QTest::mouseClick(star, Qt::LeftButton);
    flush(120);
    const int count = c.list("vocabList")->count();
    const bool stored =
        !UnidictAdaptersQt::DataStoreQt::instance().getVocabularyMeta().isEmpty();
    audit(count > 0 && stored,
          QStringLiteral("收藏按钮真点入库"),
          QStringLiteral("vocabList=%1 stored=%2")
              .arg(count)
              .arg(stored ? 1 : 0));
}

} // namespace gui_audit
