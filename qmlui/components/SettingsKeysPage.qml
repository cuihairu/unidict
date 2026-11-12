import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·热键页（LAY-001 自 MainDesktop.qml 拆出）
// 快捷键
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ColumnLayout {
    objectName: "settingskeysPage"

    property var win: null
    property var platform: null
    property var settings: null


    spacing: 8
    Label { text: Theme.zh ? "Ctrl+K：聚焦搜索框" : "Ctrl+K: focus search" }
    Label { text: Theme.zh ? "Enter：查询；Esc：清空搜索" : "Enter: search; Esc: clear search" }
    Label { text: Theme.zh ? "Ctrl+1/2/3：切换 结果/历史/生词本" : "Ctrl+1/2/3: Results / History / Wordbook" }
    Label { text: Theme.zh ? "←/→：释义区返回/前进（本次会话）" : "←/→: back / forward in the entry pane" }

    // P-5 取词窗系统级热键（quick_lookup）：按下读剪贴板
    // 弹取词窗。注册面走 GlobalHotkeys（三桌面平台已实
    // 装；Wayland 等不可用会话注册时如实报失败），注册
    // 结果如实回显
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: Theme.zh ? "取词窗热键" : "Lookup hotkey" }
        TextField {
            id: quickHotkeyField
            objectName: "quickHotkeyField"
            Layout.fillWidth: true
            text: settings.getString("quicklookup/hotkey", "Alt+Q")
            placeholderText: Theme.zh ? "如 Alt+Q" : "e.g. Alt+Q"
        }
        Button {
            objectName: "hotkeyApplyButton"
            text: Theme.zh ? "应用" : "Apply"
            onClicked: {
                platform.unregisterGlobalHotkey("quick_lookup")
                var seq = quickHotkeyField.text.trim()
                if (seq.length === 0) {
                    hotkeyState.text = Theme.zh ? "已清除取词热键"
                                                : "Lookup hotkey cleared"
                    return
                }
                var ok = platform.registerGlobalHotkey("quick_lookup", seq)
                hotkeyState.text = ok
                    ? (Theme.zh ? ("已生效: " + seq) : ("Active: " + seq))
                    : (Theme.zh
                        ? "注册失败：换个组合试试（避免与系统/其他程序冲突）"
                        : "Registration failed — try another combo")
                if (ok) settings.setString("quicklookup/hotkey", seq)
            }
        }
    }
    Label {
        id: hotkeyState
        objectName: "hotkeyState"
        visible: text.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.textSecondary
    }
    Label {
        visible: !platform.isGlobalHotkeysSupported()
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: Theme.zh
            ? "当前平台不支持系统级快捷键；复制内容后用「剪贴板取词」触发取词窗即可。"
            : "System-wide hotkeys aren't available here — use "
              + "clipboard lookup to trigger the popup instead."
        font.pixelSize: 12
        color: Theme.textTertiary
    }
    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: Theme.zh
            ? "取词窗热键按下 → 读取剪贴板内容弹窗释义（等效复制后自动取词）。"
            : "The hotkey reads the clipboard and pops the "
              + "definition (same as copy-to-lookup)."
        font.pixelSize: 12
        color: Theme.textTertiary
    }
}
