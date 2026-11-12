import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"

// 移动端主壳·Review 页（LAY-002 自 Main.qml 拆出）
// 🔁复习 Tab（P-7 批四 复习入口简单化：生词本四技能学习状态
// 清单——任一技能未到「稳」即入列，点技能圈档 未练→不稳→稳。
// 无调度/等级/激励营销，product-principles §11「预留防迁移，
// 不是课程化」口径；真数据源 = vocab.vocabularyMeta 四技能键）
//
// 窗口状态与根函数经 win 引用读写；responsive / suggestionModel
// 是 Main.qml 文件级 id，组件文件看不见父文件 id，故经本页同名字段
// 注入（实例化处同 id 名赋值——文档 id 优先于本组件同名字段，
// 与 SidebarPanel / Settings*Page 的 *Service 别名同律）；
// Theme / vocab / lookup 等 context 属性随引擎传播，无需注入。
Item {
    objectName: "mobileReviewPage"

    property var win: null
    property var responsive: null
    property var suggestionModel: null

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
                    var all = vocab.vocabularyMeta()
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
                                if (!vocab.setVocabularySkill(entryWord, skill, next))
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
