import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

Pane {
    id: root

    property int suggestMode: 0
    property string currentWord: ""
    property var resultsModel
    property var historyModel
    property var vocabModel
    property var lookup
    property bool hasEncryptedDictionary: false
    property string mdictPasswordPlaceholder: ""
    property string queryText: ""
    property alias currentTabIndex: sideTabs.currentIndex

    signal suggestModeSelected(int index)
    signal queryTextChanged(string text)
    signal querySubmitted(string text)
    signal queryCleared()
    signal applyPasswordRequested(string password)
    signal clearPasswordRequested()
    signal historyTabRequested()
    signal vocabularyTabRequested()
    signal resultWordRequested(string word)

    SplitView.preferredWidth: 360
    SplitView.minimumWidth: 280
    SplitView.maximumWidth: 520
    Material.elevation: 1
    padding: 12

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ComboBox {
                id: suggestModeCombo
                model: ["自动", "前缀", "模糊", "通配符", "正则"]
                currentIndex: root.suggestMode
                Layout.preferredWidth: 96
                onActivated: root.suggestModeSelected(currentIndex)
            }

            TextField {
                id: searchField
                Layout.fillWidth: true
                text: root.queryText
                placeholderText: root.suggestMode === 3 ? "输入通配符，例如 te*t?…" :
                    (root.suggestMode === 4 ? "输入正则，例如 ^test.* …" : "输入要查询的词条…")
                selectByMouse: true

                onTextChanged: root.queryTextChanged(text)

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                        root.querySubmitted(searchField.text)
                        event.accepted = true
                    } else if (event.key === Qt.Key_Escape) {
                        searchField.clear()
                        root.queryCleared()
                        event.accepted = true
                    }
                }
            }

            ToolButton {
                text: "查"
                enabled: searchField.text.trim().length > 0
                onClicked: root.querySubmitted(searchField.text)
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.hasEncryptedDictionary

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: "tomato"
                text: "提示：检测到加密词典。可设置 UNIDICT_MDICT_PASSWORD（或在此处输入）后重新加载。"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: mdictPasswordField
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: root.mdictPasswordPlaceholder
                }

                Button {
                    text: "Apply & Reload"
                    enabled: mdictPasswordField.text.length > 0
                    onClicked: {
                        root.applyPasswordRequested(mdictPasswordField.text)
                        mdictPasswordField.text = ""
                    }
                }

                Button {
                    text: "Clear"
                    onClicked: {
                        root.clearPasswordRequested()
                        mdictPasswordField.text = ""
                    }
                }
            }
        }

        TabBar {
            id: sideTabs
            Layout.fillWidth: true
            Material.elevation: 0
            currentIndex: 0

            TabButton { text: "结果" }
            TabButton { text: "历史" }
            TabButton { text: "生词本" }

            onCurrentIndexChanged: {
                if (currentIndex === 1) root.historyTabRequested()
                if (currentIndex === 2) root.vocabularyTabRequested()
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: sideTabs.currentIndex

            ListView {
                id: resultsList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: root.resultsModel
                keyNavigationEnabled: true

                delegate: ItemDelegate {
                    width: ListView.view.width

                    contentItem: Text {
                        width: parent.width
                        textFormat: Text.RichText
                        text: {
                            var w = model.word || ""
                            var q = searchField.text.trim()
                            if (!q) return w.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                            var idx = w.toLowerCase().indexOf(q.toLowerCase())
                            if (idx < 0) return w.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                            function esc(s) {
                                return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;")
                            }
                            return esc(w.substring(0, idx)) +
                                   "<span style='color:#2563EB;font-weight:600;'>" + esc(w.substring(idx, idx + q.length)) + "</span>" +
                                   esc(w.substring(idx + q.length))
                        }
                        elide: Text.ElideRight
                        color: parent.highlighted ? "#1D4ED8" : "#111827"
                    }

                    highlighted: model.word === root.currentWord
                    onClicked: root.resultWordRequested(model.word)
                }

                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: resultsList.count === 0
                    text: searchField.text.trim().length > 0 ? "没有建议，按回车直接查询" : "输入词条开始"
                    color: "#9CA3AF"
                }
            }

            ListView {
                id: historyList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: root.historyModel

                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: model.word
                    highlighted: model.word === root.currentWord
                    onClicked: root.resultWordRequested(model.word)
                }

                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: historyList.count === 0
                    text: "暂无历史"
                    color: "#9CA3AF"
                }
            }

            ListView {
                id: vocabList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: root.vocabModel

                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: model.word
                    highlighted: model.word === root.currentWord
                    onClicked: root.resultWordRequested(model.word)

                    contentItem: Column {
                        spacing: 2

                        Label {
                            text: model.word
                            color: "#111827"
                        }

                        Label {
                            text: model.snippet
                            color: "#6B7280"
                            font.pixelSize: 12
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                }

                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: vocabList.count === 0
                    text: "暂无生词"
                    color: "#9CA3AF"
                }
            }
        }
    }

    function focusSearchField() {
        searchField.forceActiveFocus()
    }
}
