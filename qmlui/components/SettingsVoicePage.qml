import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·语音页（LAY-001 自 MainDesktop.qml 拆出）
// 语音（M6 控件已多于窗高，走 ScrollView 收纳）
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ScrollView {
    objectName: "settingsvoicePage"

    property var win: null
    property var voice: null


    // 根级 onCompleted 的语音面初始化（LAY-001 拆页时随迁）：装载
    // 声音列表、回读 combo 选中项（原 MainDesktop syncVoiceSelections
    // 根函数——只触碰本页 combo，随页走）
    function syncVoiceSelections() {
        if (win.voiceList && win.voiceList.length > 0) {
            var current = voice.getCurrentVoice()
            var idx = win.voiceList.indexOf(current)
            if (idx >= 0) voiceCombo.currentIndex = idx
        }
        if (win.voicePresetList && win.voicePresetList.length > 0) {
            presetCombo.currentIndex = 0
        }
    }
    Component.onCompleted: {
        win.reloadVoices()
        syncVoiceSelections()
        pronSourceCombo.currentIndex = voice.pronSourceMode()
        pronAccentCombo.currentIndex = voice.pronAccent()
    }

    clip: true
    ColumnLayout {
        width: parent.width
        spacing: 10

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        ComboBox {
            id: presetCombo
            objectName: "presetCombo"
            Layout.fillWidth: true
            model: win.voicePresetList
            onActivated: {
                voice.applyVoicePreset(currentText)
            }
        }

        ToolButton {
            objectName: "stopSpeakButton"
            text: Theme.zh ? "停止" : "Stop"
            onClicked: voice.stopSpeaking()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        ComboBox {
            id: voiceCombo
            objectName: "voiceCombo"
            Layout.fillWidth: true
            model: win.voiceList
            onActivated: voice.setVoice(currentText)
        }

        ToolButton {
            objectName: "refreshVoicesButton"
            text: Theme.zh ? "刷新" : "Refresh"
            onClicked: win.reloadVoices()
        }
    }

    Label { text: Theme.zh ? "音量" : "Volume"; color: Theme.textSecondary }
    Slider {
        objectName: "volumeSlider"
        from: 0.0
        to: 1.0
        value: win.ttsVolume
        onMoved: {
            win.ttsVolume = value
            voice.setVolume(value)
        }
    }

    Label { text: Theme.zh ? "语速" : "Speed"; color: Theme.textSecondary }
    Slider {
        objectName: "rateSlider"
        from: 0.1
        to: 2.0
        value: win.ttsRate
        onMoved: {
            win.ttsRate = value
            voice.setRate(value)
        }
    }

    Label { text: Theme.zh ? "音调" : "Pitch"; color: Theme.textSecondary }
    Slider {
        objectName: "pitchSlider"
        from: -1.0
        to: 1.0
        value: win.ttsPitch
        onMoved: {
            win.ttsPitch = value
            voice.setPitch(value)
        }
    }

    // 发音源三态与口音偏好（在线源 dictionaryapi.dev，
    // 免密钥无配额）。默认本地——开在线是显式动作
    Label { text: Theme.zh ? "发音源" : "Pronunciation source"; color: Theme.textSecondary }
    ComboBox {
        id: pronSourceCombo
        objectName: "pronSourceCombo"
        Layout.fillWidth: true
        model: Theme.zh
            ? ["本地语音（系统 TTS）", "在线发音（dictionaryapi.dev）", "自动（在线优先，失败回落本地）"]
            : ["Local voice (system TTS)", "Online (dictionaryapi.dev)", "Auto (online first, local fallback)"]
        onActivated: {
            voice.setPronSourceMode(currentIndex)
            pronPrivacyHint.visible = currentIndex !== 0
        }
    }

    Label { text: Theme.zh ? "口音（在线发音）" : "Accent (online)"; color: Theme.textSecondary }
    ComboBox {
        id: pronAccentCombo
        objectName: "pronAccentCombo"
        Layout.fillWidth: true
        model: Theme.zh ? ["自动", "美音", "英音", "澳音"]
                        : ["Auto", "US", "UK", "AU"]
        onActivated: voice.setPronAccent(currentIndex)
    }

    Label {
        id: pronPrivacyHint
        visible: voice.pronSourceMode() !== 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: Theme.zh
            ? "开启在线发音后，播报会把查询词发送给发音服务 dictionaryapi.dev（仅查询词，不带历史与生词本）。"
            : "Online pronunciation sends the looked-up word to "
              + "dictionaryapi.dev (word only — no history or wordbook)."
        color: Theme.textSecondary
        font.pixelSize: 12
    }

    Label {
        id: pronOnlineStatusLabel
        visible: win.pronOnlineStatusText.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: win.pronOnlineStatusText
        color: Theme.textSecondary
        font.pixelSize: 12
    }
    }
}
