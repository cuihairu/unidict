#include "startup_launcher.h"

#include <QCoreApplication>
#include <QSettings>

namespace {

// HKCU Run 键：登录后由 explorer 拉起该值列出的命令。写带引号的完整
// 路径（Program Files 场景路径含空格必须加引号）。
QString runKeyPath() {
    return QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
}

} // namespace

StartupLauncher::StartupLauncher(QObject* parent) : QObject(parent) {}

bool StartupLauncher::isPlatformSupported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString StartupLauncher::entryName() {
    const QString name = QCoreApplication::applicationName();
    return name.isEmpty() ? QStringLiteral("Unidict") : name;
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

bool StartupLauncher::readNativeEnabled() const { return false; }
bool StartupLauncher::writeNativeEnabled(bool) { return false; }

#endif // Q_OS_WIN
