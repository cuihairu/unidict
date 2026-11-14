import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 内容页 tab「例句」（LAY-009 自 EntryResultsPane.qml 拆出）：
// pane 回指查词结果面板根（QML id 跨文件不可见，经属性引用）；
// visible 切换由 pane 侧调用点按内容页索引绑定，原注释随调用点。
ScrollView {
    property var pane

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            padding: 4

                Column {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: pane.groupItems(pane.tabData["examples"] || [], "dictionary")

                        delegate: GroupCard {
                            id: exampleGroup
                            required property var modelData
                            required property int index
                            width: parent.width
                            headerName: "examplesGroup_" + index
                            title: modelData.key
                            badge: Theme.zh ? (modelData.items.length + " 条")
                                             : (modelData.items.length + " entries")
                            collapsed: pane.contentCollapsed["examples|" + modelData.key] === true
                            onToggleRequested: pane.toggleContent("examples|" + modelData.key)

                            Column {
                                width: parent.width
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 8
                                bottomPadding: 8
                                spacing: 10

                                Repeater {
                                    model: exampleGroup.modelData.items

                                    delegate: RowLayout {
                                        id: exampleRow
                                        required property var modelData
                                        required property int index
                                        width: parent.width
                                        spacing: 8

                                        Item {
                                            objectName: "exampleSpeaker"
                                            implicitWidth: 18
                                            implicitHeight: 18
                                            Icon {
                                                anchors.centerIn: parent
                                                name: "volume"
                                                size: 16
                                                color: Theme.accent
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                // BUG-010：例句喇叭读整句（此前读词条名）；点击
                                                // 有状态行反馈
                                                onClicked: {
                                                    var plain = pane.lookup.extractTextFromHtml(
                                                        pane.snippet(exampleRow.modelData.definition || "", 300))
                                                    pane.statusReported(Theme.zh
                                                        ? ("正在朗读例句: " + pane.snippet(plain, 40))
                                                        : ("Reading example: " + pane.snippet(plain, 40)))
                                                    pane.voice.speakText(plain)
                                                }
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
                                                // 序号灰前缀（欧路例句库口径）：编号 + 目标词高亮句；
                                                // 行高用 line-height（TextEdit 无 lineHeight 属性）
                                                text: "<div style='line-height:1.5'>"
                                                      + "<font color='" + Theme.textTertiary + "'>"
                                                      + (exampleRow.index + 1) + ". </font>"
                                                      + pane.highlightWord(pane.snippet(exampleRow.modelData.definition || "", 160), pane.currentWord)
                                                      + "</div>"
                                            }

                                            Label {
                                                objectName: "exampleWordLink"
                                                text: exampleRow.modelData.word + " · " + (exampleRow.modelData.dictionary || "")
                                                color: Theme.textTertiary
                                                font.pixelSize: 11
                                                elide: Text.ElideRight

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: pane.wordRequested(exampleRow.modelData.word)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 空态
                    Label {
                        visible: (pane.tabData["examples"] || []).length === 0
                        text: Theme.zh ? "暂无例句数据" : "No examples yet"
                        color: Theme.textTertiary
                        padding: 4
                    }
                }
}
