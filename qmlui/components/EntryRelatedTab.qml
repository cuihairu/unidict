import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 内容页 tab「近义联想」（LAY-009 自 EntryResultsPane.qml 拆出）：
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
                        model: pane.groupItems(pane.tabData["related"] || [], "", pane.sourceLabel)

                        delegate: GroupCard {
                            id: relatedGroup
                            required property var modelData
                            required property int index
                            width: parent.width
                            headerName: "relatedGroup_" + index
                            title: modelData.key
                            badge: Theme.zh ? (modelData.items.length + " 个")
                                             : (modelData.items.length + " words")
                            collapsed: pane.contentCollapsed["related|" + modelData.key] === true
                            onToggleRequested: pane.toggleContent("related|" + modelData.key)

                            Flow {
                                width: parent.width
                                leftPadding: 12
                                rightPadding: 12
                                topPadding: 8
                                bottomPadding: 8
                                spacing: 10

                                Repeater {
                                    model: relatedGroup.modelData.items

                                    delegate: Label {
                                        required property var modelData
                                        objectName: "relatedLink"
                                        text: modelData.word || ""
                                        color: Theme.link
                                        font.pixelSize: 14
                                        padding: 4

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: pane.wordRequested(modelData.word)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        visible: (pane.tabData["related"] || []).length === 0
                        text: Theme.zh ? "暂无近义/联想词" : "No synonyms or related words"
                        color: Theme.textTertiary
                        padding: 4
                    }
                }
}
