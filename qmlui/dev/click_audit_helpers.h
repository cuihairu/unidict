// 离屏 UI 真点审计共享面（LAY-008 自 ui_click_audit.cpp 匿名命名空间
// 拆出）：audit 计数/日志、事件泵、真点/滚入/按键、视觉树查找、popup
// 读态、ClickCounter 信号计数。inline 函数 + inline 变量（C++17）保证
// 各 section TU 与 main 共享同一计数与日志；ClickCounter 的 moc 在主
// TU 文件尾 include（click_audit_helpers.h 的 moc 由 AUTOMOC 自动编入，恰好一次）。

#ifndef UNIDICT_CLICK_AUDIT_HELPERS_H
#define UNIDICT_CLICK_AUDIT_HELPERS_H

#include <QtGlobal>
#include <QCoreApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QList>
#include <QMouseEvent>
#include <QObject>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

namespace ui_audit {

inline int g_pass = 0;
inline int g_fail = 0;
inline QStringList g_log;

inline void audit(bool ok, const QString& name, const QString& evidence) {
    if (ok) ++g_pass; else ++g_fail;
    const QString line = QString("[%1] %2 — %3")
                             .arg(ok ? "PASS" : "FAIL", name, evidence);
    g_log << line;
    qInfo("%s", qPrintable(line));
}

inline void flushEvents() {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 60);
}

inline void settle(QQuickWindow* win) {
    // 定时器驱动的路径（建议 120ms / 内容 tab 30ms）需要真实时间流逝
    for (int i = 0; i < 30; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(15);
    }
    win->update();
    flushEvents();
}

// 真点：把 press+release 直接送到窗口事件链（QQuickWindow 按事件坐标
// 交给命中测试命中的 item，MouseArea/AbstractButton 与真机同路径）。
// 空元素直接返回（断言侧以 item 查找结果判 FAIL，不在 helper 里崩）
inline void clickItem(QQuickWindow* win, QQuickItem* item) {
    if (!item) return;
    const QPointF scene = item->mapToScene(QPointF(item->width() / 2.0,
                                                   item->height() / 2.0));
    const QPointF global = win->mapToGlobal(scene);
    QMouseEvent press(QEvent::MouseButtonPress, scene, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, scene, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    flushEvents();
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 同上，但点在 item 内的相对位置（0..1），Slider/SpinBox 定位用
inline void clickItemRel(QQuickWindow* win, QQuickItem* item, qreal rx, qreal ry) {
    if (!item) return;
    const QPointF scene = item->mapToScene(QPointF(item->width() * rx,
                                                   item->height() * ry));
    const QPointF global = win->mapToGlobal(scene);
    QMouseEvent press(QEvent::MouseButtonPress, scene, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, scene, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    flushEvents();
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 把 item 滚进其 Flickable 祖先（ScrollView 的内容体）的可视区：
// 抽屉固定高 + ScrollView 收纳后，折叠下方的控件坐标点不到，先滚再点。
// y 取内容坐标（对 contentItem 映射，与当前 contentY 无关），幂等
inline void scrollIntoView(QQuickWindow* win, QQuickItem* item) {
    if (!item) return;
    QQuickItem* flick = item->parentItem();
    while (flick && !flick->property("contentY").isValid())
        flick = flick->parentItem();
    if (!flick) return;
    QQuickItem* content = flick->property("contentItem").value<QQuickItem*>();
    const qreal y = content ? item->mapToItem(content, 0, 0).y()
                            : item->mapToItem(flick, 0, 0).y();
    const qreal h = item->height();
    const qreal viewH = flick->height();
    const qreal max =
        flick->property("contentHeight").toReal() - viewH;
    qreal target = y - (viewH - h) / 2;
    target = qBound<qreal>(0.0, target, qMax<qreal>(0.0, max));
    flick->setProperty("contentY", target);
    settle(win);
}

inline void sendKey(QQuickWindow* win, int key) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 视觉树查找：Repeater/ListView 委托的 QObject 父链与视觉树分叉，
// QObject::findChild 够不到委托；统一走 childItems 视觉树。
inline QQuickItem* visualFind(QQuickItem* root, const QByteArray& name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    const auto kids = root->childItems();
    for (QQuickItem* c : kids)
        if (QQuickItem* r = visualFind(c, name)) return r;
    return nullptr;
}

inline QQuickItem* item(QQuickWindow* win, const char* name) {
    return visualFind(win->contentItem(), name);
}

// 全量收集匹配 objectName 的视觉树节点（ListView 委托 childItems 序≠
// model 序，需要按内容挑选点谁断言谁）
inline void visualFindAll(QQuickItem* root, const QByteArray& name,
                    QList<QQuickItem*>& out) {
    if (!root) return;
    if (root->objectName() == name) out.append(root);
    for (QQuickItem* c : root->childItems()) visualFindAll(c, name, out);
}

// Popup/Dialog（QQuickPopup 不是 QQuickItem）：QObject 树定位 + 属性读态
inline QObject* popupObj(QQuickWindow* win, const char* name) {
    return win->findChild<QObject*>(name);
}

inline bool popupVisible(QObject* popup) {
    return popup && popup->property("visible").toBool();
}

// 点击信号计数器（clicked 只能用字符串信号连——QQuickAbstractButton
// 头私有；Q_OBJECT 接收器 + 文件尾 moc include，AUTOMOC 处理本 TU）
class ClickCounter : public QObject {
    Q_OBJECT
public:
    int count = 0;
public slots:
    void onClicked() { ++count; }
};

inline QString statusText(QQuickWindow* win) {
    return win->property("statusText").toString();
}

// EntryResultsPane 属性读取（元素缺失时空 QVariant，断言侧 FAIL 不崩）
inline QVariant paneProp(QQuickWindow* win, const char* prop) {
    QQuickItem* p = item(win, "entryResultsPane");
    return p ? p->property(prop) : QVariant();
}

// 侧栏面板属性读取（同上）
inline QVariant lpProp(QQuickWindow* win, const char* prop) {
    QQuickItem* p = item(win, "leftPane");
    return p ? p->property(prop) : QVariant();
}

inline void openWord(QQuickWindow* win, const QString& word) {
    QVariant w(word), nav(true);
    QMetaObject::invokeMethod(win, "openWord", Q_ARG(QVariant, w),
                              Q_ARG(QVariant, nav));
    settle(win);
}


} // namespace ui_audit

#endif // UNIDICT_CLICK_AUDIT_HELPERS_H
