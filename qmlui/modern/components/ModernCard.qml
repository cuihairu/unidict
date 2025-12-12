import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化卡片组件
Rectangle {
    id: root

    // ========== 公共属性 ==========
    property alias title: titleLabel.text
    property alias subtitle: subtitleLabel.text
    property alias content: contentItem.data

    property color backgroundColor: theme.cardBackground
    property color borderColor: theme.border
    property int borderWidth: 1
    property int borderRadius: theme.radiusMedium

    property bool shadowEnabled: true
    property var shadowLevel: root.shadow1

    property int padding: theme.spacingMD
    property int spacing: theme.spacingSM

    property bool clickable: false
    property bool hoverEnabled: clickable

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 视觉效果 ==========
    color: backgroundColor
    border.color: borderColor
    border.width: borderWidth
    radius: borderRadius

    // 阴影效果
    layer.enabled: shadowEnabled
    layer.effect: DropShadow {
        horizontalOffset: shadowLevel[0].x
        verticalOffset: shadowLevel[0].y
        radius: shadowLevel[0].radius
        samples: shadowLevel[0].samples || (shadowLevel[0].radius * 2)
        color: shadowLevel[0].color
        spread: 0.0
    }

    // 交互状态
    scale: hoverEnabled && mouseArea.containsMouse ? 1.02 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: theme.animationFast;
            easing.type: Easing.OutCubic
        }
    }

    // ========== 内容区域 ==========
    Column {
        id: column
        anchors.fill: parent
        anchors.margins: root.padding
        spacing: root.spacing

        // 标题区域
        Column {
            width: parent.width
            spacing: theme.spacingXS
            visible: titleLabel.text || subtitleLabel.text

            Label {
                id: titleLabel
                width: parent.width
                font.family: "SF Pro Display"
                font.pixelSize: 16
                font.weight: Font.Medium
                color: theme.textPrimary
                visible: text
            }

            Label {
                id: subtitleLabel
                width: parent.width
                font.family: "SF Pro Display"
                font.pixelSize: 14
                color: theme.textSecondary
                visible: text
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        // 内容容器
        Item {
            id: contentItem
            width: parent.width
            Layout.fillHeight: true
            visible: children.length > 0
        }
    }

    // ========== 交互处理 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.clickable
        hoverEnabled: root.hoverEnabled

        cursorShape: root.clickable ? Qt.PointingHandCursor : Qt.ArrowCursor

        onClicked: root.clicked()
    }

    // ========== 信号 ==========
    signal clicked()

    // ========== 动画增强 ==========
    SequentialAnimation {
        id: pulseAnimation
        running: false

        PropertyAnimation {
            target: root;
            property: "scale";
            to: 1.05;
            duration: 100
        }
        PropertyAnimation {
            target: root;
            property: "scale";
            to: 1.0;
            duration: 100
        }
    }

    function pulse() {
        pulseAnimation.start()
    }
}