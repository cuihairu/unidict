#ifndef PLATFORM_ADAPTER_H
#define PLATFORM_ADAPTER_H

#include <QMetaObject>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <memory>

class ClipboardMonitor;
class GlobalHotkeys;
class SelectionMonitor;

// 平台采集簇（剪贴板取词 / 划词取词 / 全局热键）：TD-121 拆桥第一刀，从
// LookupAdapter 上帝桥整体迁出（26 个 Q_INVOKABLE + 4 个信号）。三个采集器
// 由本对象持有并接线，词命中只发信号、处理在 QML 侧（取词窗/主窗分流）。
class PlatformAdapter : public QObject {
    Q_OBJECT
public:
    explicit PlatformAdapter(QObject* parent = nullptr);
    // 三个采集器只前向声明保持头文件精简；析构定义在 .cpp（那里已含真实
    // 类型），否则只持有不拆构的地方（dev 审计/沙箱夹具）会踩不完整类型
    ~PlatformAdapter() override;

    // P1 剪贴板监听功能
    Q_INVOKABLE void startClipboardMonitoring();
    Q_INVOKABLE void stopClipboardMonitoring();
    Q_INVOKABLE bool isClipboardMonitoring() const;
    Q_INVOKABLE void setClipboardPollInterval(int milliseconds);
    Q_INVOKABLE void setClipboardMinWordLength(int length);
    Q_INVOKABLE void setClipboardMaxWordLength(int length);
    Q_INVOKABLE void addClipboardExcludePattern(const QString& pattern);
    Q_INVOKABLE void clearClipboardExcludePatterns();

    // 剪贴板自动查词开关
    Q_INVOKABLE void setClipboardAutoLookupEnabled(bool enabled);
    Q_INVOKABLE bool isClipboardAutoLookupEnabled() const;

    // P1 划词取词（X11 PRIMARY selection；其余平台 isSupported=false，
    // start no-op——Windows/macOS 的 Selection 回落剪贴板会双触发）
    Q_INVOKABLE bool isSelectionMonitoringSupported() const;
    Q_INVOKABLE void startSelectionMonitoring();
    Q_INVOKABLE void stopSelectionMonitoring();
    Q_INVOKABLE bool isSelectionMonitoring() const;
    Q_INVOKABLE void setSelectionPollInterval(int milliseconds);
    Q_INVOKABLE void setSelectionMinLength(int length);
    Q_INVOKABLE void setSelectionMaxLength(int length);
    // P-5 取词窗定位：全局光标坐标（悬浮窗贴光标出现；不依赖平台专属
    // 挂钩，QCursor::pos() 跨平台可取）
    Q_INVOKABLE QPoint cursorScreenPos() const;

    // 词典元信息面板「打开目录」（UI 批二）：path 为文件时开其所在目录、
    // 为目录时开本体（系统文件管理器，QDesktopServices）。路径为空或
    // 不存在返回假。真实效果在外部进程，离屏审计只验接线不真点
    Q_INVOKABLE bool openContainingFolder(const QString& path) const;

    // P1 全局热键功能
    Q_INVOKABLE bool registerGlobalHotkey(const QString& action, const QString& keySequence);
    Q_INVOKABLE void unregisterGlobalHotkey(const QString& action);
    Q_INVOKABLE void unregisterAllGlobalHotkeys();
    Q_INVOKABLE QStringList registeredHotkeyActions() const;
    Q_INVOKABLE QString getHotkeyForAction(const QString& action) const;
    Q_INVOKABLE void setGlobalHotkeysEnabled(bool enabled);
    Q_INVOKABLE bool isGlobalHotkeysEnabled() const;
    Q_INVOKABLE static bool isGlobalHotkeysSupported();

signals:
    void clipboardWordDetected(const QString& word);
    // 划词取词命中（xcb 面随 selection 变化；QML 侧按取词窗开关分流）
    void selectionWordDetected(const QString& word);
    // P-5 悬浮取词的热键触发面：quick_lookup → QML 读剪贴板弹取词窗；
    // show_window → 主窗前置。系统级热键三平台实装（Windows RegisterHotKey /
    // macOS Carbon / Linux XGrabKey；Wayland 会话注册时明示失败）
    void quickLookupRequested();
    void showMainWindowRequested();

private:
    std::unique_ptr<ClipboardMonitor> m_clipboardMonitor;
    std::unique_ptr<SelectionMonitor> m_selectionMonitor;
    std::unique_ptr<GlobalHotkeys> m_globalHotkeys;
    bool m_clipboardAutoLookupEnabled = false;
    QMetaObject::Connection m_clipboardWordConnection;
    QMetaObject::Connection m_hotkeyPressedConnection;
};

#endif // PLATFORM_ADAPTER_H
