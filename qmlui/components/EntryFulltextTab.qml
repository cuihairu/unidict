import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 内容页 tab「全文检索」（LAY-009 自 EntryResultsPane.qml 拆出）：
// pane 回指查词结果面板根（QML id 跨文件不可见，经属性引用）；
// visible 切换由 pane 侧调用点按内容页索引绑定，原注释随调用点。
ScrollView {
    property var pane

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

                ListView {
                    width: parent.width
                    model: pane.tabData["fulltext"] || []
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
                                objectName: "fulltextLink"
                                text: ftRow.modelData.word || ""
                                color: Theme.link
                                font.pixelSize: 14

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: pane.wordRequested(ftRow.modelData.word)
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
                            text: pane.highlightWord(pane.snippet(ftRow.modelData.definition || "", 160), pane.currentWord)
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        visible: (pane.tabData["fulltext"] || []).length === 0
                        text: Theme.zh ? "全文检索无命中" : "No full-text matches"
                        color: Theme.textTertiary
                    }
                }
}
