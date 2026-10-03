import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 查词结果面板（欧路词典口径，docs/design-references/eudic-lookup-page.png）：
// 内容 Tab 栏（词典/例句/词组/近反义词/全文检索，选中浅蓝）+ 分组卡视图。
// 「词典」tab 每词典一个可折叠分组（标题=词典名+折叠箭头，浅灰细线分隔，
// 不用硬边框），同一词典的释义聚在组内——多字典不平铺混排；层级与去重
// 由 core searchGrouped 完成（relevance：0 词头精确 / 1 前缀 / 2 释义包含）。
Frame {
    id: root

    property var groups: []          // [{dictionary, dictionaryId, entries:[…]}]
    property var flatEntries: []     // groups 的摊平形态（兼容旧引用）
    property var entriesModel        // 兼容旧引用（flat 条目模型）
    property string currentWord: ""
    property string fallbackHtml: ""
    property var lookup
    property var clip
    property string emptyHtml: ""

    signal statusReported(string value)
    signal linkActivated(string link)
    signal wordRequested(string word)

    Material.elevation: 0
    padding: 0

    // 分组折叠状态：dictionaryId -> true（默认全展开）
    property var collapsed: ({})

    // 内容 tab 懒取缓存：key -> QVariantList；查词切换即失效
    property var tabData: ({})
    onCurrentWordChanged: {
        tabData = ({})
        collapsed = ({})
    }

    function ensureTabData(key) {
        if (tabData.hasOwnProperty(key)) return
        if (!lookup || currentWord.length === 0) { tabData[key] = []; return }
        if (key === "examples" || key === "fulltext") {
            tabData[key] = lookup.fullTextLookup(currentWord, 20)
        } else if (key === "phrases") {
            tabData[key] = lookup.relatedLookup(currentWord, "phrases")
        } else if (key === "related") {
            tabData[key] = lookup.relatedLookup(currentWord, "related")
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 内容 Tab 栏：选中浅蓝文字（欧路口径），点击懒取数据
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 8
            spacing: 20

            Repeater {
                model: [
                    { key: "dict", label: "词典" },
                    { key: "examples", label: "例句" },
                    { key: "phrases", label: "词组" },
                    { key: "related", label: "近反义词" },
                    { key: "fulltext", label: "全文检索" }
                ]

                delegate: Label {
                    required property var modelData
                    required property int index
                    property bool active: contentTab.currentIndex === index
                    text: modelData.label
                    font.pixelSize: 14
                    font.weight: active ? Font.DemiBold : Font.Normal
                    color: active ? Theme.link : Theme.textSecondary
                    padding: 6

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.ensureTabData(modelData.key)
                            contentTab.currentIndex = index
                        }
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
            property int currentIndex: 0
        }

        // ---- 词典：多词典分组卡（可折叠，细线分隔）----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 0
            clip: true
            contentWidth: availableWidth

                Column {
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: root.groups

                        delegate: Column {
                            id: groupCard
                            required property var modelData
                            readonly property string gid: modelData.dictionaryId || modelData.dictionary
                            readonly property bool collapsed: root.collapsed[gid] === true
                            width: parent.width

                            // 分组头：词典名 + 折叠箭头 + 词条数（浅灰细线分隔）
                            Rectangle {
                                width: parent.width
                                height: 40
                                color: "transparent"

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 1
                                    color: Theme.divider
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8

                                    Label {
                                        text: groupCard.collapsed ? "›" : "⌄"
                                        color: Theme.textTertiary
                                        font.pixelSize: 14
                                    }

                                    Label {
                                        text: groupCard.modelData.dictionary || "unknown"
                                        font.weight: Font.DemiBold
                                        color: Theme.text
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: groupCard.modelData.entries.length + " 条"
                                        color: Theme.textTertiary
                                        font.pixelSize: 12
                                        visible: groupCard.modelData.entries.length > 1
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var next = ({})
                                        Object.keys(root.collapsed).forEach(function(k) { next[k] = root.collapsed[k] })
                                        next[groupCard.gid] = !groupCard.collapsed
                                        root.collapsed = next
                                    }
                                }
                            }

                            // 组内释义（同一词典聚合；词性/层级行浅灰 + 释义深灰）
                            Column {
                                width: parent.width
                                visible: !groupCard.collapsed
                                leftPadding: 12
                                rightPadding: 12
                                spacing: 4

                                Repeater {
                                    model: groupCard.modelData.entries

                                    delegate: ColumnLayout {
                                        id: entryItem
                                        required property var modelData
                                        width: parent.width
                                        spacing: 2

                                        // 词头行：层级>=1（前缀/释义包含）时可跳转
                                        // 到该词的词条页；精确层就是当前词条
                                        Label {
                                            visible: entryItem.modelData.word
                                                     && entryItem.modelData.word !== root.currentWord
                                            text: (entryItem.modelData.relevance === 2 ? "词条 · " : "")
                                                  + entryItem.modelData.word
                                            color: Theme.link
                                            font.pixelSize: 13

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: root.wordRequested(entryItem.modelData.word)
                                            }
                                        }

                                        TextEdit {
                                            Layout.fillWidth: true
                                            readOnly: true
                                            selectByMouse: true
                                            textFormat: TextEdit.RichText
                                            wrapMode: TextEdit.Wrap
                                            color: Theme.text
                                            font.pixelSize: 14
                                            text: entryItem.modelData.definition || ""
                                            onLinkActivated: function(link) { root.linkActivated(link) }
                                        }

                                        Item { width: 1; height: 8 }
                                    }
                                }
                            }
                        }
                    }

                    // 空态 / 未收录回落
                    TextEdit {
                        width: parent.width
                        readOnly: true
                        selectByMouse: true
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        visible: root.groups.length === 0
                        text: root.fallbackHtml.length > 0 ? root.fallbackHtml : root.emptyHtml
                        onLinkActivated: function(link) { root.linkActivated(link) }
                    }
                }
            }

        // ---- 例句：目标词蓝色高亮 + 喇叭（数据=释义含目标词的词条，
        // 无独立例句库时以词典释义句呈现）----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 1
            clip: true
            contentWidth: availableWidth

                ListView {
                    width: parent.width
                    model: root.tabData["examples"] || []
                    spacing: 10
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: RowLayout {
                        id: exampleRow
                        required property var modelData
                        width: ListView.view.width
                        spacing: 8

                        Label {
                            text: "🔊"
                            color: Theme.link
                            font.pixelSize: 14

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.lookup.speakText(exampleRow.modelData.word || root.currentWord)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            TextEdit {
                                Layout.fillWidth: true
                                readOnly: true
                                selectByMouse: true
                                textFormat: TextEdit.RichText
                                wrapMode: TextEdit.Wrap
                                font.pixelSize: 13
                                color: Theme.text
                                text: root.highlightWord(exampleRow.modelData.definition || "", root.currentWord)
                            }

                            Label {
                                text: exampleRow.modelData.word + " · " + (exampleRow.modelData.dictionary || "")
                                color: Theme.textTertiary
                                font.pixelSize: 11
                                elide: Text.ElideRight

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.wordRequested(exampleRow.modelData.word)
                                }
                            }
                        }
                    }

                    // 空态
                    Label {
                        anchors.centerIn: parent
                        visible: (root.tabData["examples"] || []).length === 0
                        text: "暂无例句数据"
                        color: Theme.textTertiary
                    }
                }
            }

        // ---- 词组：以查询词开头的复合词头（斜体蓝 + 释义）----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 2
            clip: true
            contentWidth: availableWidth

                ListView {
                    width: parent.width
                    model: root.tabData["phrases"] || []
                    spacing: 10
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: RowLayout {
                        id: phraseRow
                        required property var modelData
                        width: ListView.view.width
                        spacing: 8

                        Label {
                            text: phraseRow.modelData.word || ""
                            color: Theme.link
                            font.italic: true
                            font.pixelSize: 14

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.wordRequested(phraseRow.modelData.word)
                            }
                        }

                        TextEdit {
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.Wrap
                            font.pixelSize: 13
                            color: Theme.textSecondary
                            text: phraseRow.modelData.definition || ""
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: (root.tabData["phrases"] || []).length === 0
                        text: "暂无词组数据"
                        color: Theme.textTertiary
                    }
                }
            }

        // ---- 近反义词：候选词蓝色链接（近义/联想词流式排布）----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 3
            clip: true
            contentWidth: availableWidth

                Flow {
                    width: parent.width
                    spacing: 10

                    Repeater {
                        model: root.tabData["related"] || []

                        delegate: Label {
                            required property var modelData
                            text: modelData.word || ""
                            color: Theme.link
                            font.pixelSize: 14
                            padding: 4

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.wordRequested(modelData.word)
                            }
                        }
                    }

                    Label {
                        visible: (root.tabData["related"] || []).length === 0
                        text: "暂无近义/联想词"
                        color: Theme.textTertiary
                        padding: 4
                    }
                }
            }

        // ---- 全文检索：释义包含目标词的词条（词条蓝链接 + 词典灰标）----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: contentTab.currentIndex === 4
            clip: true
            contentWidth: availableWidth

                ListView {
                    width: parent.width
                    model: root.tabData["fulltext"] || []
                    spacing: 10
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: ColumnLayout {
                        id: ftRow
                        required property var modelData
                        width: ListView.view.width
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Label {
                                text: ftRow.modelData.word || ""
                                color: Theme.link
                                font.pixelSize: 14

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.wordRequested(ftRow.modelData.word)
                                }
                            }

                            Item { Layout.fillWidth: true }

                            Label {
                                text: ftRow.modelData.dictionary || ""
                                color: Theme.textTertiary
                                font.pixelSize: 11
                            }
                        }

                        TextEdit {
                            Layout.fillWidth: true
                            readOnly: true
                            selectByMouse: true
                            textFormat: TextEdit.RichText
                            wrapMode: TextEdit.Wrap
                            font.pixelSize: 13
                            color: Theme.text
                            text: root.highlightWord(ftRow.modelData.definition || "", root.currentWord)
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: (root.tabData["fulltext"] || []).length === 0
                        text: "全文检索无命中"
                        color: Theme.textTertiary
                    }
                }
            }
    }
}
