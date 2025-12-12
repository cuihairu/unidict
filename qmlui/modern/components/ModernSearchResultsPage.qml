import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化搜索结果页面
Item {
    id: root

    // ========== 公共属性 ==========
    property alias lookupAdapter: internal.lookupAdapter
    property var searchResults: []

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
    }

    // ========== 页面布局 ==========
    Column {
        anchors.fill: parent
        spacing: theme.spacingMD

        // 页面标题区域
        Rectangle {
            width: parent.width
            height: 40
            color: "transparent"

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: theme.spacingSM

                Text {
                    text: "🔍"
                    font.pixelSize: 18
                    color: theme.primary
                }

                Text {
                    text: "搜索结果"
                    font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                    font.pixelSize: 18
                    font.weight: Font.SemiBold
                    color: theme.textPrimary
                }

                // 结果计数
                Text {
                    visible: searchResults.length > 0
                    text: `(${searchResults.length})`
                    font.family: "SF Pro Display"
                    font.pixelSize: 14
                    color: theme.textSecondary
                }
            }
        }

        // 搜索结果列表
        Rectangle {
            width: parent.width
            Layout.fillHeight: true
            color: "transparent"

            // 结果列表
            ListView {
                id: resultsList
                anchors.fill: parent
                anchors.margins: theme.spacingSM

                model: searchResults
                delegate: searchResultDelegate
                spacing: theme.spacingSM

                // 空状态
                Rectangle {
                    id: emptyState
                    anchors.centerIn: parent
                    width: Math.min(300, parent.width - theme.spacingLG * 2)
                    height: emptyContent.implicitHeight + theme.spacingXL * 2
                    color: "transparent"
                    visible: searchResults.length === 0

                    Column {
                        id: emptyContent
                        anchors.centerIn: parent
                        spacing: theme.spacingMD
                        width: parent.width

                        // 空状态图标
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 80
                            height: 80
                            radius: 40
                            color: theme.overlay

                            Text {
                                anchors.centerIn: parent
                                text: "🔍"
                                font.pixelSize: 32
                                color: theme.textTertiary
                            }
                        }

                        // 空状态文本
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "暂无搜索结果"
                            font.family: "SF Pro Display"
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            color: theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            text: "请输入要查询的词条，或检查拼写是否正确"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textTertiary
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        // 建议按钮
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: theme.spacingSM

                            Rectangle {
                                width: 120
                                height: 36
                                radius: theme.radiusSmall
                                color: theme.primaryLight
                                visible: false // 暂时隐藏建议功能

                                Text {
                                    anchors.centerIn: parent
                                    text: "建议搜索"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.primary
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // 实现建议搜索逻辑
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 搜索结果委托 ==========
    Component {
        id: searchResultDelegate

        Rectangle {
            id: resultItem
            width: ListView.view.width
            height: Math.max(60, contentColumn.implicitHeight + theme.spacingMD * 2)
            color: "transparent"

            // 卡片背景
            Rectangle {
                anchors.fill: parent
                color: theme.cardBackground
                radius: theme.radiusMedium
                border.color: theme.borderLight
                border.width: 1

                // 阴影效果
                layer.enabled: true
                layer.effect: DropShadow {
                    horizontalOffset: 0
                    verticalOffset: 2
                    radius: 4
                    samples: 8
                    color: "rgba(0, 0, 0, 0.05)"
                }

                // 悬停效果
                scale: mouseArea.containsMouse ? 1.01 : 1.0
                color: mouseArea.containsMouse ? theme.background : theme.cardBackground

                Behavior on scale {
                    NumberAnimation {
                        duration: theme.animationFast
                        easing.type: Easing.OutCubic
                    }
                }

                Behavior on color {
                    ColorAnimation {
                        duration: theme.animationFast
                        easing.type: Easing.OutCubic
                    }
                }
            }

            // 内容区域
            Column {
                id: contentColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: theme.spacingMD
                spacing: theme.spacingXS

                // 词条标题
                Text {
                    id: wordText
                    width: parent.width
                    text: model.word || modelData.word || ""
                    font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                    font.pixelSize: 16
                    font.weight: Font.SemiBold
                    color: theme.primary

                    // 音标或其他信息
                    Text {
                        anchors.left: parent.right
                        anchors.leftMargin: theme.spacingSM
                        anchors.baseline: parent.baseline
                        text: model.phonetic || modelData.phonetic || ""
                        font.family: "SF Pro Display"
                        font.pixelSize: 12
                        color: theme.textSecondary
                        visible: text !== ""
                    }
                }

                // 释义内容
                Text {
                    id: definitionText
                    width: parent.width
                    text: model.definition || modelData.definition || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 14
                    color: theme.textPrimary
                    wrapMode: Text.WordWrap
                    maximumLineCount: 3
                    elide: Text.ElideRight
                    lineHeight: 1.4

                    // 展开更多指示
                    Text {
                        anchors.left: parent.right
                        anchors.leftMargin: theme.spacingXS
                        anchors.baseline: parent.baseline
                        text: parent.truncated ? "..." : ""
                        font.family: "SF Pro Display"
                        font.pixelSize: 14
                        color: theme.textSecondary
                        visible: parent.truncated
                    }
                }

                // 附加信息（词性、例句等）
                Text {
                    id: additionalInfo
                    width: parent.width
                    text: model.type || modelData.type || model.example || modelData.example || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 12
                    color: theme.textSecondary
                    wrapMode: Text.WordWrap
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    visible: text !== ""
                }

                // 操作按钮
                Row {
                    spacing: theme.spacingSM
                    visible: wordText.text !== ""

                    // 朗读按钮
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: theme.primaryLight
                        visible: true

                        Text {
                            anchors.centerIn: parent
                            text: "🔊"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                speakWord(wordText.text)
                            }
                        }
                    }

                    // 收藏按钮
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: isFavorite ? theme.error : theme.overlay

                        property bool isFavorite: false

                        Text {
                            anchors.centerIn: parent
                            text: isFavorite ? "❤️" : "🤍"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                toggleFavorite(wordText.text)
                                parent.isFavorite = !parent.isFavorite
                            }
                        }
                    }

                    // 分享按钮
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: theme.overlay

                        Text {
                            anchors.centerIn: parent
                            text: "📤"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                shareResult(wordText.text, definitionText.text)
                            }
                        }
                    }
                }
            }

            // 交互区域
            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    selectWord(model.word || modelData.word, model.definition || modelData.definition)
                }
            }
        }
    }

    // ========== 公共方法 ==========
    function updateResults(results) {
        searchResults = results || []

        // 添加列表项进入动画
        resultsList.model = searchResults

        // 延迟触发进入动画
        animTimer.restart()
    }

    function showNoResults(query) {
        searchResults = []

        // 自定义空状态文本
        emptyState.visible = true

        // 更新空状态文本
        console.log("No results for:", query)
    }

    function selectWord(word, definition) {
        if (word) {
            root.wordSelected(word, definition)
        }
    }

    function speakWord(word) {
        // 调用朗读功能
        if (internal.lookupAdapter && internal.lookupAdapter.speak) {
            internal.lookupAdapter.speak(word)
        } else {
            console.log("Speaking:", word)
        }
    }

    function toggleFavorite(word) {
        // 调用收藏功能
        if (internal.lookupAdapter && internal.lookupAdapter.toggleFavorite) {
            internal.lookupAdapter.toggleFavorite(word)
        } else {
            console.log("Toggle favorite:", word)
        }
    }

    function shareResult(word, definition) {
        // 调用分享功能
        if (internal.lookupAdapter && internal.lookupAdapter.share) {
            internal.lookupAdapter.share(word, definition)
        } else {
            console.log("Share:", word, definition)
        }
    }

    // ========== 动画计时器 ==========
    Timer {
        id: animTimer
        interval: 100
        onTriggered: {
            // 为列表项添加进入动画
            for (var i = 0; i < resultsList.count; i++) {
                var item = resultsList.itemAtIndex(i)
                if (item) {
                    // 错开显示时间
                    showAnim.delay = i * 50
                    showAnim.target = item
                    showAnim.restart()
                }
            }
        }
    }

    PropertyAnimation {
        id: showAnim
        property: "opacity"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic

        property int delay: 0

        onStarted: {
            if (target) {
                timer.delay = delay
                timer.restart()
            }
        }
    }

    Timer {
        id: timer
        property int delay: 0
        interval: delay
        onTriggered: showAnim.start()
    }

    // ========== 信号 ==========
    signal wordSelected(string word, string definition)
}