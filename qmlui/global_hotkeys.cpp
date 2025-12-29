#include "global_hotkeys.h"

#include <QDebug>

GlobalHotkeys::GlobalHotkeys(QObject* parent)
    : QObject(parent) {
}

GlobalHotkeys::~GlobalHotkeys() {
    unregisterAllHotkeys();
}

bool GlobalHotkeys::registerHotkey(const QString& action, const QString& keySequence) {
    if (action.isEmpty()) return false;
    if (!m_enabled) return false;
    if (!isPlatformSupported()) {
        emit hotkeyRegistered(action, false);
        return false;
    }

    // Unregister existing hotkey for this action
    if (m_hotkeys.contains(action)) {
        unregisterHotkey(action);
    }

    QKeySequence seq(keySequence);
    if (seq.isEmpty()) {
        emit hotkeyRegistered(action, false);
        return false;
    }

    QString errorMsg;
    HotkeyHandle nativeHandle = 0;
    if (!registerNativeHotkey(seq, nativeHandle, errorMsg)) {
        qWarning() << "Failed to register hotkey" << keySequence << "for action" << action
                   << ":" << errorMsg;
        emit hotkeyRegistered(action, false);
        return false;
    }

    // Store hotkey info
    HotkeyInfo info;
    info.action = action;
    info.keySequence = seq;
    info.handle = nativeHandle;
    info.registered = true;

    m_hotkeys[action] = info;
    emit hotkeyRegistered(action, true);
    return true;
}

void GlobalHotkeys::unregisterHotkey(const QString& action) {
    auto it = m_hotkeys.find(action);
    if (it == m_hotkeys.end()) return;

    if (it->registered) {
        unregisterNativeHotkey(it->handle);
    }

    m_hotkeys.erase(it);
    emit hotkeyUnregistered(action);
}

void GlobalHotkeys::unregisterAllHotkeys() {
    for (auto& info : m_hotkeys) {
        if (info.registered) {
            unregisterNativeHotkey(info.handle);
        }
    }
    m_hotkeys.clear();
}

bool GlobalHotkeys::isPlatformSupported() {
    // TODO: Implement native global hotkeys per platform (Win/macOS/X11).
    return false;
}

void GlobalHotkeys::setEnabled(bool enabled) {
    m_enabled = enabled;
}

QStringList GlobalHotkeys::registeredActions() const {
    return m_hotkeys.keys();
}

QString GlobalHotkeys::getHotkeyForAction(const QString& action) const {
    auto it = m_hotkeys.find(action);
    if (it != m_hotkeys.end() && it->registered) {
        return it->keySequence.toString();
    }
    return QString();
}

bool GlobalHotkeys::registerNativeHotkey(const QKeySequence& keySequence, HotkeyHandle& handle, QString& errorMsg) {
    Q_UNUSED(keySequence);
    Q_UNUSED(handle);
    errorMsg = "Native global hotkeys not implemented";
    return false;
}

void GlobalHotkeys::unregisterNativeHotkey(HotkeyHandle handle) {
    Q_UNUSED(handle);
}
