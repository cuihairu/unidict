#include "global_hotkeys.h"

#include <QDebug>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#endif

GlobalHotkeys::GlobalHotkeys(QObject* parent)
    : QObject(parent) {
}

GlobalHotkeys::~GlobalHotkeys() {
#ifdef Q_OS_WIN
    if (m_filter) {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            app->removeNativeEventFilter(m_filter);
        }
        delete m_filter;
        m_filter = nullptr;
    }
#endif
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
#ifdef Q_OS_WIN
    return true; // RegisterHotKey + WM_HOTKEY，Windows 已实现
#else
    // Linux：X11 grab 复杂且 Wayland 下不可行；macOS 需 objc 桥——按需再议
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

#ifdef Q_OS_WIN
namespace {

// QKeySequence 主键 → Win32 虚拟键码（只放行热键安全集）
bool qtKeyToWinVk(int qtKey, UINT* vk) {
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        *vk = UINT('A' + (qtKey - Qt::Key_A));
        return true;
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        *vk = UINT('0' + (qtKey - Qt::Key_0));
        return true;
    }
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
        *vk = UINT(VK_F1 + (qtKey - Qt::Key_F1));
        return true;
    }
    switch (qtKey) {
    case Qt::Key_Space: *vk = VK_SPACE; return true;
    case Qt::Key_Backspace: *vk = VK_BACK; return true;
    case Qt::Key_Tab: *vk = VK_TAB; return true;
    case Qt::Key_Return: *vk = VK_RETURN; return true;
    case Qt::Key_Escape: *vk = VK_ESCAPE; return true;
    case Qt::Key_Insert: *vk = VK_INSERT; return true;
    case Qt::Key_Delete: *vk = VK_DELETE; return true;
    case Qt::Key_Home: *vk = VK_HOME; return true;
    case Qt::Key_End: *vk = VK_END; return true;
    case Qt::Key_PageUp: *vk = VK_PRIOR; return true;
    case Qt::Key_PageDown: *vk = VK_NEXT; return true;
    case Qt::Key_Left: *vk = VK_LEFT; return true;
    case Qt::Key_Right: *vk = VK_RIGHT; return true;
    case Qt::Key_Up: *vk = VK_UP; return true;
    case Qt::Key_Down: *vk = VK_DOWN; return true;
    default: return false;
    }
}

} // namespace
#endif // Q_OS_WIN

bool GlobalHotkeys::registerNativeHotkey(const QKeySequence& keySequence, HotkeyHandle& handle, QString& errorMsg) {
#ifdef Q_OS_WIN
    if (keySequence.isEmpty()) {
        errorMsg = QStringLiteral("empty key sequence");
        return false;
    }
    const int binding = keySequence[0].toCombined();
    const int mods = binding & Qt::KeyboardModifierMask;
    UINT winMods = 0;
    if (mods & Qt::ControlModifier) winMods |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier) winMods |= MOD_SHIFT;
    if (mods & Qt::AltModifier) winMods |= MOD_ALT;
    if (mods & Qt::MetaModifier) winMods |= MOD_WIN;
    if (winMods == 0) {
        errorMsg = QStringLiteral("global hotkey needs Ctrl/Alt/Shift/Win modifier");
        return false;
    }
    UINT vk = 0;
    if (!qtKeyToWinVk(binding & ~Qt::KeyboardModifierMask, &vk)) {
        errorMsg = QStringLiteral("unsupported main key");
        return false;
    }
    // 热键要挂在一个真实窗口上：取首个顶层窗口（GUI 常驻主窗口场景成立）
    const QList<QWindow*> windows = QGuiApplication::topLevelWindows();
    QWindow* target = nullptr;
    for (QWindow* w : windows) {
        if (w->isVisible()) { target = w; break; }
    }
    if (!target && !windows.isEmpty()) {
        target = windows.constFirst(); // 尚未 show 时退而取第一个（winId() 会触发创建）
    }
    if (!target) {
        errorMsg = QStringLiteral("no top-level window to attach hotkey");
        return false;
    }
    HWND hwnd = reinterpret_cast<HWND>(target->winId());
    const UINT id = UINT(m_nextHandle); // id 与 handle 同源自增，进程内唯一
    if (!RegisterHotKey(hwnd, id, winMods, vk)) {
        errorMsg = QStringLiteral("RegisterHotKey failed (error %1), key likely taken").arg(quint64(GetLastError()));
        return false;
    }
    handle = m_nextHandle++;
    m_nativeWin[handle] = { quint64(id), reinterpret_cast<void*>(hwnd) };
    // 首次注册成功才挂 filter（无热键的进程不参与 native 事件链）
    if (!m_filter) {
        if (QCoreApplication* app = QCoreApplication::instance()) {
            m_filter = new NativeFilter(this);
            app->installNativeEventFilter(m_filter);
        }
    }
    return true;
#else
    Q_UNUSED(keySequence);
    Q_UNUSED(handle);
    errorMsg = QStringLiteral("Native global hotkeys not implemented on this platform");
    return false;
#endif
}

void GlobalHotkeys::unregisterNativeHotkey(HotkeyHandle handle) {
#ifdef Q_OS_WIN
    auto it = m_nativeWin.find(handle);
    if (it == m_nativeWin.end()) return;
    UnregisterHotKey(reinterpret_cast<HWND>(it->hwnd), UINT(it->id));
    m_nativeWin.erase(it);
#else
    Q_UNUSED(handle);
#endif
}

#ifdef Q_OS_WIN
bool GlobalHotkeys::NativeFilter::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(result);
    if (eventType == "windows_generic_MSG" && message) {
        const MSG* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            const UINT id = UINT(msg->wParam);
            for (auto it = m_owner->m_nativeWin.cbegin(); it != m_owner->m_nativeWin.cend(); ++it) {
                if (it->id != id) continue;
                if (m_owner->m_enabled) {
                    for (auto hit = m_owner->m_hotkeys.cbegin(); hit != m_owner->m_hotkeys.cend(); ++hit) {
                        if (hit->handle == it.key()) {
                            emit m_owner->hotkeyPressed(hit->action);
                            break;
                        }
                    }
                }
                break;
            }
        }
    }
    return false; // 不吞消息
}
#endif // Q_OS_WIN
