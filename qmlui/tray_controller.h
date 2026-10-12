// Desktop tray controller (Qt Widgets)：gui 托盘三件套（关闭到托盘 /
// 开机自启 / 显示主窗）parity 回 qmlui。托盘不可用（部分 Linux 会话 /
// offscreen）时保持原行为——关窗即退出，不创建托盘对象。
//
// 关窗决策面：QML onClosing 查 shouldHideToTray()（托盘在 + 开关开），
// 命中则 hide 常驻（热键/剪贴板取词继续可用）；开关记忆
// ui/close_to_tray 落盘，默认开（gui 同默认，键名按 qmlui 蛇形惯例）。
// 真系统托盘交互（图标点击 / 菜单弹出）离屏不可测——审计只驱动状态位
// 与 close→hide 还原链。
//
// 依赖注记：QMenu 是 QWidget 系，宿主必须用 QApplication（qmlui 三个
// main 已从 QGuiApplication 换 QApplication；纯 QGuiApplication 下构造
// 菜单直接 fatal）。

#ifndef TRAY_CONTROLLER_H
#define TRAY_CONTROLLER_H

#include <QObject>
#include <QSystemTrayIcon>

class QMenu;
class QQuickWindow;

namespace UnidictAdaptersQt {
class SettingsQt;
}

class TrayController : public QObject {
    Q_OBJECT
    // 托盘可用性（构造期探测一次；会话中途插拔不追踪——gui 同口径）
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool iconVisible READ iconVisible CONSTANT)
    Q_PROPERTY(bool closeToTrayEnabled READ closeToTrayEnabled
                   WRITE setCloseToTrayEnabled NOTIFY closeToTrayEnabledChanged)

public:
    explicit TrayController(UnidictAdaptersQt::SettingsQt* settings,
                            QObject* parent = nullptr);

    bool available() const { return m_available; }
    bool iconVisible() const;
    bool closeToTrayEnabled() const;
    void setCloseToTrayEnabled(bool enabled);

    // 关窗决策：托盘在且开关开才拦截 close（QML onClosing 消费）
    Q_INVOKABLE bool shouldHideToTray() const;

    // 首次隐藏给一次气泡提示（gui 同文案同一次性）
    Q_INVOKABLE void notifyHiddenToTray();

    // 主窗恢复（托盘双击 / 菜单「显示主窗」）：show + 前台激活
    Q_INVOKABLE void showMainWindow();

    // 开机自启菜单项（StartupLauncher 转发；不支持的平台返回 false）
    Q_INVOKABLE bool startupSupported() const;
    Q_INVOKABLE bool startupEnabled() const;
    Q_INVOKABLE bool setStartupEnabled(bool enabled);

signals:
    void closeToTrayEnabledChanged();

private:
    void buildMenu();
    static QQuickWindow* mainWindow();

    UnidictAdaptersQt::SettingsQt* m_settings = nullptr;
    bool m_available = false;
    bool m_trayHintShown = false;
    QSystemTrayIcon* m_icon = nullptr;
    QMenu* m_menu = nullptr;
};

#endif // TRAY_CONTROLLER_H
