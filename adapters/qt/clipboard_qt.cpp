#include "clipboard_qt.h"
#include <QGuiApplication>
#include <QClipboard>

namespace UnidictAdaptersQt {

ClipboardQt::ClipboardQt(QObject* parent) : QObject(parent) {}

void ClipboardQt::setText(const QString& text) const {
    if (auto cb = QGuiApplication::clipboard()) {
        cb->setText(text);
    }
}

QString ClipboardQt::text() const {
    // QGuiApplication::clipboard() 在运行中的应用内恒非空（offscreen 平台
    // 插件同样提供内存剪贴板实现），空指针兜底按构造不可达（Q-7）
    if (auto cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return {}; // GCOVR_EXCL_LINE
}

} // namespace UnidictAdaptersQt

