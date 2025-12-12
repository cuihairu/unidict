import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化历史记录页面
Item {
    id: root

    // ========== 公共属性 ==========
    property alias historyModel: historyList.model
    property alias lookupAdapter: internal.lookupAdapter

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
        property var selectedHistory: null
        property bool isSelectionMode: false
        property var selectedItems: []
    }

    // ========== 页面布局 ==========
    Column {
        anchors.fill: parent
        spacing: theme.spacingMD

        // 页面头部
        Rectangle {
            width: parent.width
            height: 60
            color: "transparent"

            Row {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: theme.spacingMD
                anchors.rightMargin: theme.spacingMD
                spacing: theme.spacingMD

                // 标题区域
                Row {
                    spacing: theme.spacingSM

                    Text {
                        text: "🕘"
                        font.pixelSize: 18
                        color: theme.primary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        spacing: 2
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: "搜索历史"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 18
                            font.weight: Font.SemiBold
                            color: theme.textPrimary
                        }

                        Text {
                            text: `${historyList.count || 0} 条记录`
                            font.family: "SF Pro Display"
                            font.pixelSize: 12
                            color: theme.textSecondary
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // 操作按钮组
                Row {
                    spacing: theme.spacingSM

                    // 选择模式切换
                    ModernButton {
                        id: selectModeButton
                        text: internal.isSelectionMode ? "取消" : "选择"
                        buttonType: internal.isSelectionMode ? ModernButton.Secondary : ModernButton.Outline
                        size: ModernButton.Small

                        onClicked: {
                            internal.isSelectionMode = !internal.isSelectionMode
                            internal.selectedItems = []
                            selectionOverlay.clearSelection()
                        }
                    }

                    // 清空历史
                    ModernButton {
                        id: clearButton
                        text: "清空"
                        buttonType: ModernButton.Outline
                        size: ModernButton.Small
                        visible: historyList.count > 0

                        onClicked: {
                            showClearConfirmDialog()
                        }
                    }

                    // 批量操作（选择模式下显示）
                    Row {
                        visible: internal.isSelectionMode
                        spacing: theme.spacingXS

                        ModernButton {
                            text: "删除"
                            buttonType: ModernButton.Secondary
                            size: ModernButton.Small
                            enabled: internal.selectedItems.length > 0

                            onClicked: {
                                showBatchDeleteConfirm()
                            }
                        }

                        ModernButton {
                            text: `已选 ${internal.selectedItems.length}`
                            buttonType: ModernButton.Primary
                            size: ModernButton.Small
                            enabled: internal.selectedItems.length > 0

                            onClicked: {
                                // 可以显示批量操作菜单
                            }
                        }
                    }
                }
            }
        }

        // 搜索栏（历史搜索）
        ModernCard {
            width: parent.width
            height: 80
            visible: historyList.count > 5

            ModernSearchBox {
                id: historySearch
                anchors.centerIn: parent
                width: parent.width - theme.spacingLG * 2
                size: ModernSearchBox.Medium
                placeholder: "搜索历史记录..."

                onSearchRequested: function(query) {
                    filterHistory(query.trim())
                }
            }
        }

        // 历史记录列表
        Rectangle {
            width: parent.width
            Layout.fillHeight: true
            color: "transparent"

            // 选择模式覆盖层
            Rectangle {
                id: selectionOverlay
                anchors.fill: parent
                color: "transparent"
                visible: internal.isSelectionMode

                function clearSelection() {
                    internal.selectedItems = []
                    for (var i = 0; i < historyList.count; i++) {
                        var item = historyList.itemAtIndex(i)
                        if (item && item.selected) {
                            item.selected = false
                        }
                    }
                }
            }

            // 历史列表
            ListView {
                id: historyList
                anchors.fill: parent
                anchors.margins: theme.spacingSM

                model: root.historyModel
                delegate: historyItemDelegate
                spacing: theme.spacingSM

                // 滚动条样式
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    visible: historyList.contentHeight > historyList.height

                    contentItem: Rectangle {
                        implicitWidth: 6
                        radius: 3
                        color: theme.textTertiary

                        Rectangle {
                            width: 6
                            height: parent.height
                            radius: 3
                            color: theme.primary
                            opacity: historyList.contentHeight > historyList.height ? 0.5 : 0
                        }
                    }
                }

                // 空状态
                Rectangle {
                    id: emptyHistoryState
                    anchors.centerIn: parent
                    width: Math.min(300, parent.width - theme.spacingLG * 2)
                    height: emptyHistoryContent.implicitHeight + theme.spacingXL * 2
                    color: "transparent"
                    visible: historyList.count === 0

                    Column {
                        id: emptyHistoryContent
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
                                text: "🕘"
                                font.pixelSize: 32
                                color: theme.textTertiary
                            }
                        }

                        // 空状态文本
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "暂无搜索历史"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            color: theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            text: "开始搜索后，这里会显示您的查询历史"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textTertiary
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            lineHeight: 1.4
                        }

                        // 快速搜索建议
                        Column {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: theme.spacingSM
                            visible: true

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "快速搜索"
                                font.family: "SF Pro Display"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: theme.textSecondary
                            }

                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: theme.spacingSM

                                Repeater {
                                    model: ["apple", "hello", "dictionary", "language"]

                                    Rectangle {
                                        width: quickSearchText.implicitWidth + 12
                                        height: 28
                                        radius: 14
                                        color: theme.primaryLight
                                        visible: quickSearchText.text !== ""

                                        Text {
                                            id: quickSearchText
                                            anchors.centerIn: parent
                                            text: modelData || ""
                                            font.family: "SF Pro Display"
                                            font.pixelSize: 12
                                            color: theme.primary
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                searchFromHistory(modelData)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 历史记录项委托 ==========
    Component {
        id: historyItemDelegate

        ModernCard {
            width: ListView.view.width
            height: internal.isSelectionMode ? 80 : 60
            padding: theme.spacingMD
            clickable: !internal.isSelectionMode

            // 选择状态样式
            property bool selected: internal.selectedItems.indexOf(index) !== -1

            backgroundColor: selected ? theme.primaryLight : (mouseArea.containsMouse ? theme.overlay : theme.cardBackground)
            borderWidth: selected ? 2 : 1
            borderColor: selected ? theme.primary : theme.border

            // 选择模式下显示勾选框
            Rectangle {
                id: checkBox
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 24
                radius: 12
                border.color: selected ? theme.primary : theme.border
                border.width: 2
                color: selected ? theme.primary : "transparent"

                visible: internal.isSelectionMode

                Text {
                    anchors.centerIn: parent
                    text: selected ? "✓" : ""
                    color: "#FFFFFF"
                    font.family: "SF Pro Display"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
            }

            // 内容区域
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: internal.isSelectionMode ? 36 + theme.spacingSM : theme.spacingMD
                anchors.rightMargin: internal.isSelectionMode ? 36 + theme.spacingSM : theme.spacingMD
                spacing: theme.spacingXS

                // 词条和搜索时间
                Row {
                    width: parent.width
                    spacing: theme.spacingSM

                    Text {
                        text: modelData || ""
                        font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        color: theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: getFormattedTime(index)
                        font.family: "SF Pro Display"
                        font.pixelSize: 12
                        color: theme.textTertiary
                    }
                }

                // 附加信息（如搜索结果数量、来源词典等）
                Text {
                    width: parent.width
                    text: getHistoryDetails(index)
                    font.family: "SF Pro Display"
                    font.pixelSize: 13
                    color: theme.textSecondary
                    maximumLineCount: 1
                    elide: Text.ElideRight
                    visible: text !== ""
                }
            }

            // 操作按钮（非选择模式下显示）
            Row {
                anchors.right: parent.right
                anchors.rightMargin: theme.spacingMD
                anchors.verticalCenter: parent.verticalCenter
                spacing: theme.spacingXS
                visible: !internal.isSelectionMode

                // 搜索按钮
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: theme.primaryLight

                    Text {
                        anchors.centerIn: parent
                        text: "🔍"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchFromHistory(modelData)
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
                            parent.isFavorite = !parent.isFavorite
                            toggleHistoryFavorite(modelData, parent.isFavorite)
                        }
                    }
                }

                // 更多操作
                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: theme.overlay

                    Text {
                        anchors.centerIn: parent
                        text: "⋮"
                        font.pixelSize: 12
                        color: theme.textSecondary
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            showHistoryContextMenu(modelData, index)
                        }
                    }
                }
            }

            // 交互处理
            MouseArea {
                id: mouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: internal.isSelectionMode ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: {
                    if (internal.isSelectionMode) {
                        toggleSelection(index)
                    }
                }
            }

            // 进入动画
            Component.onCompleted: {
                opacity = 0
                scale = 0.8
                enterAnim.delay = index * 50
                enterAnim.target = parent
                enterAnim.restart()
            }
        }
    }

    // ========== 功能方法 ==========
    function searchFromHistory(word) {
        if (word && word.trim()) {
            root.wordSelected(word)
        }
    }

    function toggleSelection(index) {
        var selectedIndex = internal.selectedItems.indexOf(index)
        if (selectedIndex === -1) {
            internal.selectedItems.push(index)
        } else {
            internal.selectedItems.splice(selectedIndex, 1)
        }

        // 更新UI
        var item = historyList.itemAtIndex(index)
        if (item) {
            item.selected = selectedIndex === -1
        }
    }

    function getFormattedTime(index) {
        // 简单的时间格式化，实际应用中应该记录真实时间戳
        var hours = Math.floor(index / 2) % 24
        var minutes = (index * 13) % 60
        return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}`
    }

    function getHistoryDetails(index) {
        // 可以扩展显示更多信息
        if (index % 3 === 0) {
            return "牛津高阶词典"
        } else if (index % 3 === 1) {
            return "朗文当代词典"
        } else {
            return "柯林斯词典"
        }
    }

    function filterHistory(query) {
        // 实现历史记录过滤功能
        console.log("Filtering history:", query)
    }

    function toggleHistoryFavorite(word, isFavorite) {
        // 实现历史记录收藏功能
        console.log("Toggle favorite:", word, isFavorite)
    }

    function showHistoryContextMenu(word, index) {
        // 显示历史记录右键菜单
        console.log("Show context menu:", word, index)
    }

    function showClearConfirmDialog() {
        // 显示清空历史确认对话框
        console.log("Show clear history dialog")
        // 这里应该实现一个对话框组件
    }

    function showBatchDeleteConfirm() {
        // 显示批量删除确认对话框
        console.log("Show batch delete dialog for items:", internal.selectedItems)
    }

    // ========== 动画效果 ==========
    PropertyAnimation {
        id: enterAnim
        property: "opacity"
        from: 0
        to: 1
        duration: 200
        easing.type: Easing.OutCubic

        onStarted: {
            if (target) {
                scaleAnim.target = target
                scaleAnim.from = 0.8
                scaleAnim.to = 1.0
                scaleAnim.start()
            }
        }
    }

    PropertyAnimation {
        id: scaleAnim
        property: "scale"
        duration: 200
        easing.type: Easing.OutCubic
    }

    // ========== 信号 ==========
    signal wordSelected(string word)
    signal favoriteToggled(string word, bool isFavorite)
    signal historyItemDeleted(int index)
    signal historyCleared()

    // ========== 键盘快捷键 ==========
    Keys.onPressed: function(event) {
        if (event.key === Qt.Key_Delete && internal.selectedItems.length > 0) {
            showBatchDeleteConfirm()
            event.accepted = true
        } else if (event.key === Qt.Key_A && event.modifiers & Qt.ControlModifier) {
            // 全选
            internal.selectedItems = []
            for (var i = 0; i < historyList.count; i++) {
                internal.selectedItems.push(i)
            }
            selectionOverlay.clearSelection() // 先清空再重新选择
            event.accepted = true
        }
    }
}