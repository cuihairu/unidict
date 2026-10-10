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
    // M3-B 生词本编辑：标签筛选状态与现有标签集合由 MainDesktop 持有
    property var vocabTags: []
    property string vocabTagFilter: ""
    // 笔记内检索（Search within notes）：非空时列表切到笔记命中集
    property string vocabNoteFilter: ""
    property bool hasEncryptedDictionary: false
    property string mdictPasswordPlaceholder: ""
    property string queryText: ""
    property alias currentTabIndex: sideTabs.currentIndex

    signal suggestModeSelected(int index)
    signal queryTextEdited(string text)
    signal querySubmitted(string text)
    signal queryCleared()
    signal applyPasswordRequested(string password)
    signal clearPasswordRequested()
    signal historyTabRequested()
    signal vocabularyTabRequested()
    signal resultWordRequested(string word)
    // M3-B 生词本编辑：数据变更统一回 MainDesktop（它负责 reload + 状态行）
    signal vocabTagFilterRequested(string tag)
    signal vocabNoteFilterRequested(string text)
    signal vocabAddTagRequested(string word, string tag)
    signal vocabRemoveTagRequested(string word, string tag)
    signal vocabNoteSaveRequested(string word, string note)
    signal vocabExportRequested()

    SplitView.preferredWidth: 360
    SplitView.minimumWidth: 280
    SplitView.maximumWidth: 520
    Material.elevation: 1
    padding: 12

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // 查询命令条：凹陷面容器（surfaceSunken + 聚焦 accent 环）内嵌
        // 模式切换 / 输入位 / 查询按钮——主入口一件事，视觉上归拢为一个整体
        Rectangle {
            id: searchCommandBar
            objectName: "searchCommandBar"
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            radius: Theme.radiusL
            color: Theme.surfaceSunken
            border.width: searchField.activeFocus ? 2 : 1
            border.color: searchField.activeFocus ? Theme.accent : Theme.divider
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 6
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 4

                ComboBox {
                    id: suggestModeCombo
                    objectName: "suggestModeCombo"
                    model: ["自动", "前缀", "模糊", "通配符", "正则"]
                    currentIndex: root.suggestMode
                    Layout.preferredWidth: 92
                    Layout.fillHeight: true
                    onActivated: root.suggestModeSelected(currentIndex)
                    background: Rectangle {
                        radius: Theme.radiusM
                        color: suggestModeCombo.hovered ? Theme.hoverOverlay : "transparent"
                    }
                    contentItem: Text {
                        text: suggestModeCombo.displayText
                        font.pixelSize: 13
                        color: Theme.textSecondary
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 6
                    }
                }

                TextField {
                    id: searchField
                    objectName: "searchField"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: root.queryText
                    // 占位自绘（Material 样式的 placeholder 会浮动到顶部与
                    // 输入文本重叠；placeholderText 置空 + 空态 Text 覆盖）
                    placeholderText: ""
                    color: Theme.text
                    selectByMouse: true
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 14
                    background: Rectangle { color: "transparent" }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.suggestMode === 3 ? "输入通配符，例如 te*t?…" :
                            (root.suggestMode === 4 ? "输入正则，例如 ^test.* …" : "输入要查询的词条…")
                        color: Theme.textSecondary
                        font.pixelSize: 14
                        visible: searchField.text.length === 0
                    }

                    onTextChanged: root.queryTextEdited(text)

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
                    id: searchGoButton
                    objectName: "searchGoButton"
                    text: "查"
                    enabled: searchField.text.trim().length > 0
                    Layout.fillHeight: true
                    Layout.preferredWidth: 44
                    font.pixelSize: 14
                    onClicked: root.querySubmitted(searchField.text)
                    background: Rectangle {
                        radius: Theme.radiusM
                        color: !searchGoButton.enabled ? "transparent"
                            : searchGoButton.pressed ? Theme.accentPressed
                            : searchGoButton.hovered ? Theme.accentHover
                            : Theme.accent
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                    contentItem: Text {
                        text: searchGoButton.text
                        font: searchGoButton.font
                        color: searchGoButton.enabled ? Theme.accentText : Theme.textDisabled
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.hasEncryptedDictionary

            Label {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                color: Theme.danger
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

            TabButton { objectName: "sideTab0"; text: "结果" }
            TabButton { objectName: "sideTab1"; text: "历史" }
            TabButton { objectName: "sideTab2"; text: "生词本" }

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
                    objectName: "suggestItem"
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
                                   "<span style='color:" + Theme.accent + ";font-weight:600;'>" + esc(w.substring(idx, idx + q.length)) + "</span>" +
                                   esc(w.substring(idx + q.length))
                        }
                        elide: Text.ElideRight
                        color: parent.highlighted ? Theme.accent : Theme.text
                    }

                    highlighted: model.word === root.currentWord
                    onClicked: root.resultWordRequested(model.word)
                }

                ScrollBar.vertical: ScrollBar {}

                Label {
                    anchors.centerIn: parent
                    visible: resultsList.count === 0
                    text: searchField.text.trim().length > 0 ? "没有建议，按回车直接查询" : "输入词条开始"
                    color: Theme.textTertiary
                }
            }

            ListView {
                id: historyList
                objectName: "historyList"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: root.historyModel

                delegate: ItemDelegate {
                    objectName: "historyItem"
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
                    color: Theme.textTertiary
                }
            }

            // 生词本（M3-B：标签筛选行 + 标签/笔记编辑 + 导出）
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Flow {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: {
                                var chips = [{ "tag": "", "label": "全部" }]
                                for (var i = 0; i < root.vocabTags.length; i++)
                                    chips.push({ "tag": root.vocabTags[i],
                                                 "label": root.vocabTags[i] })
                                return chips
                            }
                            delegate: Rectangle {
                                objectName: modelData.tag.length > 0
                                    ? "vocabFilterChip_" + modelData.tag : "vocabFilterChip_all"
                                readonly property bool selectedChip:
                                    root.vocabTagFilter === modelData.tag
                                radius: height / 2
                                implicitHeight: 26
                                implicitWidth: filterChipLabel.implicitWidth + 16
                                color: selectedChip ? Qt.alpha(Theme.accent, 0.15)
                                                    : Theme.card
                                border.width: 1
                                border.color: selectedChip ? Theme.accent
                                                           : Theme.divider

                                Label {
                                    id: filterChipLabel
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.pixelSize: 12
                                    color: selectedChip ? Theme.accent
                                                        : Theme.textSecondary
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.vocabTagFilterRequested(modelData.tag)
                                }
                            }
                        }
                    }

                    ToolButton {
                        objectName: "vocabExportButton"
                        text: "导出CSV"
                        font.pixelSize: 12
                        onClicked: root.vocabExportRequested()
                    }
                }

                // 笔记内检索：非空时列表切到笔记命中集（MainDesktop reload）
                TextField {
                    id: vocabNoteSearchField
                    objectName: "vocabNoteSearchInput"
                    Layout.fillWidth: true
                    text: root.vocabNoteFilter
                    // 占位自绘（同 searchField：Material 浮动占位会重叠）
                    placeholderText: ""
                    color: Theme.text
                    selectByMouse: true
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 12
                    background: Rectangle {
                        radius: 6
                        color: Theme.card
                        border.width: 1
                        border.color: vocabNoteSearchField.activeFocus
                                      ? Theme.accent : Theme.divider
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "搜索笔记内容…"
                        color: Theme.textSecondary
                        font.pixelSize: 12
                        visible: vocabNoteSearchField.text.length === 0
                    }

                    onTextChanged: root.vocabNoteFilterRequested(text)

                    Keys.onPressed: function(event) {
                        if (event.key === Qt.Key_Escape) {
                            vocabNoteSearchField.clear()
                            event.accepted = true
                        }
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
                        objectName: "vocabCard"
                        readonly property string ownerWord: model.word
                        readonly property string ownerTags: model.tags || ""
                        readonly property string ownerNote: model.note || ""
                        width: ListView.view.width
                        highlighted: model.word === root.currentWord
                        onClicked: root.resultWordRequested(model.word)

                        contentItem: ColumnLayout {
                            spacing: 4

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    Layout.fillWidth: true
                                    text: ownerWord
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                ToolButton {
                                    objectName: "vocabAddTagButton"
                                    text: "+标签"
                                    font.pixelSize: 11
                                    onClicked: {
                                        tagDialog.pendingWord = ownerWord
                                        tagField.text = ""
                                        tagDialog.open()
                                    }
                                }

                                ToolButton {
                                    objectName: "vocabNoteButton"
                                    text: "笔记"
                                    font.pixelSize: 11
                                    onClicked: {
                                        noteDialog.pendingWord = ownerWord
                                        noteArea.text = ownerNote
                                        noteDialog.open()
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: model.snippet
                                color: Theme.textSecondary
                                font.pixelSize: 12
                                maximumLineCount: 2
                                elide: Text.ElideRight
                            }

                            // 词条标签 chips：点选即删（core 双命中才真）
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                visible: ownerTags.length > 0

                                Repeater {
                                    model: root.tagArray(ownerTags)
                                    delegate: Rectangle {
                                        objectName: "vocabTagChip"
                                        readonly property string tagName: modelData
                                        radius: height / 2
                                        implicitHeight: 22
                                        implicitWidth: tagChipLabel.implicitWidth + 14
                                        color: Qt.alpha(Theme.accent, 0.13)
                                        border.width: 1
                                        border.color: Qt.alpha(Theme.accent, 0.45)

                                        Label {
                                            id: tagChipLabel
                                            anchors.centerIn: parent
                                            text: parent.tagName + " ×"
                                            font.pixelSize: 11
                                            color: Theme.accent
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: root.vocabRemoveTagRequested(
                                                ownerWord, tagName)
                                        }
                                    }
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: ownerNote.length > 0
                                text: "笔记：" + ownerNote
                                color: Theme.textTertiary
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }

                    ScrollBar.vertical: ScrollBar {}

                    Label {
                        anchors.centerIn: parent
                        visible: vocabList.count === 0
                        text: root.vocabTagFilter.length > 0
                            ? "该标签下暂无生词" : "暂无生词"
                        color: Theme.textTertiary
                    }
                }
            }
        }
    }

    // —— M3-B 生词本编辑对话框（数据变更经信号回 MainDesktop 落库+刷新） ——

    Dialog {
        id: tagDialog
        objectName: "tagDialog"
        property string pendingWord: ""
        modal: true
        focus: true
        title: "加标签：" + pendingWord
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 48, 320)

        onAccepted: {
            var t = tagField.text.trim()
            if (t.length > 0) root.vocabAddTagRequested(pendingWord, t)
            tagField.text = ""
        }
        onRejected: tagField.text = ""

        contentItem: TextField {
            id: tagField
            placeholderText: "标签（如 CET4 / 易错）"
            selectByMouse: true
            Keys.onReturnPressed: tagDialog.accept()
            Keys.onEnterPressed: tagDialog.accept()
        }
    }

    Dialog {
        id: noteDialog
        objectName: "noteDialog"
        property string pendingWord: ""
        modal: true
        focus: true
        title: "笔记：" + pendingWord
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 48, 360)

        onAccepted: root.vocabNoteSaveRequested(pendingWord, noteArea.text)

        contentItem: TextArea {
            id: noteArea
            implicitHeight: 120
            wrapMode: TextEdit.Wrap
            placeholderText: "清空即删除笔记"
            selectByMouse: true
        }
    }

    // "a;b;c" → ["a","b","c"]（模型里的标签是 ';' 平铺串）
    function tagArray(s) {
        return (s && s.length > 0) ? s.split(";") : []
    }

    function focusSearchField() {
        searchField.forceActiveFocus()
    }
}
