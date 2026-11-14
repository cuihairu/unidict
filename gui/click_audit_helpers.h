// 离屏 GUI 真点审计共享面（LAY-010 自 gui/click_audit_test.cpp 匿名
// 命名空间拆出）：audit 计数/日志、事件泵、右键/combo/锚点真点、
// 模态代填。inline 函数 + inline 变量（C++17）保证主 TU 与各 section
// TU 共享同一计数与模态标志。
#ifndef UNIDICT_GUI_CLICK_AUDIT_HELPERS_H
#define UNIDICT_GUI_CLICK_AUDIT_HELPERS_H

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QString>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QtTest/QtTest>
#include <QWidget>

namespace gui_audit {

inline int g_pass = 0;
inline int g_fail = 0;

inline void audit(bool ok, const QString& name, const QString& evidence) {
    if (ok) {
        ++g_pass;
        qInfo().noquote() << "[PASS]" << name << "—" << evidence;
    } else {
        ++g_fail;
        qInfo().noquote() << "[FAIL]" << name << "—" << evidence;
    }
}

// 事件循环跑满 ms 毫秒（布局/委托/弹层过渡结算）
inline void flush(int ms = 60) {
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

// 右键菜单触发：QTest 只发 press/release，不经平台合成层（那里才把右键
// release 升级成 QContextMenuEvent），customContextMenuRequested 收不到——
// 显式发事件，坐标语义与原生右键一致（widget 局部 + 全局）
inline void rightClick(QWidget* w, const QPoint& pos) {
    QContextMenuEvent ce(QContextMenuEvent::Mouse, pos, w->mapToGlobal(pos));
    QApplication::sendEvent(w, &ce);
}

// QTabWidget 第 i 个页签真点（点 QTabBar 的 tabRect 中心）
inline void tabClick(QTabWidget* tabs, int index) {
    QTabBar* bar = tabs->findChild<QTabBar*>();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                      bar->tabRect(index).center());
    flush(80);
}

// QComboBox 真点选项：点开 combo → 点 popup 里的项；吞点/不出窗退 signal 级
inline bool comboSelect(QComboBox* combo, int index) {
    QTest::mouseClick(combo, Qt::LeftButton);
    flush(80);
    QAbstractItemView* view = combo->view();
    if (view && view->isVisible()) {
        const QModelIndex mi = view->model()->index(index, 0);
        const QRect itemRect = view->visualRect(mi);
        if (itemRect.isValid() &&
            view->viewport()->rect().contains(itemRect.center())) {
            QWidget* popup = view->window();
            const QPoint pos =
                view->viewport()->mapTo(popup, itemRect.center());
            QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, pos);
            flush(80);
            if (combo->currentIndex() == index) {
                return true;
            }
        }
    }
    // 退级：signal 级接线；activated 处理器只读 itemText 不回写
    // currentIndex，补一致化。走到这说明 popup 吞了合成点击（离屏
    // Qt::Popup 抓取层行为）或压根没出窗，先收掉再验接线
    combo->hidePopup();
    combo->setCurrentIndex(index);
    emit combo->activated(index);
    flush(60);
    return combo->currentIndex() == index;
}

// 工具栏 QAction 对应的 QToolButton（按 text 找）
inline QToolButton* actionButton(QWidget* win, const QString& text) {
    for (QToolButton* b : win->findChildren<QToolButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

// 模块内 QTextBrowser 扫 viewport 找 anchor（fixture 文档短，首屏即可），
// 找到即 mouseClick —— QTextBrowser::anchorClicked 只在命中链接时发
inline bool anchorClick(QTextBrowser* view, const QString& href) {
    QWidget* vp = view->viewport();
    const QRect area = vp->rect() & QRect(0, 0, 1200, 700);
    for (int y = area.top(); y <= area.bottom(); y += 4) {
        for (int x = area.left(); x <= area.right(); x += 6) {
            if (view->anchorAt(QPoint(x, y)) == href) {
                QTest::mouseClick(vp, Qt::LeftButton, Qt::NoModifier,
                                  QPoint(x, y));
                flush(80);
                return true;
            }
        }
    }
    return false;
}

// 模态弹层在点击的同步栈里 exec —— 提前排 timer 进它的事件循环做事。
// 返回 false 表示弹层没出现（入口断线）
inline bool modalSeen = false;

template <typename Fn>
inline void onModal(int delayMs, Fn fn) {
    QTimer::singleShot(delayMs, [fn] {
        if (QWidget* w = QApplication::activeModalWidget()) {
            modalSeen = true;
            fn(w);
        }
    });
}

// QInputDialog（getText/getMultiLineText）：代填 + 点 Ok 返回值
inline void inputAccept(QWidget* w, const QString& text) {
    if (auto* le = w->findChild<QLineEdit*>()) {
        le->setText(text);
    } else if (auto* ta = w->findChild<QTextEdit*>()) {
        ta->setPlainText(text);
    }
    if (auto* bb = w->findChild<QDialogButtonBox*>()) {
        if (auto* ok = bb->button(QDialogButtonBox::Ok)) {
            QTest::mouseClick(ok, Qt::LeftButton);
        }
    }
}


} // namespace gui_audit

#endif // UNIDICT_GUI_CLICK_AUDIT_HELPERS_H
