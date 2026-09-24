// Login startup toggle for desktop platforms (Qt-only).
// Native layer: Windows implemented (HKCU Run registry key); Linux/macOS
// remain stubs (autostart .desktop / launch agent, revisit on demand) —
// 同 global_hotkeys 的平台策略：Windows 先行，其余平台语义保持一致。
//
// 平台相关的一切都收在 #ifdef Q_OS_WIN 内（本头文件无需 win32 类型，
// 差异只在 cpp 的实现段），非 Windows 编译单元与纯 stub 等价。

#ifndef STARTUP_LAUNCHER_H
#define STARTUP_LAUNCHER_H

#include <QObject>
#include <QString>
#include <QtGlobal>

class StartupLauncher : public QObject {
    Q_OBJECT

public:
    explicit StartupLauncher(QObject* parent = nullptr);

    // Check if platform supports login startup
    Q_INVOKABLE static bool isPlatformSupported();

    // Whether the app is currently registered to start on login
    Q_INVOKABLE bool isEnabled() const;

    // Enable/disable login startup. Returns false when the platform does
    // not support it (or the registry write failed). Disabling on a
    // platform that never enabled it is a successful no-op.
    Q_INVOKABLE bool setEnabled(bool enabled);

    // Registry entry name (fixed); shown for diagnostics
    Q_INVOKABLE static QString entryName();

private:
    bool readNativeEnabled() const;
    bool writeNativeEnabled(bool enabled);
};

#endif // STARTUP_LAUNCHER_H
