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
    // 生词本视图态（UI 批二 表格/卡片双视图）："card"=详情卡，"table"=
    // 紧凑行。纯视图态留本组件（不落库），跨 tab 保留
    property string vocabViewMode: "card"
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
    // 历史置顶（历史面最后一公里）：数据变更回 MainDesktop（reload + 状态）
    signal historyPinToggled(string word, bool pinned)
    // M3-B 生词本编辑：数据变更统一回 MainDesktop（它负责 reload + 状态行）
    signal vocabTagFilterRequested(string tag)
    signal vocabNoteFilterRequested(string text)
    signal vocabAddTagRequested(string word, string tag)
    signal vocabRemoveTagRequested(string word, string tag)
    // 荧光笔多色（UI 批二）：color=色板位次（0=无色，语义 core set_note）
    signal vocabNoteSaveRequested(string word, string note, int color)
    signal vocabExportRequested()
    signal vocabImportRequested()
    signal notesExportRequested()

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

                // UI 批一 搜索建议分组：模型行带 section（精确/前缀/词条，
                // MainDesktop 侧计算），段头小灰标
                section {
                    property: "section"
                    delegate: ItemDelegate {
                        required property string section
                        objectName: "suggestSectionHeader"
                        width: ListView.view.width
                        height: sectionLabel.implicitHeight + 6
                        enabled: false
                        contentItem: Label {
                            id: sectionLabel
                            text: parent.section
                            color: Theme.textTertiary
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }
                }

                delegate: ItemDelegate {
                    objectName: "suggestItem"
                    width: ListView.view.width
                    // audit 以 property("text") 取词（内容 Text 是富文本
                    // 高亮串，不便断言）
                    text: model.word

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
                    id: historyDelegate
                    objectName: "historyItem"
                    width: ListView.view.width
                    text: model.word
                    highlighted: model.word === root.currentWord
                    onClicked: root.resultWordRequested(model.word)

                    // UI 批一 历史命中计数：右侧 ×N 小胶囊（count>1 才出，
                    // 单次查词不噪）
                    contentItem: RowLayout {
                        spacing: 6

                        Label {
                            text: historyDelegate.text
                            color: historyDelegate.highlighted ? Theme.accent : Theme.text
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // 历史置顶（F3）：置顶行常亮、非置顶半透明；点即切
                        // （core 置顶块语义：置顶插到列表头，reload 后行位变化）
                        ToolButton {
                            objectName: "historyPinButton"
                            text: "📌"
                            font.pixelSize: 12
                            opacity: model.pinned === true ? 1.0 : 0.4
                            ToolTip.visible: hovered
                            ToolTip.text: model.pinned === true ? "取消置顶" : "置顶"
                            onClicked: root.historyPinToggled(model.word,
                                                               model.pinned !== true)
                        }

                        Rectangle {
                            objectName: "historyCountChip"
                            visible: model.count > 1
                            radius: height / 2
                            color: "transparent"
                            border.width: 1
                            border.color: Theme.divider
                            implicitWidth: countLabel.implicitWidth + 10
                            implicitHeight: countLabel.implicitHeight + 4

                            Label {
                                id: countLabel
                                objectName: "historyCountLabel"
                                anchors.centerIn: parent
                                text: "×" + model.count
                                color: Theme.textSecondary
                                font.pixelSize: 10
                            }
                        }
                    }
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

                    // CSV 导入（导入导出批）：补集语义——已存在整行跳过
                    ToolButton {
                        objectName: "vocabImportButton"
                        text: "导入CSV"
                        font.pixelSize: 12
                        onClicked: root.vocabImportRequested()
                    }

                    ToolButton {
                        objectName: "notesExportButton"
                        text: "导出笔记"
                        font.pixelSize: 12
                        onClicked: root.notesExportRequested()
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

                // 视图切换行（UI 批二 表格/卡片双视图）：选中态 accent 胶囊
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Label {
                        text: "视图"
                        color: Theme.textTertiary
                        font.pixelSize: 11
                    }

                    ToolButton {
                        objectName: "vocabViewCardButton"
                        text: "卡片"
                        font.pixelSize: 11
                        onClicked: root.vocabViewMode = "card"
                        background: Rectangle {
                            radius: Theme.radiusM
                            color: root.vocabViewMode === "card"
                                       ? Qt.alpha(Theme.accent, 0.15) : "transparent"
                            border.width: root.vocabViewMode === "card" ? 1 : 0
                            border.color: Theme.accent
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: root.vocabViewMode === "card"
                                       ? Theme.accent : Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    ToolButton {
                        objectName: "vocabViewTableButton"
                        text: "表格"
                        font.pixelSize: 11
                        onClicked: root.vocabViewMode = "table"
                        background: Rectangle {
                            radius: Theme.radiusM
                            color: root.vocabViewMode === "table"
                                       ? Qt.alpha(Theme.accent, 0.15) : "transparent"
                            border.width: root.vocabViewMode === "table" ? 1 : 0
                            border.color: Theme.accent
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: root.vocabViewMode === "table"
                                       ? Theme.accent : Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                ListView {
                    id: vocabList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: root.vocabModel

                    // 双视图（UI 批二）：同一委托两种形态——卡片=详情（操作
                    // 按钮齐备），表格=紧凑行（扫读用，点击即查词）。objectName
                    // 随形态切换（审计按形态找 vocabCard/vocabTableRow）；
                    // contentItem 包一层 Item 承载两套布局的隐式尺寸切换
                    delegate: ItemDelegate {
                        id: vocabRoot
                        objectName: root.vocabViewMode === "card"
                                        ? "vocabCard" : "vocabTableRow"
                        readonly property string ownerWord: model.word
                        readonly property string ownerTags: model.tags || ""
                        readonly property string ownerNote: model.note || ""
                        readonly property int ownerNoteColor: model.noteColor || 0
                        width: vocabList.width
                        highlighted: model.word === root.currentWord
                        onClicked: root.resultWordRequested(model.word)
                        // audit 以 property("text") 取词（同 suggestItem/
                        // historyItem 口径）
                        text: model.word

                        contentItem: Item {
                            implicitHeight: root.vocabViewMode === "card"
                                                ? cardBody.implicitHeight
                                                : rowBody.implicitHeight
                            implicitWidth: Math.max(cardBody.implicitWidth,
                                                    rowBody.implicitWidth)

                            // —— 卡片态 ——
                            ColumnLayout {
                                id: cardBody
                                width: parent.width
                                visible: root.vocabViewMode === "card"
                                spacing: 4

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Label {
                                        Layout.fillWidth: true
                                        text: vocabRoot.ownerWord
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
                                            tagDialog.pendingWord = vocabRoot.ownerWord
                                            tagField.text = ""
                                            tagDialog.open()
                                        }
                                    }

                                    ToolButton {
                                        objectName: "vocabNoteButton"
                                        text: "笔记"
                                        font.pixelSize: 11
                                        onClicked: {
                                            noteDialog.pendingWord = vocabRoot.ownerWord
                                            noteArea.text = vocabRoot.ownerNote
                                            noteDialog.pendingColor = vocabRoot.ownerNoteColor
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
                                    visible: vocabRoot.ownerTags.length > 0

                                    Repeater {
                                        model: root.tagArray(vocabRoot.ownerTags)
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
                                                    vocabRoot.ownerWord, tagName)
                                            }
                                        }
                                    }
                                }

                                // 笔记荧光（UI 批二 多色）：带色位渲染荧光底
                                // 衬（位次映射 Theme.noteHighlight 唯一出口）
                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: vocabRoot.ownerNote.length > 0
                                             && vocabRoot.ownerNoteColor > 0
                                    radius: Theme.radiusS
                                    color: Theme.noteHighlight(vocabRoot.ownerNoteColor)
                                    implicitHeight: noteHiLabel.implicitHeight + 8

                                    Label {
                                        id: noteHiLabel
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        leftPadding: 6
                                        rightPadding: 6
                                        text: "笔记：" + vocabRoot.ownerNote
                                        color: Theme.text
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: vocabRoot.ownerNote.length > 0
                                             && vocabRoot.ownerNoteColor === 0
                                    text: "笔记：" + vocabRoot.ownerNote
                                    color: Theme.textTertiary
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            // —— 表格态 ——
                            RowLayout {
                                id: rowBody
                                width: parent.width
                                anchors.verticalCenter: parent.verticalCenter
                                visible: root.vocabViewMode === "table"
                                spacing: 6

                                // 荧光点：带色笔记的紧凑可视位
                                Rectangle {
                                    objectName: "vocabRowNoteDot"
                                    visible: vocabRoot.ownerNote.length > 0
                                             && vocabRoot.ownerNoteColor > 0
                                    implicitWidth: 9
                                    implicitHeight: 9
                                    radius: 4
                                    color: Theme.noteHighlight(vocabRoot.ownerNoteColor)
                                }

                                Label {
                                    text: vocabRoot.ownerWord
                                    color: vocabRoot.highlighted
                                               ? Theme.accent : Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // 标签汇总：首个 + 多出计数（表格态不铺 chips）
                                Label {
                                    visible: vocabRoot.ownerTags.length > 0
                                    text: {
                                        var a = root.tagArray(vocabRoot.ownerTags)
                                        return a.length > 1
                                                   ? a[0] + " +" + (a.length - 1) : a[0]
                                    }
                                    color: Theme.textTertiary
                                    font.pixelSize: 11
                                }
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
        // 荧光笔色板位次（UI 批二）：0=无色 1-4=黄/绿/蓝/粉，打开时按
        // 该词既有色位回填（ownerNoteColor）
        property int pendingColor: 0
        modal: true
        focus: true
        title: "笔记：" + pendingWord
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: Overlay.overlay
        width: Math.min(parent.width - 48, 360)

        onAccepted: root.vocabNoteSaveRequested(pendingWord, noteArea.text,
                                                pendingColor)

        contentItem: ColumnLayout {
            spacing: 8

            TextArea {
                id: noteArea
                objectName: "noteDialogArea"
                implicitHeight: 120
                wrapMode: TextEdit.Wrap
                placeholderText: "清空即删除笔记"
                selectByMouse: true
            }

            // 荧光色板：点选即换（与词条卡头 notePopup 同款映射）
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: 5
                    delegate: Rectangle {
                        objectName: "noteColorSwatch_" + index
                        readonly property int colorIdx: index
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: height / 2
                        color: index === 0 ? "transparent"
                                           : Theme.noteHighlight(index)
                        border.width: noteDialog.pendingColor === index ? 2 : 1
                        border.color: noteDialog.pendingColor === index
                                          ? Theme.accent : Theme.divider

                        // 无色档斜杠指示（透明底不能空到看不出可点）
                        Rectangle {
                            visible: index === 0
                            anchors.centerIn: parent
                            width: 14
                            height: 1
                            color: Theme.divider
                            rotation: 45
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: noteDialog.pendingColor = index
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }
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
