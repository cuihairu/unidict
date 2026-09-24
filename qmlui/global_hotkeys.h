// Global hotkey handler for desktop platforms (Qt-only).
// Registers system-wide hotkeys for quick dictionary lookup.
// Native layer: Windows implemented (RegisterHotKey + WM_HOTKEY); Linux/macOS
// remain stubs (X11 grab complexity / objc bridging, revisit on demand).
//
// 平台相关的一切（native filter、win32 结构）都收在 #ifdef Q_OS_WIN 内：
// 非 Windows 编译单元保持与纯 stub 等价，不引入任何 native 事件链依赖。

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

// NOTE: Native global hotkeys are currently not implemented.
// We keep this type as an opaque numeric handle for future support.
using HotkeyHandle = quint64;

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

    QMap<QString, HotkeyInfo> m_hotkeys;
    HotkeyHandle m_nextHandle = 1;
    bool m_enabled = true;
};

#endif // GLOBAL_HOTKEYS_H
