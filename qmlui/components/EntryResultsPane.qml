import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 查词结果面板（欧路词典口径，docs/design-references/eudic-lookup-page.png）：
// 内容 Tab 栏（词典/例句/词组/近义联想/全文检索，选中浅蓝）+ 分组卡视图。
// 「词典」tab 每词典一个可折叠分组（标题=词典名+折叠箭头，浅灰细线分隔，
// 不用硬边框），同一词典的释义聚在组内——多字典不平铺混排；层级与去重
// 由 core searchGrouped 完成（relevance：0 词头精确 / 1 词形还原 /
// 2 前缀 / 3 释义包含 / 4 词头模糊）。
Frame {
    id: root
    objectName: "entryResultsPane"

    property var groups: []          // [{dictionary, dictionaryId, entries:[…]}]
    property var flatEntries: []     // groups 的摊平形态（兼容旧引用）
    property var entriesModel        // 兼容旧引用（flat 条目模型）
    property string currentWord: ""
    property string fallbackHtml: ""
    // 释义字号（F7）：MainDesktop 传 win.defFontSize（ui/def_font_size
    // 落盘），结果卡正文的 TextEdit 基准字号随之翻面
    property int defFontSize: 14
    property var lookup
    property var clip
    // TTS 服务（MainDesktop 传 win.voiceService）：词条/例句喇叭朗读依赖；
    // 未接线时两个 speak 调用点报 speakText of undefined 且朗读哑掉
    property var voice
    // 未收录态（界面重设计批）：原始英文错误不再直出，改设计态 + 建议词
    property bool notFound: false
    property string notFoundWord: ""
    property var suggestions: []

    signal statusReported(string value)
    signal linkActivated(string link)
    signal wordRequested(string word)
    // 单一居中空态的两个引导动作（原型 empty 区）
    signal emptyLookupRequested(string word)
    signal openDictsRequested()

    Material.elevation: 0
    padding: 0
    // 面板本体融入 rightPane（D2 elevation0 + hairline 语言），分组卡自带头
    // 部与描边承担结构——不再套一层硬边框盒
    background: Rectangle { color: "transparent"; border.width: 0 }

    // 分组折叠状态：dictionaryId -> true（默认全展开）
    property var collapsed: ({})

    // UI 批一：内容 tab 分区折叠状态（例句/词组/近义联想按分区键），查词
    // 切换与词典分组折叠一同复位；命中词典 chip 的「只看该词典」过滤
    property var contentCollapsed: ({})
    property string soloGid: ""
    onCurrentWordChanged: {
        tabData = ({})
        collapsed = ({})
        contentCollapsed = ({})
        soloGid = ""
    }

    function toggleContent(key) {
        var next = {}
        Object.keys(contentCollapsed).forEach(function(k) { next[k] = contentCollapsed[k] })
        next[key] = !contentCollapsed[key]
        contentCollapsed = next
    }

    // 摊平条目按字段分区（保持首现序）：分组键缺值归「其他」，labelFn
    // 可覆盖取键（近义联想按 source 出中文分区名）
    function groupItems(items, field, labelFn) {
        var keys = [], byKey = {}
        for (var i = 0; i < items.length; i++) {
            var it = items[i] || {}
            var k = labelFn ? labelFn(it) : (it[field] || "其他")
            if (!byKey[k]) { byKey[k] = []; keys.push(k) }
            byKey[k].push(it)
        }
        var out = []
        for (var j = 0; j < keys.length; j++)
            out.push({ key: keys[j], items: byKey[keys[j]] })
        return out
    }

    function sourceLabel(it) {
        if (it.source === "fuzzy")
            return Theme.zh ? "模糊联想" : "Fuzzy matches"
        return Theme.zh ? "前缀候选" : "Prefix candidates"
    }

    // 内容 tab 懒取缓存：key -> QVariantList；查词切换即失效
    property var tabData: ({})

    // BUG-010：例句/全文 tab 首查走 fullTextLookup，大词库是秒级阻塞——
    // 点击先切 tab、报「检索中」状态行，让一帧渲染后再取数（Timer 30ms），
    // 消除「点了没反应」的观感
    property string pendingTabKey: ""
    readonly property var heavyTabs: ["examples", "fulltext"]
    Timer {
        id: tabLoadTimer
        interval: 30
        onTriggered: {
            root.ensureTabData(root.pendingTabKey)
            var n = (root.tabData[root.pendingTabKey] || []).length
            root.pendingTabKey = ""
            root.statusReported(Theme.zh ? ("共 " + n + " 条") : (n + " items"))
        }
    }

    function ensureTabData(key) {
        if (tabData.hasOwnProperty(key)) return
        // BUG-010：tabData[key]=… 原地变异不发 changed 信号，ListView 的
        // model: tabData[key] 绑定永不重评估 → 四个内容 tab 永远空表。
        // 必须整赋新对象触发绑定
        var data = {}
        Object.keys(tabData).forEach(function(k) { data[k] = tabData[k] })
        if (!lookup || currentWord.length === 0) {
            data[key] = []
        } else if (key === "examples" || key === "fulltext") {
            data[key] = lookup.fullTextLookup(currentWord, 20)
        } else if (key === "phrases") {
            data[key] = lookup.relatedLookup(currentWord, "phrases")
        } else if (key === "related") {
            data[key] = lookup.relatedLookup(currentWord, "related")
        }
        tabData = data
    }

    function requestTab(key) {
        if (heavyTabs.indexOf(key) >= 0 && !tabData.hasOwnProperty(key)
                && currentWord.length > 0 && lookup) {
            statusReported(Theme.zh ? "检索中…（首次构建索引稍慢）"
                                    : "Searching… (first run builds the index)")
            pendingTabKey = key
            tabLoadTimer.restart()
        } else {
            ensureTabData(key)
        }
    }

    // 例句高亮：纯文本里把查询词染成链接蓝（大小写不敏感；文本已 sanitize
    // 过的纯文本再 escape，安全拼接富文本）
    function highlightWord(text, word) {
        if (!word || word.length === 0 || !text) return (text || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        var esc = text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        var escWord = word.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
        var re = new RegExp(escWord.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"), "gi")
        return esc.replace(re, "<font color='" + Theme.link + "'>" + escWord + "</font>")
    }

    function escapeHtml(text) {
        return (text || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
    }

    // 片段预览（gap 文档 P1 "长释义需要 snippet"）：折叠空白后在词边界
    // 收尾截断，余额用省略号。列表行（例句/全文 tab）160 字符足够一行
    // 预览；搜索建议的 90 字符口径在 MainDesktop 侧，两者分开
    function snippet(text, maxLen) {
        if (!text) return ""
        var t = text.replace(/\s+/g, " ").trim()
        if (t.length <= maxLen) return t
        var cut = t.substring(0, maxLen)
        var lastSpace = cut.lastIndexOf(" ")
        if (lastSpace > maxLen * 0.6) cut = cut.substring(0, lastSpace)
        return cut + "…"
    }

    // MDict 富文本的命中标注换主题色：core highlight_term 只落中性的
    // <span class="udict-hl"> marker，这里定点换成本主题链接蓝（正则
    // 锚定 marker 的 class 属性，文档自身的 span 不受波及）
    function markToThemeFont(html) {
        if (!html) return ""
        return html.replace(/<span class="udict-hl">([\s\S]*?)<\/span>/g,
                            "<font color='" + Theme.link + "'>$1</font>")
    }

    // 纯文本释义按 "; " 切义项逐行（CC-CEDICT 形态 "[拼音] a; b; c"——
    // 义项糊成一坨正是「乱」观感的来源之一）；MDict 富文本走 HTML 管线：
    // 文本节点级命中词标注（lookup.highlightHtml → core highlight_term）。
    // 分号若收尾 HTML 实体（&amp;）不切，防止切在转义产物内部。
    // 义项内的英文括号注释（(idiom)/(lit.)/(of …)）染三级灰弱化——欧路
    // 词典的词性/语域标签槽位，用真实数据呈现（CC-CEDICT 无词性字段，
    // 括号注释是仅有的结构化标注）
    function formatDefinition(entry) {
        var def = (entry && entry.definition) || ""
        if (def.length === 0) return ""
        if (entry.metadata && entry.metadata.format === "MDict")
            return root.lookup ? root.markToThemeFont(
                                     root.lookup.highlightHtml(def, root.currentWord)) : def
        // HTML 富文本释义（StarDict sametypesequence=h / WikDict 等外部数据）：
        // 走白名单渲染路径（sanitize → 高亮 → 主题色），不能落纯文本路径——
        // highlightWord 会把标签转义成 &lt;div&gt; 糊屏（缺陷单 2026-10-13）。
        // 清洗后无文本时 renderRichDefinition 内部降级为转义纯文本。
        if (def.indexOf("<") !== -1 && def.indexOf(">") !== -1)
            return root.lookup
                    ? root.lookup.renderRichDefinition(def, root.currentWord, Theme.link)
                    : root.escapeHtml(def)
        var lines = []
        var cur = ""
        for (var i = 0; i < def.length; i++) {
            var ch = def.charAt(i)
            if (ch === ";" && i + 1 < def.length && def.charAt(i + 1) === " "
                    && !/&[a-zA-Z#][a-zA-Z0-9]{0,9}$/.test(cur)) {
                lines.push(cur)
                cur = ""
                i++  // 跳过分号后的空格
            } else {
                cur += ch
            }
        }
        lines.push(cur)
        for (var j = 0; j < lines.length; j++) {
            // 命中词高亮先做（highlightWord 内部 escape 后正则替换，与例句/
            // 全文 tab 同惯用法）；括号弱化后做——font 标签无括号，互不踩
            lines[j] = root.highlightWord(lines[j], root.currentWord)
            lines[j] = lines[j].replace(/\([A-Za-z][^()]*\)/g,
                "<font color='" + Theme.textTertiary + "'>$&</font>")
        }
        // TextEdit 无 lineHeight 属性：行高用 Qt rich text 的 line-height
        return "<div style='line-height:1.55'>" + lines.join("<br/>") + "</div>"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 内容 Tab 栏（原型 entry-tabs）：选中玫瑰文字 + 2px 玫瑰下划线，
        // 13px/500，点击懒取数据
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Repeater {
                model: [
                    { key: "dict", label: Theme.zh ? "词典" : "Dictionary" },
                    { key: "examples", label: Theme.zh ? "例句" : "Examples" },
                    { key: "phrases", label: Theme.zh ? "词组" : "Phrases" },
                    { key: "related", label: Theme.zh ? "近义联想" : "Related" },
                    { key: "fulltext", label: Theme.zh ? "全文检索" : "Full text" }
                ]

                delegate: Item {
                    required property var modelData
                    required property int index
                    readonly property bool active: contentTab.currentIndex === index
                    objectName: "contentTab_" + modelData.key
                    implicitWidth: tabLabel.implicitWidth
                    implicitHeight: tabLabel.implicitHeight + indicator.height

                    // 悬停 ghost 面（非选中 tab，D2 hoverOverlay 语言）
                    Rectangle {
                        anchors.fill: tabLabel
                        radius: Theme.radiusS
                        color: Theme.hoverOverlay
                        visible: tabMouse.containsMouse && !parent.active
                    }

                    Label {
                        id: tabLabel
                        text: modelData.label
                        font.pixelSize: 13
                        font.weight: parent.active ? Font.DemiBold : Font.Normal
                        color: parent.active ? Theme.accent : Theme.textSecondary
                        padding: 6

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                // 先切 tab（立即有视觉反馈），懒取数据走
                                // requestTab：重组 tab 给加载状态行
                                contentTab.currentIndex = index
                                root.requestTab(modelData.key)
                            }
                        }
                    }

                    // 选中态底部玫瑰条（原型 tab 口径：选中项与内容区连通）
                    Rectangle {
                        id: indicator
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.bottom: parent.bottom
                        width: tabLabel.implicitWidth - 8
                        height: 2
                        radius: 1
                        color: Theme.accent
                        visible: parent.active
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.divider
        }

        // 内容页集合：显式 visible 绑定（StackLayout 的隐式页管理对内嵌
        // ScrollView 空态 Label 有互串问题，这里直接按索引切）
        QtObject {
            id: contentTab
            objectName: "contentTabState"
            property int currentIndex: 0
        }

        // ---- 词典：多词典分组卡（D3：卡头/分组/行动作行）----
        // 命中词典 chip 概览行（UI 批一）+ 每词典一张 GroupCard（radiusL +
        // divider 描边，卡间 8px 呼吸）；卡内每词条一行：词头蓝链（层级
        // 标记）+ 右侧朗读/复制轻动作
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 0 && root.groups.length > 0
            clip: true
            contentWidth: availableWidth
            padding: 4

                Column {
                    width: parent.width
                    spacing: 8

                    // 命中词典 chip 概览：词典名 · 条数，点击只看该词典
                    // （再点取消）；chips 即各分组卡的聚焦入口
                    Flow {
                        width: parent.width
                        visible: root.groups.length > 1
                        spacing: 6

                        Repeater {
                            model: root.groups

                            delegate: Rectangle {
                                id: dictChip
                                required property var modelData
                                required property int index
                                readonly property string gid: modelData.dictionaryId || modelData.dictionary
                                readonly property bool solo: root.soloGid === gid
                                objectName: "dictChip_" + index
                                width: dictChipLabel.implicitWidth + 20
                                height: dictChipLabel.implicitHeight + 8
                                radius: height / 2
                                color: solo ? Theme.hoverOverlay : "transparent"
                                border.width: 1
                                border.color: solo ? Theme.link : Theme.divider

                                Label {
                                    id: dictChipLabel
                                    anchors.centerIn: parent
                                    text: (parent.modelData.dictionary || "unknown")
                                          + " · " + parent.modelData.entries.length
                                    color: parent.solo ? Theme.link : Theme.textSecondary
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.soloGid = parent.solo ? "" : parent.gid
                                }
                            }
                        }
                    }

                    Repeater {
                        model: root.groups

                        delegate: GroupCard {
                            id: groupCard
                            required property var modelData
                            required property int index
                            readonly property string gid: modelData.dictionaryId || modelData.dictionary
                            readonly property bool isCollapsed: root.collapsed[gid] === true
                            objectName: "groupCard_" + index
                            headerName: "groupHeader_" + index
                            width: parent.width
                            title: modelData.dictionary || "unknown"
                            badge: modelData.entries.length > 1
                                   ? (Theme.zh
                                       ? modelData.entries.length + " 条"
                                       : modelData.entries.length + " entries")
                                   : ""
                            collapsed: isCollapsed
                            // 命中词典 chip 聚焦过滤：非选中词典整卡收起
                            visible: root.soloGid.length === 0 || root.soloGid === gid
                            onToggleRequested: {
                                var next = ({})
                                Object.keys(root.collapsed).forEach(
                                    function(k) { next[k] = root.collapsed[k] })
                                next[gid] = !isCollapsed
                                root.collapsed = next
                            }

                            // 组内释义（同一词典聚合；词性/层级行浅灰 + 释义深灰）
                            Column {
                                width: parent.width
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 8
                                bottomPadding: 4
                                spacing: 6

                                Repeater {
                                    model: groupCard.modelData.entries

                                    delegate: ColumnLayout {
                                        id: entryItem
                                        required property var modelData
                                        width: parent.width
                                        spacing: 3

                                        // 行动作行：词头蓝链（层级>=1 可跳转
                                        // 到该词的词条页；精确层就是当前词条，
                                        // 此时行内只有右侧动作）。层级标记按
                                        // relevance 出：1 原形 · / 2 前缀 · /
                                        // 3 词条 · / 4 模糊 ·——非当前词头的
                                        // 蓝链一律带层级前缀，前缀命中不再和
                                        // 精确层混观（ui_click_audit S9 逐字
                                        // 断言此文本，改动须同步两侧）
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Label {
                                                objectName: "entryWordLink"
                                                visible: entryItem.modelData.word
                                                         && entryItem.modelData.word !== root.currentWord
                                                text: ((entryItem.modelData.relevance === 3
                                                        ? (Theme.zh ? "词条 · " : "Entry \u00b7 ")
                                                        : entryItem.modelData.relevance === 2
                                                        ? (Theme.zh ? "前缀 · " : "Prefix \u00b7 ")
                                                        : entryItem.modelData.relevance === 1
                                                        ? (Theme.zh ? "原形 · " : "Lemma \u00b7 ")
                                                        : entryItem.modelData.relevance === 4
                                                        ? (Theme.zh ? "模糊 · " : "Fuzzy \u00b7 ")
                                                        : "")
                                                      + entryItem.modelData.word)
                                                color: Theme.link
                                                font.pixelSize: 13

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.wordRequested(entryItem.modelData.word)
                                                }
                                            }

                                            Item { Layout.fillWidth: true }

                                            // 本条朗读：读该词条释义纯文本
                                            // （例句喇叭同式，点击必有状态行）
                                            Item {
                                                objectName: "entrySpeakAction"
                                                implicitWidth: 16
                                                implicitHeight: 16
                                                Icon {
                                                    anchors.centerIn: parent
                                                    name: "volume"
                                                    size: 14
                                                    color: speakMouse.containsMouse
                                                           ? Theme.textSecondary : Theme.textTertiary
                                                }

                                                MouseArea {
                                                    id: speakMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (!root.lookup) return
                                                        var plain = root.lookup.extractTextFromHtml(
                                                            root.snippet(entryItem.modelData.definition || "", 300))
                                                        root.statusReported(Theme.zh
                                                            ? ("正在朗读释义: " + root.snippet(plain, 40))
                                                            : ("Reading definition: " + root.snippet(plain, 40)))
                                                        root.voice.speakText(plain)
                                                    }
                                                }
                                            }

                                            // 本条复制：复制该词条释义纯文本
                                            Item {
                                                objectName: "entryCopyAction"
                                                implicitWidth: 16
                                                implicitHeight: 16
                                                Icon {
                                                    anchors.centerIn: parent
                                                    name: "copy"
                                                    size: 14
                                                    color: copyMouse.containsMouse
                                                           ? Theme.textSecondary : Theme.textTertiary
                                                }

                                                MouseArea {
                                                    id: copyMouse
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (!root.clip) return
                                                        root.clip.setText(root.lookup
                                                            ? root.lookup.extractTextFromHtml(
                                                                entryItem.modelData.definition || "")
                                                            : (entryItem.modelData.definition || ""))
                                                        root.statusReported(Theme.zh ? "已复制本条释义"
                                                                                     : "Definition copied")
                                                    }
                                                }
                                            }
                                        }

                                        TextEdit {
                                            objectName: "definitionBodyText"
                                            Layout.fillWidth: true
                                            readOnly: true
                                            selectByMouse: true
                                            textFormat: TextEdit.RichText
                                            wrapMode: TextEdit.Wrap
                                            color: Theme.text
                                            // 释义字号（F7）：ui/def_font_size
                                            // 档位（默认 14），外观 tab 即选即用
                                            font.pixelSize: root.defFontSize
                                            text: root.formatDefinition(entryItem.modelData)
                                            onLinkActivated: function(link) { root.linkActivated(link) }
                                        }

                                        Item { width: 1; height: 10 }
                                    }
                                }
                            }
                        }
                    }

                }
            }

        // ---- 空态 / 未收录态（界面重设计批）：单一居中态 + 引导动作 ----
        // 1) 空态：查一个词 + 「试试查 hello」主引导 +打开词典管理（原型
        //    empty 区同构）；2) 未收录：中文错误文案 + 可点建议词。
        //    原始英文错误（Word not found: …）与空态散文案不再直出。
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 0
                     && root.groups.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 8

                // 字形块（原型 empty .glyph：52px 卡片圆角 + 24px 描边图标）
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    implicitWidth: 52
                    implicitHeight: 52
                    radius: 14
                    color: Theme.card
                    border.width: 1
                    border.color: Theme.divider
                    Icon {
                        anchors.centerIn: parent
                        name: root.notFound ? "search" : "book"
                        size: 24
                        color: Theme.textTertiary
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.notFound
                        ? (Theme.zh
                            ? "未找到「" + root.notFoundWord + "」"
                            : "Not found: \u201c" + root.notFoundWord + "\u201d")
                        : (Theme.zh ? "查一个词" : "Look up a word")
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.2
                    color: Theme.text
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 340
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: root.notFound
                        ? (Theme.zh ? "未找到，请试试其他词"
                                    : "No match \u2014 try another word")
                        : (Theme.zh
                            ? "输入英文或中文词条，回车即查。支持 MDX / StarDict / JSON 词典全文检索。"
                            : "Type an English or Chinese headword and press Enter. "
                              + "MDX / StarDict / JSON dictionaries, full text included.")
                    font.pixelSize: 13
                    color: Theme.textTertiary
                }

                // 无相近词时给明确下一步（有建议时由 chip 接管）
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.notFound && root.suggestions.length === 0
                    text: Theme.zh ? "词典里没有相近的词，换个拼写再试试"
                                   : "No close matches — try a different spelling"
                    font.pixelSize: 12
                    color: Theme.textTertiary
                }

                // 未收录建议：可点就地重查（与侧栏建议同序）
                Flow {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 420
                    spacing: 8
                    visible: root.notFound && root.suggestions.length > 0

                    Repeater {
                        model: root.notFound ? root.suggestions : []

                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            height: 26
                            width: suggestChipLabel.implicitWidth + 20
                            radius: height / 2
                            color: Qt.alpha(Theme.accent, 0.10)
                            border.width: 1
                            border.color: Theme.accent

                            Label {
                                id: suggestChipLabel
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: 12
                                color: Theme.accent
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.wordRequested(modelData)
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 4
                    spacing: 8
                    // 未收录且有建议时让位给建议 chip（避免双 CTA 打架）
                    visible: !root.notFound || root.suggestions.length === 0

                    AppButton {
                        objectName: "emptyTryHelloButton"
                        primary: true
                        text: Theme.zh ? "试试查「hello」" : "Try \u201chello\u201d"
                        onClicked: root.emptyLookupRequested("hello")
                    }
                    AppButton {
                        objectName: "emptyOpenDictsButton"
                        text: Theme.zh ? "打开词典管理" : "Manage dictionaries"
                        onClicked: root.openDictsRequested()
                    }
                }
            }
        }

        // ---- 例句：目标词蓝色高亮 + 喇叭（数据=释义含目标词的词条，
        // 无独立例句库时以词典释义句呈现）。UI 批一：按来源词典分区折叠
        // （GroupCard 同词典 tab 卡语言，例句序号在分区内重排）----
        EntryExamplesTab {
            visible: contentTab.currentIndex === 1
            pane: root
        }

        // ---- 词组：以查询词开头的复合词头（斜体蓝 + 释义）。UI 批一：
        // 按来源词典分区折叠（adapter 补 dictionary 字段，缺值归「其他」）----
        EntryPhrasesTab {
            visible: contentTab.currentIndex === 2
            pane: root
        }

        // ---- 近义联想：候选词蓝色链接（近义/联想词流式排布）。UI 批一：
        // 按候选来源分区折叠——前缀候选 / 模糊联想（adapter 逐词钉 source）----
        EntryRelatedTab {
            visible: contentTab.currentIndex === 3
            pane: root
        }

        // ---- 全文检索：释义包含目标词的词条（词条蓝链接 + 词典灰标）----
        EntryFulltextTab {
            visible: contentTab.currentIndex === 4
            pane: root
        }
    }
}