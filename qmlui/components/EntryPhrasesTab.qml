import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 内容页 tab「词组」（LAY-009 自 EntryResultsPane.qml 拆出）：
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
                        model: pane.groupItems(pane.tabData["phrases"] || [], "dictionary")

                        delegate: GroupCard {
                            id: phraseGroup
                            required property var modelData
                            required property int index
                            width: parent.width
                            headerName: "phrasesGroup_" + index
                            title: modelData.key
                            badge: Theme.zh ? (modelData.items.length + " 条")
                                             : (modelData.items.length + " entries")
                            collapsed: pane.contentCollapsed["phrases|" + modelData.key] === true
                            onToggleRequested: pane.toggleContent("phrases|" + modelData.key)

                            Column {
                                width: parent.width
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 8
                                bottomPadding: 8
                                spacing: 10

                                Repeater {
                                    model: phraseGroup.modelData.items

                                    delegate: RowLayout {
                                        id: phraseRow
                                        required property var modelData
                                        width: parent.width
                                        spacing: 8

                                        Label {
                                            objectName: "phraseLink"
                                            text: phraseRow.modelData.word || ""
                                            color: Theme.link
                                            font.italic: true
                                            font.pixelSize: 14

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: pane.wordRequested(phraseRow.modelData.word)
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
                                }
                            }
                        }
                    }

                    Label {
                        visible: (pane.tabData["phrases"] || []).length === 0
                        text: Theme.zh ? "暂无词组数据" : "No phrases yet"
                        color: Theme.textTertiary
                        padding: 4
                    }
                }
}
