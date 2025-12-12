import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"
import "components"

// 现代化桥接组件 - 连接现有功能与新界面
Item {
    id: root

    // ========== 属性桥接 ==========
    property alias lookup: lookupAdapter
    property alias responsive: responsive

    // 现有功能的状态
    property string currentWord: ""
    property string currentDefinition: ""
    property int currentPage: 0
    property var historyModel: []
    property var vocabModel: []

    // 错误和状态
    property var lastError: ({})
    property var lastVerify: ({})
    property bool lastPreviewIncludeRemoteOnly: true
    property bool lastPreviewIncludeLocalOnly: true
    property bool lastPreviewTakeRemoteNewer: true
    property bool lastPreviewTakeLocalNewer: true

    // ========== 现有组件实例 ==========
    LookupAdapter {
        id: lookupAdapter
    }

    ResponsiveLayout {
        id: responsive
    }

    // ========== 现代化主题 ==========
    Theme { id: theme }

    // ========== 现代化容器 ==========
    Rectangle {
        id: modernContainer
        anchors.fill: parent
        color: theme.background

        // ========== 现代化头部导航 ==========
        Rectangle {
            id: modernHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: responsive.safeAreaTop

            height: responsive.isMobile ? 60 : 72
            color: "transparent"

            // 毛玻璃效果背景
            Rectangle {
                anchors.fill: parent
                color: "rgba(255, 255, 255, 0.8)"

                layer.enabled: true
                layer.effect: GaussianBlur {
                    radius: 20
                    samples: 32
                    source: parent
                }

                border.color: theme.borderLight
                border.width: 1
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: theme.spacingMD
                anchors.rightMargin: theme.spacingMD
                spacing: theme.spacingMD

                // 品牌标识
                Row {
                    spacing: theme.spacingSM

                    // Logo 圆形背景
                    Rectangle {
                        id: logoBg
                        anchors.verticalCenter: parent.verticalCenter
                        width: responsive.isMobile ? 36 : 42
                        height: width
                        radius: width / 2
                        color: theme.primary

                        // Logo 动画
                        SequentialAnimation {
                            running: true
                            loops: Animation.Infinite

                            PropertyAnimation {
                                target: logoBg
                                property: "scale"
                                to: 1.05
                                duration: 3000
                            }
                            PropertyAnimation {
                                target: logoBg
                                property: "scale"
                                to: 1.0
                                duration: 3000
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "📚"
                            font.pixelSize: responsive.isMobile ? 18 : 20
                            color: "#FFFFFF"
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: -2

                        Text {
                            text: "Unidict"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: responsive.isMobile ? 18 : 20
                            font.weight: Font.Bold
                            color: theme.textPrimary
                        }

                        Text {
                            text: "Professional Dictionary"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: responsive.isMobile ? 10 : 11
                            color: theme.textSecondary
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // 现代化导航标签
                Row {
                    spacing: theme.spacingSM
                    visible: !responsive.isMobile

                    ModernButton {
                        text: "搜索"
                        buttonType: currentPage === 0 ? ModernButton.Primary : ModernButton.Ghost
                        size: ModernButton.Small

                        onClicked: {
                            currentPage = 0
                            showToast("切换到搜索页面")
                        }
                    }

                    ModernButton {
                        text: "历史"
                        buttonType: currentPage === 1 ? ModernButton.Primary : ModernButton.Ghost
                        size: ModernButton.Small

                        onClicked: {
                            currentPage = 1
                            loadHistory()
                            showToast("切换到历史页面")
                        }
                    }

                    ModernButton {
                        text: "词汇"
                        buttonType: currentPage === 2 ? ModernButton.Primary : ModernButton.Ghost
                        size: ModernButton.Small

                        onClicked: {
                            currentPage = 2
                            loadVocabulary()
                            showToast("切换到词汇页面")
                        }
                    }

                    ModernButton {
                        text: "语音"
                        buttonType: currentPage === 3 ? ModernButton.Primary : ModernButton.Ghost
                        size: ModernButton.Small

                        onClicked: {
                            currentPage = 3
                            showToast("切换到语音页面")
                        }
                    }
                }

                // 移动端菜单
                ModernButton {
                    id: mobileMenu
                    visible: responsive.isMobile
                    buttonType: ModernButton.Icon
                    size: ModernButton.Small
                    iconSource: "qrc:/icons/menu.svg"

                    onClicked: mobileDrawer.open()
                }
            }
        }

        // ========== 现代化搜索区域 ==========
        ModernCard {
            id: searchArea
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: modernHeader.bottom
            anchors.topMargin: theme.spacingLG
            anchors.leftMargin: theme.spacingMD
            anchors.rightMargin: theme.spacingMD

            height: responsive.isMobile ? 90 : 100
            shadowLevel: theme.shadow2

            // 搜索输入框
            ModernSearchBox {
                id: modernSearch
                anchors.centerIn: parent
                width: parent.width - theme.spacingLG * 2
                size: responsive.isMobile ? ModernSearchBox.Medium : ModernSearchBox.Large

                placeholder: "输入要查询的词条..."

                onSearchRequested: function(query) {
                    if (query.trim()) {
                        performModernSearch(query.trim())
                    }
                }

                Component.onCompleted: {
                    // 延迟获取焦点，避免界面加载时的冲突
                    focusTimer.restart()
                }
            }
        }

        // ========== 现代化内容区域 ==========
        Item {
            id: modernContent
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: searchArea.bottom
            anchors.bottom: parent.bottom
            anchors.margins: theme.spacingMD

            // 现代化分页容器
            StackLayout {
                id: modernStack
                anchors.fill: parent
                currentIndex: currentPage

                // 页面 1: 搜索结果页面
                ModernSearchResultsPage {
                    id: searchResultsPage
                    lookupAdapter: root.lookupAdapter
                    onWordSelected: function(word, definition) {
                        currentWord = word
                        currentDefinition = definition
                        showToast("已选择: " + word)
                    }
                }

                // 页面 2: 历史记录页面
                ModernHistoryPage {
                    id: historyPage
                    historyModel: root.historyModel
                    onWordSelected: function(word) {
                        modernSearch.text = word
                        performModernSearch(word)
                        currentPage = 0
                    }
                }

                // 页面 3: 词汇管理页面
                ModernVocabularyPage {
                    id: vocabularyPage
                    vocabModel: root.vocabModel
                    onWordSelected: function(word) {
                        modernSearch.text = word
                        performModernSearch(word)
                        currentPage = 0
                    }
                }

                // 页面 4: 语音功能页面
                ModernVoicePage {
                    id: voicePage
                    currentWord: root.currentWord
                }
            }
        }

        // ========== 移动端抽屉 ==========
        Drawer {
            id: mobileDrawer
            width: Math.min(320, parent.width * 0.8)
            edge: Qt.LeftEdge
            modal: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

            // 现代化抽屉背景
            Rectangle {
                anchors.fill: parent
                color: theme.surface

                Column {
                    width: parent.width
                    spacing: theme.spacingMD
                    padding: theme.spacingMD

                    // 用户信息区域
                    ModernCard {
                        width: parent.width
                        height: 80
                        padding: theme.spacingMD

                        Row {
                            spacing: theme.spacingMD

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
                                spacing: 2

                                Text {
                                    text: "用户"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 16
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Text {
                                    text: "专业版"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.textSecondary
                                }
                            }
                        }
                    }

                    // 导航菜单项
                    Column {
                        width: parent.width
                        spacing: theme.spacingXS

                        Repeater {
                            model: [
                                { title: "搜索", icon: "🔍", page: 0 },
                                { title: "历史", icon: "🕘", page: 1 },
                                { title: "词汇", icon: "📚", page: 2 },
                                { title: "语音", icon: "🔊", page: 3 }
                            ]

                            delegate: ModernNavigationItem {
                                width: parent.width
                                title: modelData.title
                                icon: modelData.icon
                                isActive: currentPage === modelData.page

                                onClicked: {
                                    currentPage = modelData.page
                                    mobileDrawer.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 功能桥接方法 ==========
    function performModernSearch(query) {
        // 使用现有的 LookupAdapter 进行搜索
        var results = lookupAdapter.search(query, 20) // 限制结果数量

        if (results && results.length > 0) {
            currentWord = results[0].word || query
            currentDefinition = results[0].definition || ""

            // 更新搜索结果页面
            searchResultsPage.updateResults(results)

            // 添加到历史
            addToHistory(query)

            showToast(`找到 ${results.length} 个结果`)
        } else {
            searchResultsPage.showNoResults(query)
            showToast("未找到相关结果")
        }

        // 确保在搜索结果页面
        currentPage = 0
    }

    function loadHistory() {
        try {
            var history = lookupAdapter.searchHistory(50) // 获取最近50条历史
            historyModel = history || []
            historyPage.updateHistory(historyModel)
        } catch (e) {
            console.error("加载历史记录失败:", e)
            historyModel = []
            showToast("加载历史记录失败")
        }
    }

    function loadVocabulary() {
        try {
            var vocab = lookupAdapter.vocabulary() || []
            vocabModel = vocab
            vocabularyPage.updateVocabulary(vocabModel)
        } catch (e) {
            console.error("加载词汇表失败:", e)
            vocabModel = []
            showToast("加载词汇表失败")
        }
    }

    function addToHistory(word) {
        if (!word || word.trim() === "") return

        // 检查重复
        for (var i = 0; i < historyModel.length; i++) {
            if (historyModel[i] === word) {
                historyModel.splice(i, 1)
                break
            }
        }

        historyModel.unshift(word)

        // 限制历史记录数量
        if (historyModel.length > 100) {
            historyModel = historyModel.slice(0, 100)
        }
    }

    function showToast(message) {
        console.log("Toast:", message)
        // 这里可以集成原有的 Toast 逻辑
    }

    // ========== 辅助计时器 ==========
    Timer {
        id: focusTimer
        interval: 500
        onTriggered: modernSearch.focus()
    }

    // ========== 错误处理 ==========
    Connections {
        target: lookupAdapter

        function onError(error) {
            lastError = error
            showToast("查询出错: " + (error.message || "未知错误"))
        }
    }
}