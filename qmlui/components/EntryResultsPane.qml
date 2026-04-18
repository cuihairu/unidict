import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Frame {
    id: root

    property var entriesModel
    property int selectedEntryIndex: 0
    property bool showAllDictionaries: true
    property string currentWord: ""
    property string currentPronunciation: ""
    property string fallbackHtml: ""
    property var lookup
    property var clip
    property string emptyHtml: ""

    signal selectedEntryIndexChanged(int index)
    signal pronunciationChanged(string value)
    signal showAllDictionariesChanged(bool value)
    signal statusChanged(string value)
    signal linkActivated(string link)

    Material.elevation: 0
    padding: 12

    StackLayout {
        anchors.fill: parent
        currentIndex: entriesModel && entriesModel.count > 0 ? (showAllDictionaries ? 0 : 1) : 2

        ScrollView {
            clip: true

            Column {
                width: parent.width
                spacing: 10

                Repeater {
                    model: root.entriesModel

                    delegate: Frame {
                        width: parent.width
                        padding: 10

                        background: Rectangle {
                            color: "#FFFFFF"
                            radius: 12
                            border.color: "#E5E7EB"
                        }

                        ColumnLayout {
                            width: parent.width
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Label {
                                    text: model.dictionary || "unknown"
                                    font.weight: Font.DemiBold
                                    color: "#111827"
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                ToolButton {
                                    text: "朗读"
                                    onClicked: root.lookup.speakText(root.currentWord)
                                }

                                ToolButton {
                                    text: "复制"
                                    onClicked: {
                                        root.clip.setText(model.definitionText || "")
                                        root.statusChanged("已复制释义 · " + (model.dictionary || "unknown"))
                                    }
                                }

                                ToolButton {
                                    text: "设为当前"
                                    onClicked: {
                                        root.selectedEntryIndexChanged(index)
                                        root.pronunciationChanged(model.pronunciation || "")
                                        root.showAllDictionariesChanged(false)
                                    }
                                }
                            }

                            TextEdit {
                                width: parent.width
                                readOnly: true
                                selectByMouse: true
                                textFormat: TextEdit.RichText
                                wrapMode: TextEdit.Wrap
                                text: model.definitionHtml || ""
                                onLinkActivated: function(link) { root.linkActivated(link) }
                            }
                        }
                    }
                }
            }
        }

        ColumnLayout {
            spacing: 10
            visible: root.entriesModel && root.entriesModel.count > 0

            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                clip: true
                contentWidth: dictTabsRow.implicitWidth
                interactive: contentWidth > width

                Row {
                    id: dictTabsRow
                    spacing: 6

                    Repeater {
                        model: root.entriesModel

                        delegate: ToolButton {
                            text: model.dictionary || "unknown"
                            checked: index === root.selectedEntryIndex
                            checkable: true
                            onClicked: {
                                root.selectedEntryIndexChanged(index)
                                root.pronunciationChanged(model.pronunciation || "")
                            }
                        }
                    }
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextEdit {
                    width: parent.width
                    readOnly: true
                    selectByMouse: true
                    textFormat: TextEdit.RichText
                    wrapMode: TextEdit.Wrap
                    text: (root.entriesModel && root.entriesModel.count > 0 &&
                           root.selectedEntryIndex >= 0 && root.selectedEntryIndex < root.entriesModel.count)
                        ? (root.entriesModel.get(root.selectedEntryIndex).definitionHtml || "")
                        : ""
                    onLinkActivated: function(link) { root.linkActivated(link) }
                }
            }
        }

        ScrollView {
            clip: true

            TextEdit {
                width: parent.width
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                wrapMode: TextEdit.Wrap
                text: root.fallbackHtml.length > 0 ? root.fallbackHtml : root.emptyHtml
                onLinkActivated: function(link) { root.linkActivated(link) }
            }
        }
    }
}
