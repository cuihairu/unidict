#include "startup_launcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>

namespace {

// HKCU Run 键：登录后由 explorer 拉起该值列出的命令。写带引号的完整
// 路径（Program Files 场景路径含空格必须加引号）。
QString runKeyPath() {
    return QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

#ifndef Q_OS_WIN

// 测试缝（startup_launcher.h 头注）：审计/沙箱把自启落盘重定向到临时
// 目录，真机默认目录见 platformDir()。Windows 走注册表不吃此缝。
QString autostartDir() {
    const QString env = qEnvironmentVariable("UNIDICT_AUTOSTART_DIR");
    if (!env.isEmpty()) return env;
#ifdef Q_OS_DARWIN
    // launchd LaunchAgents：登录时逐 plist RunAtLoad 拉起
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
           + QStringLiteral("/Library/LaunchAgents");
#else
    // XDG autostart：桌面会话启动时扫描 config 下 autostart/ 的 .desktop
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
           + QStringLiteral("/autostart");
#endif
}

QString nativeFilePath() {
#ifdef Q_OS_DARWIN
    return autostartDir() + QStringLiteral("/com.unidict.autostart.plist");
#else
    return autostartDir() + QStringLiteral("/unidict.desktop");
#endif
}

#endif // !Q_OS_WIN

} // namespace

StartupLauncher::StartupLauncher(QObject* parent) : QObject(parent) {}

bool StartupLauncher::isPlatformSupported() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    return true;
#else
    // Android/iOS/wasm 等：无桌面会话语义，保持 stub（不假装）
    return false;
#endif
}

QString StartupLauncher::entryName() {
    const QString name = QCoreApplication::applicationName();
    return name.isEmpty() ? QStringLiteral("Unidict") : name;
}

QString StartupLauncher::entryFileName() {
#ifdef Q_OS_WIN
    return entryName();
#elif defined(Q_OS_DARWIN)
    return QStringLiteral("com.unidict.autostart.plist");
#else
    return QStringLiteral("unidict.desktop");
#endif
}

bool StartupLauncher::isEnabled() const {
    if (!isPlatformSupported()) {
        return false;
    }
    return readNativeEnabled();
}

bool StartupLauncher::setEnabled(bool enabled) {
    if (!isPlatformSupported()) {
        // stub 平台：关闭是成功 no-op（本就不会自启），开启不可实现
        return !enabled;
    }
    return writeNativeEnabled(enabled);
}

#ifdef Q_OS_WIN

bool StartupLauncher::readNativeEnabled() const {
    QSettings runKey(runKeyPath(), QSettings::NativeFormat);
    return runKey.contains(entryName());
}

bool StartupLauncher::writeNativeEnabled(bool enabled) {
    QSettings runKey(runKeyPath(), QSettings::NativeFormat);
    if (enabled) {
        runKey.setValue(entryName(),
                        QStringLiteral("\"%1\"").arg(QCoreApplication::applicationFilePath()));
    } else {
        runKey.remove(entryName());
    }
    runKey.sync();
    return runKey.status() == QSettings::NoError;
}

#else

bool StartupLauncher::readNativeEnabled() const {
    return QFileInfo::exists(nativeFilePath());
}

bool StartupLauncher::writeNativeEnabled(bool enabled) {
    const QString path = nativeFilePath();
    if (!enabled) {
        // 未启用过也视为成功（幂等关闭，与 Windows remove 语义对齐）
        return !QFileInfo::exists(path) || QFile::remove(path);
    }
    if (!QDir().mkpath(autostartDir())) return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
#ifdef Q_OS_DARWIN
    // ProgramArguments 是数组：路径含空格不需要引号
    const QString body = QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "    <key>Label</key><string>com.unidict.autostart</string>\n"
        "    <key>ProgramArguments</key>\n"
        "    <array>\n"
        "        <string>%1</string>\n"
        "    </array>\n"
        "    <key>RunAtLoad</key><true/>\n"
        "</dict>\n"
        "</plist>\n")
        .arg(QCoreApplication::applicationFilePath());
#else
    // Exec 带引号：路径含空格也合法（freedesktop Exec 引号规则）
    const QString body = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Exec=\"%1\"\n"
        "Name=%2\n"
        "Terminal=false\n"
        "X-GNOME-Autostart-enabled=true\n"
        "Comment=Unidict dictionary lookup on login\n")
        .arg(QCoreApplication::applicationFilePath(), entryName());
#endif
    file.write(body.toUtf8());
    file.close();
    return file.error() == QFileDevice::NoError;
}

#endif // Q_OS_WIN
