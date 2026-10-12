#include "global_hotkeys.h"

#include <QDebug>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#endif

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
// X11 抓键实现：xcb 会话下 XGrabKey 根窗抓键，QSocketNotifier 泵连接 fd。
// Wayland 会话无此语义（合成器独占输入），注册时明示失败（诚实门控）。
// Xlib.h 必须排在所有 Qt 头之后——它 #define None/Bool/Status 等宏，
// 后续再包含 Qt 头会被打碎（qcoreevent.h 的 enum Type 首当其冲）
#include <QGuiApplication>
#include <QSocketNotifier>
#include <X11/Xlib.h>
#endif

#ifdef Q_OS_MACOS
#include <QGuiApplication>
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
#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    delete m_xNotifier;
    m_xNotifier = nullptr;
    if (m_xDisplay) {
        XCloseDisplay(static_cast<Display*>(m_xDisplay));
        m_xDisplay = nullptr;
    }
#endif
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
#elif defined(Q_OS_MACOS)
    return true; // Carbon RegisterEventHotKey（global_hotkeys_mac.mm 桥）
#elif defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    // 编译期找到 X11 头/库才暴露支持面；Wayland 会话在注册时明示失败
    return true;
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

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
void GlobalHotkeys::pumpXEvents() {
    Display* d = static_cast<Display*>(m_xDisplay);
    if (!d) return;
    while (XPending(d) > 0) {
        XEvent ev;
        XNextEvent(d, &ev);
        if (ev.type != KeyPress) continue;
        // NumLock(Mod2)/CapsLock(Lock)/其他粘滞修饰不参与匹配
        const unsigned state = ev.xkey.state
                               & ~(Mod2Mask | LockMask | Mod5Mask | Mod3Mask);
        for (auto it = m_nativeX.constBegin(); it != m_nativeX.constEnd();
             ++it) {
            if (it->keycode != ev.xkey.keycode || it->mods != state) continue;
            if (m_enabled) dispatchByHandle(it.key());
        }
    }
}
#endif

void GlobalHotkeys::dispatchByHandle(HotkeyHandle handle) {
    for (auto it = m_hotkeys.constBegin(); it != m_hotkeys.constEnd(); ++it) {
        if (it->handle == handle) {
            emit hotkeyPressed(it->action);
            return;
        }
    }
}

#ifdef Q_OS_MACOS
void macHotkeyTrampoline(int hotkeyId, void* ctx) {
    if (GlobalHotkeys* owner = static_cast<GlobalHotkeys*>(ctx))
        owner->dispatchByHandle(HotkeyHandle(hotkeyId));
}
#endif

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
namespace {

bool g_xGrabTaken = false;  // XGrabKey 命中 BadAccess（键被占）经此回传

// Xlib 异步错误默认直接杀进程：抓键前后临时换 handler + XSync 同步化
int xGrabErrorHandler(Display*, XErrorEvent* e) {
    if (e->error_code == BadAccess) g_xGrabTaken = true;
    return 0;
}

// Qt 主键 → X keysym 名（与 Windows 同一安全集：字母/数字/F1-24/常用键）
bool qtKeyToXKeysym(int qtKey, const char** sym) {
    static const char* const fKeys[24] = {
        "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10",
        "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19",
        "F20", "F21", "F22", "F23", "F24"};
    switch (qtKey) {
    case Qt::Key_A: *sym = "a"; return true;
    case Qt::Key_B: *sym = "b"; return true;
    case Qt::Key_C: *sym = "c"; return true;
    case Qt::Key_D: *sym = "d"; return true;
    case Qt::Key_E: *sym = "e"; return true;
    case Qt::Key_F: *sym = "f"; return true;
    case Qt::Key_G: *sym = "g"; return true;
    case Qt::Key_H: *sym = "h"; return true;
    case Qt::Key_I: *sym = "i"; return true;
    case Qt::Key_J: *sym = "j"; return true;
    case Qt::Key_K: *sym = "k"; return true;
    case Qt::Key_L: *sym = "l"; return true;
    case Qt::Key_M: *sym = "m"; return true;
    case Qt::Key_N: *sym = "n"; return true;
    case Qt::Key_O: *sym = "o"; return true;
    case Qt::Key_P: *sym = "p"; return true;
    case Qt::Key_Q: *sym = "q"; return true;
    case Qt::Key_R: *sym = "r"; return true;
    case Qt::Key_S: *sym = "s"; return true;
    case Qt::Key_T: *sym = "t"; return true;
    case Qt::Key_U: *sym = "u"; return true;
    case Qt::Key_V: *sym = "v"; return true;
    case Qt::Key_W: *sym = "w"; return true;
    case Qt::Key_X: *sym = "x"; return true;
    case Qt::Key_Y: *sym = "y"; return true;
    case Qt::Key_Z: *sym = "z"; return true;
    case Qt::Key_0: *sym = "0"; return true;
    case Qt::Key_1: *sym = "1"; return true;
    case Qt::Key_2: *sym = "2"; return true;
    case Qt::Key_3: *sym = "3"; return true;
    case Qt::Key_4: *sym = "4"; return true;
    case Qt::Key_5: *sym = "5"; return true;
    case Qt::Key_6: *sym = "6"; return true;
    case Qt::Key_7: *sym = "7"; return true;
    case Qt::Key_8: *sym = "8"; return true;
    case Qt::Key_9: *sym = "9"; return true;
    case Qt::Key_Space: *sym = "space"; return true;
    case Qt::Key_Backspace: *sym = "BackSpace"; return true;
    case Qt::Key_Tab: *sym = "Tab"; return true;
    case Qt::Key_Return: *sym = "Return"; return true;
    case Qt::Key_Escape: *sym = "Escape"; return true;
    case Qt::Key_Insert: *sym = "Insert"; return true;
    case Qt::Key_Delete: *sym = "Delete"; return true;
    case Qt::Key_Home: *sym = "Home"; return true;
    case Qt::Key_End: *sym = "End"; return true;
    case Qt::Key_PageUp: *sym = "Prior"; return true;
    case Qt::Key_PageDown: *sym = "Next"; return true;
    case Qt::Key_Left: *sym = "Left"; return true;
    case Qt::Key_Right: *sym = "Right"; return true;
    case Qt::Key_Up: *sym = "Up"; return true;
    case Qt::Key_Down: *sym = "Down"; return true;
    default:
        if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
            *sym = fKeys[qtKey - Qt::Key_F1];
            return true;
        }
        return false;
    }
}

} // namespace
#endif // Q_OS_LINUX && UNIDICT_HAS_X11

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
#elif defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    // 诚实门控：Wayland 合成器独占输入，根窗抓键拿不到全局按键
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
        errorMsg = QStringLiteral("global hotkey needs an X11 (xcb) session; "
                                  "current platform: %1")
                       .arg(QGuiApplication::platformName());
        return false;
    }
    if (!m_xDisplay) {
        m_xDisplay = XOpenDisplay(nullptr);
        if (!m_xDisplay) {
            errorMsg = QStringLiteral("no X display available");
            return false;
        }
    }
    Display* d = static_cast<Display*>(m_xDisplay);
    const int binding = keySequence[0].toCombined();
    const int mods = binding & Qt::KeyboardModifierMask;
    unsigned xMods = 0;
    if (mods & Qt::ControlModifier) xMods |= ControlMask;
    if (mods & Qt::ShiftModifier) xMods |= ShiftMask;
    if (mods & Qt::AltModifier) xMods |= Mod1Mask;
    if (mods & Qt::MetaModifier) xMods |= Mod4Mask;
    if (xMods == 0) {
        errorMsg = QStringLiteral("global hotkey needs Ctrl/Alt/Shift/Super modifier");
        return false;
    }
    const char* sym = nullptr;
    if (!qtKeyToXKeysym(binding & ~Qt::KeyboardModifierMask, &sym)) {
        errorMsg = QStringLiteral("unsupported main key");
        return false;
    }
    const KeyCode code = XKeysymToKeycode(d, XStringToKeysym(sym));
    if (code == 0) {
        errorMsg = QStringLiteral("no keycode for keysym %1").arg(sym);
        return false;
    }
    // 根窗抓键只对精确修饰态生效：补 NumLock(Mod2)/CapsLock(Lock) 变体
    // 各抓一遍（Xorg 默认修饰位；四态全抓、注销对称全解）
    const unsigned variants[4] = {
        xMods, xMods | Mod2Mask, xMods | LockMask,
        xMods | Mod2Mask | LockMask};
    XErrorHandler old = XSetErrorHandler(xGrabErrorHandler);
    g_xGrabTaken = false;
    for (unsigned v : variants)
        XGrabKey(d, code, v, DefaultRootWindow(d), True,
                 GrabModeAsync, GrabModeAsync);
    XSync(d, False);
    XSetErrorHandler(old);
    if (g_xGrabTaken) {
        for (unsigned v : variants)  // 清掉可能已成功的部分抓取
            XUngrabKey(d, code, v, DefaultRootWindow(d));
        XSync(d, False);
        errorMsg = QStringLiteral("XGrabKey failed (key likely taken)");
        return false;
    }
    handle = m_nextHandle++;
    m_nativeX[handle] = { quint32(code), quint32(xMods) };
    // 首次注册成功才泵 X 连接 fd（无热键进程不挂 notifier）
    if (!m_xNotifier) {
        m_xNotifier = new QSocketNotifier(
            ConnectionNumber(d), QSocketNotifier::Read, this);
        connect(m_xNotifier, &QSocketNotifier::activated, this,
                [this] { pumpXEvents(); });
    }
    return true;
#elif defined(Q_OS_MACOS)
    const int binding = keySequence[0].toCombined();
    const int mods = binding & Qt::KeyboardModifierMask;
    // 单 Shift 热键会吞掉正常打字，与 Windows 同口径要求主修饰
    //（Carbon 位映射收在桥内，cpp 不拉平台头）
    if ((mods & (Qt::ControlModifier | Qt::AltModifier
                 | Qt::MetaModifier)) == 0) {
        errorMsg = QStringLiteral("global hotkey needs Ctrl/Option/Cmd modifier");
        return false;
    }
    unidict_mac_install_callback(&macHotkeyTrampoline, this);
    quintptr ref = 0;
    if (!unidict_mac_register_hotkey(binding & ~Qt::KeyboardModifierMask,
                                     mods, int(m_nextHandle), &ref)) {
        errorMsg = QStringLiteral("RegisterEventHotKey failed (key likely taken)");
        return false;
    }
    handle = m_nextHandle++;
    m_nativeMac[handle] = ref;
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
#elif defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    auto it = m_nativeX.find(handle);
    if (it == m_nativeX.end()) return;
    if (m_xDisplay) {
        Display* d = static_cast<Display*>(m_xDisplay);
        const unsigned base = it->mods;
        const unsigned variants[4] = {
            base, base | Mod2Mask, base | LockMask,
            base | Mod2Mask | LockMask};
        for (unsigned v : variants)
            XUngrabKey(d, it->keycode, v, DefaultRootWindow(d));
        XSync(d, False);
    }
    m_nativeX.erase(it);
#elif defined(Q_OS_MACOS)
    auto it = m_nativeMac.find(handle);
    if (it == m_nativeMac.end()) return;
    unidict_mac_unregister_hotkey(it.value());
    m_nativeMac.erase(it);
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
