import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"

// 移动端主壳·Voice 页（LAY-002 自 Main.qml 拆出）
// 语音设置 Tab
//
// 窗口状态与根函数经 win 引用读写；responsive / suggestionModel
// 是 Main.qml 文件级 id，组件文件看不见父文件 id，故经本页同名字段
// 注入（实例化处同 id 名赋值——文档 id 优先于本组件同名字段，
// 与 SidebarPanel / Settings*Page 的 *Service 别名同律）；
// Theme / vocab / lookup 等 context 属性随引擎传播，无需注入。
Item {
    objectName: "mobileVoicePage"

    property var win: null
    property var responsive: null
    property var suggestionModel: null

    ScrollView {
        anchors.fill: parent
        contentHeight: voiceColumn.implicitHeight

        Column {
            id: voiceColumn
            width: parent.width
            anchors.margins: responsive.baseMargin
            spacing: responsive.baseSpacing

            GroupBox {
                title: "🎵 语音设置"
                width: parent.width
                font.pixelSize: responsive.normalFont

                Column {
                    width: parent.width
                    spacing: responsive.baseSpacing

                    // 语音引擎状态
                    Row {
                        spacing: responsive.baseSpacing
                        Label {
                            text: "引擎状态:"
                            font.pixelSize: responsive.normalFont
                        }
                        Label {
                            text: voice.availableVoices().length > 0 ? "✅ 可用" : "❌ 不可用"
                            color: voice.availableVoices().length > 0 ? Theme.success : Theme.danger
                            font.pixelSize: responsive.normalFont
                        }
                    }

                    // 音量控制
                    Column {
                        width: parent.width
                        spacing: responsive.baseSpacing / 2

                        Label {
                            text: "音量: " + Math.round(volumeSlider.value * 100) + "%"
                            font.pixelSize: responsive.normalFont
                        }
                        Slider {
                            id: volumeSlider
                            from: 0.0
                            to: 1.0
                            value: 0.8
                            stepSize: 0.1
                            width: parent.width
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            onValueChanged: voice.setVolume(value)
                        }
                    }

                    // 语速控制
                    Column {
                        width: parent.width
                        spacing: responsive.baseSpacing / 2

                        Label {
                            text: "语速: " + rateSlider.value.toFixed(1) + "x"
                            font.pixelSize: responsive.normalFont
                        }
                        Slider {
                            id: rateSlider
                            from: 0.1
                            to: 2.0
                            value: 1.0
                            stepSize: 0.1
                            width: parent.width
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            onValueChanged: voice.setRate(value)
                        }
                    }

                    // 音调控制
                    Column {
                        width: parent.width
                        spacing: responsive.baseSpacing / 2

                        Label {
                            text: "音调: " + pitchSlider.value.toFixed(1)
                            font.pixelSize: responsive.normalFont
                        }
                        Slider {
                            id: pitchSlider
                            from: -1.0
                            to: 1.0
                            value: 0.0
                            stepSize: 0.1
                            width: parent.width
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            onValueChanged: voice.setPitch(value)
                        }
                    }
                }
            }

            GroupBox {
                title: "🎤 可用语音"
                width: parent.width
                font.pixelSize: responsive.normalFont

                ListView {
                    width: parent.width
                    height: Math.min(200, count * responsive.minTouchTarget)
                    clip: true
                    model: voice.availableVoices()
                    delegate: ItemDelegate {
                        width: parent.width
                        height: Math.max(36, responsive.minTouchTarget)
                        text: modelData
                        font.pixelSize: responsive.normalFont
                        onClicked: {
                            voice.setVoice(modelData)
                            voiceStatus.text = "当前语音: " + modelData
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: parent.count === 0
                        text: "No voices available"
                        color: Theme.textTertiary
                        font.pixelSize: responsive.smallFont
                    }
                }
            }

            Label {
                id: voiceStatus
                text: "当前语音: 系统默认"
                font.italic: true
                font.pixelSize: responsive.normalFont
            }

            GroupBox {
                title: "🧪 测试语音"
                width: parent.width
                font.pixelSize: responsive.normalFont

                Column {
                    width: parent.width
                    spacing: responsive.baseSpacing

                    TextField {
                        id: testText
                        width: parent.width
                        height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                        placeholderText: "输入要测试的文本..."
                        text: "Hello, this is a pronunciation test."
                        font.pixelSize: responsive.normalFont
                    }

                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        Button {
                            text: "🎯 测试播放"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: voice.speakText(testText.text)
                        }
                        Button {
                            text: "⏹️ 停止"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: voice.stopSpeaking()
                        }
                    }
                }
            }
        }
    }
}
