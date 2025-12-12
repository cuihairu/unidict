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
    if (auto cb = QGuiApplication::clipboard()) {
        return cb->text();
    }
    return {};
}

} // namespace UnidictAdaptersQt

