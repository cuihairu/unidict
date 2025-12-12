import QtQuick 2.15
import QtQuick.Controls 2.15
import "../Theme.qml"

// 现代化导航项组件
Rectangle {
    id: root

    // ========== 公共属性 ==========
    property alias title: titleText.text
    property alias icon: iconText.text
    property bool isActive: false
    property bool showBadge: false
    property alias badgeText: badgeLabel.text

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 基础样式 ==========
    color: "transparent"
    height: 48
    radius: theme.radiusSmall

    // 激活状态背景
    Rectangle {
        id: activeBackground
        anchors.fill: parent
        color: theme.primaryLight
        radius: theme.radiusSmall
        opacity: isActive ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: theme.animationFast
                easing.type: Easing.OutCubic
            }
        }
    }

    // 悬停状态
    scale: mouseArea.containsMouse ? 1.02 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: theme.animationFast
            easing.type: Easing.OutCubic
        }
    }

    // ========== 内容布局 ==========
    Row {
        anchors.left: parent.left
        anchors.leftMargin: theme.spacingMD
        anchors.verticalCenter: parent.verticalCenter
        spacing: theme.spacingMD

        // 图标
        Text {
            id: iconText
            font.pixelSize: 18
            anchors.verticalCenter: parent.verticalCenter

            // 根据状态调整颜色
            color: isActive ? theme.primary : theme.textSecondary
        }

        // 标题
        Text {
            id: titleText
            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
            font.pixelSize: 15
            font.weight: isActive ? Font.Medium : Font.Normal
            anchors.verticalCenter: parent.verticalCenter

            color: isActive ? theme.primary : theme.textPrimary
        }

        // 弹性空间
        Item { Layout.fillWidth: true }

        // 徽章
        Rectangle {
            id: badge
            visible: showBadge
            anchors.verticalCenter: parent.verticalCenter
            width: badgeLabel.implicitWidth + 8
            height: 18
            radius: 9
            color: theme.error

            Text {
                id: badgeLabel
                anchors.centerIn: parent
                font.family: "SF Pro Display"
                font.pixelSize: 10
                font.weight: Font.Medium
                color: "#FFFFFF"
            }
        }

        // 箭头图标
        Text {
            visible: isActive
            text: "→"
            font.pixelSize: 14
            color: theme.primary
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ========== 交互处理 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onClicked: root.clicked()
    }

    // ========== 信号 ==========
    signal clicked()

    // ========== 动画效果 ==========
    SequentialAnimation {
        id: selectionAnimation
        running: false

        PropertyAnimation {
            target: root
            property: "scale"
            to: 0.98
            duration: 50
        }
        PropertyAnimation {
            target: root
            property: "scale"
            to: 1.0
            duration: 50
        }
    }

    function animateSelection() {
        selectionAnimation.start()
    }
}