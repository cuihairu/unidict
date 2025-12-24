// Global hotkey handler for desktop platforms (Qt-only).
// Registers system-wide hotkeys for quick dictionary lookup.

#ifndef GLOBAL_HOTKEYS_H
#define GLOBAL_HOTKEYS_H

#include <QObject>
#include <QString>
#include <QKeySequence>
#include <QMap>

// Platform-specific hotkey handle
#ifdef Q_OS_WIN
#include <windows.h>
using HotkeyHandle = int;
#elif defined(Q_OS_MACOS)
#include <Carbon/Carbon.h>
using HotkeyHandle = EventHotKeyRef;
#elif defined(Q_OS_LINUX)
// Linux uses platform-specific APIs (X11, Wayland)
// This is a placeholder implementation
using HotkeyHandle = unsigned long;
#else
using HotkeyHandle = int;
#endif

class GlobalHotkeys : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkeys(QObject* parent = nullptr);
    ~GlobalHotkeys();

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
    bool registerNativeHotkey(const QKeySequence& keySequence, QString& errorMsg);
    void unregisterNativeHotkey(HotkeyHandle handle);

    // Platform-specific event processing
#ifdef Q_OS_WIN
    static bool nativeEventFilter(const QByteArray& eventType, void* message, long* result);
#endif

    struct HotkeyInfo {
        QString action;
        QKeySequence keySequence;
        HotkeyHandle handle;
        bool registered;
    };

    QMap<QString, HotkeyInfo> m_hotkeys;
    HotkeyHandle m_nextHandle = 1;
    bool m_enabled = true;

    // Platform-specific implementation details
#ifdef Q_OS_WIN
    static QMap<HotkeyHandle, QString> s_handleToAction;
    static GlobalHotkeys* s_instance;
#endif
};

#endif // GLOBAL_HOTKEYS_H
