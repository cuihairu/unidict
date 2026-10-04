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
        resultGroups = []
        firstEntry = ({})
        if (lookup) {
            // 与主窗 openWord 同参聚合（sanitize/交叉引用重写都在 core 侧）
            var groups = lookup.aggregateLookup(t, {
                "maxTotalResults": 8,
                "sanitizeHtml": true,
                "rewriteCrossRefs": true
            })
            resultGroups = groups || []
            if (resultGroups.length > 0 && resultGroups[0].entries.length > 0)
                firstEntry = resultGroups[0].entries[0]
            else notFound = true
        }
        // 贴光标（+12/+16 避免盖住指针）；越屏回钳回安全区
        var p = lookup ? lookup.cursorScreenPos() : ({ x: 0, y: 0 })
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

    function _defHtml() {
        var def = firstEntry.definition || ""
        if (!def) return ""
        return "<div style='font-size:13px;line-height:1.6;color:" + Theme.text + "'>"
               + def + "</div>"
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
                    onClicked: if (pane.lookup) pane.lookup.speakText(pane.word)
                }
                ToolButton {
                    objectName: "qlVocabButton"
                    text: "📖"
                    ToolTip.text: "加入生词本"
                    onClicked: {
                        if (!pane.lookup) return
                        var defText = pane.lookup.extractTextFromHtml(
                            pane.firstEntry.definition || "")
                        pane.lookup.addToVocabulary(pane.word, defText)
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

            // 词典名灰标（最优组来源）
            Label {
                text: pane.resultGroups.length > 0
                    ? ("· " + (pane.resultGroups[0].dictionary || "")) : ""
                font.pixelSize: 11
                color: Theme.textTertiary
                visible: text.length > 0
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
                text: pane.notFound
                    ? ("未收录: " + pane.word + "\n「在主窗打开」查看相近词与建议")
                    : ""
                visible: pane.notFound
                font.pixelSize: 13
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
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