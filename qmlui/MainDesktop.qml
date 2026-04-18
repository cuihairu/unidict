import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "components"

ApplicationWindow {
    id: win
    visible: true
    width: 1200
    height: 760
    minimumWidth: 900
    minimumHeight: 560
    title: currentWord && currentWord.length > 0 ? ("Unidict · " + currentWord) : "Unidict"

    Material.theme: Material.Light
    Material.primary: "#111827"
    Material.accent: "#3B82F6"
    Material.background: "#F8FAFC"

    property string currentWord: ""
    property string fallbackHtml: ""
    property string statusText: ""
    property int selectedEntryIndex: 0
    property bool showAllDictionaries: true
    property string currentPronunciation: ""
    property bool lastLookupNotFound: false
    property var navBackStack: []
    property var navForwardStack: []
    property var voiceList: []
    property var voicePresetList: []
    property bool clipboardEnabled: false
    property bool clipboardMonitoring: false
    property int clipboardPollMs: 500
    property int clipboardMinLen: 2
    property int clipboardMaxLen: 50
    property int suggestMode: 0 // 0:auto 1:prefix 2:fuzzy 3:wildcard 4:regex
    property real ttsVolume: 0.8
    property real ttsRate: 1.0
    property real ttsPitch: 0.0
    property string searchQuery: ""

    function hasEncryptedDictionary() {
        var _stamp = lookup.dictionariesStamp
        var metas = lookup.dictionariesMeta()
        for (var i = 0; i < metas.length; ++i) {
            var m = metas[i]
            var desc = m.description ? m.description : ""
            if (desc.indexOf("[encrypted]") !== -1) return true
        }
        return false
    }

    function _clampSelectedEntry() {
        if (entriesModel.count <= 0) {
            selectedEntryIndex = 0
            currentPronunciation = ""
            return
        }
        if (selectedEntryIndex < 0) selectedEntryIndex = 0
        if (selectedEntryIndex >= entriesModel.count) selectedEntryIndex = entriesModel.count - 1
        currentPronunciation = entriesModel.get(selectedEntryIndex).pronunciation || ""
    }

    function reloadVoices() {
        voiceList = lookup.availableVoices()
        voicePresetList = lookup.getVoicePresets()
    }

    function syncVoiceSelections() {
        if (voiceList && voiceList.length > 0) {
            var current = lookup.getCurrentVoice()
            var idx = voiceList.indexOf(current)
            if (idx >= 0) voiceCombo.currentIndex = idx
        }
        if (voicePresetList && voicePresetList.length > 0) {
            presetCombo.currentIndex = 0
        }
    }

    function _escapeHtml(s) {
        if (!s) return ""
        return ("" + s)
            .replace(/&/g, "&amp;")
            .replace(/</g, "&lt;")
            .replace(/>/g, "&gt;")
            .replace(/\"/g, "&quot;")
            .replace(/'/g, "&#39;")
    }

    function _toHtml(defRaw) {
        if (!defRaw || defRaw.length === 0) return ""
        if (defRaw.indexOf("<") !== -1 && defRaw.indexOf(">") !== -1) {
            return decorateHtml(lookup.sanitizeHtml(defRaw))
        }
        return decorateHtml(_escapeHtml(defRaw).replace(/\n\n+/g, "</p><p>").replace(/\n/g, "<br/>"), true)
    }

    function decorateHtml(html, isAlreadyBody) {
        var body = html || ""
        if (!isAlreadyBody) {
            body = "<div>" + body + "</div>"
        } else {
            body = "<p>" + body + "</p>"
        }
        body = body.replace(/<a\s/gi, "<a style='color:#2563EB;text-decoration:none;' ")
        body = body.replace(/<pre/gi, "<pre style='white-space:pre-wrap;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;background:#F3F4F6;border:1px solid #E5E7EB;border-radius:8px;padding:10px;'")
        return "<div style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial; font-size:14px; line-height:1.7; color:#111827;'>" +
               body +
               "</div>"
    }

    function _handleLink(link) {
        if (!link || link.length === 0) return
        if (link.indexOf("unidict://lookup") !== 0) return
        var word = ""
        var q = link.indexOf("?word=")
        if (q >= 0) {
            word = decodeURIComponent(link.substring(q + 6))
        } else {
            var slash = link.lastIndexOf("/")
            if (slash >= 0) word = decodeURIComponent(link.substring(slash + 1))
        }
        if (word && word.length > 0) openWord(word)
    }

    function reloadHistory() {
        historyModel.clear()
        var items = lookup.searchHistory(200)
        for (var i = 0; i < items.length; i++) historyModel.append({ "word": items[i] })
    }

    function reloadVocabulary() {
        vocabModel.clear()
        var items = lookup.vocabulary()
        for (var i = 0; i < items.length; i++) {
            var word = items[i].word || ""
            var def = items[i].definition || ""
            var snippet = lookup.extractTextFromHtml(def).replace(/\s+/g, " ").trim()
            vocabModel.append({
                "word": word,
                "snippet": snippet.length > 90 ? (snippet.substring(0, 90) + "…") : snippet
            })
        }
    }

    function reloadSuggestions(prefix) {
        resultsModel.clear()
        if (!prefix || prefix.trim().length === 0) return
        var q = prefix.trim()
        var items = []
        try {
            if (suggestMode === 1) {
                items = lookup.suggestPrefix(q, 50)
            } else if (suggestMode === 2) {
                items = lookup.suggestFuzzy(q, 50)
            } else if (suggestMode === 3) {
                items = lookup.searchWildcard(q, 50)
            } else if (suggestMode === 4) {
                items = lookup.searchRegex(q, 50)
            } else {
                items = lookup.suggestPrefix(q, 50)
                if (!items || items.length === 0) items = lookup.suggestFuzzy(q, 50)
            }
        } catch (e) {
            items = []
        }
        for (var i = 0; i < items.length; i++) resultsModel.append({ "word": items[i] })
    }

    function openWord(word, recordNav) {
        if (!word || word.trim().length === 0) return
        if (recordNav === undefined) recordNav = true
        var w = word.trim()

        if (recordNav && currentWord && currentWord.length > 0 && currentWord !== w) {
            navBackStack.push(currentWord)
            if (navBackStack.length > 100) navBackStack.shift()
            navForwardStack = []
        }

        searchQuery = w
        currentWord = w
        fallbackHtml = ""
        lastLookupNotFound = false

        entriesModel.clear()

        var entries = lookup.aggregateLookup(w, {
            "maxTotalResults": 20,
            "sanitizeHtml": true,
            "rewriteCrossRefs": true
        })

        if (entries && entries.length > 0) {
            for (var i = 0; i < entries.length; i++) {
                var dict = entries[i].dictionary || "unknown"
                var defHtml = entries[i].definition || ""
                entriesModel.append({
                    "dictionary": dict,
                    "pronunciation": entries[i].pronunciation || "",
                    "definitionHtml": decorateHtml(defHtml),
                    "definitionText": lookup.extractTextFromHtml(defHtml)
                })
            }
            selectedEntryIndex = 0
            _clampSelectedEntry()
            statusText = "找到 " + entriesModel.count + " 个词典结果"
        } else {
            var def = lookup.lookupDefinition(w)
            fallbackHtml = _toHtml(def)
            lastLookupNotFound = def && def.startsWith("Word not found")
            if (lastLookupNotFound) {
                statusText = "未找到: " + w
                reloadSuggestions(w)
            } else {
                statusText = "已查询: " + w
            }
        }
        reloadHistory()
    }

    function goBack() {
        if (navBackStack.length <= 0) return
        var prev = navBackStack.pop()
        if (currentWord && currentWord.length > 0) navForwardStack.push(currentWord)
        openWord(prev, false)
    }

    function goForward() {
        if (navForwardStack.length <= 0) return
        var next = navForwardStack.pop()
        if (currentWord && currentWord.length > 0) navBackStack.push(currentWord)
        openWord(next, false)
    }

    Component.onCompleted: {
        reloadHistory()
        reloadVocabulary()
        reloadVoices()
        syncVoiceSelections()
        ttsVolume = lookup.getVolume()
        ttsRate = lookup.getRate()
        ttsPitch = lookup.getPitch()
        clipboardEnabled = lookup.isClipboardAutoLookupEnabled()
        clipboardMonitoring = lookup.isClipboardMonitoring()
        if (clipboardEnabled && !clipboardMonitoring) {
            lookup.startClipboardMonitoring()
        }
        searchQuery = ""
        statusText = lookup.loadedDictionaries().length > 0 ? "就绪" : "未加载词典：请设置 UNIDICT_DICTS 环境变量"
    }

    header: ToolBar {
        Material.elevation: 1
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Label {
                text: "Unidict"
                font.pixelSize: 18
                font.weight: Font.DemiBold
                color: "#111827"
            }

            Label {
                text: {
                    var _stamp = lookup.dictionariesStamp
                    return lookup.loadedDictionaries().length > 0
                        ? ("· " + lookup.loadedDictionaries().length + " 本词典 · " + lookup.indexedWordCount() + " 词条")
                        : "· 未加载词典"
                }
                color: "#6B7280"
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: "历史"
                onClicked: leftPane.currentTabIndex = 1
            }
            ToolButton {
                text: "生词本"
                onClicked: leftPane.currentTabIndex = 2
            }
            ToolButton {
                text: "设置"
                onClicked: toolsDrawer.open()
            }
        }
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 12

        SidebarPanel {
            id: leftPane
            suggestMode: suggestMode
            currentWord: currentWord
            resultsModel: resultsModel
            historyModel: historyModel
            vocabModel: vocabModel
            lookup: lookup
            hasEncryptedDictionary: hasEncryptedDictionary()
            mdictPasswordPlaceholder: lookup.hasMdictPassword()
                ? "MDict password is set (enter to replace)"
                : "Enter MDict password"
            queryText: searchQuery
            onSuggestModeSelected: function(index) {
                suggestMode = index
                reloadSuggestions(searchQuery)
            }
            onQueryTextChanged: function(text) {
                searchQuery = text
                suggestTimer.restart()
            }
            onQuerySubmitted: function(text) {
                searchQuery = text
                openWord(text)
            }
            onQueryCleared: {
                searchQuery = ""
                resultsModel.clear()
            }
            onApplyPasswordRequested: function(password) {
                if (!lookup.setMdictPassword(password)) return
                var ok = lookup.reloadDictionariesFromEnv()
                statusText = ok ? "已重新加载词典" : "重新加载失败（请检查 UNIDICT_DICTS）"
            }
            onClearPasswordRequested: {
                lookup.clearMdictPassword()
                statusText = "已清除 MDict 密码"
            }
            onHistoryTabRequested: reloadHistory()
            onVocabularyTabRequested: reloadVocabulary()
            onResultWordRequested: function(word) {
                searchQuery = word
                openWord(word)
            }
        }

        Pane {
            id: rightPane
            SplitView.fillWidth: true
            Material.elevation: 1
            padding: 16

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: currentWord && currentWord.length > 0 ? currentWord : "—"
                            font.pixelSize: 24
                            font.weight: Font.DemiBold
                            color: "#111827"
                            elide: Text.ElideRight
                        }

                        Label {
                            text: (lastLookupNotFound ? "未找到该词条，试试左侧建议"
                                : (entriesModel.count > 0 ? ("释义 · " + entriesModel.get(selectedEntryIndex).dictionary)
                                   : (currentWord && currentWord.length > 0 ? "释义" : "在左侧输入词条开始查询")))
                            color: "#6B7280"
                            elide: Text.ElideRight
                        }
                    }

                    ToolButton {
                        text: "←"
                        enabled: navBackStack.length > 0
                        onClicked: goBack()
                        ToolTip.visible: hovered
                        ToolTip.text: "返回"
                        ToolTip.delay: 200
                    }

                    ToolButton {
                        text: "→"
                        enabled: navForwardStack.length > 0
                        onClicked: goForward()
                        ToolTip.visible: hovered
                        ToolTip.text: "前进"
                        ToolTip.delay: 200
                    }

                    ToolButton {
                        text: "朗读"
                        enabled: currentWord.length > 0
                        onClicked: lookup.speakText(currentWord)
                    }

                    ToolButton {
                        text: "收藏"
                        enabled: currentWord.length > 0 && entriesModel.count > 0
                        onClicked: {
                            lookup.addToVocabulary(currentWord, entriesModel.get(selectedEntryIndex).definitionHtml)
                            reloadVocabulary()
                            statusText = "已加入生词本: " + currentWord
                        }
                    }

                    ToolButton {
                        text: "复制"
                        enabled: entriesModel.count > 0 || fallbackHtml.length > 0
                        onClicked: {
                            if (entriesModel.count > 0) {
                                clip.setText(entriesModel.get(selectedEntryIndex).definitionText)
                                statusText = "已复制释义 · " + entriesModel.get(selectedEntryIndex).dictionary
                            } else {
                                clip.setText(lookup.extractTextFromHtml(fallbackHtml))
                                statusText = "已复制"
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12
                    visible: entriesModel.count > 0

                    Label {
                        text: currentPronunciation && currentPronunciation.length > 0 ? currentPronunciation : ""
                        color: "#6B7280"
                        visible: text.length > 0
                    }

                    Item { Layout.fillWidth: true }

                    CheckBox {
                        text: "全部词典"
                        checked: showAllDictionaries
                        onToggled: showAllDictionaries = checked
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    EntryResultsPane {
                        anchors.fill: parent
                        entriesModel: entriesModel
                        selectedEntryIndex: selectedEntryIndex
                        showAllDictionaries: showAllDictionaries
                        currentWord: currentWord
                        currentPronunciation: currentPronunciation
                        fallbackHtml: fallbackHtml
                        lookup: lookup
                        clip: clip
                        emptyHtml: decorateHtml("<p style='color:#6B7280'>在左侧输入词条开始查询</p>")
                        onSelectedEntryIndexChanged: function(index) { selectedEntryIndex = index }
                        onPronunciationChanged: function(value) { currentPronunciation = value }
                        onShowAllDictionariesChanged: function(value) { showAllDictionaries = value }
                        onStatusChanged: function(value) { statusText = value }
                        onLinkActivated: function(link) { _handleLink(link) }
                    }
                }
            }
        }
    }

    footer: ToolBar {
        Material.elevation: 1
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label {
                text: statusText
                color: "#6B7280"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                text: "清空历史"
                onClicked: {
                    lookup.clearHistory()
                    reloadHistory()
                    statusText = "历史已清空"
                }
            }
        }
    }

    Drawer {
        id: toolsDrawer
        edge: Qt.RightEdge
        width: Math.min(420, win.width * 0.42)
        modal: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        Pane {
            anchors.fill: parent
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 12

                Label {
                    text: "工具与设置"
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    color: "#111827"
                }

                TabBar {
                    id: toolsTabs
                    Layout.fillWidth: true
                    currentIndex: 0
                    TabButton { text: "取词" }
                    TabButton { text: "语音" }
                    TabButton { text: "词典" }
                    TabButton { text: "快捷键" }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: toolsTabs.currentIndex

                    // 取词
                    ColumnLayout {
                        spacing: 10

                        Switch {
                            text: "剪贴板取词（自动查词）"
                            checked: clipboardEnabled
                            onToggled: {
                                clipboardEnabled = checked
                                lookup.setClipboardAutoLookupEnabled(checked)
                                if (checked) lookup.startClipboardMonitoring()
                                else lookup.stopClipboardMonitoring()
                            }
                        }

                        Label {
                            text: "轮询间隔: " + clipboardPollMs + " ms"
                            color: "#6B7280"
                        }
                        Slider {
                            from: 100
                            to: 2000
                            stepSize: 100
                            value: clipboardPollMs
                            onMoved: {
                                clipboardPollMs = Math.round(value / 100) * 100
                                lookup.setClipboardPollInterval(clipboardPollMs)
                            }
                        }

                        RowLayout {
                            spacing: 10
                            Label { text: "最短"; color: "#6B7280" }
                            SpinBox {
                                from: 1
                                to: 20
                                value: clipboardMinLen
                                onValueChanged: {
                                    clipboardMinLen = value
                                    lookup.setClipboardMinWordLength(value)
                                }
                            }
                            Label { text: "最长"; color: "#6B7280" }
                            SpinBox {
                                from: 10
                                to: 200
                                value: clipboardMaxLen
                                onValueChanged: {
                                    clipboardMaxLen = value
                                    lookup.setClipboardMaxWordLength(value)
                                }
                            }
                        }

                        Label {
                            text: "状态: " + (clipboardMonitoring ? "监控中" : "未监控")
                            color: "#6B7280"
                        }
                    }

                    // 语音
                    ColumnLayout {
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ComboBox {
                                id: presetCombo
                                Layout.fillWidth: true
                                model: voicePresetList
                                onActivated: {
                                    lookup.applyVoicePreset(currentText)
                                }
                            }

                            ToolButton {
                                text: "停止"
                                onClicked: lookup.stopSpeaking()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ComboBox {
                                id: voiceCombo
                                Layout.fillWidth: true
                                model: voiceList
                                onActivated: lookup.setVoice(currentText)
                            }

                            ToolButton {
                                text: "刷新"
                                onClicked: reloadVoices()
                            }
                        }

                        Label { text: "音量"; color: "#6B7280" }
                        Slider {
                            from: 0.0
                            to: 1.0
                            value: ttsVolume
                            onMoved: {
                                ttsVolume = value
                                lookup.setVolume(value)
                            }
                        }

                        Label { text: "语速"; color: "#6B7280" }
                        Slider {
                            from: 0.1
                            to: 2.0
                            value: ttsRate
                            onMoved: {
                                ttsRate = value
                                lookup.setRate(value)
                            }
                        }

                        Label { text: "音调"; color: "#6B7280" }
                        Slider {
                            from: -1.0
                            to: 1.0
                            value: ttsPitch
                            onMoved: {
                                ttsPitch = value
                                lookup.setPitch(value)
                            }
                        }
                    }

                    // 词典
	                    ScrollView {
	                        clip: true
	                        ColumnLayout {
	                            width: parent.width
	                            spacing: 8

                            Label {
                                text: lookup.loadedDictionaries().length > 0
                                    ? ("已加载: " + lookup.loadedDictionaries().length + " 本")
                                    : "未加载词典：请设置 UNIDICT_DICTS"
                                wrapMode: Text.WordWrap
                            }

                            Repeater {
                                model: lookup.dictionariesMeta()
                                delegate: Frame {
                                    width: parent.width
                                    padding: 10
                                    ColumnLayout {
                                        width: parent.width
                                        spacing: 2
                                        Label {
                                            text: (modelData.name || "unknown") + " (" + (modelData.wordCount || 0) + ")"
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            text: modelData.description || ""
                                            color: "#6B7280"
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 快捷键
                    ColumnLayout {
                        spacing: 8
                        Label { text: "Ctrl+K：聚焦搜索框" }
                        Label { text: "Enter：查询；Esc：清空搜索" }
                        Label { text: "Ctrl+1/2/3：切换 结果/历史/生词本" }
                        Label { text: "←/→：释义区返回/前进（本次会话）" }
                    }
                }
            }
        }
    }

    Timer {
        id: suggestTimer
        interval: 120
        repeat: false
        onTriggered: reloadSuggestions(searchQuery)
    }

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: clipboardMonitoring = lookup.isClipboardMonitoring()
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: leftPane.focusSearchField()
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: leftPane.currentTabIndex = 0
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: leftPane.currentTabIndex = 1
    }
    Shortcut {
        sequence: "Ctrl+3"
        onActivated: leftPane.currentTabIndex = 2
    }

    ListModel { id: resultsModel }
    ListModel { id: historyModel }
    ListModel { id: vocabModel }
    ListModel { id: entriesModel }

    Connections {
        target: entriesModel
        function onCountChanged() { win._clampSelectedEntry() }
    }

    Connections {
        target: lookup
        function onClipboardWordDetected(word) {
            if (!clipboardEnabled) return
            openWord(word)
        }
    }

    onSelectedEntryIndexChanged: _clampSelectedEntry()
}
