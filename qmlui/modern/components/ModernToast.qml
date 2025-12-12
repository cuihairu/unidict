import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化通知提示组件
Rectangle {
    id: root

    // ========== 公共属性 ==========
    property alias text: messageText.text
    property alias title: titleText.text
    property alias icon: iconText.text

    // 通知类型
    enum ToastType {
        Info,
        Success,
        Warning,
        Error,
        Loading
    }

    property int type: ModernToast.Info
    property bool showIcon: true
    property bool showProgress: false
    property real progress: 0

    // 显示控制
    property bool autoHide: true
    property int duration: 3000
    property bool canDismiss: true

    // 位置控制
    enum ToastPosition {
        Top,
        Bottom,
        Center
    }

    property int position: ModernToast.Bottom
    property int offsetX: 0
    property int offsetY: 0

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    property bool isShowing: false
    property bool isDismissing: false

    // ========== 样式计算 ==========
    readonly property color bgColor: {
        switch(type) {
            case ModernToast.Success: return theme.success
            case ModernToast.Warning: return theme.warning
            case ModernToast.Error: return theme.error
            case ModernToast.Loading: return theme.textSecondary
            case ModernToast.Info:
            default: return theme.info
        }
    }

    readonly property color textColor: "#FFFFFF"
    readonly property color iconColor: "#FFFFFF"
    readonly property color progressBarColor: "rgba(255, 255, 255, 0.3)"

    // ========== 基础样式 ==========
    color: Qt.rgba(bgColor.r, bgColor.g, bgColor.b, 0.95)
    radius: theme.radiusLarge
    border.color: "rgba(255, 255, 255, 0.2)"
    border.width: 1

    // 最小宽度
    width: Math.max(300, contentColumn.implicitWidth + theme.spacingLG * 2)
    height: contentColumn.implicitHeight + theme.spacingLG * 2

    // 初始状态
    opacity: 0
    scale: 0.8

    // 阴影效果
    layer.enabled: true
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: 4
        radius: 12
        samples: 24
        color: "rgba(0, 0, 0, 0.15)"
        spread: 0.0
    }

    // ========== 内容布局 ==========
    Column {
        id: contentColumn
        anchors.centerIn: parent
        spacing: theme.spacingSM
        width: parent.width - theme.spacingLG * 2

        // 标题行
        Row {
            width: parent.width
            spacing: theme.spacingSM
            visible: titleText.text !== ""

            Text {
                id: titleText
                font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                font.pixelSize: 15
                font.weight: Font.SemiBold
                color: textColor
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // 关闭按钮
            Rectangle {
                width: 20
                height: 20
                radius: 10
                color: progressBarColor
                visible: canDismiss

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: textColor
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onEntered: {
                        parent.color = "rgba(255, 255, 255, 0.4)"
                    }

                    onExited: {
                        parent.color = progressBarColor
                    }

                    onClicked: {
                        dismiss()
                    }
                }
            }
        }

        // 主要内容行
        Row {
            width: parent.width
            spacing: theme.spacingMD
            visible: contentRow.visible

            // 图标
            Text {
                id: iconText
                font.pixelSize: 18
                color: iconColor
                visible: showIcon
                anchors.verticalCenter: parent.verticalCenter
            }

            // 加载动画（Loading类型时显示）
            BusyIndicator {
                id: loadingIndicator
                visible: type === ModernToast.Loading
                running: visible
                implicitWidth: 18
                implicitHeight: 18
                anchors.verticalCenter: parent.verticalCenter

                Material.accent: textColor
            }

            // 消息文本
            Text {
                id: messageText
                font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                font.pixelSize: 14
                color: textColor
                wrapMode: Text.WordWrap
                lineHeight: 1.4
                Layout.fillWidth: true
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // 进度条
        Rectangle {
            width: parent.width
            height: 3
            radius: 1.5
            color: progressBarColor
            visible: showProgress && !isDismissing

            Rectangle {
                width: parent.width * progress
                height: parent.height
                radius: parent.radius
                color: textColor

                Behavior on width {
                    NumberAnimation {
                        duration: 200
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        // 自动隐藏进度条
        Rectangle {
            id: autoHideProgress
            width: parent.width
            height: 2
            radius: 1
            color: progressBarColor
            visible: autoHide && !isDismissing

            Rectangle {
                width: parent.width
                height: parent.height
                radius: parent.radius
                color: textColor

                SequentialAnimation on width {
                    id: autoHideAnimation
                    running: isShowing
                    loops: 1

                    PropertyAnimation {
                        from: parent.width
                        to: 0
                        duration: root.duration
                        easing.type: Easing.Linear
                    }
                    onFinished: {
                        if (root.autoHide && !isDismissing) {
                            root.dismiss()
                        }
                    }
                }
            }
        }
    }

    // ========== 公共方法 ==========
    function show(message, title, toastType, showDuration) {
        if (message) text = message
        if (title) root.title = title
        if (toastType !== undefined) type = toastType
        if (showDuration !== undefined) duration = showDuration

        // 显示动画
        showAnimation.restart()
    }

    function showSuccess(message, title, duration) {
        show(message || "操作成功", title || "成功", ModernToast.Success, duration || 3000)
    }

    function showError(message, title, duration) {
        show(message || "操作失败", title || "错误", ModernToast.Error, duration || 5000)
    }

    function showWarning(message, title, duration) {
        show(message || "请注意", title || "警告", ModernToast.Warning, duration || 4000)
    }

    function showInfo(message, title, duration) {
        show(message || "提示信息", title || "信息", ModernToast.Info, duration || 3000)
    }

    function showLoading(message, title) {
        show(message || "正在处理...", title || "加载中", ModernToast.Loading, 0) // 不自动隐藏
    }

    function showProgress(message, title, progressValue) {
        show(message || "处理中...", title || "进度", ModernToast.Info, 0)
        showProgress = true
        progress = progressValue || 0
    }

    function updateProgress(progressValue) {
        progress = progressValue || 0
        if (progressValue >= 1.0) {
            dismiss()
        }
    }

    function dismiss() {
        if (!isDismissing) {
            isDismissing = true
            hideAnimation.restart()
        }
    }

    // ========== 动画效果 ==========
    SequentialAnimation {
        id: showAnimation
        running: false

        PropertyAnimation {
            target: root
            property: "opacity"
            to: 1
            duration: theme.animationFast
            easing.type: Easing.OutCubic
        }
        PropertyAnimation {
            target: root
            property: "scale"
            to: 1.0
            duration: theme.animationFast
            easing.type: Easing.OutBack
        }
        onFinished: {
            isShowing = true
            isDismissing = false
        }
    }

    SequentialAnimation {
        id: hideAnimation
        running: false

        PropertyAnimation {
            target: root
            property: "scale"
            to: 0.95
            duration: theme.animationFast
            easing.type: Easing.InCubic
        }
        PropertyAnimation {
            target: root
            property: "opacity"
            to: 0
            duration: theme.animationFast
            easing.type: Easing.InCubic
        }
        onFinished: {
            isShowing = false
            isDismissing = false
            root.visible = false
            root.destroy() // 销毁自身
        }
    }

    // ========== 键盘支持 ==========
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Escape && canDismiss) {
            dismiss()
            event.accepted = true
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (canDismiss) {
                dismiss()
                event.accepted = true
            }
        }
    }

    // ========== 自动聚焦键盘 ==========
    Component.onCompleted: {
        if (canDismiss) {
            root.forceActiveFocus()
        }
    }
}