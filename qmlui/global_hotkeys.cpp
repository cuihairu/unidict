#include "global_hotkeys.h"

#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDebug>

// Platform-specific includes
#ifdef Q_OS_WIN
#include <windows.h>
QMap<HotkeyHandle, QString> GlobalHotkeys::s_handleToAction;
GlobalHotkeys* GlobalHotkeys::s_instance = nullptr;
#elif defined(Q_OS_MACOS)
#include <Carbon/Carbon.h>
#endif

GlobalHotkeys::GlobalHotkeys(QObject* parent)
    : QObject(parent) {

#ifdef Q_OS_WIN
    s_instance = this;
    qApp->installNativeEventFilter([](const QByteArray& eventType, void* message, long* result) {
        return nativeEventFilter(eventType, message, result);
    });
#endif
}

GlobalHotkeys::~GlobalHotkeys() {
    unregisterAllHotkeys();

#ifdef Q_OS_WIN
    s_instance = nullptr;
#endif
}

bool GlobalHotkeys::registerHotkey(const QString& action, const QString& keySequence) {
    if (action.isEmpty()) return false;

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
    if (!registerNativeHotkey(seq, errorMsg)) {
        qWarning() << "Failed to register hotkey" << keySequence << "for action" << action
                   << ":" << errorMsg;
        emit hotkeyRegistered(action, false);
        return false;
    }

    // Store hotkey info
    HotkeyInfo info;
    info.action = action;
    info.keySequence = seq;
    info.handle = m_nextHandle++;
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
#ifdef Q_OS_WIN
    return true;
#elif defined(Q_OS_MACOS)
    return true;
#elif defined(Q_OS_LINUX)
    // Linux support varies by display server
    // Basic X11 support could be added
    return false;
#else
    return false;
#endif
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

bool GlobalHotkeys::registerNativeHotkey(const QKeySequence& keySequence, QString& errorMsg) {
#ifdef Q_OS_WIN
    int modifiers = 0;
    int vk = 0;

    for (int i = 0; i < keySequence.count(); ++i) {
        int key = keySequence[i];

        if (key & Qt::KeyboardModifierMask) {
            // Modifiers
            if (key & Qt::ControlModifier) modifiers |= MOD_CONTROL;
            if (key & Qt::AltModifier)     modifiers |= MOD_ALT;
            if (key & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
            if (key & Qt::MetaModifier)    modifiers |= MOD_WIN;
        } else {
            // Key code
            vk = key & ~Qt::KeyboardModifierMask;

            // Map Qt key to Windows virtual key
            if (vk >= Qt::Key_A && vk <= Qt::Key_Z) {
                vk = 'A' + (vk - Qt::Key_A);
            } else if (vk >= Qt::Key_0 && vk <= Qt::Key_9) {
                vk = '0' + (vk - Qt::Key_0);
            } else if (vk == Qt::Key_F1) vk = VK_F1;
            else if (vk == Qt::Key_F2) vk = VK_F2;
            else if (vk == Qt::Key_F3) vk = VK_F3;
            else if (vk == Qt::Key_F4) vk = VK_F4;
            else if (vk == Qt::Key_F5) vk = VK_F5;
            else if (vk == Qt::Key_F6) vk = VK_F6;
            else if (vk == Qt::Key_F7) vk = VK_F7;
            else if (vk == Qt::Key_F8) vk = VK_F8;
            else if (vk == Qt::Key_F9) vk = VK_F9;
            else if (vk == Qt::Key_F10) vk = VK_F10;
            else if (vk == Qt::Key_F11) vk = VK_F11;
            else if (vk == Qt::Key_F12) vk = VK_F12;
            else if (vk == Qt::Key_Space) vk = VK_SPACE;
            else if (vk == Qt::Key_Escape) vk = VK_ESCAPE;
            else if (vk == Qt::Key_Tab) vk = VK_TAB;
            else if (vk == Qt::Key_Enter) vk = VK_RETURN;
            else if (vk == Qt::Key_Return) vk = VK_RETURN;
            else if (vk == Qt::Key_Backspace) vk = VK_BACK;
            else if (vk == Qt::Key_Delete) vk = VK_DELETE;
            else if (vk == Qt::Key_Insert) vk = VK_INSERT;
            else if (vk == Qt::Key_Home) vk = VK_HOME;
            else if (vk == Qt::Key_End) vk = VK_END;
            else if (vk == Qt::Key_PageUp) vk = VK_PRIOR;
            else if (vk == Qt::Key_PageDown) vk = VK_NEXT;
            else if (vk == Qt::Key_Left) vk = VK_LEFT;
            else if (vk == Qt::Key_Up) vk = VK_UP;
            else if (vk == Qt::Key_Right) vk = VK_RIGHT;
            else if (vk == Qt::Key_Down) vk = VK_DOWN;
        }
    }

    if (vk == 0) {
        errorMsg = "Invalid key";
        return false;
    }

    HotkeyHandle handle = m_nextHandle;
    if (!RegisterHotKey(NULL, handle, modifiers, vk)) {
        errorMsg = "RegisterHotKey failed";
        return false;
    }

    s_handleToAction[handle] = QString();  // Will be set after return
    return true;

#elif defined(Q_OS_MACOS)
    // macOS implementation using Carbon Event Manager
    OSStatus err;
    EventHotKeyID hotKeyID;
    hotKeyID.signature = 'Unid';
    hotKeyID.id = m_nextHandle;

    uint32_t modifiers = 0;
    uint32_t key = 0;

    for (int i = 0; i < keySequence.count(); ++i) {
        int k = keySequence[i];

        if (k & Qt::ControlModifier) modifiers |= controlKey;
        if (k & Qt::AltModifier)     modifiers |= optionKey;
        if (k & Qt::ShiftModifier)   modifiers |= shiftKey;
        if (k & Qt::MetaModifier)    modifiers |= cmdKey;

        if (!(k & Qt::KeyboardModifierMask)) {
            key = k & ~Qt::KeyboardModifierMask;
        }
    }

    err = RegisterEventHotKey(key, modifiers, hotKeyID,
                              GetApplicationEventTarget(), 0,
                              &m_hotkeys[action].handle);

    if (err != noErr) {
        errorMsg = "RegisterEventHotKey failed";
        return false;
    }

    return true;

#else
    // Linux/not implemented
    errorMsg = "Platform not supported";
    return false;
#endif
}

void GlobalHotkeys::unregisterNativeHotkey(HotkeyHandle handle) {
#ifdef Q_OS_WIN
    UnregisterHotKey(NULL, handle);
    s_handleToAction.remove(handle);
#elif defined(Q_OS_MACOS)
    UnregisterEventHotKey(handle);
#endif
}

#ifdef Q_OS_WIN
bool GlobalHotkeys::nativeEventFilter(const QByteArray& eventType, void* message, long* result) {
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            HotkeyHandle handle = msg->wParam;
            auto it = s_handleToAction.find(handle);
            if (it != s_handleToAction.end() && s_instance) {
                emit s_instance->hotkeyPressed(it.value());
            }
            *result = 0;
            return true;
        }
    }
    return false;
}
#endif
