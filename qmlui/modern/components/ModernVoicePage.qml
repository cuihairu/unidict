import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Multimedia
import "../Theme.qml"

// 现代化语音功能页面
Item {
    id: root

    // ========== 公共属性 ==========
    property string currentWord: ""
    property alias lookupAdapter: internal.lookupAdapter

    // 语音设置
    property double speechRate: 1.0
    property double speechPitch: 1.0
    property double speechVolume: 0.8
    property string selectedVoice: ""
    property var availableVoices: []

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
        property bool isRecording: false
        property bool isPlaying: false
        property bool isSpeaking: false
        property var recordingLevel: 0
        property var recordedAudio: null
        property var playbackTimer: null
    }

    // ========== 页面布局 ==========
    Column {
        anchors.fill: parent
        spacing: theme.spacingMD

        // 页面头部
        Rectangle {
            width: parent.width
            height: 70
            color: "transparent"

            Row {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                spacing: theme.spacingMD

                // 标题区域
                Row {
                    spacing: theme.spacingSM

                    Text {
                        text: "🔊"
                        font.pixelSize: 18
                        color: theme.primary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        spacing: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: "语音功能"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 18
                            font.weight: Font.SemiBold
                            color: theme.textPrimary
                        }

                        Text {
                            text: currentWord ? `当前: ${currentWord}` : "请先选择一个词汇"
                            font.family: "SF Pro Display"
                            font.pixelSize: 12
                            color: theme.textSecondary
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // 状态指示器
                Row {
                    spacing: theme.spacingSM

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: internal.isRecording ? theme.error :
                              internal.isSpeaking ? theme.warning :
                              internal.isPlaying ? theme.info :
                              theme.textTertiary

                        SequentialAnimation on opacity {
                            running: internal.isRecording || internal.isSpeaking
                            loops: Animation.Infinite
                            PropertyAnimation { to: 0.3; duration: 500 }
                            PropertyAnimation { to: 1.0; duration: 500 }
                        }
                    }

                    Text {
                        text: internal.isRecording ? "录音中" :
                              internal.isSpeaking ? "朗读中" :
                              internal.isPlaying ? "播放中" :
                              "就绪"
                        font.family: "SF Pro Display"
                        font.pixelSize: 12
                        color: theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        // 主要功能区域
        Row {
            width: parent.width
            Layout.fillHeight: true
            spacing: theme.spacingMD

            // 左侧 - 录音和播放控制
            Column {
                width: parent.width / 2
                height: parent.height
                spacing: theme.spacingMD

                // 朗读控制面板
                ModernCard {
                    width: parent.width
                    height: 280
                    title: "文本朗读"
                    subtitle: currentWord || "请选择要朗读的词汇"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingLG

                        // 可视化音频波形
                        Rectangle {
                            id: waveform
                            width: parent.width
                            height: 80
                            color: theme.overlay
                            radius: theme.radiusMedium

                            // 音频波形可视化
                            Row {
                                anchors.centerIn: parent
                                spacing: 2

                                Repeater {
                                    model: 30
                                    Rectangle {
                                        width: 3
                                        height: Math.random() * 60 + 10
                                        radius: 1.5
                                        color: internal.isSpeaking ? theme.primary :
                                               theme.textTertiary

                                        SequentialAnimation on height {
                                            running: internal.isSpeaking
                                            loops: Animation.Infinite
                                            PropertyAnimation {
                                                to: Math.random() * 60 + 10
                                                duration: 150
                                            }
                                        }
                                    }
                                }
                            }

                            // 当前显示的文本
                            Text {
                                anchors.centerIn: parent
                                text: currentWord || "准备朗读..."
                                font.family: "SF Pro Display"
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: theme.textPrimary
                                visible: !internal.isSpeaking
                            }
                        }

                        // 朗读控制按钮
                        Row {
                            anchors.horizontalCenter: parent
                            spacing: theme.spacingMD

                            ModernButton {
                                text: internal.isSpeaking ? "⏸️ 停止" : "🔊 朗读"
                                buttonType: internal.isSpeaking ? ModernButton.Secondary : ModernButton.Primary
                                size: ModernButton.Large
                                enabled: currentWord !== "" && !internal.isSpeaking
                                loading: internal.isSpeaking

                                onClicked: {
                                    if (internal.isSpeaking) {
                                        stopSpeaking()
                                    } else {
                                        startSpeaking(currentWord)
                                    }
                                }
                            }

                            ModernButton {
                                text: "📝 文本"
                                buttonType: ModernButton.Outline
                                size: ModernButton.Large
                                visible: false // 暂时隐藏

                                onClicked: {
                                    showTextInputDialog()
                                }
                            }
                        }
                    }
                }

                // 录音功能面板
                ModernCard {
                    width: parent.width
                    Layout.fillHeight: true
                    title: "录音功能"
                    subtitle: "录制发音练习"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingLG

                        // 录音可视化
                        Rectangle {
                            width: parent.width
                            height: 100
                            color: theme.overlay
                            radius: theme.radiusMedium

                            Column {
                                anchors.centerIn: parent
                                spacing: theme.spacingMD

                                // 录音音量指示器
                                Row {
                                    anchors.horizontalCenter: parent
                                    spacing: 2

                                    Repeater {
                                        model: 20
                                        Rectangle {
                                            width: 4
                                            height: internal.isRecording ?
                                                   (index < internal.recordingLevel ? 40 : 4) : 4
                                            radius: 2
                                            color: internal.isRecording ?
                                                   (index < internal.recordingLevel ? theme.error : theme.border) :
                                                   theme.textTertiary

                                            Behavior on height {
                                                NumberAnimation {
                                                    duration: 50
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                    }
                                }

                                Text {
                                    anchors.horizontalCenter: parent
                                    text: internal.isRecording ? "录音中..." : "点击开始录音"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    color: internal.isRecording ? theme.error : theme.textSecondary
                                }
                            }
                        }

                        // 录音控制按钮
                        Row {
                            anchors.horizontalCenter: parent
                            spacing: theme.spacingMD

                            ModernButton {
                                text: internal.isRecording ? "⏹️ 停止" : "🎤 录音"
                                buttonType: internal.isRecording ? ModernButton.Secondary : ModernButton.Primary
                                size: ModernButton.Large
                                loading: internal.isRecording

                                onClicked: {
                                    if (internal.isRecording) {
                                        stopRecording()
                                    } else {
                                        startRecording()
                                    }
                                }
                            }

                            ModernButton {
                                text: "🔂 重录"
                                buttonType: ModernButton.Outline
                                size: ModernButton.Large
                                enabled: internal.recordedAudio !== null

                                onClicked: {
                                    startRecording()
                                }
                            }

                            ModernButton {
                                text: "▶️ 播放"
                                buttonType: ModernButton.Outline
                                size: ModernButton.Large
                                enabled: internal.recordedAudio !== null

                                onClicked: {
                                    playRecording()
                                }
                            }
                        }
                    }
                }
            }

            // 右侧 - 语音设置
            Column {
                width: parent.width / 2
                height: parent.height
                spacing: theme.spacingMD

                // 语音设置面板
                ModernCard {
                    width: parent.width
                    height: 300
                    title: "语音设置"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingMD

                        // 语速设置
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Row {
                                width: parent.width
                                spacing: theme.spacingMD

                                Text {
                                    text: "语速"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: `${Math.round(speechRate * 100)}%`
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    color: theme.textSecondary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Slider {
                                id: speechRateSlider
                                width: parent.width
                                from: 0.5
                                to: 2.0
                                value: speechRate
                                stepSize: 0.1

                                Material.accent: theme.primary

                                onValueChanged: {
                                    speechRate = value
                                }
                            }
                        }

                        // 音调设置
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Row {
                                width: parent.width
                                spacing: theme.spacingMD

                                Text {
                                    text: "音调"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: `${Math.round(speechPitch * 100)}%`
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    color: theme.textSecondary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Slider {
                                id: speechPitchSlider
                                width: parent.width
                                from: 0.5
                                to: 2.0
                                value: speechPitch
                                stepSize: 0.1

                                Material.accent: theme.primary

                                onValueChanged: {
                                    speechPitch = value
                                }
                            }
                        }

                        // 音量设置
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Row {
                                width: parent.width
                                spacing: theme.spacingMD

                                Text {
                                    text: "音量"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: `${Math.round(speechVolume * 100)}%`
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    color: theme.textSecondary
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Slider {
                                id: speechVolumeSlider
                                width: parent.width
                                from: 0.0
                                to: 1.0
                                value: speechVolume
                                stepSize: 0.1

                                Material.accent: theme.primary

                                onValueChanged: {
                                    speechVolume = value
                                }
                            }
                        }

                        // 语音选择
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Text {
                                text: "语音选择"
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: theme.textPrimary
                            }

                            ComboBox {
                                id: voiceComboBox
                                width: parent.width
                                model: availableVoices
                                currentIndex: 0

                                Material.background: theme.surface
                                Material.foreground: theme.textPrimary

                                onActivated: function(index) {
                                    selectedVoice = availableVoices[index]
                                }
                            }
                        }
                    }
                }

                // 快捷操作面板
                ModernCard {
                    width: parent.width
                    Layout.fillHeight: true
                    title: "快捷操作"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingMD

                        // 预设配置
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Text {
                                text: "预设配置"
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: theme.textPrimary
                            }

                            Row {
                                width: parent.width
                                spacing: theme.spacingSM

                                ModernButton {
                                    text: "慢速"
                                    buttonType: ModernButton.Outline
                                    size: ModernButton.Small
                                    Layout.fillWidth: true

                                    onClicked: {
                                        applyPreset("slow")
                                    }
                                }

                                ModernButton {
                                    text: "正常"
                                    buttonType: ModernButton.Outline
                                    size: ModernButton.Small
                                    Layout.fillWidth: true

                                    onClicked: {
                                        applyPreset("normal")
                                    }
                                }

                                ModernButton {
                                    text: "快速"
                                    buttonType: ModernButton.Outline
                                    size: ModernButton.Small
                                    Layout.fillWidth: true

                                    onClicked: {
                                        applyPreset("fast")
                                    }
                                }
                            }
                        }

                        // 对比朗读
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Text {
                                text: "对比朗读"
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: theme.textPrimary
                            }

                            ModernButton {
                                width: parent.width
                                text: "🔁 对比发音"
                                buttonType: ModernButton.Secondary

                                onClicked: {
                                    startComparisonMode()
                                }
                            }
                        }

                        // 导出设置
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Text {
                                text: "导出设置"
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: theme.textPrimary
                            }

                            ModernButton {
                                width: parent.width
                                text: "💾 保存录音"
                                buttonType: ModernButton.Outline
                                enabled: internal.recordedAudio !== null

                                onClicked: {
                                    saveRecording()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 功能方法 ==========
    function startSpeaking(text) {
        console.log("Start speaking:", text)
        internal.isSpeaking = true

        // 模拟朗读过程
        speakingTimer.duration = text.length * 100 // 基于文本长度的模拟时间
        speakingTimer.restart()
    }

    function stopSpeaking() {
        console.log("Stop speaking")
        internal.isSpeaking = false
        speakingTimer.stop()
    }

    function startRecording() {
        console.log("Start recording")
        internal.isRecording = true
        internal.recordingLevel = 0

        // 模拟录音音量变化
        recordingTimer.restart()
    }

    function stopRecording() {
        console.log("Stop recording")
        internal.isRecording = false
        recordingTimer.stop()

        // 模拟录音完成
        internal.recordedAudio = "mock_audio_data"
    }

    function playRecording() {
        console.log("Play recording")
        if (internal.recordedAudio) {
            internal.isPlaying = true
            playbackTimer.restart()
        }
    }

    function applyPreset(preset) {
        switch(preset) {
            case "slow":
                speechRate = 0.8
                speechPitch = 0.9
                break
            case "normal":
                speechRate = 1.0
                speechPitch = 1.0
                break
            case "fast":
                speechRate = 1.3
                speechPitch = 1.1
                break
        }

        // 更新滑块显示
        speechRateSlider.value = speechRate
        speechPitchSlider.value = speechPitch
    }

    function startComparisonMode() {
        console.log("Start comparison mode")
        // 实现对比朗读功能
    }

    function saveRecording() {
        console.log("Save recording")
        // 实现保存录音功能
    }

    function showTextInputDialog() {
        console.log("Show text input dialog")
        // 显示文本输入对话框
    }

    // ========== 计时器 ==========
    Timer {
        id: speakingTimer
        interval: 1000
        onTriggered: {
            internal.isSpeaking = false
        }
    }

    Timer {
        id: recordingTimer
        interval: 100
        repeat: true
        running: internal.isRecording

        onTriggered: {
            // 模拟录音音量变化
            internal.recordingLevel = Math.random() * 15 + 5
        }
    }

    Timer {
        id: playbackTimer
        interval: 2000
        onTriggered: {
            internal.isPlaying = false
        }
    }

    // ========== 音频相关组件（实际应用中需要真正的音频库） ==========
    Audio {
        id: audioPlayer
        // 音频播放器配置
    }

    // ========== 信号 ==========
    signal recordingStarted()
    signal recordingStopped()
    signal recordingPlayed()
    signal speakingStarted(string text)
    signal speakingStopped()
    signal settingsChanged()
}