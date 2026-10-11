import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15

// P-5 悬浮取词窗（剪贴板取词 / quick_lookup 热键的浮层形态）：无边框置顶
// 小窗贴光标出现，展示聚合查询的最优组首条释义（完整词条面仍在主窗
// EntryResultsPane，这里管「快」——不打断当前阅读流，失焦即收）。
// 触发与呈现分离：showFor(word) 被剪贴板取词与热键读剪贴板两条路径共用；
// 「在主窗打开」把词送回主窗词条卡并前置主窗。
Window {
    id: pane
    objectName: "quickLookupPane"

    property string word: ""
    property var lookup
    property var platform
    property var voice
    property var vocab
    signal openInMainRequested(string word)
    signal statusReported(string message)

    visible: false
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"
    width: 420
    height: 260

    // —— 查询状态 ——
    property var resultGroups: []
    property var firstEntry: ({})
    property bool notFound: false
    // 非精确命中（UI 批四 信息层级对照主窗分组卡的匹配标注）：聚合层
    // 带 模糊/全文 匹配，typo 也有组——浮窗须讲清「查的不是精确词头，
    // 现在显示的是谁」，并给可点改看词（组内去重词面，至多 3）
    property bool fuzzyHit: false
    property var altWords: []
    // 未收录内联建议（聚合全空时的 did-you-mean，可点就地重查）
    property var missSuggestions: []
    property bool _everActive: false

    onActiveChanged: {
        // 失焦即收：Tool 窗点击外部会失去激活；首次 show 的激活空洞用
        // _everActive 挡掉（从未激活过不因 onActiveChanged(false) 自关）
        if (active) _everActive = true
        else if (visible && _everActive) close()
    }
    onVisibleChanged: if (!visible) _everActive = false

    function showFor(w) {
        var t = (w || "").trim()
        if (!t) return
        word = t
        notFound = false
        fuzzyHit = false
        altWords = []
        resultGroups = []
        firstEntry = ({})
        missSuggestions = []
        if (lookup) {
            // 与主窗 openWord 同参聚合（sanitize/交叉引用重写都在 core 侧）
            var groups = lookup.aggregateLookup(t, {
                "maxTotalResults": 8,
                "sanitizeHtml": true,
                "rewriteCrossRefs": true
            })
            resultGroups = groups || []
            if (resultGroups.length > 0 && resultGroups[0].entries.length > 0) {
                firstEntry = resultGroups[0].entries[0]
                // 非精确命中识别 + 组内去重词面（至多 3，含首条自身）
                var hitWord = (firstEntry.word || "").toLowerCase()
                fuzzyHit = hitWord !== "" && hitWord !== t.toLowerCase()
                var seen = []
                for (var g = 0; g < resultGroups.length; ++g) {
                    var es = resultGroups[g].entries || []
                    for (var i = 0; i < es.length; ++i) {
                        var ew = es[i].word || ""
                        if (ew.length > 0 && seen.indexOf(ew) < 0) {
                            seen.push(ew)
                            if (seen.length >= 3) break
                        }
                    }
                    if (seen.length >= 3) break
                }
                altWords = seen
            } else {
                notFound = true
                // 未收录建议与主窗同序口径：前缀层优先，空则模糊层
                var sugg = lookup.suggestPrefix(t, 3)
                if (!sugg || sugg.length === 0) sugg = lookup.suggestFuzzy(t, 3)
                missSuggestions = sugg || []
            }
        }
        // 贴光标（+12/+16 避免盖住指针）；越屏回钳回安全区
        var p = platform ? platform.cursorScreenPos() : ({ x: 0, y: 0 })
        var nx = p.x + 12, ny = p.y + 16
        var sw = Screen.desktopAvailableWidth
        var sh = Screen.desktopAvailableHeight
        if (sw > 0 && nx + pane.width > sw) nx = Math.max(0, sw - pane.width - 8)
        if (sh > 0 && ny + pane.height > sh) ny = Math.max(0, sh - pane.height - 8)
        pane.x = nx
        pane.y = ny
        pane.show()
        pane.requestActivate()
    }

    // UI 批四 对齐批三移动端口径：走 presentEntry 一站式管线（清洗→
    // 交叉引用→.mdd 资源重写 file://），配图随富文本直出，不再裸包
    function _defHtml() {
        var def = firstEntry.definition || ""
        if (!def) return ""
        var dictId = resultGroups.length > 0
            ? (resultGroups[0].dictionaryId || resultGroups[0].dictionary || "")
            : ""
        var html = lookup ? lookup.presentEntry(def, dictId).html : def
        return "<div style='font-size:13px;line-height:1.6;color:" + Theme.text + "'>"
               + (html || def) + "</div>"
    }

    // 词典来源行（UI 批四 对照主窗命中概览）：多词典命中给「等 N 词典」
    function _dictLabel() {
        if (resultGroups.length === 0) return ""
        var name = resultGroups[0].dictionary || ""
        return resultGroups.length > 1
            ? ("· " + name + " 等 " + resultGroups.length + " 词典")
            : ("· " + name)
    }

    function _phonetic() {
        if (!firstEntry.definition) return ""
        var ph = lookup
            ? lookup.extractPhonetics(
                  lookup.extractTextFromHtml(firstEntry.definition))
            : ({})
        var parts = []
        if (ph.british) parts.push("英 " + ph.british)
        if (ph.american) parts.push("美 " + ph.american)
        return parts.join("  ")
    }

    // 无边框窗自绘圆角卡片（Tool 浮层不套窗体皮肤）
    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: 10
        color: Theme.card
        border.width: 1
        border.color: Theme.divider

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            // 头行：词 + 音标 + 朗读/生词本/关闭
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Label {
                    text: pane.word
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: pane._phonetic()
                    font.pixelSize: 12
                    color: Theme.textTertiary
                    visible: text.length > 0
                }
                ToolButton {
                    objectName: "qlSpeakButton"
                    text: "🔊"
                    ToolTip.text: "朗读"
                    onClicked: if (pane.lookup) pane.voice.speakText(pane.word)
                }
                ToolButton {
                    objectName: "qlVocabButton"
                    text: "📖"
                    ToolTip.text: "加入生词本"
                    onClicked: {
                        if (!pane.lookup) return
                        var defText = pane.lookup.extractTextFromHtml(
                            pane.firstEntry.definition || "")
                        pane.vocab.addToVocabulary(pane.word, defText)
                        pane.statusReported("已加入生词本: " + pane.word)
                    }
                }
                ToolButton {
                    objectName: "qlCloseButton"
                    text: "✕"
                    ToolTip.text: "关闭"
                    onClicked: pane.close()
                }
            }

            // 词典来源灰标（最优组来源；多词典命中带「等 N 词典」概览）
            Label {
                text: pane._dictLabel()
                font.pixelSize: 11
                color: Theme.textTertiary
                visible: text.length > 0
                objectName: "qlDictLabel"
            }

            // 非精确命中改看行（UI 批四）：讲清显示的是谁 + 可点换词
            RowLayout {
                visible: pane.fuzzyHit && pane.altWords.length > 0
                Layout.fillWidth: true
                spacing: 4

                Label {
                    text: "未精确命中，改看："
                    font.pixelSize: 11
                    color: Theme.textTertiary
                }
                Repeater {
                    model: pane.altWords.length
                    delegate: Label {
                        objectName: "qlAltWord_" + index
                        required property int index
                        text: pane.altWords[index] || ""
                        font.pixelSize: 12
                        font.underline: (pane.altWords[index] || "").toLowerCase()
                                        !== pane.word.toLowerCase()
                        color: (pane.altWords[index] || "").toLowerCase()
                                   === pane.word.toLowerCase()
                               ? Theme.textSecondary : Theme.accent
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: pane.showFor(pane.altWords[parent.index] || "")
                        }
                    }
                }
            }

            // 释义主体：聚合管线已 sanitize/重写，直接富文本呈现
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextEdit {
                    readOnly: true
                    selectByMouse: true
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: 13
                    color: Theme.text
                    text: pane.notFound ? "" : pane._defHtml()
                }
            }
            Label {
                objectName: "qlNotFoundLabel"
                text: pane.notFound
                    ? ("未收录: " + pane.word
                       + (pane.missSuggestions.length > 0
                          ? "" : "\n「在主窗打开」查看相近词与建议"))
                    : ""
                visible: pane.notFound
                font.pixelSize: 13
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // 未收录内联建议（UI 批四）：可点就地重查，主窗兜完整建议面
            ColumnLayout {
                visible: pane.notFound && pane.missSuggestions.length > 0
                Layout.fillWidth: true
                spacing: 2

                Label {
                    text: "您要找的是："
                    font.pixelSize: 11
                    color: Theme.textTertiary
                }
                Repeater {
                    model: pane.missSuggestions.length
                    delegate: ItemDelegate {
                        objectName: "qlSuggestItem_" + index
                        Layout.fillWidth: true
                        height: 26
                        text: pane.missSuggestions[index] || ""
                        font.pixelSize: 13
                        onClicked: pane.showFor(pane.missSuggestions[index] || "")
                    }
                }
            }

            // 底行：主窗打开（完整词条卡 + 建议/历史）
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }
                Button {
                    objectName: "qlCloseBottomButton"
                    flat: true
                    text: "关闭"
                    onClicked: pane.close()
                }
                Button {
                    objectName: "qlOpenMainButton"
                    text: "在主窗打开"
                    highlighted: true
                    onClicked: pane.openInMainRequested(pane.word)
                }
            }
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: pane.close()
    }
}