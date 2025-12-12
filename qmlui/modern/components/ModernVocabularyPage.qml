import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化词汇管理页面
Item {
    id: root

    // ========== 公共属性 ==========
    property alias vocabModel: vocabList.model
    property alias lookupAdapter: internal.lookupAdapter

    // 视图模式
    enum ViewMode {
        Grid,
        List,
        Card
    }

    property int viewMode: ModernVocabularyPage.Grid
    property string selectedCategory: "全部"
    property var categories: ["全部", "常用", "学术", "商务", "生活", "技术", "医学", "法律"]

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
        property var selectedItems: []
        property bool isSelectionMode: false
        property string searchQuery: ""
        property string sortBy: "name" // name, time, frequency
        property bool sortAscending: true
        property var filteredVocab: []
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

            Column {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                spacing: theme.spacingSM

                Row {
                    width: parent.width
                    spacing: theme.spacingMD

                    // 标题区域
                    Row {
                        spacing: theme.spacingSM

                        Text {
                            text: "📚"
                            font.pixelSize: 18
                            color: theme.primary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Column {
                            spacing: 1
                            anchors.verticalCenter: parent.verticalCenter

                            Text {
                                text: "我的词汇库"
                                font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                                font.pixelSize: 18
                                font.weight: Font.SemiBold
                                color: theme.textPrimary
                            }

                            Text {
                                text: `${vocabList.count || 0} 个词汇`
                                font.family: "SF Pro Display"
                                font.pixelSize: 12
                                color: theme.textSecondary
                            }
                        }
                    }

                    // 弹性空间
                    Item { Layout.fillWidth: true }

                    // 统计信息
                    Row {
                        spacing: theme.spacingMD
                        visible: !internal.isSelectionMode

                        Repeater {
                            model: [
                                { label: "已掌握", value: "45%", color: theme.success },
                                { label: "学习中", value: "30%", color: theme.warning },
                                { label: "待学习", value: "25%", color: theme.info }
                            ]

                            Column {
                                spacing: 2

                                Text {
                                    text: modelData.label
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 11
                                    color: theme.textSecondary
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Text {
                                    text: modelData.value
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.SemiBold
                                    color: modelData.color
                                    horizontalAlignment: Text.AlignHCenter
                                }
                            }
                        }
                    }

                    // 操作按钮组
                    Row {
                        spacing: theme.spacingSM

                        // 添加词汇
                        ModernButton {
                            text: "+ 添加"
                            buttonType: ModernButton.Primary
                            size: ModernButton.Small
                            visible: !internal.isSelectionMode

                            onClicked: {
                                showAddVocabDialog()
                            }
                        }

                        // 视图切换
                        Row {
                            spacing: 2
                            visible: !internal.isSelectionMode

                            ModernButton {
                                text: "⚏"
                                buttonType: viewMode === ModernVocabularyPage.List ? ModernButton.Primary : ModernButton.Ghost
                                size: ModernButton.Small

                                onClicked: viewMode = ModernVocabularyPage.List
                            }

                            ModernButton {
                                text: "⊞"
                                buttonType: viewMode === ModernVocabularyPage.Grid ? ModernButton.Primary : ModernButton.Ghost
                                size: ModernButton.Small

                                onClicked: viewMode = ModernVocabularyPage.Grid
                            }

                            ModernButton {
                                text: "⊟"
                                buttonType: viewMode === ModernVocabularyPage.Card ? ModernButton.Primary : ModernButton.Ghost
                                size: ModernButton.Small

                                onClicked: viewMode = ModernVocabularyPage.Card
                            }
                        }

                        // 排序和筛选
                        ModernButton {
                            text: "排序"
                            buttonType: ModernButton.Outline
                            size: ModernButton.Small
                            visible: !internal.isSelectionMode

                            onClicked: {
                                sortMenu.open()
                            }
                        }

                        // 选择模式
                        ModernButton {
                            text: internal.isSelectionMode ? "取消" : "选择"
                            buttonType: internal.isSelectionMode ? ModernButton.Secondary : ModernButton.Outline
                            size: ModernButton.Small

                            onClicked: {
                                internal.isSelectionMode = !internal.isSelectionMode
                                internal.selectedItems = []
                            }
                        }
                    }
                }
            }
        }

        // 分类标签和搜索
        ModernCard {
            width: parent.width
            height: 100

            Column {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                spacing: theme.spacingMD

                // 分类标签
                Row {
                    width: parent.width
                    spacing: theme.spacingSM

                    Text {
                        text: "分类："
                        font.family: "SF Pro Display"
                        font.pixelSize: 12
                        color: theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Repeater {
                        model: categories

                        Rectangle {
                            width: categoryText.implicitWidth + 12
                            height: 28
                            radius: 14
                            color: selectedCategory === modelData ? theme.primary : theme.overlay

                            Text {
                                id: categoryText
                                anchors.centerIn: parent
                                text: modelData || ""
                                font.family: "SF Pro Display"
                                font.pixelSize: 12
                                color: selectedCategory === modelData ? "#FFFFFF" : theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    selectedCategory = modelData
                                    filterVocabulary()
                                }
                            }
                        }
                    }
                }

                // 搜索框
                ModernSearchBox {
                    width: parent.width
                    size: ModernSearchBox.Medium
                    placeholder: "搜索词汇、释义或标签..."

                    onSearchRequested: function(query) {
                        internal.searchQuery = query.trim()
                        filterVocabulary()
                    }
                }
            }
        }

        // 词汇列表/网格
        Rectangle {
            width: parent.width
            Layout.fillHeight: true
            color: "transparent"

            // 列表视图
            ListView {
                id: vocabList
                anchors.fill: parent
                anchors.margins: theme.spacingSM
                visible: viewMode === ModernVocabularyPage.List

                model: root.vocabModel
                delegate: vocabListItemDelegate
                spacing: theme.spacingSM

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            // 网格视图
            GridView {
                id: vocabGrid
                anchors.fill: parent
                anchors.margins: theme.spacingSM
                visible: viewMode === ModernVocabularyPage.Grid

                cellWidth: 200
                cellHeight: 120

                model: root.vocabModel
                delegate: vocabGridItemDelegate
                spacing: theme.spacingSM
            }

            // 卡片视图
            ListView {
                id: vocabCards
                anchors.fill: parent
                anchors.margins: theme.spacingSM
                visible: viewMode === ModernVocabularyPage.Card

                model: root.vocabModel
                delegate: vocabCardDelegate
                spacing: theme.spacingMD

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
            }

            // 空状态
            Rectangle {
                id: emptyVocabState
                anchors.centerIn: parent
                width: Math.min(350, parent.width - theme.spacingXL * 2)
                height: emptyVocabContent.implicitHeight + theme.spacingXL * 2
                color: "transparent"
                visible: (vocabList.count === 0 && viewMode === ModernVocabularyPage.List) ||
                         (vocabGrid.count === 0 && viewMode === ModernVocabularyPage.Grid) ||
                         (vocabCards.count === 0 && viewMode === ModernVocabularyPage.Card)

                Column {
                    id: emptyVocabContent
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
                            text: "📚"
                            font.pixelSize: 32
                            color: theme.textTertiary
                        }
                    }

                    // 空状态文本
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "词汇库为空"
                        font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                        font.pixelSize: 16
                        font.weight: Font.Medium
                        color: theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: parent.width
                        text: "添加您想要学习的词汇，开始构建个人词汇库"
                        font.family: "SF Pro Display"
                        font.pixelSize: 14
                        color: theme.textTertiary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        lineHeight: 1.4
                    }

                    // 快速添加按钮
                    ModernButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "+ 添加第一个词汇"
                        buttonType: ModernButton.Primary

                        onClicked: {
                            showAddVocabDialog()
                        }
                    }
                }
            }
        }
    }

    // ========== 词汇列表项委托 ==========
    Component {
        id: vocabListItemDelegate

        ModernCard {
            width: ListView.view.width
            height: internal.isSelectionMode ? 80 : 70
            clickable: !internal.isSelectionMode

            // 选择状态
            property bool selected: internal.selectedItems.indexOf(index) !== -1
            backgroundColor: selected ? theme.primaryLight : theme.cardBackground
            borderWidth: selected ? 2 : 1
            borderColor: selected ? theme.primary : theme.border

            // 选择框
            Rectangle {
                id: listCheckBox
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

            // 词汇信息
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: internal.isSelectionMode ? 36 + theme.spacingSM : theme.spacingMD
                anchors.rightMargin: theme.spacingMD
                spacing: 2

                Text {
                    width: parent.width
                    text: modelData.word || ""
                    font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: theme.textPrimary
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: modelData.definition || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 13
                    color: theme.textSecondary
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }

                Row {
                    spacing: theme.spacingSM

                    // 学习状态
                    Rectangle {
                        width: statusText.implicitWidth + 6
                        height: 16
                        radius: 8
                        color: getStatusColor(modelData.status || "unknown")

                        Text {
                            id: statusText
                            anchors.centerIn: parent
                            text: getStatusText(modelData.status || "unknown")
                            font.family: "SF Pro Display"
                            font.pixelSize: 10
                            color: "#FFFFFF"
                        }
                    }

                    // 标签
                    Repeater {
                        model: modelData.tags || []
                        visible: modelData.tags && modelData.tags.length > 0

                        Rectangle {
                            width: tagText.implicitWidth + 6
                            height: 16
                            radius: 8
                            color: theme.textTertiary

                            Text {
                                id: tagText
                                anchors.centerIn: parent
                                text: modelData || ""
                                font.family: "SF Pro Display"
                                font.pixelSize: 10
                                color: "#FFFFFF"
                            }
                        }
                    }
                }
            }

            // 操作按钮
            Row {
                anchors.right: parent.right
                anchors.rightMargin: theme.spacingMD
                anchors.verticalCenter: parent.verticalCenter
                spacing: theme.spacingXS
                visible: !internal.isSelectionMode

                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: theme.primaryLight

                    Text {
                        anchors.centerIn: parent
                        text: "🔊"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            speakVocab(modelData.word || "")
                        }
                    }
                }

                Rectangle {
                    width: 28
                    height: 28
                    radius: 14
                    color: isFavorite ? theme.error : theme.overlay

                    property bool isFavorite: modelData.favorite || false

                    Text {
                        anchors.centerIn: parent
                        text: isFavorite ? "❤️" : "🤍"
                        font.pixelSize: 12
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            toggleFavorite(modelData.word || "", !parent.isFavorite)
                        }
                    }
                }
            }

            // 交互
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: internal.isSelectionMode ? Qt.PointingHandCursor : Qt.ArrowCursor

                onClicked: {
                    if (internal.isSelectionMode) {
                        toggleSelection(index)
                    } else {
                        showVocabDetails(modelData)
                    }
                }
            }
        }
    }

    // ========== 词汇网格项委托 ==========
    Component {
        id: vocabGridItemDelegate

        ModernCard {
            width: 180
            height: 100
            clickable: !internal.isSelectionMode

            // 选择状态
            property bool selected: internal.selectedItems.indexOf(index) !== -1
            backgroundColor: selected ? theme.primaryLight : theme.cardBackground
            borderWidth: selected ? 2 : 1
            borderColor: selected ? theme.primary : theme.border

            // 选择框
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.top
                anchors.margins: theme.spacingSM
                width: 20
                height: 20
                radius: 10
                border.color: selected ? theme.primary : theme.border
                border.width: 2
                color: selected ? theme.primary : "transparent"
                visible: internal.isSelectionMode

                Text {
                    anchors.centerIn: parent
                    text: selected ? "✓" : ""
                    color: "#FFFFFF"
                    font.family: "SF Pro Display"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }

            Column {
                anchors.fill: parent
                anchors.margins: theme.spacingSM
                spacing: theme.spacingXS

                Text {
                    width: parent.width
                    text: modelData.word || ""
                    font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: theme.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    text: modelData.definition || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 11
                    color: theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    wrapMode: Text.WordWrap
                }

                // 状态标签
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: statusText.implicitWidth + 6
                    height: 16
                    radius: 8
                    color: getStatusColor(modelData.status || "unknown")

                    Text {
                        id: statusText
                        anchors.centerIn: parent
                        text: getStatusText(modelData.status || "unknown")
                        font.family: "SF Pro Display"
                        font.pixelSize: 9
                        color: "#FFFFFF"
                    }
                }
            }

            // 交互
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    if (internal.isSelectionMode) {
                        toggleSelection(index)
                    } else {
                        showVocabDetails(modelData)
                    }
                }
            }
        }
    }

    // ========== 词汇卡片委托 ==========
    Component {
        id: vocabCardDelegate

        ModernCard {
            width: ListView.view.width
            height: 150
            clickable: !internal.isSelectionMode

            // 选择状态
            property bool selected: internal.selectedItems.indexOf(index) !== -1
            backgroundColor: selected ? theme.primaryLight : theme.cardBackground
            borderWidth: selected ? 2 : 1
            borderColor: selected ? theme.primary : theme.border

            Column {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                spacing: theme.spacingMD

                // 标题行
                Row {
                    width: parent.width
                    spacing: theme.spacingSM

                    Text {
                        text: modelData.word || ""
                        font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                        font.pixelSize: 18
                        font.weight: Font.SemiBold
                        color: theme.textPrimary
                        Layout.fillWidth: true
                    }

                    // 状态标签
                    Rectangle {
                        width: statusText.implicitWidth + 8
                        height: 20
                        radius: 10
                        color: getStatusColor(modelData.status || "unknown")

                        Text {
                            id: statusText
                            anchors.centerIn: parent
                            text: getStatusText(modelData.status || "unknown")
                            font.family: "SF Pro Display"
                            font.pixelSize: 11
                            color: "#FFFFFF"
                        }
                    }
                }

                // 释义
                Text {
                    width: parent.width
                    text: modelData.definition || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 14
                    color: theme.textPrimary
                    wrapMode: Text.WordWrap
                    lineHeight: 1.4
                }

                // 标签和操作
                Row {
                    width: parent.width
                    spacing: theme.spacingMD

                    // 标签
                    Row {
                        spacing: theme.spacingXS
                        Layout.fillWidth: true

                        Repeater {
                            model: modelData.tags || []
                            visible: modelData.tags && modelData.tags.length > 0

                            Rectangle {
                                width: tagText.implicitWidth + 8
                                height: 20
                                radius: 10
                                color: theme.textTertiary

                                Text {
                                    id: tagText
                                    anchors.centerIn: parent
                                    text: modelData || ""
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 10
                                    color: "#FFFFFF"
                                }
                            }
                        }
                    }

                    // 操作按钮
                    Row {
                        spacing: theme.spacingXS
                        visible: !internal.isSelectionMode

                        Rectangle {
                            width: 32
                            height: 32
                            radius: 16
                            color: theme.primaryLight

                            Text {
                                anchors.centerIn: parent
                                text: "🔊"
                                font.pixelSize: 14
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    speakVocab(modelData.word || "")
                                }
                            }
                        }

                        Rectangle {
                            width: 32
                            height: 32
                            radius: 16
                            color: isFavorite ? theme.error : theme.overlay

                            property bool isFavorite: modelData.favorite || false

                            Text {
                                anchors.centerIn: parent
                                text: isFavorite ? "❤️" : "🤍"
                                font.pixelSize: 14
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    toggleFavorite(modelData.word || "", !parent.isFavorite)
                                }
                            }
                        }
                    }
                }
            }

            // 交互
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true

                onClicked: {
                    if (internal.isSelectionMode) {
                        toggleSelection(index)
                    } else {
                        showVocabDetails(modelData)
                    }
                }
            }
        }
    }

    // ========== 功能方法 ==========
    function getStatusColor(status) {
        switch(status) {
            case "mastered": return theme.success
            case "learning": return theme.warning
            case "new": return theme.info
            default: return theme.textTertiary
        }
    }

    function getStatusText(status) {
        switch(status) {
            case "mastered": return "已掌握"
            case "learning": return "学习中"
            case "new": return "待学习"
            default: return "未知"
        }
    }

    function toggleSelection(index) {
        var selectedIndex = internal.selectedItems.indexOf(index)
        if (selectedIndex === -1) {
            internal.selectedItems.push(index)
        } else {
            internal.selectedItems.splice(selectedIndex, 1)
        }
    }

    function filterVocabulary() {
        // 实现词汇筛选逻辑
        console.log("Filter vocabulary by category:", selectedCategory, "search:", internal.searchQuery)
    }

    function speakVocab(word) {
        console.log("Speak:", word)
        // 调用朗读功能
    }

    function toggleFavorite(word, isFavorite) {
        console.log("Toggle favorite:", word, isFavorite)
        // 调用收藏功能
    }

    function showVocabDetails(vocab) {
        console.log("Show details:", vocab)
        // 显示词汇详情
    }

    function showAddVocabDialog() {
        console.log("Show add vocab dialog")
        // 显示添加词汇对话框
    }

    // ========== 排序菜单 ==========
    Menu {
        id: sortMenu

        MenuItem {
            text: "按名称排序"
            checked: internal.sortBy === "name"
            onTriggered: {
                internal.sortBy = "name"
                filterVocabulary()
            }
        }

        MenuItem {
            text: "按添加时间排序"
            checked: internal.sortBy === "time"
            onTriggered: {
                internal.sortBy = "time"
                filterVocabulary()
            }
        }

        MenuItem {
            text: "按学习频率排序"
            checked: internal.sortBy === "frequency"
            onTriggered: {
                internal.sortBy = "frequency"
                filterVocabulary()
            }
        }

        MenuSeparator {}

        MenuItem {
            text: internal.sortAscending ? "降序排列" : "升序排列"
            onTriggered: {
                internal.sortAscending = !internal.sortAscending
                filterVocabulary()
            }
        }
    }

    // ========== 信号 ==========
    signal vocabAdded(string word, string definition)
    signal vocabDeleted(string word)
    signal favoriteToggled(string word, bool isFavorite)
    signal statusChanged(string word, string newStatus)
}