import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化搜索框组件
Rectangle {
    id: root

    // ========== 公共属性 ==========
    property alias text: searchInput.text
    property alias placeholder: searchInput.placeholderText
    property alias font: searchInput.font

    property int size: ModernSearchBox.Medium
    enum SearchSize {
        Small,
        Medium,
        Large
    }

    property bool loading: false
    property bool focused: searchInput.activeFocus

    // 搜索配置
    property int searchDelay: 300
    property var searchCallback: null

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 尺寸配置 ==========
    readonly property var sizeConfig: {
        "Small": {
            "height": 36,
            "fontSize": 12,
            "iconSize": 16,
            "borderRadius": 8,
            "padding": 8
        },
        "Medium": {
            "height": 44,
            "fontSize": 14,
            "iconSize": 18,
            "borderRadius": 10,
            "padding": 12
        },
        "Large": {
            "height": 52,
            "fontSize": 16,
            "iconSize": 20,
            "borderRadius": 12,
            "padding": 16
        }
    }

    // ========== 基础样式 ==========
    color: theme.surface
    border.color: focused ? theme.primary : theme.border
    border.width: focused ? 2 : 1
    radius: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].borderRadius

    // 尺寸
    height: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].height

    // 阴影效果
    layer.enabled: focused
    layer.effect: DropShadow {
        horizontalOffset: 0
        verticalOffset: 4
        radius: 8
        samples: 16
        color: "rgba(99, 102, 241, 0.15)"
        spread: 0.0
    }

    // ========== 动画效果 ==========
    Behavior on border.color {
        ColorAnimation {
            duration: theme.animationFast;
            easing.type: Easing.OutCubic
        }
    }

    Behavior on color {
        ColorAnimation {
            duration: theme.animationFast;
            easing.type: Easing.OutCubic
        }
    }

    // ========== 内容布局 ==========
    Row {
        anchors.fill: parent
        anchors.leftMargin: theme.spacingSM
        anchors.rightMargin: theme.spacingSM
        spacing: theme.spacingXS

        // 搜索图标
        Image {
            id: searchIcon
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/icons/search.svg"
            sourceSize.width: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].iconSize
            sourceSize.height: sourceSize.width
            fillMode: Image.PreserveAspectFit

            layer.enabled: true
            layer.effect: ColorOverlay {
                color: theme.textTertiary
            }
        }

        // 文本输入区域
        TextInput {
            id: searchInput
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - searchIcon.width - clearButton.width - loadingIndicator.width - parent.spacing * 3

            font.family: "SF Pro Display"
            font.pixelSize: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].fontSize
            color: theme.textPrimary

            placeholderText: root.placeholder || "搜索词条..."
            placeholderTextColor: theme.textTertiary

            selectByMouse: true
            selectionColor: theme.primaryLight
            selectedTextColor: "#FFFFFF"

            clip: true

            // 搜索防抖
            onTextChanged: {
                searchTimer.restart()
            }

            Keys.onPressed: function(event) {
                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    performSearch()
                    event.accepted = true
                } else if (event.key === Qt.Key_Escape) {
                    clearSearch()
                    event.accepted = true
                }
            }
        }

        // 加载指示器
        BusyIndicator {
            id: loadingIndicator
            anchors.verticalCenter: parent.verticalCenter
            visible: loading
            running: loading
            implicitWidth: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].iconSize
            implicitHeight: implicitWidth

            Material.accent: theme.primary
        }

        // 清除按钮
        Rectangle {
            id: clearButton
            anchors.verticalCenter: parent.verticalCenter
            width: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].iconSize + 4
            height: width
            radius: width / 2
            color: theme.textTertiary
            visible: searchInput.text && !loading

            // 悬停效果
            scale: mouseArea.containsMouse ? 1.1 : 1.0
            opacity: mouseArea.containsMouse ? 0.8 : 1.0

            Behavior on scale {
                NumberAnimation {
                    duration: theme.animationFast;
                    easing.type: Easing.OutCubic
                }
            }

            Behavior on opacity {
                NumberAnimation {
                    duration: theme.animationFast;
                    easing.type: Easing.OutCubic
                }
            }

            // 清除图标
            Label {
                anchors.centerIn: parent
                text: "×"
                color: theme.surface
                font.family: "SF Pro Display"
                font.pixelSize: sizeConfig[size === ModernSearchBox.Small ? "Small" : size === ModernSearchBox.Large ? "Large" : "Medium"].fontSize + 2
                font.bold: true
            }

            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: clearSearch()
            }
        }
    }

    // ========== 搜索计时器 ==========
    Timer {
        id: searchTimer
        interval: searchDelay
        onTriggered: performSearch()
    }

    // ========== 公共方法 ==========
    function performSearch() {
        if (searchCallback && typeof searchCallback === 'function') {
            searchCallback(searchInput.text)
        }
        root.searchRequested(searchInput.text)
    }

    function clearSearch() {
        searchInput.text = ""
        searchInput.forceActiveFocus()
    }

    function focus() {
        searchInput.forceActiveFocus()
    }

    // ========== 信号 ==========
    signal searchRequested(string query)
    signal searchCleared()

    // ========== 特效动画 ==========
    SequentialAnimation {
        id: shakeAnimation
        running: false

        PropertyAnimation { target: root; property: "x"; to: -4; duration: 50 }
        PropertyAnimation { target: root; property: "x"; to: 4; duration: 50 }
        PropertyAnimation { target: root; property: "x"; to: -4; duration: 50 }
        PropertyAnimation { target: root; property: "x"; to: 4; duration: 50 }
        PropertyAnimation { target: root; property: "x"; to: 0; duration: 50 }
    }

    function shake() {
        shakeAnimation.start()
    }

    // 焦点样式
    Keys.onTabPressed: {
        // 切换焦点到下一个控件
        nextItemInFocusChain().forceActiveFocus()
    }
}