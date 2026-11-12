import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·取词页（LAY-001 自 MainDesktop.qml 拆出）
// 取词
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ColumnLayout {
    objectName: "settingslookupPage"


    property var win: null
    property var platform: null
    property var settings: null
    property var startup: null


    spacing: 10

    Switch {
        objectName: "clipboardSwitch"
        text: Theme.zh ? "剪贴板取词（自动查词）"
                       : "Clipboard lookup (auto-search)"
        checked: win.clipboardEnabled
        onToggled: {
            win.clipboardEnabled = checked
            platform.setClipboardAutoLookupEnabled(checked)
            if (checked) platform.startClipboardMonitoring()
            else platform.stopClipboardMonitoring()
        }
    }

    // P-5 取词窗形态：弹悬浮窗不打扰前台应用；关则回退
    // 主窗直接展示
    Switch {
        objectName: "quickLookupSwitch"
        text: Theme.zh ? "取词悬浮窗（弹小窗显示释义）"
                       : "Lookup popup (definitions in a small window)"
        checked: win.quickLookupEnabled
        onToggled: {
            win.quickLookupEnabled = checked
            settings.setBool("quicklookup/enabled", checked)
        }
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: Theme.zh
            ? "取词悬浮窗贴光标出现，失焦即收，不打断当前阅读；"
              + "关闭后剪贴板取词回到主窗直接展示。"
            : "The popup follows your cursor and closes on blur, "
              + "so reading is never interrupted. Turn it off to show "
              + "lookups in the main window instead."
        font.pixelSize: 12
        color: Theme.textTertiary
    }

    Label {
        text: Theme.zh ? ("轮询间隔: " + win.clipboardPollMs + " ms")
                       : ("Poll interval: " + win.clipboardPollMs + " ms")
        color: Theme.textSecondary
    }
    Slider {
        objectName: "pollSlider"
        from: 100
        to: 2000
        stepSize: 100
        value: win.clipboardPollMs
        onMoved: {
            win.clipboardPollMs = Math.round(value / 100) * 100
            platform.setClipboardPollInterval(win.clipboardPollMs)
        }
    }

    RowLayout {
        spacing: 10
        Label { text: Theme.zh ? "最短" : "Min"; color: Theme.textSecondary }
        SpinBox {
            objectName: "minLenSpin"
            from: 1
            to: 20
            value: win.clipboardMinLen
            onValueChanged: {
                win.clipboardMinLen = value
                platform.setClipboardMinWordLength(value)
            }
        }
        Label { text: Theme.zh ? "最长" : "Max"; color: Theme.textSecondary }
        SpinBox {
            objectName: "maxLenSpin"
            from: 10
            to: 200
            value: win.clipboardMaxLen
            onValueChanged: {
                win.clipboardMaxLen = value
                platform.setClipboardMaxWordLength(value)
            }
        }
    }

    Label {
        text: Theme.zh
            ? ("状态: " + (win.clipboardMonitoring ? "监控中" : "未监控"))
            : ("Status: " + (win.clipboardMonitoring ? "watching" : "idle"))
        color: Theme.textSecondary
    }

    // 划词取词（X11）：选中即查，命中分流与剪贴板同款
    // （取词窗开 → 悬浮窗；关 → 主窗直接展示）。Qt 仅
    // xcb 有独立 selection，其他平台整块隐藏（不假装）
    Switch {
        visible: platform.isSelectionMonitoringSupported()
        objectName: "selectionSwitch"
        text: Theme.zh ? "划词取词（选中即查，X11）"
                       : "Selection lookup (select to search, X11)"
        checked: win.selectionEnabled
        onToggled: {
            win.selectionEnabled = checked
            settings.setBool("selection/enabled", checked)
            if (checked) platform.startSelectionMonitoring()
            else platform.stopSelectionMonitoring()
            win.selectionMonitoring = platform.isSelectionMonitoring()
        }
    }
    Label {
        visible: platform.isSelectionMonitoringSupported()
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.textTertiary
        text: Theme.zh
            ? "在任意应用中划选文本即查（X11 主选区）；"
              + "超长选段与网址不算查词意图，忽略。"
            : "Select text in any app to look it up (X11 primary "
              + "selection). Long selections and URLs are ignored."
    }
    Label {
        visible: platform.isSelectionMonitoringSupported()
        objectName: "selectionStatusLabel"
        text: Theme.zh
            ? ("划词: " + (win.selectionMonitoring ? "监控中" : "未监控"))
            : ("Selection: " + (win.selectionMonitoring ? "watching" : "idle"))
        color: Theme.textSecondary
    }

    // F13b：macOS 焦点窗选区读取（AX API，取词热键文本源）。
    // 非 macOS 整块隐藏（不假装）。授权口径 = 方案 d：
    // 首次取词弹一次系统提示 + 此处内联引导直到授权
    Switch {
        visible: platform.isMacSelectionReaderSupported()
        objectName: "macSelectionSwitch"
        text: Theme.zh ? "读焦点窗选区（macOS）"
                       : "Read focused-window selection (macOS)"
        checked: settings.getBool("quicklookup/macReadSelection", true)
        onToggled: {
            settings.setBool("quicklookup/macReadSelection", checked)
            win.statusText = checked
                ? (Theme.zh ? "取词将读取焦点窗选区" : "Lookup will read the focused window selection")
                : (Theme.zh ? "取词改用剪贴板" : "Lookup falls back to the clipboard")
        }
    }
    Label {
        visible: platform.isMacSelectionReaderSupported()
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.textTertiary
        text: Theme.zh
            ? "在任意应用中选中文本后按取词热键即查（读焦点窗选区）。"
            : "Select text anywhere, then press the lookup hotkey "
              + "(reads the focused window selection)."
    }
    RowLayout {
        visible: platform.isMacSelectionReaderSupported()
                 && !platform.isMacAccessibilityTrusted()
        objectName: "macPermissionRow"
        Layout.fillWidth: true
        spacing: 8
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 12
            color: Theme.textTertiary
            text: Theme.zh
                ? "未获「辅助功能」授权，取词时读不到焦点窗选区"
                  + "（回落剪贴板）。授权后无需重启，下次取词即生效。"
                : "Accessibility permission not granted — the focused "
                  + "window selection can't be read (falls back to the "
                  + "clipboard). No restart needed after granting."
        }
        Button {
            objectName: "macPermissionOpenButton"
            text: Theme.zh ? "打开系统设置" : "Open System Settings"
            font.pixelSize: 12
            onClicked: Qt.openUrlExternally(
                           platform.macAccessibilitySettingsUrl())
        }
    }

    // 登录自启（巡检续批④ F9）：Win=HKCU Run 键、
    // Linux=XDG autostart .desktop、macOS=LaunchAgent
    // plist（UNIDICT_AUTOSTART_DIR 测试缝供审计密封）；
    // 移动端等 stub 平台整块隐藏（不假装）
    Switch {
        visible: startup.isPlatformSupported()
        objectName: "startupToggle"
        text: Theme.zh ? "开机自启" : "Start at login"
        checked: startup.isEnabled()
        onToggled: {
            var ok = startup.setEnabled(checked)
            if (!ok) checked = !checked
            win.statusText = ok
                ? (checked
                    ? (Theme.zh ? "已开启开机自启" : "Start at login enabled")
                    : (Theme.zh ? "已关闭开机自启" : "Start at login disabled"))
                : (Theme.zh ? "开机自启设置失败" : "Couldn't change start at login")
        }
    }
    Label {
        visible: startup.isPlatformSupported()
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.textTertiary
        text: Theme.zh
            ? ("登录桌面后自动拉起 Unidict（" +
               (Qt.platform.os === "osx"
                    ? "LaunchAgent"
                    : Qt.platform.os === "windows"
                          ? "注册表 Run 键"
                          : "XDG autostart") + "）。")
            : ("Launch Unidict when you log in (" +
               (Qt.platform.os === "osx"
                    ? "LaunchAgent"
                    : Qt.platform.os === "windows"
                          ? "registry Run key"
                          : "XDG autostart") + ").")
    }
}
