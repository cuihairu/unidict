// Login startup toggle for desktop platforms (Qt-only).
// Native layers all implemented: Windows HKCU Run registry key; Linux XDG
// autostart (.desktop in $XDG_CONFIG_HOME/autostart, default ~/.config);
// macOS launchd LaunchAgent (plist in ~/Library/LaunchAgents). Mobile/web
// stay unsupported stubs — 托盘/gui 三 target 与 qmlui 共用本实现。
//
// 测试缝：UNIDICT_AUTOSTART_DIR 重定向 Linux/macOS 落盘目录（审计/沙箱
// 密封用；Windows 走注册表不受它影响）。平台差异全收在 cpp 实现段，
// 本头文件无需平台类型。

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
    // not support it (or the file/registry write failed). Disabling on a
    // platform that never enabled it is a successful no-op.
    Q_INVOKABLE bool setEnabled(bool enabled);

    // Registry entry name (fixed); shown for diagnostics
    Q_INVOKABLE static QString entryName();

    // 注册物文件名（Linux .desktop / macOS .plist；Windows=注册表值名），
    // 审计侧按 UNIDICT_AUTOSTART_DIR + 此名做落盘断言，避免名字两处漂移
    Q_INVOKABLE static QString entryFileName();

private:
    bool readNativeEnabled() const;
    bool writeNativeEnabled(bool enabled);
};

#endif // STARTUP_LAUNCHER_H
