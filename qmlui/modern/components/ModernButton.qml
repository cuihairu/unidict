import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化按钮组件
Rectangle {
    id: root

    // ========== 按钮类型 ==========
    enum ButtonType {
        Primary,     // 主要按钮
        Secondary,   // 次要按钮
        Outline,     // 轮廓按钮
        Ghost,       // 幽灵按钮
        Icon         // 图标按钮
    }

    // ========== 公共属性 ==========
    property alias text: buttonLabel.text
    property alias iconSource: iconImage.source
    property alias font: buttonLabel.font

    property int buttonType: ModernButton.Primary
    property int size: ModernButton.Medium

    enum ButtonSize {
        Small,
        Medium,
        Large
    }

    property color customColor: "transparent"
    property bool loading: false
    property bool disabled: false
    property bool fullWidth: false

    // 尺寸配置
    readonly property var sizeConfig: {
        "Small": {
            "height": 32,
            "fontSize": 12,
            "paddingH": 12,
            "iconSize": 16,
            "borderRadius": 6
        },
        "Medium": {
            "height": 40,
            "fontSize": 14,
            "paddingH": 16,
            "iconSize": 18,
            "borderRadius": 8
        },
        "Large": {
            "height": 48,
            "fontSize": 16,
            "paddingH": 20,
            "iconSize": 20,
            "borderRadius": 10
        }
    }

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 视觉效果计算 ==========
    readonly property color bgColor: {
        if (disabled) return theme.textDisabled
        if (buttonType === ModernButton.Primary) return theme.primary
        if (buttonType === ModernButton.Secondary) return theme.secondary
        if (buttonType === ModernButton.Outline || buttonType === ModernButton.Ghost) return "transparent"
        if (buttonType === ModernButton.Icon) return "transparent"
        return customColor
    }

    readonly property color textColor: {
        if (disabled) return theme.textDisabled
        if (buttonType === ModernButton.Primary) return "#FFFFFF"
        if (buttonType === ModernButton.Secondary) return "#FFFFFF"
        if (buttonType === ModernButton.Outline || buttonType === ModernButton.Ghost) return theme.primary
        if (buttonType === ModernButton.Icon) return theme.primary
        return theme.textPrimary
    }

    readonly property color borderColor: {
        if (disabled) return theme.textDisabled
        if (buttonType === ModernButton.Outline) return theme.primary
        if (buttonType === ModernButton.Secondary) return theme.secondary
        return "transparent"
    }

    // ========== 基础样式 ==========
    color: bgColor
    border.color: borderColor
    border.width: (buttonType === ModernButton.Outline || buttonType === ModernButton.Secondary) ? 1 : 0
    radius: sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].borderRadius

    // 尺寸
    height: sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].height
    width: fullWidth ? parent.width : implicitWidth

    implicitWidth: contentRow.implicitWidth + paddingH * 2
    property int paddingH: sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].paddingH

    // 交互状态
    opacity: disabled ? 0.5 : (loading ? 0.7 : 1.0)
    scale: mouseArea.containsMouse && !disabled && !loading ? 1.02 : 1.0

    // ========== 阴影效果 ==========
    layer.enabled: !disabled && buttonType !== ModernButton.Ghost && buttonType !== ModernButton.Icon
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: 2
        radius: 4
        samples: 8
        color: "rgba(0, 0, 0, 0.1)"
        spread: 0.0
        visible: !disabled && buttonType !== ModernButton.Ghost && buttonType !== ModernButton.Icon
    }

    // ========== 动画 ==========
    Behavior on scale {
        NumberAnimation {
            duration: theme.animationFast;
            easing.type: Easing.OutCubic
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: theme.animationNormal;
            easing.type: Easing.OutCubic
        }
    }

    // ========== 内容布局 ==========
    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: theme.spacingXS
        layoutDirection: root.layoutDirection

        // 图标
        Image {
            id: iconImage
            visible: source && source.toString() !== ""
            sourceSize.width: root.sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].iconSize
            sourceSize.height: sourceSize.width
            fillMode: Image.PreserveAspectFit
            anchors.verticalCenter: parent.verticalCenter

            // 图标颜色染色
            layer.enabled: true
            layer.effect: ColorOverlay {
                color: textColor
            }
        }

        // 加载动画
        BusyIndicator {
            id: loadingIndicator
            visible: loading
            running: loading
            implicitWidth: root.sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].iconSize
            implicitHeight: implicitWidth

            Material.accent: textColor
        }

        // 文本
        Label {
            id: buttonLabel
            visible: text && !loading
            color: textColor
            font.family: "SF Pro Display"
            font.pixelSize: root.sizeConfig[size === ModernButton.Small ? "Small" : size === ModernButton.Large ? "Large" : "Medium"].fontSize
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ========== 交互处理 ==========
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: !disabled && !loading
        hoverEnabled: true

        cursorShape: Qt.PointingHandCursor

        onClicked: root.clicked()
        onPressed: root.pressed()
        onReleased: root.released()
        onCanceled: root.canceled()
    }

    // ========== 信号 ==========
    signal clicked()
    signal pressed()
    signal released()
    signal canceled()

    // ========== 特效动画 ==========
    SequentialAnimation {
        id: successAnimation
        running: false

        PropertyAnimation {
            target: root;
            property: "scale";
            to: 0.95;
            duration: 100
        }
        PropertyAnimation {
            target: root;
            property: "scale";
            to: 1.0;
            duration: 100
        }
    }

    function success() {
        successAnimation.start()
    }

    // 键盘导航支持
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) {
            if (!disabled && !loading) {
                root.clicked()
                event.accepted = true
            }
        }
    }

    // 焦点样式
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: theme.primary
        border.width: 2
        radius: parent.radius
        visible: root.activeFocus
    }
}