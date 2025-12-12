import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects
import "modern"
import "modern/components"
import "mobile/common"

// 现代化词典主界面
ApplicationWindow {
    id: win
    visible: true

    // 响应式窗口尺寸
    width: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.width : 1200
    height: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.height : 800

    title: "Unidict - 现代化词典"
    minimumWidth: 800
    minimumHeight: 600

    // ========== 主题配置 ==========
    Theme { id: theme }

    Material.theme: Material.Light
    Material.background: theme.background
    Material.primary: theme.primary
    Material.accent: theme.accent

    // ========== 状态管理 ==========
    property string currentWord: ""
    property string currentDefinition: ""
    property int currentPage: 0
    property string searchText: ""

    // 搜索历史
    property var searchHistory: []
    property var favoriteWords: []

    // ========== 响应式布局 ==========
    ResponsiveLayout {
        id: responsive
        anchors.fill: parent
    }

    // ========== 主背景 ==========
    Rectangle {
        id: mainBackground
        anchors.fill: parent
        color: theme.background

        // 背景装饰渐变
        Rectangle {
            id: backgroundGradient
            anchors.fill: parent
            color: "transparent"
            visible: !responsive.isMobile

            gradient: Gradient {
                GradientStop { position: 0.0; color: "rgba(99, 102, 241, 0.05)" }
                GradientStop { position: 0.5; color: "transparent" }
                GradientStop { position: 1.0; color: "rgba(139, 92, 246, 0.05)" }
            }
        }
    }

    // ========== 主容器 ==========
    Item {
        id: mainContainer
        anchors.fill: parent
        anchors.margins: responsive.baseMargin

        // 顶部导航栏
        Rectangle {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: responsive.safeAreaTop

            height: responsive.isMobile ? 64 : 80
            color: "transparent"

            // 导航栏内容
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: theme.spacingMD
                anchors.rightMargin: theme.spacingMD
                spacing: theme.spacingMD

                // Logo 和品牌
                Row {
                    spacing: theme.spacingSM

                    Rectangle {
                        id: logoIcon
                        anchors.verticalCenter: parent.verticalCenter
                        width: responsive.isMobile ? 40 : 48
                        height: width
                        radius: width / 2
                        color: theme.primary

                        Text {
                            anchors.centerIn: parent
                            text: "📚"
                            font.pixelSize: responsive.isMobile ? 20 : 24
                            color: "#FFFFFF"
                        }

                        // Logo 动画
                        SequentialAnimation {
                            running: true
                            loops: Animation.Infinite

                            PropertyAnimation {
                                target: logoIcon
                                property: "rotation"
                                to: 5
                                duration: 2000
                            }
                            PropertyAnimation {
                                target: logoIcon
                                property: "rotation"
                                to: -5
                                duration: 2000
                            }
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -theme.spacingXS

                        Label {
                            text: "Unidict"
                            font.family: "SF Pro Display"
                            font.pixelSize: responsive.isMobile ? 20 : 24
                            font.weight: Font.Bold
                            color: theme.textPrimary
                        }

                        Label {
                            text: "现代化词典"
                            font.family: "SF Pro Display"
                            font.pixelSize: responsive.isMobile ? 12 : 14
                            color: theme.textSecondary
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // 功能按钮组
                Row {
                    spacing: theme.spacingSM
                    visible: !responsive.isMobile

                    ModernButton {
                        id: syncButton
                        text: "🔄 同步"
                        size: ModernButton.Small
                        buttonType: ModernButton.Ghost

                        onClicked: {
                            showNotification("同步功能开发中...")
                        }
                    }

                    ModernButton {
                        id: settingsButton
                        text: "⚙️ 设置"
                        size: ModernButton.Small
                        buttonType: ModernButton.Ghost

                        onClicked: {
                            win.navigateTo(4)
                        }
                    }
                }

                // 移动端菜单按钮
                ModernButton {
                    id: mobileMenuButton
                    visible: responsive.isMobile
                    iconSource: "qrc:/icons/menu.svg"
                    buttonType: ModernButton.Icon
                    size: ModernButton.Medium

                    onClicked: navDrawer.open()
                }
            }
        }

        // 搜索区域
        ModernCard {
            id: searchCard
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.topMargin: theme.spacingLG

            height: responsive.isMobile ? 100 : 120

            // 搜索框
            ModernSearchBox {
                id: searchBox
                anchors.centerIn: parent
                width: parent.width - theme.spacingXL * 2
                size: responsive.isMobile ? ModernSearchBox.Medium : ModernSearchBox.Large

                placeholder: "输入要查询的词条..."

                onSearchRequested: function(query) {
                    if (query.trim()) {
                        performSearch(query.trim())
                    }
                }

                Component.onCompleted: {
                    forceActiveFocus()
                }
            }
        }

        // 主要内容区域
        Item {
            id: contentArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: searchCard.bottom
            anchors.bottom: parent.bottom
            anchors.bottomMargin: responsive.safeAreaBottom
            anchors.topMargin: theme.spacingLG

            // 桌面端布局
            RowLayout {
                anchors.fill: parent
                visible: !responsive.isMobile
                spacing: theme.spacingMD

                // 左侧面板 - 搜索结果
                ModernCard {
                    id: resultsPanel
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    title: "搜索结果"
                    subtitle: currentWord ? "当前: " + currentWord : "输入词条开始查询"

                    // 搜索结果列表
                    ListView {
                        id: resultsList
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        model: searchResults
                        delegate: searchResultDelegate
                        spacing: theme.spacingSM

                        // 空状态
                        Label {
                            anchors.centerIn: parent
                            visible: resultsList.count === 0
                            text: "暂无搜索结果\n输入词条开始查询"
                            font.family: "SF Pro Display"
                            font.pixelSize: 16
                            color: theme.textTertiary
                            horizontalAlignment: Text.AlignHCenter
                            lineHeight: 1.5
                        }
                    }
                }

                // 右侧面板 - 详细信息和功能
                ColumnLayout {
                    Layout.preferredWidth: 350
                    Layout.fillHeight: true
                    spacing: theme.spacingMD

                    // 词汇详情卡片
                    ModernCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 300

                        title: "词汇详情"
                        visible: currentDefinition

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: theme.spacingMD
                            anchors.topMargin: theme.spacingXL

                            Text {
                                width: parent.width
                                text: currentDefinition
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                color: theme.textPrimary
                                wrapMode: Text.WordWrap
                                lineHeight: 1.6
                            }
                        }
                    }

                    // 功能按钮卡片
                    ModernCard {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        title: "工具箱"

                        Column {
                            anchors.fill: parent
                            anchors.margins: theme.spacingMD
                            anchors.topMargin: theme.spacingXL
                            spacing: theme.spacingMD

                            ModernButton {
                                width: parent.width
                                text: "🔊 朗读"
                                buttonType: ModernButton.Secondary

                                onClicked: {
                                    if (currentWord) {
                                        showNotification("朗读功能: " + currentWord)
                                    } else {
                                        showNotification("请先选择一个词汇")
                                    }
                                }
                            }

                            ModernButton {
                                width: parent.width
                                text: "❤️ 收藏"
                                buttonType: ModernButton.Outline

                                onClicked: {
                                    if (currentWord) {
                                        addToFavorites(currentWord)
                                        showNotification("已收藏: " + currentWord)
                                    } else {
                                        showNotification("请先选择一个词汇")
                                    }
                                }
                            }

                            ModernButton {
                                width: parent.width
                                text: "📝 笔记"
                                buttonType: ModernButton.Outline

                                onClicked: {
                                    if (currentWord) {
                                        showNotification("笔记功能开发中...")
                                    } else {
                                        showNotification("请先选择一个词汇")
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 移动端布局
            StackLayout {
                anchors.fill: parent
                visible: responsive.isMobile
                currentIndex: currentPage

                // 搜索结果页
                ModernCard {
                    title: "搜索结果"

                    ListView {
                        id: mobileResultsList
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        model: searchResults
                        delegate: searchResultDelegate
                        spacing: theme.spacingSM

                        Label {
                            anchors.centerIn: parent
                            visible: mobileResultsList.count === 0
                            text: "暂无搜索结果"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textTertiary
                        }
                    }
                }

                // 词汇详情页
                ModernCard {
                    title: currentWord || "词汇详情"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        Text {
                            width: parent.width
                            text: currentDefinition || "选择一个词汇查看详情"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textPrimary
                            wrapMode: Text.WordWrap
                            lineHeight: 1.6
                        }
                    }
                }

                // 历史记录页
                ModernCard {
                    title: "搜索历史"

                    ListView {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        model: searchHistory
                        delegate: historyDelegate
                        spacing: theme.spacingSM

                        Label {
                            anchors.centerIn: parent
                            visible: searchHistory.length === 0
                            text: "暂无搜索历史"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textTertiary
                        }
                    }
                }

                // 收藏夹页
                ModernCard {
                    title: "我的收藏"

                    ListView {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        model: favoriteWords
                        delegate: favoriteDelegate
                        spacing: theme.spacingSM

                        Label {
                            anchors.centerIn: parent
                            visible: favoriteWords.length === 0
                            text: "暂无收藏词汇"
                            font.family: "SF Pro Display"
                            font.pixelSize: 14
                            color: theme.textTertiary
                        }
                    }
                }
            }
        }
    }

    // ========== 侧边导航栏（移动端） ==========
    Drawer {
        id: navDrawer
        width: Math.min(300, win.width * 0.8)
        edge: Qt.LeftEdge
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Rectangle {
            anchors.fill: parent
            color: theme.surface

            Column {
                width: parent.width
                spacing: theme.spacingMD
                padding: theme.spacingMD

                // 用户头像区域
                Row {
                    spacing: theme.spacingMD
                    padding: theme.spacingMD

                    Rectangle {
                        width: 48
                        height: 48
                        radius: 24
                        color: theme.primary

                        Text {
                            anchors.centerIn: parent
                            text: "👤"
                            font.pixelSize: 20
                            color: "#FFFFFF"
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -theme.spacingXS

                        Label {
                            text: "用户"
                            font.family: "SF Pro Display"
                            font.pixelSize: 16
                            font.weight: Font.Medium
                            color: theme.textPrimary
                        }

                        Label {
                            text: "专业版用户"
                            font.family: "SF Pro Display"
                            font.pixelSize: 12
                            color: theme.textSecondary
                        }
                    }
                }

                // 导航菜单
                Repeater {
                    model: [
                        { title: "搜索", icon: "🔍", page: 0 },
                        { title: "详情", icon: "📖", page: 1 },
                        { title: "历史", icon: "🕘", page: 2 },
                        { title: "收藏", icon: "❤️", page: 3 }
                    ]

                    delegate: Rectangle {
                        width: parent.width
                        height: 48
                        color: "transparent"
                        radius: theme.radiusSmall

                        // 选中状态
                        Rectangle {
                            anchors.fill: parent
                            color: theme.primaryLight
                            radius: theme.radiusSmall
                            opacity: currentPage === modelData.page ? 0.2 : 0
                        }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: theme.spacingMD
                            spacing: theme.spacingMD

                            Text {
                                text: modelData.icon
                                font.pixelSize: 16
                            }

                            Label {
                                text: modelData.title
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                color: currentPage === modelData.page ? theme.primary : theme.textPrimary
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                navigateTo(modelData.page)
                                navDrawer.close()
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 数据模型 ==========
    ListModel {
        id: searchResults
        // 示例数据，实际应用中从后端获取
    }

    // ========== 委托组件 ==========
    Component {
        id: searchResultDelegate

        ModernCard {
            width: ListView.view.width
            height: 80
            padding: theme.spacingMD

            property bool isCurrent: currentWord === model.word

            backgroundColor: isCurrent ? theme.primaryLight : theme.cardBackground
            shadowEnabled: !isCurrent
            clickable: true

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: theme.spacingXS

                Label {
                    text: model.word || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 16
                    font.weight: isCurrent ? Font.Medium : Font.Normal
                    color: isCurrent ? theme.primary : theme.textPrimary
                }

                Label {
                    text: model.definition || ""
                    font.family: "SF Pro Display"
                    font.pixelSize: 13
                    color: theme.textSecondary
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            onClicked: {
                currentWord = model.word
                currentDefinition = model.definition
                if (responsive.isMobile) {
                    currentPage = 1 // 切换到详情页
                }
            }
        }
    }

    Component {
        id: historyDelegate

        ModernCard {
            width: ListView.view.width
            height: 60
            padding: theme.spacingMD
            clickable: true

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: modelData || ""
                font.family: "SF Pro Display"
                font.pixelSize: 14
                color: theme.textPrimary
            }

            onClicked: {
                performSearch(modelData)
                navDrawer.close()
            }
        }
    }

    Component {
        id: favoriteDelegate

        ModernCard {
            width: ListView.view.width
            height: 60
            padding: theme.spacingMD
            clickable: true

            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: modelData || ""
                font.family: "SF Pro Display"
                font.pixelSize: 14
                color: theme.textPrimary
            }

            onClicked: {
                performSearch(modelData)
                navDrawer.close()
            }
        }
    }

    // ========== 公共方法 ==========
    function navigateTo(page) {
        currentPage = page
    }

    function performSearch(query) {
        searchText = query
        currentWord = query
        // 这里应该调用实际的搜索API
        // 模拟搜索结果
        searchResults.clear()

        // 添加示例搜索结果
        searchResults.append({
            "word": query,
            "definition": "这是 \"" + query + "\" 的释义内容。\n\n1. 第一种解释\n2. 第二种解释\n3. 第三种解释"
        })

        // 添加到搜索历史
        addToHistory(query)

        // 移动端切换到结果页
        if (responsive.isMobile) {
            currentPage = 0
        }

        showNotification("搜索完成: " + query)
    }

    function addToHistory(word) {
        // 避免重复
        var index = searchHistory.indexOf(word)
        if (index !== -1) {
            searchHistory.splice(index, 1)
        }
        searchHistory.unshift(word)

        // 限制历史记录数量
        if (searchHistory.length > 50) {
            searchHistory = searchHistory.slice(0, 50)
        }
    }

    function addToFavorites(word) {
        // 避免重复
        if (favoriteWords.indexOf(word) === -1) {
            favoriteWords.push(word)
        }
    }

    function showNotification(message) {
        console.log("Notification:", message)
        // 这里可以实现Toast通知
    }

    // ========== 键盘快捷键 ==========
    Keys.onPressed: function(event) {
        if (event.modifiers & Qt.ControlModifier) {
            switch (event.key) {
                case Qt.Key_K:
                    searchBox.focus()
                    event.accepted = true
                    break
                case Qt.Key_Slash:
                    searchBox.focus()
                    event.accepted = true
                    break
            }
        }
    }
}