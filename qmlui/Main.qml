import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
// import QtQuick.Dialogs 6.3  // 暂时注释掉，使用自定义文件对话框
import "components"
import "mobile/common"

ApplicationWindow {
    id: win
    visible: true
    // 响应式窗口尺寸
    width: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.width : 640
    height: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.height : 480
    title: "Unidict (QML)"
    color: Theme.window

    // 移动端全屏显示
    visibility: (Qt.platform.os === "android" || Qt.platform.os === "ios") ?
                ApplicationWindow.FullScreen : ApplicationWindow.Windowed

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.accent: Theme.accent
    Material.primary: Theme.card
    Material.background: Theme.window
    Material.foreground: Theme.text

    property string currentWord: ""
    property string currentDefinition: ""
    // 命中来源词典（UI 批三 配图链）：Exact 查词走 aggregateLookup 取首
    // 分组，富文本 + dictionaryId 供 presentEntry 重写 .mdd 资源为
    // file:// 路径；纯文本 currentDefinition 保留原语义（存生词/TTS/AI）
    property string currentDefinitionHtml: ""
    property string currentDictId: ""
    property int currentPage: 0
    property alias toastText: toastLabel.text
    property int toastDuration: 1800
    property bool toastAtTop: false
    property string lastIndexPath: ""
    property bool quickUpgrade: false
    property string quickInPath: ""
    property var lastError: ({})
    property var lastVerify: ({})
    property bool lastPreviewIncludeRemoteOnly: true
    property bool lastPreviewIncludeLocalOnly: true
    property bool lastPreviewTakeRemoteNewer: true
    property bool lastPreviewTakeLocalNewer: true
    property var historyModel: vocab.searchHistory(200)
    property var vocabModel: vocab.vocabulary()

    function navigateTo(page) {
        currentPage = page
        navDrawer.close()
    }

    // 搜索建议分组（UI 批三，桌面 MainDesktop 同款口径）：core
    // suggest_sections 的 kind → 段头文案。容错段：乱序/包含/通配
    function suggestKindLabel(kind) {
        var map = { exact: "精确", prefix: "前缀", anagram: "乱序",
                    substring: "包含", wildcard: "通配", fuzzy: "词条" }
        return map[kind] !== undefined ? map[kind] : map.fuzzy
    }

    function reloadSuggestions(prefix) {
        suggestionModel.clear()
        if (!prefix || prefix.trim().length === 0) return
        var q = prefix.trim()
        // 一次调用取齐分段候选（精确→前缀→乱序→通配/包含→词条），
        // 跨段去重与扫描段耗时上限都在 core 侧
        var secs = []
        try { secs = lookup.suggestAll(q, 50) || [] } catch (e) { secs = [] }
        for (var i = 0; i < secs.length; i++) {
            var label = suggestKindLabel(secs[i].kind)
            var ws = secs[i].words || []
            for (var j = 0; j < ws.length; j++)
                suggestionModel.append({ "word": ws[j], "section": label })
        }
    }

    function showToast(msg, ms) {
        toastLabel.text = msg
        toastRect.opacity = 0.0
        toastRect.visible = true
        toastPause.duration = ms ? ms : toastDuration
        toastAnim.restart()
    }

    // 响应式布局配置
    ResponsiveLayout {
        id: responsive
        anchors.fill: parent
    }

    Drawer {
        id: navDrawer
        width: Math.min(320, win.width * 0.8)
        edge: Qt.LeftEdge
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        Column {
            width: parent.width
            spacing: 12
            padding: 20
            Label {
                text: "Navigate"
                font.pixelSize: 18
                font.bold: true
                color: Theme.text
            }
            Repeater {
                model: [
                    { title: "Search Hub", page: 0 },
                    { title: "History", page: 1 },
                    { title: "Vocabulary", page: 2 },
                    { title: "Voice", page: 3 },
                    { title: "Review", page: 4 }
                ]
                delegate: Button {
                    text: modelData.title
                    width: parent.width
                    flat: true
                    highlighted: win.currentPage === modelData.page
                    onClicked: win.navigateTo(modelData.page)
                }
            }
        }
    }

    // 底栏工具栏（UI 批三）：五页导航从顶栏 emoji 行移到底部拇指区，
    // 点按只做页面切换（Review 仅导航，背单词内容不在此碰）
    footer: ToolBar {
        Material.background: Theme.card
        Material.foreground: Theme.text
        RowLayout {
            anchors.fill: parent
            RoundButton {
                text: "🔍"
                ToolTip.text: "Search"
                ToolTip.visible: hovered
                ToolTip.delay: 200
                highlighted: win.currentPage === 0
                onClicked: win.navigateTo(0)
            }
            RoundButton {
                text: "🕘"
                ToolTip.text: "History"
                ToolTip.visible: hovered
                ToolTip.delay: 200
                highlighted: win.currentPage === 1
                onClicked: win.navigateTo(1)
            }
            RoundButton {
                text: "📚"
                ToolTip.text: "Vocabulary"
                ToolTip.visible: hovered
                ToolTip.delay: 200
                highlighted: win.currentPage === 2
                onClicked: win.navigateTo(2)
            }
            RoundButton {
                text: "🔊"
                ToolTip.text: "Voice"
                ToolTip.visible: hovered
                ToolTip.delay: 200
                highlighted: win.currentPage === 3
                onClicked: win.navigateTo(3)
            }
            RoundButton {
                text: "🔁"
                ToolTip.text: "Review"
                ToolTip.visible: hovered
                ToolTip.delay: 200
                highlighted: win.currentPage === 4
                onClicked: win.navigateTo(4)
            }
            Item { Layout.fillWidth: true }
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: responsive.baseMargin
        anchors.topMargin: responsive.safeAreaTop + responsive.baseMargin
        anchors.bottomMargin: responsive.safeAreaBottom + responsive.baseMargin

        ToolBar {
            id: heroBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            Material.background: Theme.accent
            Material.foreground: Theme.accentText
            RowLayout {
                anchors.fill: parent
                ToolButton {
                    icon.name: "menu"
                    onClicked: navDrawer.open()
                }
                Label {
                    text: "Unidict"
                    font.pixelSize: 20
                    font.bold: true
                    Layout.fillWidth: true
                }
            }
        }

        Frame {
            id: sectionTabs
            anchors.top: heroBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: responsive.baseSpacing
            padding: responsive.baseSpacing
            background: Rectangle {
                color: Theme.card
                radius: Theme.radiusL
                border.color: Theme.divider
            }
            Column {
                width: parent.width
                spacing: responsive.baseSpacing / 2
                Label {
                    text: ["Search hub","History","Vocabulary","Voice studio","Review"][win.currentPage] || "Search hub"
                    font.pixelSize: responsive.normalFont
                    font.bold: true
                    color: Theme.text
                }
                ProgressBar {
                    from: 0
                    to: 4
                    value: win.currentPage
                }
            }
        }

        StackLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: sectionTabs.bottom
            anchors.bottom: parent.bottom
            anchors.topMargin: responsive.baseSpacing
            currentIndex: win.currentPage

            // Search tab（page 0）——ca8f835 重排事故把输入区与结果区副本
            // 粘残：此处只剩引用不存在 id 的孤块（input/searchBtn/modeBox
            // 无定义、resultsView 与真身重复 id），按 4370c36 骨架恢复输入
            // 区；结果显示区真身见下方「结果显示区域」
            MobileSearchPage {
                objectName: "mobileSearchPage"
                win: win
                responsive: responsive
                suggestionModel: suggestionModel
            }
            // History Tab
            MobileHistoryPage {
                objectName: "mobileHistoryPage"
                win: win
                responsive: responsive
                suggestionModel: suggestionModel
            }
            // Vocab Tab
            MobileVocabPage {
                objectName: "mobileVocabPage"
                win: win
                responsive: responsive
                suggestionModel: suggestionModel
            }
            // 语音设置 Tab
            MobileVoicePage {
                objectName: "mobileVoicePage"
                win: win
                responsive: responsive
                suggestionModel: suggestionModel
            }
            // 🔁复习 Tab（P-7 批四 复习入口简单化：生词本四技能学习状态
            // 清单——任一技能未到「稳」即入列，点技能圈档 未练→不稳→稳。
            // 无调度/等级/激励营销，product-principles §11「预留防迁移，
            // 不是课程化」口径；真数据源 = vocab.vocabularyMeta 四技能键）
            MobileReviewPage {
                objectName: "mobileReviewPage"
                win: win
                responsive: responsive
                suggestionModel: suggestionModel
            }
        }
    }

    PageIndicator {
        count: 5
        currentIndex: win.currentPage
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: responsive.safeAreaBottom + responsive.baseMargin
        visible: win.width > 480
    }

    Rectangle {
        id: toastRect
        color: Theme.text
        radius: Theme.radiusS
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: win.toastAtTop ? undefined : parent.bottom
        anchors.bottomMargin: win.toastAtTop ? 0 : (responsive.safeAreaBottom + responsive.baseMargin)
        anchors.top: win.toastAtTop ? parent.top : undefined
        anchors.topMargin: win.toastAtTop ? (responsive.safeAreaTop + responsive.baseMargin) : 0
        visible: false
        opacity: 0.0
        z: 999
        Row {
            anchors.margins: 12
            anchors.fill: parent
            spacing: 8
            Label {
                id: toastLabel
                color: Theme.window
                font.pixelSize: responsive.smallFont
                wrapMode: Text.Wrap
            }
        }
        implicitWidth: Math.min(win.width - 2*responsive.baseMargin, toastLabel.implicitWidth + 24)
        implicitHeight: toastLabel.implicitHeight + 16
        MouseArea {
            anchors.fill: parent
            onClicked: { toastAnim.stop(); toastRect.visible = false; toastRect.opacity = 0.0 }
        }
    }

    SequentialAnimation {
        id: toastAnim
        running: false
        PropertyAnimation { target: toastRect; property: "opacity"; from: 0.0; to: 1.0; duration: 150 }
        PauseAnimation { id: toastPause; duration: 1800 }
        PropertyAnimation { target: toastRect; property: "opacity"; from: 1.0; to: 0.0; duration: 250 }
        onStopped: toastRect.visible = false
    }

    // 搜索建议分组模型（UI 批三）：行带 section（精确/前缀/词条），
    // reloadSuggestions 按段稳定重排后填充
    ListModel { id: suggestionModel }
}
