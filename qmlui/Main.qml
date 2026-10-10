import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
// import QtQuick.Dialogs 6.3  // 暂时注释掉，使用自定义文件对话框
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
    property var historyModel: lookup.searchHistory(200)
    property var vocabModel: lookup.vocabulary()

    function navigateTo(page) {
        currentPage = page
        navDrawer.close()
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
                RoundButton {
                    text: "🔍"
                    ToolTip.text: "Search"
                    ToolTip.visible: hovered
                    ToolTip.delay: 200
                    onClicked: win.navigateTo(0)
                }
                RoundButton {
                    text: "🕘"
                    ToolTip.text: "History"
                    ToolTip.visible: hovered
                    ToolTip.delay: 200
                    onClicked: win.navigateTo(1)
                }
                RoundButton {
                    text: "📚"
                    ToolTip.text: "Vocabulary"
                    ToolTip.visible: hovered
                    ToolTip.delay: 200
                    onClicked: win.navigateTo(2)
                }
                RoundButton {
                    text: "🔊"
                    ToolTip.text: "Voice"
                    ToolTip.visible: hovered
                    ToolTip.delay: 200
                    onClicked: win.navigateTo(3)
                }
                RoundButton {
                    text: "🔁"
                    ToolTip.text: "Review"
                    ToolTip.visible: hovered
                    ToolTip.delay: 200
                    onClicked: win.navigateTo(4)
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
            Item {
                Column {
                    width: parent.width
                    spacing: responsive.baseSpacing

                    // 搜索输入区域 - 响应式布局
                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        TextField {
                            id: input
                            width: responsive.isMobile ? parent.width : Math.max(200, parent.width * 0.5)
                            height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                            placeholderText: "Enter a word..."
                            font.pixelSize: responsive.normalFont
                            onTextChanged: suggestions.model = input.text.length > 0 ? lookup.suggestPrefix(input.text, 20) : []
                            onAccepted: searchBtn.clicked()
                        }

                        ComboBox {
                            id: modeBox
                            model: ["Exact", "Prefix", "Fuzzy", "Wildcard", "Regex"]
                            currentIndex: 0
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            width: responsive.isMobile ? Math.min(120, parent.width * 0.3) : 120
                        }

                        Button {
                            id: searchBtn
                            text: "Search"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            width: responsive.isMobile ? Math.min(80, parent.width * 0.2) : implicitWidth
                            onClicked: {
                                currentWord = input.text
                                if (modeBox.currentText === "Exact") {
                                    currentDefinition = lookup.lookupDefinition(currentWord)
                                    resultsView.model = []
                                } else if (modeBox.currentText === "Prefix") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.suggestPrefix(currentWord, 100)
                                } else if (modeBox.currentText === "Fuzzy") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.suggestFuzzy(currentWord, 100)
                                } else if (modeBox.currentText === "Wildcard") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.searchWildcard(currentWord, 100)
                                } else if (modeBox.currentText === "Regex") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.searchRegex(currentWord, 100)
                                }
                            }
                        }

                        Button {
                            text: "Save to Vocabulary"
                            enabled: currentWord.length > 0 && currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: lookup.addToVocabulary(currentWord, currentDefinition)
                        }
                    }

                    Column {
                        spacing: responsive.baseSpacing / 2
                        // 顶部加密提示横幅（若存在加密词典）
                        Label {
                            id: encryptedBanner
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: responsive.smallFont
                            color: Theme.danger
                            text: {
                                var _stamp = lookup.dictionariesStamp
                                var metas = lookup.dictionariesMeta()
                                var hasEncrypted = false
                                for (var i = 0; i < metas.length; ++i) {
                                    var m = metas[i]
                                    var desc = m.description ? m.description : ""
                                    if (desc.indexOf("[encrypted]") !== -1) { hasEncrypted = true; break }
                                }
                                return hasEncrypted ? "Warning: Some dictionaries are encrypted. Content may require UNIDICT_MDICT_PASSWORD or may be unsupported depending on encryption type." : ''
                            }
                            visible: text.length > 0
                        }
                        RowLayout {
                            width: parent.width
                            spacing: responsive.baseSpacing / 2
                            visible: encryptedBanner.visible

                            TextField {
                                id: mdictPasswordField
                                Layout.fillWidth: true
                                echoMode: TextInput.Password
                                placeholderText: lookup.hasMdictPassword()
                                    ? "MDict password is set (enter to replace)"
                                    : "Enter MDict password"
                            }
                            Button {
                                text: "Apply & Reload"
                                enabled: mdictPasswordField.text.length > 0
                                onClicked: {
                                    var okPw = lookup.setMdictPassword(mdictPasswordField.text)
                                    if (!okPw) { win.showToast("Password empty", 1500); return }
                                    var ok = lookup.reloadDictionariesFromEnv()
                                    win.showToast(ok ? "Reloaded dictionaries" : "Reload failed", 1500)
                                    mdictPasswordField.text = ""
                                }
                            }
                            Button {
                                text: "Clear"
                                onClicked: {
                                    lookup.clearMdictPassword()
                                    mdictPasswordField.text = ""
                                    win.showToast("Password cleared", 1500)
                                }
                            }
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: responsive.smallFont
                            text: {
                                var _stamp = lookup.dictionariesStamp
                                var metas = lookup.dictionariesMeta()
                                if (metas.length === 0) return "No dictionaries loaded. Set UNIDICT_DICTS to paths (':' or ';' separated)."
                                var lines = []
                                var hasEncrypted = false
                                for (var i = 0; i < metas.length; ++i) {
                                    var m = metas[i]
                                    var desc = m.description ? m.description : ""
                                    if (desc.indexOf("[encrypted]") !== -1) hasEncrypted = true
                                    lines.push("- " + m.name + " (" + m.wordCount + (desc.length>0? (", " + desc) : "") + ")")
                                }
                                var header = hasEncrypted ? "Loaded dictionaries (some encrypted):\n" : "Loaded dictionaries:\n"
                                return header + lines.join("\n")
                            }
                        }
                        Label {
                            text: "Indexed words: " + lookup.indexedWordCount()
                            font.pixelSize: responsive.smallFont
                        }
                    }

                    // 结果显示区域 - 移动端垂直布局，桌面端水平布局
                    Item {
                        width: parent.width
                        height: responsive.isMobile ? 400 : 260

                        // 移动端垂直布局
                        Column {
                            anchors.fill: parent
                            spacing: responsive.baseSpacing
                            visible: responsive.isMobile

                            ListView {
                                id: suggestions
                                width: parent.width
                                height: 120
                                clip: true
                                model: []
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: Math.max(36, responsive.minTouchTarget)
                                    text: modelData
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        input.text = modelData
                                        searchBtn.clicked()
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "Suggestions will appear here"
                                    color: Theme.textTertiary
                                    font.pixelSize: responsive.smallFont
                                }
                            }

                            ListView {
                                id: resultsView
                                width: parent.width
                                height: 120
                                clip: true
                                model: []
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: Math.max(36, responsive.minTouchTarget)
                                    text: modelData
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        input.text = modelData
                                        modeBox.currentIndex = 0
                                        searchBtn.clicked()
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "Search results will appear here"
                                    color: Theme.textTertiary
                                    font.pixelSize: responsive.smallFont
                                }
                            }

                            ScrollView {
                                width: parent.width
                                height: 160
                                DefinitionContent {}
                            }
                        }

                        // 桌面端水平布局
                        Row {
                            anchors.fill: parent
                            spacing: responsive.baseSpacing
                            visible: !responsive.isMobile

                            ListView {
                                id: suggestionsDesktop
                                width: parent.width * 0.33
                                height: 260
                                clip: true
                                model: suggestions.model
                                delegate: ItemDelegate {
                                    text: modelData
                                    onClicked: {
                                        input.text = modelData
                                        searchBtn.clicked()
                                    }
                                }
                            }
                            ListView {
                                id: resultsViewDesktop
                                width: parent.width * 0.33
                                height: 260
                                clip: true
                                model: resultsView.model
                                delegate: ItemDelegate {
                                    text: modelData
                                    onClicked: {
                                        input.text = modelData
                                        modeBox.currentIndex = 0
                                        searchBtn.clicked()
                                    }
                                }
                            }
                            ScrollView {
                                width: parent.width - suggestionsDesktop.width - resultsViewDesktop.width - 16
                                height: 260
                                DefinitionContent {}
                            }
                        }
                    }
                    Button {
                        text: "Save to Vocab"
                        enabled: currentWord.length > 0 && currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        width: responsive.isMobile ? parent.width : implicitWidth
                        onClicked: lookup.addToVocabulary(currentWord, currentDefinition)
                    }

                    // AI Tools
                    GroupBox {
                        title: "AI Tools"
                        width: parent.width
                        Column {
                            width: parent.width
                            spacing: responsive.baseSpacing / 2
                            Flow {
                                width: parent.width
                                spacing: responsive.baseSpacing
                                Button {
                                    text: "Translate Definition → Chinese"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        var txt = currentDefinition.length>0 ? currentDefinition : input.text
                                        if (!txt || txt.length===0) { win.showToast("Nothing to translate"); return }
                                        aiOut.text = ai.translate(txt, "zh")
                                    }
                                }
                                Button {
                                    text: "Grammar Check"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        var txt = currentDefinition.length>0 ? currentDefinition : input.text
                                        if (!txt || txt.length===0) { win.showToast("Nothing to check"); return }
                                        aiOut.text = ai.grammarCheck(txt)
                                    }
                                }
                            }
                            TextArea {
                                id: aiOut
                                readOnly: true
                                wrapMode: Text.Wrap
                                width: parent.width
                                height: Math.max(80, implicitHeight)
                                font.pixelSize: responsive.smallFont
                                placeholderText: "AI output will appear here"
                            }
                        }
                    }
                }
            }

            // History Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: responsive.baseSpacing

                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        Button {
                            text: "Refresh"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: historyModel = lookup.searchHistory(200)
                        }
                        Button {
                            text: "Clear"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: { lookup.clearHistory(); historyModel = [] }
                        }
                    }

                    TextField {
                        id: historyFilter
                        placeholderText: "Filter history..."
                        width: parent.width
                        height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        onTextChanged: {
                            var all = lookup.searchHistory(200)
                            if (historyFilter.text.length === 0) { historyModel = all; return }
                            var filtered = []
                            for (var i = 0; i < all.length; ++i) {
                                var w = all[i]
                                if (w.toLowerCase().indexOf(historyFilter.text.toLowerCase()) !== -1) filtered.push(w)
                            }
                            historyModel = filtered
                        }
                    }

                    ListView {
                        id: historyList
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.top: historyFilter.bottom
                        anchors.topMargin: responsive.baseSpacing
                        model: historyModel
                        delegate: ItemDelegate {
                            width: parent.width
                            height: Math.max(36, responsive.minTouchTarget)
                            text: modelData
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                win.navigateTo(0)
                                input.text = modelData
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.count === 0
                            text: "No search history"
                            color: Theme.textTertiary
                            font.pixelSize: responsive.smallFont
                        }
                    }
                }
            }

            // Vocab Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: responsive.baseSpacing

                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        Button {
                            text: "Refresh"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: vocabModel = lookup.vocabulary()
                        }
                        Button {
                            text: "Sort A→Z"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                var all = lookup.vocabulary()
                                all.sort(function(a,b){ return a.word.toLowerCase() < b.word.toLowerCase() ? -1 : (a.word.toLowerCase() > b.word.toLowerCase() ? 1 : 0) })
                                vocabModel = all
                            }
                        }
                        Button {
                            text: "Sort Z→A"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                var all = lookup.vocabulary()
                                all.sort(function(a,b){ return a.word.toLowerCase() > b.word.toLowerCase() ? -1 : (a.word.toLowerCase() < b.word.toLowerCase() ? 1 : 0) })
                                vocabModel = all
                            }
                        }
                        Button {
                            text: "Sort by Time"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                var all = lookup.vocabularyMeta()
                                // Descending by added_at; missing treated as 0
                                all.sort(function(a,b){ var ta = a.added_at || 0; var tb = b.added_at || 0; return tb - ta })
                                // Map back to simple modelData used by list
                                var simple = []
                                for (var i = 0; i < all.length; ++i) { simple.push({word: all[i].word, definition: all[i].definition}) }
                                vocabModel = simple
                            }
                        }
                        Button {
                            text: "Clear"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: { lookup.clearVocabulary(); vocabModel = [] }
                        }
                        Button {
                            text: "Export CSV"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: saveDialog.open()
                        }
                        Button {
                            text: "Choose Sync File"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: syncChooseDialog.open()
                        }
                        Button {
                            text: "Preview Sync"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                if (sync.syncFile().length === 0) { win.showToast("Please choose a sync file first"); return }
                                var m = sync.previewDiff()
                                if (!m.ok) {
                                    win.lastError = ({ context: "sync_preview", path: sync.syncFile(), error: (m.error||"") })
                                    win.showToast("Preview failed. See Error Details.", 2200)
                                    return
                                }
                                var msg = "Preview: "
                                msg += "localOnly=" + (m.localOnly ? m.localOnly.length : 0)
                                msg += ", remoteOnly=" + (m.remoteOnly ? m.remoteOnly.length : 0)
                                msg += ", remoteNewer=" + (m.remoteNewer ? m.remoteNewer.length : 0)
                                msg += ", localNewer=" + (m.localNewer ? m.localNewer.length : 0)
                                syncStatus.text = msg
                                win.showToast(msg)
                                // Populate details dialog
                                previewDetails.fill(m)
                                win.lastError = {}
                                previewDialog.open()
                            }
                        }
                        Button {
                            text: "Sync Now"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                if (sync.syncFile().length === 0) { syncStatus.text = "Please choose a sync file first"; return }
                                var ok = sync.syncNow()
                                syncStatus.text = ok ? ("Synced: " + sync.syncFile()) : "Sync failed"
                                if (!ok) {
                                    win.lastError = ({ context: "sync_now", path: sync.syncFile(), error: (sync.lastError ? sync.lastError() : "unknown error") })
                                    win.showToast("Sync failed. See Error Details.", 2200)
                                } else {
                                    win.lastError = {}
                                    win.showToast("Synced with " + sync.syncFile())
                                }
                                // Refresh local view
                                vocabModel = lookup.vocabulary()
                                historyModel = lookup.searchHistory(200)
                            }
                        }
                    }

                    MobileFileDialog {
                        id: syncChooseDialog
                        title: "Choose Sync File"
                        selectExisting: false
                        nameFilters: ["JSON files (*.json)", "All files (*)"]
                        onAccepted: {
                            var p = syncChooseDialog.fileUrl.toString().replace("file://","")
                            sync.setSyncFile(p)
                            syncStatus.text = "Sync file: " + p
                        }
                    }

                    // 显示同步状态
                    Label {
                        id: syncStatus
                        width: parent.width
                        text: ""
                        font.pixelSize: responsive.smallFont
                        wrapMode: Text.Wrap
                        color: Theme.textTertiary
                    }
                    // 预览详情对话框
                    Dialog {
                        id: previewDialog
                        modal: true
                        title: "Sync Preview Details"
                        standardButtons: Dialog.Ok
                        contentItem: ScrollView {
                            width: Math.min(win.width*0.9, 600)
                            height: Math.min(win.height*0.7, 400)
                            Column {
                                id: previewDetails
                                spacing: 8
                                padding: 8
                                property var data: ({})
                                function fill(m) {
                                    previewDetails.data = m
                                    localOnlyLbl.text = "Local Only (" + (m.localOnly?m.localOnly.length:0) + "): " + (m.localOnly? m.localOnly.join(", "):"")
                                    remoteOnlyLbl.text = "Remote Only (" + (m.remoteOnly?m.remoteOnly.length:0) + "): " + (m.remoteOnly? m.remoteOnly.join(", "):"")
                                    var rn = m.remoteNewer || []
                                    var ln = m.localNewer || []
                                    var rnStr = []
                                    for (var i=0;i<rn.length;++i) rnStr.push(rn[i].word + " (local:" + rn[i].local_ts + ", remote:" + rn[i].remote_ts + ")")
                                    var lnStr = []
                                    for (var j=0;j<ln.length;++j) lnStr.push(ln[j].word + " (local:" + ln[j].local_ts + ", remote:" + ln[j].remote_ts + ")")
                                    remoteNewerLbl.text = "Remote Newer (" + rn.length + "): " + rnStr.join(", ")
                                    localNewerLbl.text = "Local Newer (" + ln.length + "): " + lnStr.join(", ")
                                }
                                CheckBox {
                                    id: cbRemoteOnly; text: "Pull remote-only into local";
                                    checked: settings.getBool("sync/preview/includeRemoteOnly", true)
                                    onToggled: {
                                        settings.setBool("sync/preview/includeRemoteOnly", checked)
                                        win.lastPreviewIncludeRemoteOnly = checked
                                    }
                                }
                                CheckBox {
                                    id: cbLocalOnly;  text: "Push local-only to remote";
                                    checked: settings.getBool("sync/preview/includeLocalOnly", true)
                                    onToggled: {
                                        settings.setBool("sync/preview/includeLocalOnly", checked)
                                        win.lastPreviewIncludeLocalOnly = checked
                                    }
                                }
                                CheckBox {
                                    id: cbRemoteNewer; text: "Take remote newer into local";
                                    checked: settings.getBool("sync/preview/takeRemoteNewer", true)
                                    onToggled: {
                                        settings.setBool("sync/preview/takeRemoteNewer", checked)
                                        win.lastPreviewTakeRemoteNewer = checked
                                    }
                                }
                                CheckBox {
                                    id: cbLocalNewer;  text: "Take local newer into remote";
                                    checked: settings.getBool("sync/preview/takeLocalNewer", true)
                                    onToggled: {
                                        settings.setBool("sync/preview/takeLocalNewer", checked)
                                        win.lastPreviewTakeLocalNewer = checked
                                    }
                                }
                                Label { id: localOnlyLbl; wrapMode: Text.Wrap; width: parent.width }
                                Label { id: remoteOnlyLbl; wrapMode: Text.Wrap; width: parent.width }
                                Label { id: remoteNewerLbl; wrapMode: Text.Wrap; width: parent.width }
                                Label { id: localNewerLbl; wrapMode: Text.Wrap; width: parent.width }
                                // Selection lists
                                GroupBox {
                                    title: "Select Words"
                                    width: parent.width
                                    Column {
                                        width: parent.width
                                        spacing: 4
                                        // Remote Only list
                                        Label { text: "Remote Only (select to pull into local)"; font.pixelSize: responsive.smallFont }
                                        ListView {
                                            width: parent.width; height: 100; clip: true
                                            model: previewDetails.data.remoteOnly || []
                                            delegate: Row {
                                                spacing: 6
                                                CheckBox { id: cb; checked: true }
                                                Label { text: modelData; elide: Text.ElideRight; width: parent.width - 60 }
                                            }
                                            property var selected: function() {
                                                var out = []; for (var i=0;i<count;++i) { var d= model.get(i); var v = itemAtIndex(i); if (v && v.children[0].checked) out.push(d) } return out;
                                            }
                                        }
                                        // Local Only list
                                        Label { text: "Local Only (select to push to remote)"; font.pixelSize: responsive.smallFont }
                                        ListView {
                                            id: lvLocalOnly; width: parent.width; height: 100; clip: true
                                            model: previewDetails.data.localOnly || []
                                            delegate: Row {
                                                spacing: 6
                                                CheckBox { id: cb2; checked: true }
                                                Label { text: modelData; elide: Text.ElideRight; width: parent.width - 60 }
                                            }
                                            property var selected: function() {
                                                var out = []; for (var i=0;i<count;++i) { var d= model.get(i); var v = itemAtIndex(i); if (v && v.children[0].checked) out.push(d) } return out;
                                            }
                                        }
                                        // Remote Newer list
                                        Label { text: "Remote Newer (select to override local with remote)"; font.pixelSize: responsive.smallFont }
                                        ListView {
                                            id: lvRemoteNewer; width: parent.width; height: 100; clip: true
                                            model: previewDetails.data.remoteNewer || []
                                            delegate: Row {
                                                spacing: 6
                                                CheckBox { id: cb3; checked: true }
                                                Label { text: modelData.word; elide: Text.ElideRight; width: parent.width - 60 }
                                            }
                                            property var selected: function() {
                                                var out = []; for (var i=0;i<count;++i) { var d= model.get(i); var v = itemAtIndex(i); if (v && v.children[0].checked) out.push(d.word) } return out;
                                            }
                                        }
                                        // Local Newer list
                                        Label { text: "Local Newer (select to override remote with local)"; font.pixelSize: responsive.smallFont }
                                        ListView {
                                            id: lvLocalNewer; width: parent.width; height: 100; clip: true
                                            model: previewDetails.data.localNewer || []
                                            delegate: Row {
                                                spacing: 6
                                                CheckBox { id: cb4; checked: true }
                                                Label { text: modelData.word; elide: Text.ElideRight; width: parent.width - 60 }
                                            }
                                            property var selected: function() {
                                                var out = []; for (var i=0;i<count;++i) { var d= model.get(i); var v = itemAtIndex(i); if (v && v.children[0].checked) out.push(d.word) } return out;
                                            }
                                        }
                                    }
                                }
                                Button {
                                    text: "Apply"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: {
                                        var sel = {
                                            remoteOnly: (previewDetails.children[6].children[1].selected ? previewDetails.children[6].children[1].selected() : []),
                                            localOnly: (lvLocalOnly.selected ? lvLocalOnly.selected() : []),
                                            remoteNewer: (lvRemoteNewer.selected ? lvRemoteNewer.selected() : []),
                                            localNewer: (lvLocalNewer.selected ? lvLocalNewer.selected() : [])
                                        }
                                        // Show confirmation summary
                                        confirmApplyDialog.selection = sel
                                        var cnt = (sel.remoteOnly?sel.remoteOnly.length:0) + (sel.localOnly?sel.localOnly.length:0) + (sel.remoteNewer?sel.remoteNewer.length:0) + (sel.localNewer?sel.localNewer.length:0)
                                        confirmApplyDialog.summaryText = "Apply selected changes?\n" +
                                            "+ pull remote-only: " + (sel.remoteOnly?sel.remoteOnly.length:0) + "\n" +
                                            "+ push local-only: " + (sel.localOnly?sel.localOnly.length:0) + "\n" +
                                            "* update local from remote: " + (sel.remoteNewer?sel.remoteNewer.length:0) + "\n" +
                                            "* update remote from local: " + (sel.localNewer?sel.localNewer.length:0) + "\n" +
                                            "Total: " + cnt
                                        confirmApplyDialog.open()
                                    }
                                }
                                Button {
                                    text: "Show Last Sync Changes"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: {
                                        var r = sync.lastChanges()
                                        if (!r.ok) { win.showToast("No changes or error: " + (r.error||"")); return }
                                        var c = r.changes || {}
                                        var parts = []
                                        if (c.pulled_remote_only) parts.push("pulled_remote_only: " + c.pulled_remote_only.length)
                                        if (c.pushed_local_only) parts.push("pushed_local_only: " + c.pushed_local_only.length)
                                        if (c.updated_local_from_remote) parts.push("updated_local_from_remote: " + c.updated_local_from_remote.length)
                                        if (c.updated_remote_from_local) parts.push("updated_remote_from_local: " + c.updated_remote_from_local.length)
                                        var txt = "Last changes: " + parts.join(", ")
                                        win.showToast(txt)
                                    }
                                }
                                Button {
                                    text: "Copy Preview"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: {
                                        var txt = localOnlyLbl.text + "\n" + remoteOnlyLbl.text + "\n" + remoteNewerLbl.text + "\n" + localNewerLbl.text
                                        clip.setText(txt)
                                        win.showToast("Copied preview to clipboard")
                                    }
                                }
                                Row {
                                    spacing: responsive.baseSpacing/2
                                    Button {
                                        text: "Export Selection..."
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: previewExportSelDialog.open()
                                    }
                                    Button {
                                        text: "Import Selection..."
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: previewImportSelDialog.open()
                                    }
                                }
                            }
                        }
                    }
                    // Confirm apply dialog
                    Dialog {
                        id: confirmApplyDialog
                        modal: true
                        title: "Confirm Apply"
                        standardButtons: Dialog.Ok | Dialog.Cancel
                        property var selection: ({})
                        property string summaryText: ""
                        onAccepted: {
                            var ok = sync.applySelection(selection)
                            if (!ok) {
                                win.lastError = ({ context: "sync_apply", path: sync.syncFile(), error: (sync.lastError ? sync.lastError() : "unknown error"), selection: selection })
                            } else {
                                win.lastError = {}
                            }
                            win.showToast(ok ? "Applied selected changes" : "Apply failed. See Error Details.")
                            if (ok) {
                                previewDialog.close()
                                // Refresh local view models
                                vocabModel = lookup.vocabulary()
                                historyModel = lookup.searchHistory(200)
                            }
                        }
                        contentItem: Column {
                            spacing: responsive.baseSpacing
                            padding: responsive.baseSpacing
                            Label { text: confirmApplyDialog.summaryText; wrapMode: Text.Wrap; width: Math.min(win.width*0.8, 480) }
                        }
                    }
                    // Sync 错误详情弹窗
                    Dialog {
                        id: syncErrorDialog
                        modal: true
                        title: "Sync Error Details"
                        standardButtons: Dialog.Ok
                        contentItem: ScrollView {
                            width: Math.min(win.width*0.9, 600)
                            height: Math.min(win.height*0.6, 420)
                            Column {
                                spacing: 6
                                padding: 8
                                Label { text: "Context: " + (win.lastError.context || ""); wrapMode: Text.Wrap }
                                Label { text: "Sync File: " + (sync.syncFile() || ""); wrapMode: Text.Wrap }
                                TextArea {
                                    text: win.lastError.error || ""
                                    readOnly: true
                                    wrapMode: TextArea.Wrap
                                    width: Math.min(win.width*0.9, 560)
                                    height: 160
                                }
                                Label {
                                    text: "Hints: Ensure the JSON file is valid and writable, choose a proper sync file, or export selection to a new file. You can also retry preview below."
                                    wrapMode: Text.Wrap
                                    width: Math.min(win.width*0.9, 560)
                                    color: Theme.textTertiary
                                }
                                Row {
                                    spacing: responsive.baseSpacing/2
                                    Button {
                                        text: "Copy"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: {
                                            var txt = "Context: " + (win.lastError.context||"") + "\n" +
                                                      "Sync File: " + (sync.syncFile()||"") + "\n" +
                                                      (win.lastError.error||"")
                                            clip.setText(txt)
                                            win.showToast("Copied error details")
                                        }
                                    }
                                    Button {
                                        text: "Choose Sync File..."
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: syncChooseDialog.open()
                                    }
                                    Button {
                                        text: "Open Containing Folder"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: {
                                            var p = sync.syncFile() || ""
                                            if (p.length > 0) {
                                                var pp = p.replace(/\\\\/g,'/'); var idx = pp.lastIndexOf('/'); var dir = (idx>0) ? pp.substring(0, idx) : pp
                                                Qt.openUrlExternally("file://" + dir)
                                            }
                                        }
                                    }
                                    Button {
                                        text: "Retry Preview"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.smallFont
                                        onClicked: {
                                            if (sync.syncFile().length === 0) { win.showToast("Please choose a sync file first"); return }
                                            var m = sync.previewDiff()
                                            if (!m.ok) {
                                                win.lastError = ({ context: "sync_preview", path: sync.syncFile(), error: (m.error||"") })
                                                win.showToast("Preview failed", 1500)
                                            } else {
                                                previewDetails.fill(m)
                                                win.lastError = {}
                                                syncErrorDialog.close()
                                                previewDialog.open()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // Export/Import selection dialogs
                    MobileFileDialog {
                        id: previewExportSelDialog
                        title: "Export Selection"
                        selectExisting: false
                        nameFilters: ["JSON (*.json)","All (*)"]
                        onAccepted: {
                            var path = previewExportSelDialog.fileUrl.toString().replace("file://","")
                            var sel = {
                                remoteOnly: (previewDetails.children[6].children[1].selected ? previewDetails.children[6].children[1].selected() : []),
                                localOnly: (lvLocalOnly.selected ? lvLocalOnly.selected() : []),
                                remoteNewer: (lvRemoteNewer.selected ? lvRemoteNewer.selected() : []),
                                localNewer: (lvLocalNewer.selected ? lvLocalNewer.selected() : [])
                            }
                            var ok = sync.exportSelection(sel, path)
                            win.showToast(ok ? "Exported selection" : "Export selection failed")
                        }
                    }
                    MobileFileDialog {
                        id: previewImportSelDialog
                        title: "Import Selection"
                        selectExisting: true
                        nameFilters: ["JSON (*.json)","All (*)"]
                        onAccepted: {
                            var path = previewImportSelDialog.fileUrl.toString().replace("file://","")
                            var r = sync.importSelection(path)
                            if (!r.ok) { win.showToast("Import failed: " + (r.error||"")); return }
                            var sel = r.selection || {}
                            // Apply to views (set checkboxes)
                            var toLowerSet = function(lst) { var s={}; for (var i=0;i<lst.length;++i) s[lst[i].toLowerCase()] = true; return s }
                            var ro = toLowerSet(sel.remoteOnly || [])
                            var lo = toLowerSet(sel.localOnly || [])
                            var rn = toLowerSet(sel.remoteNewer || [])
                            var ln = toLowerSet(sel.localNewer || [])
                            // For each list, set checked based on presence
                            var lvRemoteOnly = previewDetails.children[6].children[1]
                            if (lvRemoteOnly && lvRemoteOnly.count !== undefined) {
                                for (var i=0;i<lvRemoteOnly.count;++i) { var d = lvRemoteOnly.model.get(i); var item = lvRemoteOnly.itemAtIndex(i); if (item) item.children[0].checked = !!ro[(d||"").toLowerCase()] }
                            }
                            if (lvLocalOnly && lvLocalOnly.count !== undefined) {
                                for (var j=0;j<lvLocalOnly.count;++j) { var d2 = lvLocalOnly.model.get(j); var item2 = lvLocalOnly.itemAtIndex(j); if (item2) item2.children[0].checked = !!lo[(d2||'').toLowerCase()] }
                            }
                            if (lvRemoteNewer && lvRemoteNewer.count !== undefined) {
                                for (var k=0;k<lvRemoteNewer.count;++k) { var d3 = lvRemoteNewer.model.get(k); var item3 = lvRemoteNewer.itemAtIndex(k); if (item3) item3.children[0].checked = !!rn[(d3.word||'').toLowerCase()] }
                            }
                            if (lvLocalNewer && lvLocalNewer.count !== undefined) {
                                for (var k2=0;k2<lvLocalNewer.count;++k2) { var d4 = lvLocalNewer.model.get(k2); var item4 = lvLocalNewer.itemAtIndex(k2); if (item4) item4.children[0].checked = !!ln[(d4.word||'').toLowerCase()] }
                            }
                            win.showToast("Imported selection (not yet applied)")
                        }
                    }

                    // 删除确认弹窗（移动端友好）
                    Dialog {
                        id: vocabRemoveDialog
                        modal: true
                        title: "Remove Word"
                        standardButtons: Dialog.Ok | Dialog.Cancel
                        property string pendingWord: ""
                        onAccepted: {
                            if (pendingWord.length > 0) {
                                lookup.removeVocabularyWord(pendingWord)
                                vocabModel = lookup.vocabulary()
                                pendingWord = ""
                            }
                        }
                        onRejected: pendingWord = ""
                        contentItem: Column {
                            spacing: responsive.baseSpacing
                            padding: responsive.baseSpacing
                            Label {
                                text: "Remove from vocabulary: " + vocabRemoveDialog.pendingWord + " ?"
                                wrapMode: Text.WordWrap
                                font.pixelSize: responsive.normalFont
                            }
                        }
                    }

                    TextField {
                        id: vocabFilter
                        placeholderText: "Filter vocabulary..."
                        width: parent.width
                        height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        onTextChanged: {
                            var all = lookup.vocabulary()
                            if (vocabFilter.text.length === 0) { vocabModel = all; return }
                            var filtered = []
                            for (var i = 0; i < all.length; ++i) {
                                var item = all[i]
                                if (item.word.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1 ||
                                    item.definition.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1) {
                                    filtered.push(item)
                                }
                            }
                            // 笔记内容也参与匹配（Search within notes）：core
                            // 大小写不敏感子串命中词去重并入
                            var noteHits = lookup.searchNotes(vocabFilter.text)
                            for (var n = 0; n < noteHits.length; ++n) {
                                var nw = noteHits[n].word
                                var dup = false
                                for (var f = 0; f < filtered.length; ++f) {
                                    if (filtered[f].word === nw) { dup = true; break }
                                }
                                if (!dup) {
                                    for (var a = 0; a < all.length; ++a) {
                                        if (all[a].word === nw) { filtered.push(all[a]); break }
                                    }
                                }
                            }
                            vocabModel = filtered
                        }
                    }

                    ListView {
                        id: vocabList
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.top: vocabFilter.bottom
                        anchors.topMargin: responsive.baseSpacing
                        model: vocabModel
                        delegate: ItemDelegate {
                            width: parent.width
                            height: Math.max(60, responsive.minTouchTarget + 16)

                            Row {
                                anchors.fill: parent
                                anchors.margins: responsive.baseSpacing
                                spacing: responsive.baseSpacing

                                Label {
                                    id: vocabLabel
                                    width: parent.width - removeBtn.width - 3*responsive.baseSpacing
                                    text: modelData.word + ": " + modelData.definition
                                    font.pixelSize: responsive.normalFont
                                    wrapMode: Text.WordWrap
                                    maximumLineCount: 3
                                    elide: Text.ElideRight
                                }

                                Button {
                                    id: removeBtn
                                    text: "Remove"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    onClicked: {
                                        vocabRemoveDialog.pendingWord = modelData.word
                                        vocabRemoveDialog.open()
                                    }
                                }
                            }

                            onClicked: {
                                win.navigateTo(0)
                                input.text = modelData.word
                            }
                            onPressAndHold: {
                                // Show confirm dialog on long-press (mobile friendly)
                                vocabRemoveDialog.pendingWord = modelData.word
                                vocabRemoveDialog.open()
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.count === 0
                            text: "No vocabulary saved"
                            color: Theme.textTertiary
                            font.pixelSize: responsive.smallFont
                        }
                    }
                }

                MobileFileDialog {
                    id: saveDialog
                    title: "Export Vocabulary CSV"
                    selectExisting: false
                    nameFilters: ["CSV files (*.csv)", "All files (*)"]
                    onAccepted: {
                        var p = saveDialog.fileUrl.toString().replace("file://", "")
                        var ok = lookup.exportVocabCsv(p)
                        win.showToast(ok ? ("Exported CSV: " + p) : "Export failed")
                    }
                }
            }

            // 语音设置 Tab
            Item {
                ScrollView {
                    anchors.fill: parent
                    contentHeight: voiceColumn.implicitHeight

                    Column {
                        id: voiceColumn
                        width: parent.width
                        anchors.margins: responsive.baseMargin
                        spacing: responsive.baseSpacing

                        GroupBox {
                            title: "🎵 语音设置"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                // 语音引擎状态
                                Row {
                                    spacing: responsive.baseSpacing
                                    Label {
                                        text: "引擎状态:"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Label {
                                        text: lookup.availableVoices().length > 0 ? "✅ 可用" : "❌ 不可用"
                                        color: lookup.availableVoices().length > 0 ? Theme.success : Theme.danger
                                        font.pixelSize: responsive.normalFont
                                    }
                                }

                                // 音量控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "音量: " + Math.round(volumeSlider.value * 100) + "%"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: volumeSlider
                                        from: 0.0
                                        to: 1.0
                                        value: 0.8
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setVolume(value)
                                    }
                                }

                                // 语速控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "语速: " + rateSlider.value.toFixed(1) + "x"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: rateSlider
                                        from: 0.1
                                        to: 2.0
                                        value: 1.0
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setRate(value)
                                    }
                                }

                                // 音调控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "音调: " + pitchSlider.value.toFixed(1)
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: pitchSlider
                                        from: -1.0
                                        to: 1.0
                                        value: 0.0
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setPitch(value)
                                    }
                                }
                            }
                        }

                        GroupBox {
                            title: "🎤 可用语音"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            ListView {
                                width: parent.width
                                height: Math.min(200, count * responsive.minTouchTarget)
                                clip: true
                                model: lookup.availableVoices()
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: Math.max(36, responsive.minTouchTarget)
                                    text: modelData
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        lookup.setVoice(modelData)
                                        voiceStatus.text = "当前语音: " + modelData
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "No voices available"
                                    color: Theme.textTertiary
                                    font.pixelSize: responsive.smallFont
                                }
                            }
                        }

                        Label {
                            id: voiceStatus
                            text: "当前语音: 系统默认"
                            font.italic: true
                            font.pixelSize: responsive.normalFont
                        }

                        GroupBox {
                            title: "🧪 测试语音"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                TextField {
                                    id: testText
                                    width: parent.width
                                    height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                                    placeholderText: "输入要测试的文本..."
                                    text: "Hello, this is a pronunciation test."
                                    font.pixelSize: responsive.normalFont
                                }

                                Flow {
                                    width: parent.width
                                    spacing: responsive.baseSpacing

                                    Button {
                                        text: "🎯 测试播放"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.normalFont
                                        onClicked: lookup.speakText(testText.text)
                                    }
                                    Button {
                                        text: "⏹️ 停止"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.normalFont
                                        onClicked: lookup.stopSpeaking()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 🔁复习 Tab（P-7 批四 复习入口简单化：生词本四技能学习状态
            // 清单——任一技能未到「稳」即入列，点技能圈档 未练→不稳→稳。
            // 无调度/等级/激励营销，product-principles §11「预留防迁移，
            // 不是课程化」口径；真数据源 = lookup.vocabularyMeta 四技能键）
            Item {
                ScrollView {
                    anchors.fill: parent
                    contentHeight: reviewColumn.implicitHeight

                    Column {
                        id: reviewColumn
                        width: parent.width
                        anchors.margins: responsive.baseMargin
                        spacing: responsive.baseSpacing

                        GroupBox {
                            id: reviewBox
                            title: "🔁 待复习单词"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            // QVariantList 无变更通知，入列/圈档后手动 reloadReview()
                            property var reviewWords: []

                            function computeReview() {
                                var out = []
                                var all = lookup.vocabularyMeta()
                                for (var i = 0; i < all.length; i++) {
                                    var it = all[i]
                                    if ((it.listen || 0) < 2 || (it.speak || 0) < 2 ||
                                        (it.read || 0) < 2 || (it.write || 0) < 2)
                                        out.push(it)
                                }
                                return out
                            }

                            function reloadReview() {
                                reviewWords = computeReview()
                            }

                            Component.onCompleted: reloadReview()
                            // 每次切到复习页刷新（生词本可能在他处增删）
                            Connections {
                                target: win
                                function onCurrentPageChanged() { reviewBox.reloadReview() }
                            }

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                Label {
                                    width: parent.width
                                    text: "待复习 " + (reviewBox.reviewWords.length || 0) +
                                          " 词 · 点技能圈档：未练 → 不稳 → 稳"
                                    font.pixelSize: responsive.smallFont
                                    color: Theme.textTertiary
                                    wrapMode: Text.WordWrap
                                }

                                ListView {
                                    width: parent.width
                                    height: Math.min(480, (reviewBox.reviewWords.length || 0) * 76)
                                    clip: true
                                    spacing: responsive.baseSpacing / 2
                                    model: reviewBox.reviewWords

                                    delegate: Rectangle {
                                        id: reviewCard
                                        width: ListView.view.width
                                        height: reviewCardCol.implicitHeight + responsive.baseSpacing
                                        color: Theme.card
                                        radius: Theme.radiusS
                                        border.color: Theme.divider

                                        property var entry: modelData
                                        property string entryWord: modelData.word || ""

                                        // 复习动作 = 圈档（写四技能，DataStore 落盘）。
                                        // 四技能全「稳」→ 重算清单让该词出列
                                        function cycleSkill(skill) {
                                            var next = ((entry[skill] || 0) + 1) % 3
                                            if (!lookup.setVocabularySkill(entryWord, skill, next))
                                                return
                                            entry[skill] = next
                                            if ((entry.listen || 0) >= 2 && (entry.speak || 0) >= 2 &&
                                                (entry.read || 0) >= 2 && (entry.write || 0) >= 2)
                                                reviewBox.reloadReview()
                                        }

                                        Column {
                                            id: reviewCardCol
                                            anchors.fill: parent
                                            anchors.margins: responsive.baseSpacing / 2
                                            spacing: responsive.baseSpacing / 2

                                            Label {
                                                text: reviewCard.entryWord
                                                font.pixelSize: responsive.normalFont
                                                font.bold: true
                                                color: Theme.text
                                                TapHandler {
                                                    onTapped: {
                                                        win.navigateTo(0)
                                                        input.text = reviewCard.entryWord
                                                        searchBtn.clicked()
                                                    }
                                                }
                                            }

                                            Label {
                                                width: parent.width
                                                text: {
                                                    var t = lookup.extractTextFromHtml(reviewCard.entry.definition || "")
                                                    t = t.replace(/\s+/g, " ").trim()
                                                    return t.length > 60 ? (t.substring(0, 60) + "…") : t
                                                }
                                                font.pixelSize: responsive.smallFont
                                                color: Theme.textSecondary
                                                elide: Text.ElideRight
                                                visible: text.length > 0
                                            }

                                            Row {
                                                spacing: responsive.baseSpacing / 2

                                                Repeater {
                                                    model: [
                                                        { skill: "listen", label: "听" },
                                                        { skill: "speak", label: "说" },
                                                        { skill: "read", label: "读" },
                                                        { skill: "write", label: "写" }
                                                    ]
                                                    delegate: Button {
                                                        property int lvl: reviewCard.entry[modelData.skill] || 0
                                                        text: modelData.label
                                                        font.pixelSize: responsive.smallFont
                                                        width: (reviewCard.width - responsive.baseSpacing * 2.5) / 4
                                                        Material.background:
                                                            lvl === 2 ? Theme.success
                                                          : lvl === 1 ? Theme.warning
                                                          : Theme.divider
                                                        Material.foreground: lvl === 0 ? Theme.textSecondary : "#ffffff"
                                                        onClicked: reviewCard.cycleSkill(modelData.skill)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Label {
                                    visible: (reviewBox.reviewWords.length || 0) === 0
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: "全部掌握 🎉\n在生词本里加词，点技能圈档开始复习"
                                    font.pixelSize: responsive.smallFont
                                    color: Theme.textTertiary
                                }
                            }
                        }
                    }
                }
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
}
