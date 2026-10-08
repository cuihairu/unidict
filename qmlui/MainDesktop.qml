import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs
import "components"

ApplicationWindow {
    id: win
    visible: true
    width: 1200
    height: 760
    minimumWidth: 900
    minimumHeight: 560
    title: currentWord && currentWord.length > 0 ? ("Unidict · " + currentWord) : "Unidict"

    Material.theme: Theme.dark ? Material.Dark : Material.Light
    Material.primary: Theme.card
    Material.accent: Theme.accent
    Material.background: Theme.window
    Material.foreground: Theme.text

    property string currentWord: ""
    property string fallbackHtml: ""
    property string statusText: ""
    // M3-B 生词本编辑：标签筛选状态 + 全量标签集合（供筛选 chips）
    property string vocabTagFilter: ""
    property var vocabTags: []
    property int selectedEntryIndex: 0
    property bool showAllDictionaries: true
    property string currentPronunciation: ""
    // 欧路面板（docs/design-references/eudic-lookup-page.png）：分组聚合
    // 结果 + 卡头音标 + 匹配层级。resultGroups=[{dictionary,dictionaryId,
    // entries:[…]}]（core searchGrouped），flatEntries 为其摊平形态
    property var resultGroups: []
    property var flatEntries: []
    property var headPhonetics: ({})
    property int matchLevel: -1   // 0 精确 / 1 词形还原 / 2 前缀 / 3 释义包含 / 4 模糊
    property bool lastLookupNotFound: false
    property var navBackStack: []
    property var navForwardStack: []
    property var voiceList: []
    property var voicePresetList: []
    property string pronOnlineStatusText: ""
    property bool clipboardEnabled: false
    property bool clipboardMonitoring: false
    property int clipboardPollMs: 500
    property int clipboardMinLen: 2
    property int clipboardMaxLen: 50
    // P-5 取词窗形态开关：开=剪贴板取词弹悬浮窗（不打扰前台应用）；
    // 关=回退旧行为（主窗直接展示）。持久化在 quicklookup/enabled
    property bool quickLookupEnabled: true
    property int suggestMode: 0 // 0:auto 1:prefix 2:fuzzy 3:wildcard 4:regex
    property real ttsVolume: 0.8
    property real ttsRate: 1.0
    property real ttsPitch: 0.0
    property string searchQuery: ""
    // 根作用域抓取 context 属性：SidebarPanel 子树内同名属性会遮蔽
    property var lookupService: lookup
    // 同理抓 context 的 clipboard（子组件有 property var clip，裸名会被
    // 实例属性遮蔽成自引用——绑定失效取默认 null）
    property var clipService: clip
    // 拼音行只在查询词本身是汉字时展示：查英文词走 fulltext 层时首条
    // 中文条目的 [拼音] 不是当前词的读音，不能冒充卡头音标
    // （CJK 基本区 \u4e00-\u9fff）
    readonly property bool showPinyinPhonetic: (headPhonetics.pinyin || "").length > 0
        && /[\u4e00-\u9fff]/.test(currentWord)

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
        body = body.replace(/<a\s/gi, "<a style='color:" + Theme.accent + ";text-decoration:none;' ")
        body = body.replace(/<pre/gi, "<pre style='white-space:pre-wrap;font-family:ui-monospace,SFMono-Regular,Menlo,Monaco,Consolas,monospace;background:" + Theme.window + ";border:1px solid " + Theme.divider + ";border-radius:" + Theme.radiusM + "px;padding:10px;'")
        return "<div style='font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,Helvetica,Arial; font-size:14px; line-height:1.7; color:" + Theme.text + ";'>" +
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
        // 全量 meta 一次拿：标签集合（筛选 chips）取全集，卡片按当前
        // 筛选取子集（core 按标签筛选保持存储序）
        var all = lookup.vocabularyMeta()
        var tags = []
        for (var j = 0; j < all.length; j++) {
            var ts = all[j].tags || []
            for (var k = 0; k < ts.length; k++) {
                if (tags.indexOf(ts[k]) < 0) tags.push(ts[k])
            }
        }
        vocabTags = tags
        var items = vocabTagFilter.length > 0
            ? lookup.vocabularyByTag(vocabTagFilter) : all
        for (var i = 0; i < items.length; i++) {
            var word = items[i].word || ""
            var def = items[i].definition || ""
            var snippet = lookup.extractTextFromHtml(def).replace(/\s+/g, " ").trim()
            vocabModel.append({
                "word": word,
                "snippet": snippet.length > 90 ? (snippet.substring(0, 90) + "…") : snippet,
                "tags": (items[i].tags || []).join(";"),
                "note": lookup.getVocabNote(word)
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

    // 复位到查词主页空态：供离屏截图沙盒逐场景重放（openWord/切 tab 的
    // 状态会跨尺寸与主题残留）。不动词典/生词本/历史数据，只清查询面。
    function resetHome() {
        currentWord = ""
        searchQuery = ""
        fallbackHtml = ""
        lastLookupNotFound = false
        selectedEntryIndex = 0
        navBackStack = []
        navForwardStack = []
        entriesModel.clear()
        resultsModel.clear()
        resultGroups = []
        flatEntries = []
        headPhonetics = ({})
        matchLevel = -1
        statusText = lookup.loadedDictionaries().length > 0 ? "就绪" : "未加载词典：请设置 UNIDICT_DICTS 环境变量"
    }

    function openWord(word, recordNav) {
        if (!word || word.trim().length === 0) return
        if (recordNav === undefined) recordNav = true
        var w = word.trim()

        if (recordNav && currentWord && currentWord.length > 0 && currentWord !== w) {
            // BUG-010：push/pop 原地改数组不发 changed 信号，enabled 绑定
            // （navBackStack.length > 0）永不重评估 → ←/→ 永远禁用，点了
            // 没反应。必须整赋新数组（copy-on-write）触发绑定
            var backStack = navBackStack.slice()
            backStack.push(currentWord)
            if (backStack.length > 100) backStack.shift()
            navBackStack = backStack
            navForwardStack = []
        }

        searchQuery = w
        currentWord = w
        fallbackHtml = ""
        lastLookupNotFound = false

        entriesModel.clear()
        resultGroups = []
        flatEntries = []
        headPhonetics = ({})
        matchLevel = -1

        var groups = lookup.aggregateLookup(w, {
            "maxTotalResults": 20,
            "sanitizeHtml": true,
            "rewriteCrossRefs": true
        })

        var firstDefText = ""
        if (groups && groups.length > 0) {
            var flat = 0
            for (var g = 0; g < groups.length; g++) {
                var gg = groups[g]
                for (var i = 0; i < gg.entries.length; i++) {
                    var e = gg.entries[i]
                    var dict = e.dictionary || gg.dictionary || "unknown"
                    var defHtml = e.definition || ""
                    var defText = lookup.extractTextFromHtml(defHtml)
                    entriesModel.append({
                        "word": e.word || w,
                        "dictionary": dict,
                        "pronunciation": e.pronunciation || "",
                        "definitionHtml": decorateHtml(defHtml),
                        "definitionText": defText
                    })
                    if (flatEntries.length === 0) matchLevel = e.relevance
                    flatEntries.push(e)
                    if (firstDefText === "") firstDefText = defText
                    flat++
                }
            }
            // 分组卡用带 entries 的原始组结构；组内条目与 flat 视图同序
            resultGroups = groups
            if (firstDefText.length > 0) headPhonetics = lookup.extractPhonetics(firstDefText)
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
        // 同 openWord：整赋触发 changed，enabled 绑定重评估
        var backStack = navBackStack.slice()
        var prev = backStack.pop()
        navBackStack = backStack
        if (currentWord && currentWord.length > 0) navForwardStack = navForwardStack.concat([currentWord])
        openWord(prev, false)
    }

    function goForward() {
        if (navForwardStack.length <= 0) return
        var fwdStack = navForwardStack.slice()
        var next = fwdStack.pop()
        navForwardStack = fwdStack
        if (currentWord && currentWord.length > 0) navBackStack = navBackStack.concat([currentWord])
        openWord(next, false)
    }

    // BUG-010：词条卡朗读统一入口——点击必有状态行反馈（此前本地 TTS
    // 不可用时静默，用户视角就是「点了没反应」）；英/美喇叭经
    // speakWordWithAccent 区分口音（在线发音源），本地 TTS 缺失时明说
    // 去哪开。accent 用发音源口音枚举（0 自动/1 美/2 英/3 澳），-1 表
    // 无口音语义（朗读/拼音）
    function speakHeadword(label, accent) {
        if (lookup.pronSourceMode() === 0 && !lookup.hasLocalTts()) {
            statusText = "本地语音不可用，可在 设置→语音 换用在线发音"
            return
        }
        statusText = "正在朗读（" + label + "）: " + currentWord
        lookup.speakWordWithAccent(currentWord, accent)
    }

    // P-5 取词窗统一入口：word 来自剪贴板取词或热键读剪贴板
    function showQuickLookupFor(word) {
        quickLookupPane.showFor(word)
    }

    // quick_lookup 热键面：读剪贴板，掐首尾标点后交给取词窗。剪贴板
    // 常是整句（复制即查），短语条目在词头库真实存在，不强行截成单词；
    // 超过 200 字符的段落不属于「取词」意图，直接放弃并提示
    function quickLookupFromClipboard() {
        var t = (clip.text() || "").replace(/\s+/g, " ").trim()
        if (!t) {
            statusText = "剪贴板为空，没有可取的词"
            return
        }
        t = t.replace(/^[\s"'“”‘’(\[{]+/, "").replace(/[\s"'“”‘’)\]},.;:!?]+$/, "")
        if (!t) {
            statusText = "剪贴板里没有可取的词"
            return
        }
        if (t.length > 200) {
            statusText = "剪贴板内容过长（>200 字符），不是取词场景"
            return
        }
        quickLookupPane.showFor(t)
    }

    Component.onCompleted: {
        reloadHistory()
        reloadVocabulary()
        reloadVoices()
        syncVoiceSelections()
        pronSourceCombo.currentIndex = lookup.pronSourceMode()
        pronAccentCombo.currentIndex = lookup.pronAccent()
        ttsVolume = lookup.getVolume()
        ttsRate = lookup.getRate()
        ttsPitch = lookup.getPitch()
        clipboardEnabled = lookup.isClipboardAutoLookupEnabled()
        clipboardMonitoring = lookup.isClipboardMonitoring()
        if (clipboardEnabled && !clipboardMonitoring) {
            lookup.startClipboardMonitoring()
        }
        // P-5 取词窗：开关持久化 + 系统级热键自注册（仅支持平台生效，
        // Linux/桌面非 Windows 是 stub，注册静默失败由设置页提示）
        quickLookupEnabled = settings.getBool("quicklookup/enabled", true)
        if (lookup.isGlobalHotkeysSupported()) {
            var hk = settings.getString("quicklookup/hotkey", "Alt+Q")
            if (hk.length > 0) lookup.registerGlobalHotkey("quick_lookup", hk)
        }
        searchQuery = ""
        statusText = lookup.loadedDictionaries().length > 0 ? "就绪" : "未加载词典：请设置 UNIDICT_DICTS 环境变量"
    }

    header: ToolBar {
        Material.elevation: 0
        background: Rectangle {
            color: Theme.window
            // hairline 分隔替代 Material 投影：扁平层次，靠表面色阶区分
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.divider }
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 8
            spacing: 12

            Label {
                text: "Unidict"
                font.pixelSize: 17
                font.weight: Font.DemiBold
                font.letterSpacing: 0.3
                color: Theme.text
            }

            // 词典状态 chip：胶囊底 + accent 点，加载态一眼可辨
            Rectangle {
                radius: height / 2
                implicitHeight: 24
                implicitWidth: statusLabel.implicitWidth + 26
                color: Theme.card
                border.width: 1
                border.color: Theme.divider
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        anchors.verticalCenter: parent.verticalCenter
                        color: lookup.loadedDictionaries().length > 0 ? Theme.success : Theme.warning
                    }
                    Label {
                        id: statusLabel
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            var _stamp = lookup.dictionariesStamp
                            return lookup.loadedDictionaries().length > 0
                                ? (lookup.loadedDictionaries().length + " 本词典 · " + lookup.indexedWordCount() + " 词条")
                                : "未加载词典"
                        }
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }
                }
            }

            Item { Layout.fillWidth: true }

            component HeaderGhostButton : ToolButton {
                leftPadding: 10
                rightPadding: 10
                background: Rectangle {
                    radius: Theme.radiusM
                    color: parent.hovered ? Theme.hoverOverlay : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                contentItem: Text {
                    text: parent.text
                    font.pixelSize: 13
                    color: Theme.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
            HeaderGhostButton {
                objectName: "headerHistoryButton"
                text: "历史"
                onClicked: leftPane.currentTabIndex = 1
            }
            HeaderGhostButton {
                objectName: "headerVocabButton"
                text: "生词本"
                onClicked: leftPane.currentTabIndex = 2
            }
            HeaderGhostButton {
                objectName: "headerSettingsButton"
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
            objectName: "leftPane"
            // win./id 限定：SidebarPanel 作用域内同名实例属性会遮蔽外层
            // 绑定（自引用循环→绑定失效取默认值）。resultsModel 等是 id，
            // id 解析优先于实例属性，裸名即可
            suggestMode: win.suggestMode
            currentWord: win.currentWord
            resultsModel: resultsModel
            historyModel: historyModel
            vocabModel: vocabModel
            lookup: win.lookupService
            vocabTags: win.vocabTags
            vocabTagFilter: win.vocabTagFilter
            // win. 限定：这里处于 SidebarPanel 作用域，同名属性会遮蔽根函数
            hasEncryptedDictionary: win.hasEncryptedDictionary()
            mdictPasswordPlaceholder: lookupService.hasMdictPassword()
                ? "MDict password is set (enter to replace)"
                : "Enter MDict password"
            queryText: searchQuery
            onSuggestModeSelected: function(index) {
                win.suggestMode = index
                reloadSuggestions(searchQuery)
            }
            onQueryTextEdited: function(text) {
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
            // —— M3-B 生词本编辑：变更 → core 落库 → 重载模型 + 状态行 ——
            onVocabTagFilterRequested: function(tag) {
                vocabTagFilter = tag
                reloadVocabulary()
            }
            onVocabAddTagRequested: function(word, tag) {
                if (lookup.addVocabTag(word, tag)) {
                    reloadVocabulary()
                    statusText = "已加标签「" + tag + "」→ " + word
                } else {
                    statusText = "加标签失败（空标签或词条不存在）"
                }
            }
            onVocabRemoveTagRequested: function(word, tag) {
                if (lookup.removeVocabTag(word, tag)) {
                    reloadVocabulary()
                    statusText = "已移除标签「" + tag + "」→ " + word
                } else {
                    statusText = "移除标签失败（词条或标签未命中）"
                }
            }
            onVocabNoteSaveRequested: function(word, note) {
                lookup.setVocabNote(word, note)
                reloadVocabulary()
                statusText = note.length > 0 ? "已保存笔记 → " + word
                                             : "已清除笔记 → " + word
            }
            onVocabExportRequested: vocabExportDialog.open()
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

                    // 词条卡头（欧路口径，docs/design-references/
                    // eudic-lookup-page.png）：大号词头 + 匹配层级灰标签 +
                    // 音标行（英/美喇叭）+ 生词本/笔记/复制轻入口
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            Label {
                                text: currentWord && currentWord.length > 0 ? currentWord : "—"
                                font.pixelSize: 32
                                font.weight: Font.DemiBold
                                color: Theme.text
                                elide: Text.ElideRight
                            }

                            // 匹配层级标签：非精确层或未收录才有（词典没有
                            // 词频/考试数据，不造标签；真实层级标注见
                            // resultGroups 的 relevance）。欧路考试标签 chip
                            // 口径：细描边圆角胶囊、浅灰字
                            Rectangle {
                                visible: matchLevel > 0 || lastLookupNotFound
                                radius: height / 2
                                color: "transparent"
                                border.width: 1
                                border.color: Theme.divider
                                implicitWidth: levelChip.implicitWidth + 14
                                implicitHeight: levelChip.implicitHeight + 5

                                Label {
                                    id: levelChip
                                    anchors.centerIn: parent
                                    objectName: "levelChip"
                                    text: lastLookupNotFound ? "未收录"
                                        : (matchLevel === 1 ? "原形匹配"
                                        : matchLevel === 2 ? "前缀匹配"
                                        : matchLevel === 4 ? "模糊匹配" : "释义匹配")
                                    font.pixelSize: 11
                                    color: Theme.textSecondary
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        // 音标行（欧路口径）：喇叭+语种标签浅灰、音标值正文
                        // 色；英/美喇叭分口音发音，点击必有状态反馈（BUG-010）
                        RowLayout {
                            spacing: 16
                            visible: entriesModel.count > 0 || fallbackHtml.length > 0

                            // 英音
                            RowLayout {
                                visible: (headPhonetics.british || "").length > 0
                                spacing: 2

                                Label {
                                    objectName: "phonSpeakerBr"
                                    text: "🔊"
                                    color: Theme.textTertiary
                                    font.pixelSize: 14

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword("英音", 2)
                                    }
                                }

                                Label {
                                    text: "英"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: headPhonetics.british || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 美音
                            RowLayout {
                                visible: (headPhonetics.american || "").length > 0
                                spacing: 2

                                Label {
                                    objectName: "phonSpeakerUs"
                                    text: "🔊"
                                    color: Theme.textTertiary
                                    font.pixelSize: 14

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword("美音", 1)
                                    }
                                }

                                Label {
                                    text: "美"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: headPhonetics.american || ""
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 拼音（仅中文查询开它时显示）
                            RowLayout {
                                visible: win.showPinyinPhonetic
                                spacing: 2

                                Label {
                                    objectName: "phonSpeakerPy"
                                    text: "🔊"
                                    color: Theme.textTertiary
                                    font.pixelSize: 14

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: win.speakHeadword("拼音", -1)
                                    }
                                }

                                Label {
                                    text: "拼"
                                    color: Theme.textTertiary
                                    font.pixelSize: 12
                                }

                                Label {
                                    text: "[" + (headPhonetics.pinyin || "") + "]"
                                    color: Theme.textSecondary
                                    font.pixelSize: 14
                                }
                            }

                            // 无音标数据时给一个总喇叭（TTS 朗读词头）
                            Label {
                                visible: entriesModel.count > 0
                                         && (headPhonetics.british || "").length === 0
                                         && (headPhonetics.american || "").length === 0
                                         && !win.showPinyinPhonetic
                                objectName: "phonSpeakerFallback"
                                text: "🔊 朗读"
                                color: Theme.textTertiary
                                font.pixelSize: 14

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: win.speakHeadword("朗读", -1)
                                }
                            }

                            Item { Layout.fillWidth: true }

                            // 欧路口径：音标行与轻操作之间一道细竖线分隔
                            Rectangle {
                                visible: currentWord.length > 0
                                width: 1
                                height: 14
                                color: Theme.divider
                            }

                            Label {
                                visible: currentWord.length > 0 && entriesModel.count > 0
                                objectName: "vocabAction"
                                text: "📖 生词本"
                                color: Theme.textSecondary
                                font.pixelSize: 13

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        lookup.addToVocabulary(currentWord, entriesModel.get(0).definitionHtml)
                                        reloadVocabulary()
                                        statusText = "已加入生词本: " + currentWord
                                    }
                                }
                            }

                            Label {
                                visible: currentWord.length > 0
                                objectName: "noteAction"
                                text: "✎ 笔记"
                                color: Theme.textSecondary
                                font.pixelSize: 13

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        notePopup.noteWord = currentWord
                                        noteArea.text = lookup.getVocabNote(currentWord)
                                        notePopup.open()
                                    }
                                }
                            }

                            Label {
                                visible: entriesModel.count > 0 || fallbackHtml.length > 0
                                objectName: "copyAction"
                                text: "⧉ 复制"
                                color: Theme.textSecondary
                                font.pixelSize: 13

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        // win. 限定：本 MouseArea 处于 Item 作用域，
                                        // 裸名 clip 命中 QQuickItem.clip 布尔属性
                                        // （复制点击一直是 TypeError——BUG-010 主诉之一）
                                        if (entriesModel.count > 0) {
                                            win.clipService.setText(entriesModel.get(0).definitionText)
                                            statusText = "已复制释义 · " + entriesModel.get(0).dictionary
                                        } else {
                                            win.clipService.setText(lookup.extractTextFromHtml(fallbackHtml))
                                            statusText = "已复制"
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ToolButton {
                        objectName: "navBackButton"
                        text: "←"
                        enabled: navBackStack.length > 0
                        onClicked: goBack()
                        ToolTip.visible: hovered
                        ToolTip.text: "返回"
                        ToolTip.delay: 200
                    }

                    ToolButton {
                        objectName: "navForwardButton"
                        text: "→"
                        enabled: navForwardStack.length > 0
                        onClicked: goForward()
                        ToolTip.visible: hovered
                        ToolTip.text: "前进"
                        ToolTip.delay: 200
                    }
                }

                Frame {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    // D3：结果面板去外层硬边框盒——分组卡自承结构（与面板
                    // 侧 background:transparent 同一口径），内容直接坐在
                    // rightPane 表面上
                    background: Rectangle { color: "transparent"; border.width: 0 }

                    EntryResultsPane {
                        anchors.fill: parent
                        // win./id 限定同名遮蔽（词头行高亮、例句 tab 的
                        // lookup 调用、空态回落都依赖这些绑定真实生效）
                        groups: resultGroups
                        flatEntries: win.flatEntries
                        entriesModel: entriesModel   // id，裸名即 id
                        currentWord: win.currentWord
                        fallbackHtml: win.fallbackHtml
                        lookup: win.lookupService
                        clip: win.clipService
                        emptyHtml: decorateHtml("<p style='color:" + Theme.textSecondary + ";'>在左侧输入词条开始查询</p>")
                        onStatusReported: function(value) { statusText = value }
                        onLinkActivated: function(link) { _handleLink(link) }
                        onWordRequested: function(word) { openWord(word) }
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
                objectName: "statusLabel"
                text: statusText
                color: Theme.textSecondary
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ToolButton {
                objectName: "clearHistoryButton"
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
        objectName: "toolsDrawer"
        edge: Qt.RightEdge
        width: Math.min(420, win.width * 0.42)
        // 原型口径：抽屉只占右栏上半（下方露出主窗格卡片），不铺满窗高；
        // M6 语音控件增多后内容超高，由 ScrollView 兜底，不再撑高抽屉
        height: 535
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
                    color: Theme.text
                }

                TabBar {
                    id: toolsTabs
                    Layout.fillWidth: true
                    currentIndex: 0
                    TabButton { objectName: "toolsTab0"; text: "取词" }
                    TabButton { objectName: "toolsTab1"; text: "语音" }
                    TabButton { objectName: "toolsTab2"; text: "词典" }
                    TabButton { objectName: "toolsTab3"; text: "快捷键" }
                    TabButton { objectName: "toolsTab4"; text: "同步备份" }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: toolsTabs.currentIndex

                    // 取词
                    ColumnLayout {
                        spacing: 10

                        Switch {
                            objectName: "clipboardSwitch"
                            text: "剪贴板取词（自动查词）"
                            checked: clipboardEnabled
                            onToggled: {
                                clipboardEnabled = checked
                                lookup.setClipboardAutoLookupEnabled(checked)
                                if (checked) lookup.startClipboardMonitoring()
                                else lookup.stopClipboardMonitoring()
                            }
                        }

                        // P-5 取词窗形态：弹悬浮窗不打扰前台应用；关则回退
                        // 主窗直接展示
                        Switch {
                            objectName: "quickLookupSwitch"
                            text: "取词悬浮窗（弹小窗显示释义）"
                            checked: quickLookupEnabled
                            onToggled: {
                                quickLookupEnabled = checked
                                settings.setBool("quicklookup/enabled", checked)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "取词悬浮窗贴光标出现，失焦即收，不打断当前阅读；"
                                  + "关闭后剪贴板取词回到主窗直接展示。"
                            font.pixelSize: 12
                            color: Theme.textTertiary
                        }

                        Label {
                            text: "轮询间隔: " + clipboardPollMs + " ms"
                            color: Theme.textSecondary
                        }
                        Slider {
                            objectName: "pollSlider"
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
                            Label { text: "最短"; color: Theme.textSecondary }
                            SpinBox {
                                objectName: "minLenSpin"
                                from: 1
                                to: 20
                                value: clipboardMinLen
                                onValueChanged: {
                                    clipboardMinLen = value
                                    lookup.setClipboardMinWordLength(value)
                                }
                            }
                            Label { text: "最长"; color: Theme.textSecondary }
                            SpinBox {
                                objectName: "maxLenSpin"
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
                            color: Theme.textSecondary
                        }
                    }

                    // 语音（M6 控件已多于窗高，走 ScrollView 收纳）
                    ScrollView {
                        clip: true
                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ComboBox {
                                id: presetCombo
                                objectName: "presetCombo"
                                Layout.fillWidth: true
                                model: voicePresetList
                                onActivated: {
                                    lookup.applyVoicePreset(currentText)
                                }
                            }

                            ToolButton {
                                objectName: "stopSpeakButton"
                                text: "停止"
                                onClicked: lookup.stopSpeaking()
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            ComboBox {
                                id: voiceCombo
                                objectName: "voiceCombo"
                                Layout.fillWidth: true
                                model: voiceList
                                onActivated: lookup.setVoice(currentText)
                            }

                            ToolButton {
                                objectName: "refreshVoicesButton"
                                text: "刷新"
                                onClicked: reloadVoices()
                            }
                        }

                        Label { text: "音量"; color: Theme.textSecondary }
                        Slider {
                            objectName: "volumeSlider"
                            from: 0.0
                            to: 1.0
                            value: ttsVolume
                            onMoved: {
                                ttsVolume = value
                                lookup.setVolume(value)
                            }
                        }

                        Label { text: "语速"; color: Theme.textSecondary }
                        Slider {
                            objectName: "rateSlider"
                            from: 0.1
                            to: 2.0
                            value: ttsRate
                            onMoved: {
                                ttsRate = value
                                lookup.setRate(value)
                            }
                        }

                        Label { text: "音调"; color: Theme.textSecondary }
                        Slider {
                            objectName: "pitchSlider"
                            from: -1.0
                            to: 1.0
                            value: ttsPitch
                            onMoved: {
                                ttsPitch = value
                                lookup.setPitch(value)
                            }
                        }

                        // 发音源三态与口音偏好（在线源 dictionaryapi.dev，
                        // 免密钥无配额）。默认本地——开在线是显式动作
                        Label { text: "发音源"; color: Theme.textSecondary }
                        ComboBox {
                            id: pronSourceCombo
                            objectName: "pronSourceCombo"
                            Layout.fillWidth: true
                            model: ["本地语音（系统 TTS）", "在线发音（dictionaryapi.dev）", "自动（在线优先，失败回落本地）"]
                            onActivated: {
                                lookup.setPronSourceMode(currentIndex)
                                pronPrivacyHint.visible = currentIndex !== 0
                            }
                        }

                        Label { text: "口音（在线发音）"; color: Theme.textSecondary }
                        ComboBox {
                            id: pronAccentCombo
                            objectName: "pronAccentCombo"
                            Layout.fillWidth: true
                            model: ["自动", "美音", "英音", "澳音"]
                            onActivated: lookup.setPronAccent(currentIndex)
                        }

                        Label {
                            id: pronPrivacyHint
                            visible: lookup.pronSourceMode() !== 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "开启在线发音后，播报会把查询词发送给发音服务 dictionaryapi.dev（仅查询词，不带历史与生词本）。"
                            color: Theme.textSecondary
                            font.pixelSize: 12
                        }

                        Label {
                            id: pronOnlineStatusLabel
                            visible: pronOnlineStatusText.length > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: pronOnlineStatusText
                            color: Theme.textSecondary
                            font.pixelSize: 12
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
                                            color: Theme.textSecondary
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

                        // P-5 取词窗系统级热键（quick_lookup）：按下读剪贴板
                        // 弹取词窗。注册面走 GlobalHotkeys（仅 Windows 生效，
                        // 其余平台是 stub），注册结果如实回显
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: "取词窗热键" }
                            TextField {
                                id: quickHotkeyField
                                Layout.fillWidth: true
                                text: settings.getString("quicklookup/hotkey", "Alt+Q")
                                placeholderText: "如 Alt+Q"
                            }
                            Button {
                                objectName: "hotkeyApplyButton"
                                text: "应用"
                                onClicked: {
                                    lookup.unregisterGlobalHotkey("quick_lookup")
                                    var seq = quickHotkeyField.text.trim()
                                    if (seq.length === 0) {
                                        hotkeyState.text = "已清除取词热键"
                                        return
                                    }
                                    var ok = lookup.registerGlobalHotkey("quick_lookup", seq)
                                    hotkeyState.text = ok
                                        ? ("已生效: " + seq)
                                        : "注册失败：换个组合试试（避免与系统/其他程序冲突）"
                                    if (ok) settings.setString("quicklookup/hotkey", seq)
                                }
                            }
                        }
                        Label {
                            id: hotkeyState
                            visible: text.length > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }
                        Label {
                            visible: !lookup.isGlobalHotkeysSupported()
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "当前平台不支持系统级快捷键；复制内容后用「剪贴板取词」触发取词窗即可。"
                            font.pixelSize: 12
                            color: Theme.textTertiary
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "取词窗热键按下 → 读取剪贴板内容弹窗释义（等效复制后自动取词）。"
                            font.pixelSize: 12
                            color: Theme.textTertiary
                        }
                    }

                    // 同步备份（B7：同步默认关闭、显式开启明示范围；
                    // 口令加密备份自救口。传输绑定归 B5 剩余，先落开关与设置）
                    // 内容高于抽屉固定高，同语音 tab 走 ScrollView 收纳
                    ScrollView {
                        clip: true
                        ColumnLayout {
                            width: parent.width
                            spacing: 10

                        Switch {
                            id: syncSwitch
                            objectName: "syncSwitch"
                            text: "多设备同步"
                            checked: syncManager.syncEnabled
                            onToggled: {
                                if (checked) {
                                    // 开启须经「确认开启」按钮——范围明示常驻上方
                                    checked = false
                                    syncState.text = "先确认同步范围，再点「确认开启」"
                                } else {
                                    syncManager.disable()
                                    syncState.text = "已关闭同步（默认不连接任何服务器）"
                                }
                            }
                        }

                        Label {
                            id: syncScopeLabel
                            objectName: "syncScopeLabel"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: syncManager.scopeText()
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: "组 ID" }
                            TextField {
                                id: syncGidField
                                objectName: "syncGidField"
                                Layout.fillWidth: true
                                text: syncManager.groupId
                                placeholderText: "16-64 位字母/数字/_/-"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: "同步形态" }
                            ComboBox {
                                id: syncFormCombo
                                Layout.fillWidth: true
                                textRole: "label"
                                model: [
                                    { label: "自建中转（自有服务器）", value: "selfhost" },
                                    { label: "官方托管（Worker 地址）", value: "hosted" },
                                    { label: "局域网直传（接入中）", value: "lan" }
                                ]
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: syncFormCombo.currentIndex === 0

                            Label { text: "中转地址" }
                            TextField {
                                id: syncRelayField
                                objectName: "syncRelayField"
                                Layout.fillWidth: true
                                text: syncManager.relayUrl
                                placeholderText: "如 http://192.168.1.10:8788"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            visible: syncFormCombo.currentIndex === 1

                            Label { text: "托管地址" }
                            TextField {
                                id: syncHostedField
                                objectName: "syncHostedField"
                                Layout.fillWidth: true
                                text: syncManager.hostedUrl
                                placeholderText: "https://unidict-sync.<你的子域>.workers.dev"
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "本机设备标识：" + syncManager.deviceId
                            font.pixelSize: 12
                            color: Theme.textTertiary
                        }

                        Button {
                            objectName: "syncApplyButton"
                            text: syncManager.syncEnabled
                                  ? "重新保存设置" : "确认开启（我已了解同步范围）"
                            onClicked: {
                                if (syncManager.enable(syncGidField.text.trim())) {
                                    syncManager.transportForm =
                                        syncFormCombo.model[syncFormCombo.currentIndex].value
                                    if (syncFormCombo.currentIndex === 0)
                                        syncManager.relayUrl = syncRelayField.text.trim()
                                    if (syncFormCombo.currentIndex === 1)
                                        syncManager.hostedUrl = syncHostedField.text.trim()
                                    syncState.text = "同步设置已保存，可点「立即同步」"
                                } else {
                                    syncState.text = syncManager.lastError()
                                }
                            }
                        }

                        Button {
                            objectName: "syncNowButton"
                            text: "立即同步"
                            enabled: syncManager.syncEnabled
                            onClicked: {
                                if (syncManager.syncNow())
                                    syncState.text = syncManager.syncStatusText()
                                else
                                    syncState.text = syncManager.lastError()
                            }
                        }

                        Label {
                            id: syncState
                            objectName: "syncStateLabel"
                            visible: text.length > 0
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 12
                            color: Theme.textSecondary
                        }

                        Label {
                            text: "加密备份（防设备全丢）"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "备份生词本（词/分组标签/笔记）：口令加密、不经过任何服务器。"
                                  + "恢复时缺的补上、已有的不动；口令遗失无法解密，请牢记。"
                            font.pixelSize: 12
                            color: Theme.textTertiary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: "口令" }
                            TextField {
                                id: backupPassField
                                objectName: "backupPassField"
                                Layout.fillWidth: true
                                echoMode: TextInput.Password
                                placeholderText: "备份口令"
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label { text: "文件" }
                            TextField {
                                id: backupPathField
                                objectName: "backupPathField"
                                Layout.fillWidth: true
                                text: documentsPath + "/Unidict/unidict-backup.udbk"
                            }
                        }

                        RowLayout {
                            spacing: 8

                            Button {
                                objectName: "backupExportButton"
                                text: "导出备份"
                                onClicked: {
                                    if (backupPassField.text.length === 0) {
                                        syncState.text = "请先输入备份口令"
                                        return
                                    }
                                    syncState.text = syncManager.exportBackup(
                                        backupPathField.text, backupPassField.text)
                                        ? ("备份已导出：" + backupPathField.text)
                                        : syncManager.lastError()
                                }
                            }

                            Button {
                                objectName: "backupRestoreButton"
                                text: "恢复备份"
                                onClicked: {
                                    if (backupPassField.text.length === 0) {
                                        syncState.text = "请先输入备份口令"
                                        return
                                    }
                                    var r = syncManager.restoreBackup(
                                        backupPathField.text, backupPassField.text)
                                    syncState.text = r.ok
                                        ? ("已恢复 " + r.words + " 词 / " + r.notes
                                           + " 笔记 / " + r.tags + " 标签")
                                        : (r.error || syncManager.lastError())
                                }
                            }
                        }
                    }
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

    // 同步开关状态回读：开启走「确认开启」按钮（onToggled 曾把 checked
    // 打回），enabledChanged 时以管理器实际状态为准；同步轮次结束回读
    // 状态行（位点/待发/错误）
    Connections {
        target: syncManager
        function onEnabledChanged() { syncSwitch.checked = syncManager.syncEnabled }
        function onSyncStateChanged() {
            if (syncState.visible) syncState.text = syncManager.syncStatusText()
        }
    }

    Connections {
        target: lookup
        function onClipboardWordDetected(word) {
            if (!clipboardEnabled) return
            // P-5：取词窗形态弹悬浮窗（前台应用不被打扰）；关闭时回退
            // 旧行为（主窗直接展示）
            if (quickLookupEnabled) quickLookupPane.showFor(word)
            else openWord(word)
        }
        // quick_lookup 热键 → 读剪贴板弹取词窗；show_window → 主窗前置
        function onQuickLookupRequested() {
            win.quickLookupFromClipboard()
        }
        function onShowMainWindowRequested() {
            win.show()
            win.raise()
            win.requestActivate()
        }
        function onPronOnlineStatus(message) {
            pronOnlineStatusText = message
            // BUG-010：发音链路状态镜像到主页脚状态行——设置抽屉没开时，
            // 发音点击照样有可观测反馈（含「本地语音不可用」的明示）
            statusText = message
        }
    }

    // P-5 悬浮取词窗（无边框置顶小窗，默认隐藏）
    QuickLookupPane {
        id: quickLookupPane
        lookup: win.lookupService
    }

    Connections {
        target: quickLookupPane
        // 取词窗「在主窗打开」：收窗 → 主窗词条卡 → 前置主窗
        function onOpenInMainRequested(word) {
            quickLookupPane.close()
            openWord(word)
            win.show()
            win.raise()
            win.requestActivate()
        }
        function onStatusReported(message) {
            statusText = message
        }
    }

    // M3-B 生词本 CSV 导出目标选择（core export_vocabulary_csv 口径：
    // UTF-8 BOM + word,definition,tags,note 四列）
    FileDialog {
        id: vocabExportDialog
        title: "导出生词本 CSV"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)", "所有文件 (*)"]
        defaultSuffix: "csv"
        onAccepted: {
            var path = decodeURIComponent(
                selectedFile.toString().replace(/^file:\/\//, ""))
            statusText = lookup.exportVocabCsv(path)
                ? "已导出 CSV → " + path
                : "导出失败（路径不可写）→ " + path
        }
    }

    // 词条卡头的笔记轻入口（core setVocabNote 口径：空串保存即删除）
    Popup {
        id: notePopup
        objectName: "notePopup"
        property string noteWord: ""
        anchors.centerIn: parent
        width: 440
        height: 260
        padding: 16
        modal: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: "笔记 · " + notePopup.noteWord
                font.weight: Font.DemiBold
                color: Theme.text
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: noteArea
                    wrapMode: TextArea.Wrap
                    placeholderText: "为该词条记点什么（清空保存即删除）"
                    color: Theme.text
                }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignRight

                Button {
                    objectName: "noteCancelButton"
                    flat: true
                    text: "取消"
                    onClicked: notePopup.close()
                }
                Button {
                    objectName: "noteSaveButton"
                    text: "保存"
                    onClicked: {
                        lookup.setVocabNote(notePopup.noteWord, noteArea.text)
                        statusText = noteArea.text.length > 0
                            ? "已保存笔记: " + notePopup.noteWord
                            : "已删除笔记: " + notePopup.noteWord
                        notePopup.close()
                    }
                }
            }
        }
    }

    onSelectedEntryIndexChanged: _clampSelectedEntry()
}
