#include "tray_controller.h"

#include "startup_launcher.h"
#include "../adapters/qt/settings_qt.h"

#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQuickWindow>
#include <QWindow>

namespace {

// 自绘托盘图标（无资源文件依赖）：圆角底 + "U"——gui/main.cpp
// trayIconPixmap 同款，两壳托盘观感一致
QPixmap trayIconPixmap() {
    QPixmap pm(32, 32);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x3a, 0x6e, 0xa5));
    p.drawRoundedRect(2, 2, 28, 28, 6, 6);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("U"));
    return pm;
}

QString closeToTrayKey() { return QStringLiteral("ui/close_to_tray"); }

} // namespace

// 主窗：首个可见顶层 QQuickWindow（GUI 常驻单主窗场景成立；全隐藏时
// 退而取第一个——与 global_hotkeys 挂窗同口径）
QQuickWindow* TrayController::mainWindow() {
    const QList<QWindow*> windows = QGuiApplication::topLevelWindows();
    for (QWindow* w : windows) {
        if (w->isVisible()) {
            if (auto* qw = qobject_cast<QQuickWindow*>(w)) return qw;
        }
    }
    for (QWindow* w : windows) {
        if (auto* qw = qobject_cast<QQuickWindow*>(w)) return qw;
    }
    return nullptr;
}

TrayController::TrayController(UnidictAdaptersQt::SettingsQt* settings,
                               QObject* parent)
    : QObject(parent), m_settings(settings) {
    m_available = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_available) {
        return; // 关窗即退出（与托盘出现前的行为一致）
    }
    m_icon = new QSystemTrayIcon(trayIconPixmap(), this);
    buildMenu();
    m_icon->setToolTip(QStringLiteral("Unidict"));
    connect(m_icon, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::Trigger ||
                    reason == QSystemTrayIcon::DoubleClick) {
                    showMainWindow();
                }
            });
    m_icon->show();
}

bool TrayController::iconVisible() const {
    return m_icon && m_icon->isVisible();
}

bool TrayController::closeToTrayEnabled() const {
    return m_settings ? m_settings->getBool(closeToTrayKey(), true) : true;
}

void TrayController::setCloseToTrayEnabled(bool enabled) {
    if (closeToTrayEnabled() == enabled) return;
    if (m_settings) m_settings->setBool(closeToTrayKey(), enabled);
    emit closeToTrayEnabledChanged();
}

bool TrayController::shouldHideToTray() const {
    return iconVisible() && closeToTrayEnabled();
}

void TrayController::notifyHiddenToTray() {
    if (!m_icon || m_trayHintShown) return;
    m_trayHintShown = true;
    m_icon->showMessage(QStringLiteral("Unidict"),
                        QStringLiteral("已隐藏到托盘，双击托盘图标恢复；退出请用托盘菜单。"),
                        QSystemTrayIcon::Information, 3000);
}

void TrayController::showMainWindow() {
    if (QQuickWindow* w = mainWindow()) {
        w->show();
        w->raise();
        w->requestActivate();
    }
}

bool TrayController::startupSupported() const {
    return StartupLauncher::isPlatformSupported();
}

bool TrayController::startupEnabled() const {
    return StartupLauncher().isEnabled();
}

bool TrayController::setStartupEnabled(bool enabled) {
    return StartupLauncher().setEnabled(enabled);
}

void TrayController::buildMenu() {
    m_menu = new QMenu();

    QAction* showAction = m_menu->addAction(QStringLiteral("显示主窗"));
    connect(showAction, &QAction::triggered, this,
            [this] { showMainWindow(); });

    QAction* trayToggle =
        m_menu->addAction(QStringLiteral("关闭时隐藏到托盘"));
    trayToggle->setCheckable(true);
    trayToggle->setChecked(closeToTrayEnabled());
    connect(trayToggle, &QAction::toggled, this,
            [this](bool checked) { setCloseToTrayEnabled(checked); });
    // 菜单勾选态跟随设置（审计/未来 UI 改键后同步）
    connect(this, &TrayController::closeToTrayEnabledChanged, trayToggle,
            [trayToggle, this] { trayToggle->setChecked(closeToTrayEnabled()); });

    QAction* startupToggle = m_menu->addAction(QStringLiteral("开机自启"));
    startupToggle->setCheckable(true);
    startupToggle->setChecked(startupEnabled());
    if (!StartupLauncher::isPlatformSupported()) {
        startupToggle->setEnabled(false);
        startupToggle->setToolTip(QStringLiteral("当前平台暂不支持开机自启"));
    }
    connect(startupToggle, &QAction::toggled, this,
            [this](bool checked) { setStartupEnabled(checked); });

    m_menu->addSeparator();
    QAction* quitAction = m_menu->addAction(QStringLiteral("退出"));
    connect(quitAction, &QAction::triggered, this,
            [] { QCoreApplication::quit(); });

    m_icon->setContextMenu(m_menu);
}
