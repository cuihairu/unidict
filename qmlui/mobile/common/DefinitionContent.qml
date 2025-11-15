import QtQuick 2.15
import QtQuick.Controls 2.15

// 定义内容显示组件 - 可复用
Column {
    id: root
    width: parent.width - 20
    spacing: responsive.baseSpacing

    // 播放控制区域
    Flow {
        width: parent.width
        spacing: responsive.baseSpacing
        visible: currentWord.length > 0

        Button {
            id: speakButton
            text: lookup.isSpeaking() ? "⏸️停止" : "🔊播放"
            enabled: currentWord.length > 0
            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onClicked: {
                if (lookup.isSpeaking()) {
                    lookup.stopSpeaking()
                } else {
                    lookup.speakText(currentWord)
                }
            }
            // 监听播放状态变化
            Timer {
                interval: 200
                running: true
                repeat: true
                onTriggered: speakButton.text = lookup.isSpeaking() ? "⏸️停止" : "🔊播放"
            }
        }

        Button {
            text: "📖定义"
            enabled: currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onClicked: lookup.speakText(currentDefinition)
        }

        Label {
            text: currentWord
            font.bold: true
            font.pixelSize: responsive.normalFont
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    TextArea {
        id: definitionView
        readOnly: true
        wrapMode: Text.Wrap
        width: parent.width
        height: responsive.isMobile ? 140 : 200
        text: currentDefinition
        font.pixelSize: responsive.normalFont

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: Qt.rgba(0,0,0,0.2)
            border.width: 1
            radius: 4
            z: -1
        }

        Text {
            anchors.centerIn: parent
            visible: !parent.text
            text: "Definition will appear here"
            color: "gray"
            font.pixelSize: responsive.smallFont
        }
    }
}