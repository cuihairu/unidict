// Global hotkey handler for desktop platforms (Qt-only).
// Registers system-wide hotkeys for quick dictionary lookup.
// Native layers: Windows implemented (RegisterHotKey + WM_HOTKEY);
// Linux implemented (xcb 会话下 XGrabKey 根窗抓键 + QSocketNotifier 泵
// X 连接 fd——需编译期找到 X11 头/库（UNIDICT_HAS_X11），Wayland 会话
// 注册时明示失败，不假装)；macOS implemented (Carbon RegisterEventHotKey，
// global_hotkeys_mac.mm 桥，构建需 OBJCXX)。
//
// 平台相关的一切（native filter、win32/Xlib/Carbon 结构）都收在 #ifdef 内：
// 头文件不拉任何平台系统头（Xlib 用 void* 遮蔽、win32 用 void* 遮蔽）。

#ifndef GLOBAL_HOTKEYS_H
#define GLOBAL_HOTKEYS_H

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QMap>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <QAbstractNativeEventFilter>
#endif

class QSocketNotifier;

// 句柄只是进程内递增的簿记键（各平台 native 表的查找键），不跨进程
using HotkeyHandle = quint64;

#ifdef Q_OS_MACOS
// Carbon 桥（global_hotkeys_mac.mm）：Qt 键/修饰（Qt::Modifier 位）→
// 虚拟键码与 EventModifiers 并注册；回调经安装的一次性 trampoline
// 派发回 owner。Carbon 头只出现在 .mm，两侧经本声明相遇。
bool unidict_mac_register_hotkey(int qtKey, int qtMods, int hotkeyId,
                                 quintptr* refOut);
void unidict_mac_unregister_hotkey(quintptr ref);
void unidict_mac_install_callback(void (*cb)(int hotkeyId, void* ctx),
                                  void* ctx);
#endif

class GlobalHotkeys : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkeys(QObject* parent = nullptr);
    ~GlobalHotkeys() override;

    // Register a hotkey for a specific action
    Q_INVOKABLE bool registerHotkey(const QString& action, const QString& keySequence);
    Q_INVOKABLE void unregisterHotkey(const QString& action);
    Q_INVOKABLE void unregisterAllHotkeys();

    // Check if platform supports global hotkeys
    Q_INVOKABLE static bool isPlatformSupported();

    // Enable/disable all hotkeys
    Q_INVOKABLE void setEnabled(bool enabled);
    Q_INVOKABLE bool isEnabled() const { return m_enabled; }

    // Get registered hotkeys
    Q_INVOKABLE QStringList registeredActions() const;
    Q_INVOKABLE QString getHotkeyForAction(const QString& action) const;

signals:
    // Emitted when a registered hotkey is pressed
    void hotkeyPressed(const QString& action);

    // Hotkey registration status
    void hotkeyRegistered(const QString& action, bool success);
    void hotkeyUnregistered(const QString& action);

private:
    // handle → 动作派发（mac trampoline 从 Carbon 事件回调进来）
    void dispatchByHandle(HotkeyHandle handle);

    bool registerNativeHotkey(const QKeySequence& keySequence, HotkeyHandle& handle, QString& errorMsg);
    void unregisterNativeHotkey(HotkeyHandle handle);

    struct HotkeyInfo {
        QString action;
        QKeySequence keySequence;
        HotkeyHandle handle;
        bool registered;
    };

#ifdef Q_OS_WIN
    // WM_HOTKEY 拦截器：仅 Windows 存在；首次注册成功才挂到 app，
    // 无热键的进程不参与 native 事件链（挂早了在 Qt 6.10 offscreen
    // 下会踩 QGuiApplication 构造期 screenAdded 空指针的雷）
    class NativeFilter : public QAbstractNativeEventFilter {
    public:
        explicit NativeFilter(GlobalHotkeys* owner) : m_owner(owner) {}
        bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
    private:
        GlobalHotkeys* m_owner;
    };

    struct NativeWinHotkey {
        quint64 id;   // RegisterHotKey 的原子 id
        void* hwnd;   // HWND，void* 避免头文件拉 windows.h
    };
    QMap<HotkeyHandle, NativeWinHotkey> m_nativeWin;
    NativeFilter* m_filter = nullptr;
    friend class NativeFilter;
#endif

#if defined(Q_OS_LINUX) && defined(UNIDICT_HAS_X11)
    // X11 抓键面：Display* 遮蔽成 void* 避免头文件拉 Xlib；
    // notifier 只在首次抓键成功后建（无热键进程不泵 X fd）
    struct NativeXHotkey {
        quint32 keycode;  // XKeysymToKeycode 的物理键码
        quint32 mods;     // 精确修饰组合（不含 NumLock/CapsLock 变体）
    };
    QMap<HotkeyHandle, NativeXHotkey> m_nativeX;
    void* m_xDisplay = nullptr;
    QSocketNotifier* m_xNotifier = nullptr;
    void pumpXEvents();
#endif

#ifdef Q_OS_MACOS
    // Carbon 面：EventHotKeyRef 遮蔽成 quintptr；id 即 handle
    QMap<HotkeyHandle, quintptr> m_nativeMac;
    friend void macHotkeyTrampoline(int hotkeyId, void* ctx);
#endif

    QMap<QString, HotkeyInfo> m_hotkeys;
    HotkeyHandle m_nextHandle = 1;
    bool m_enabled = true;
};

#endif // GLOBAL_HOTKEYS_H
