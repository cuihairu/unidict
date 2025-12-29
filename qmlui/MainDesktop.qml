import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

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

        searchField.text = w
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
        searchField.forceActiveFocus()
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
                text: lookup.loadedDictionaries().length > 0
                    ? ("· " + lookup.loadedDictionaries().length + " 本词典 · " + lookup.indexedWordCount() + " 词条")
                    : "· 未加载词典"
                color: "#6B7280"
            }

            Item { Layout.fillWidth: true }

            ToolButton {
                text: "历史"
                onClicked: sideTabs.currentIndex = 1
            }
            ToolButton {
                text: "生词本"
                onClicked: sideTabs.currentIndex = 2
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

        Pane {
            id: leftPane
            SplitView.preferredWidth: 360
            SplitView.minimumWidth: 280
            SplitView.maximumWidth: 520
            Material.elevation: 1
            padding: 12

            ColumnLayout {
                anchors.fill: parent
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    ComboBox {
                        id: suggestModeCombo
                        model: ["自动", "前缀", "模糊", "通配符", "正则"]
                        currentIndex: suggestMode
                        Layout.preferredWidth: 96
                        onActivated: {
                            suggestMode = currentIndex
                            reloadSuggestions(searchField.text)
                        }
                    }

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: suggestMode === 3 ? "输入通配符，例如 te*t?…" :
                            (suggestMode === 4 ? "输入正则，例如 ^test.* …" : "输入要查询的词条…")
                        selectByMouse: true

                        onTextChanged: {
                            suggestTimer.restart()
                            if (sideTabs.currentIndex !== 0) sideTabs.currentIndex = 0
                        }

                        Keys.onPressed: function(event) {
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                openWord(searchField.text)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                searchField.clear()
                                resultsModel.clear()
                                event.accepted = true
                            }
                        }
                    }

                    ToolButton {
                        text: "查"
                        enabled: searchField.text.trim().length > 0
                        onClicked: openWord(searchField.text)
                    }
                }

                TabBar {
                    id: sideTabs
                    Layout.fillWidth: true
                    Material.elevation: 0
                    currentIndex: 0

                    TabButton { text: "结果" }
                    TabButton { text: "历史" }
                    TabButton { text: "生词本" }

                    onCurrentIndexChanged: {
                        if (currentIndex === 1) reloadHistory()
                        if (currentIndex === 2) reloadVocabulary()
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: sideTabs.currentIndex

                    // 结果
                    ListView {
                        id: resultsList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: resultsModel
                        keyNavigationEnabled: true

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            contentItem: Text {
                                width: parent.width
                                textFormat: Text.RichText
                                text: {
                                    var w = model.word || ""
                                    var q = searchField.text.trim()
                                    if (!q) return win._escapeHtml(w)
                                    var idx = w.toLowerCase().indexOf(q.toLowerCase())
                                    if (idx < 0) return win._escapeHtml(w)
                                    return win._escapeHtml(w.substring(0, idx)) +
                                           "<span style='color:#2563EB;font-weight:600;'>" + win._escapeHtml(w.substring(idx, idx + q.length)) + "</span>" +
                                           win._escapeHtml(w.substring(idx + q.length))
                                }
                                elide: Text.ElideRight
                                color: parent.highlighted ? "#1D4ED8" : "#111827"
                            }
                            highlighted: model.word === currentWord
                            onClicked: openWord(model.word)
                        }

                        ScrollBar.vertical: ScrollBar {}

                        Label {
                            anchors.centerIn: parent
                            visible: resultsList.count === 0
                            text: searchField.text.trim().length > 0 ? "没有建议，按回车直接查询" : "输入词条开始"
                            color: "#9CA3AF"
                        }
                    }

                    // 历史
                    ListView {
                        id: historyList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: historyModel

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            text: model.word
                            highlighted: model.word === currentWord
                            onClicked: openWord(model.word)
                        }

                        ScrollBar.vertical: ScrollBar {}

                        Label {
                            anchors.centerIn: parent
                            visible: historyList.count === 0
                            text: "暂无历史"
                            color: "#9CA3AF"
                        }
                    }

                    // 生词本（词汇表）
                    ListView {
                        id: vocabList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: vocabModel

                        delegate: ItemDelegate {
                            width: ListView.view.width
                            text: model.word
                            highlighted: model.word === currentWord
                            onClicked: openWord(model.word)

                            contentItem: Column {
                                spacing: 2
                                Label {
                                    text: model.word
                                    color: "#111827"
                                }
                                Label {
                                    text: model.snippet
                                    color: "#6B7280"
                                    font.pixelSize: 12
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        ScrollBar.vertical: ScrollBar {}

                        Label {
                            anchors.centerIn: parent
                            visible: vocabList.count === 0
                            text: "暂无生词"
                            color: "#9CA3AF"
                        }
                    }
                }
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
                    Material.elevation: 0
                    padding: 12

                    StackLayout {
                        anchors.fill: parent
                        currentIndex: entriesModel.count > 0 ? (showAllDictionaries ? 0 : 1) : 2

	                        // 全部词典
	                        ScrollView {
	                            clip: true
	                            Column {
	                                width: parent.width
	                                spacing: 10

                                Repeater {
                                    model: entriesModel
                                    delegate: Frame {
                                        width: parent.width
                                        padding: 10
                                        background: Rectangle {
                                            color: "#FFFFFF"
                                            radius: 12
                                            border.color: "#E5E7EB"
                                        }

                                        ColumnLayout {
                                            width: parent.width
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: 8

                                                Label {
                                                    text: model.dictionary || "unknown"
                                                    font.weight: Font.DemiBold
                                                    color: "#111827"
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }

                                                ToolButton {
                                                    text: "朗读"
                                                    onClicked: lookup.speakText(currentWord)
                                                }

                                                ToolButton {
                                                    text: "复制"
                                                    onClicked: {
                                                        clip.setText(model.definitionText || "")
                                                        statusText = "已复制释义 · " + (model.dictionary || "unknown")
                                                    }
                                                }

                                                ToolButton {
                                                    text: "设为当前"
                                                    onClicked: {
                                                        selectedEntryIndex = index
                                                        currentPronunciation = model.pronunciation || ""
                                                        showAllDictionaries = false
                                                    }
                                                }
                                            }

                                            TextEdit {
                                                width: parent.width
                                                readOnly: true
                                                selectByMouse: true
                                                textFormat: TextEdit.RichText
                                                wrapMode: TextEdit.Wrap
                                                text: model.definitionHtml || ""
                                                onLinkActivated: function(link) { _handleLink(link) }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 单词典
                        ColumnLayout {
                            spacing: 10
                            visible: entriesModel.count > 0

                            Flickable {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                clip: true
                                contentWidth: dictTabsRow.implicitWidth
                                interactive: contentWidth > width

                                Row {
                                    id: dictTabsRow
                                    spacing: 6

                                    Repeater {
                                        model: entriesModel
                                        delegate: ToolButton {
                                            text: model.dictionary || "unknown"
                                            checked: index === selectedEntryIndex
                                            checkable: true
                                            onClicked: {
                                                selectedEntryIndex = index
                                                currentPronunciation = model.pronunciation || ""
                                            }
                                        }
                                    }
                                }
                            }

	                        ScrollView {
	                            Layout.fillWidth: true
	                            Layout.fillHeight: true
	                            clip: true
	
	                            TextEdit {
	                                width: parent.width
	                                readOnly: true
	                                selectByMouse: true
	                                textFormat: TextEdit.RichText
	                                wrapMode: TextEdit.Wrap
	                                text: (entriesModel.count > 0 && selectedEntryIndex >= 0 && selectedEntryIndex < entriesModel.count)
	                                    ? (entriesModel.get(selectedEntryIndex).definitionHtml || "")
	                                    : ""
	                                onLinkActivated: function(link) { _handleLink(link) }
	                            }
	                        }
	                        }

                        // 无结果/提示
	                        ScrollView {
	                            clip: true
	                            TextEdit {
	                                width: parent.width
	                                readOnly: true
	                                selectByMouse: true
	                                textFormat: TextEdit.RichText
	                                wrapMode: TextEdit.Wrap
                                text: fallbackHtml.length > 0 ? fallbackHtml : decorateHtml("<p style='color:#6B7280'>在左侧输入词条开始查询</p>")
                                onLinkActivated: function(link) { _handleLink(link) }
                            }
                        }
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
        onTriggered: reloadSuggestions(searchField.text)
    }

    Timer {
        interval: 500
        running: true
        repeat: true
        onTriggered: clipboardMonitoring = lookup.isClipboardMonitoring()
    }

    Shortcut {
        sequence: "Ctrl+K"
        onActivated: searchField.forceActiveFocus()
    }

    Shortcut {
        sequence: "Ctrl+1"
        onActivated: sideTabs.currentIndex = 0
    }
    Shortcut {
        sequence: "Ctrl+2"
        onActivated: sideTabs.currentIndex = 1
    }
    Shortcut {
        sequence: "Ctrl+3"
        onActivated: sideTabs.currentIndex = 2
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
