import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// 定义内容显示组件 - 可复用
// UI 批三：查词页「词典/笔记」双页——词典页走 presentEntry 一站式管线
// （清洗 → 交叉引用 → .mdd 资源重写为 file://，配图随富文本直出），
// 笔记页荧光笔多色（色值映射同桌面 Theme.noteHighlight，core 只存位次）。
Column {
    id: root
    width: parent.width - 20
    spacing: responsive.baseSpacing
    signal lookupRequested(string word)

    function _handleLink(link) {
        if (!link || link.length === 0) return
        if (link.indexOf("unidict://lookup") !== 0) return
        var w = ""
        var q = link.indexOf("?word=")
        if (q >= 0) {
            w = decodeURIComponent(link.substring(q + 6))
        } else {
            var slash = link.lastIndexOf("/")
            if (slash >= 0) w = decodeURIComponent(link.substring(slash + 1))
        }
        if (w && w.length > 0) root.lookupRequested(w)
    }

    property int contentTab: 0
    property string noteText: ""
    property int noteColor: 0

    // currentWord 变化时重读笔记（直挂 handler 会随作用域失效，经
    // root 属性绑定桥转发才可靠）
    property string boundWord: currentWord
    onBoundWordChanged: root.reloadNote()

    function reloadNote() {
        noteText = vocab.getVocabNote(currentWord)
        noteColor = vocab.getVocabNoteColor(currentWord)
    }

    // 词典页富文本：有 aggregateLookup 富文本源走 presentEntry（配图
    // 重写在这里发生），否则回落纯文本口径（含 Word not found 提示）
    function dictHtml() {
        var raw = currentDefinitionHtml
        if (!raw || raw.length === 0) return currentDefinition || ""
        var p = lookup.presentEntry(raw, currentDictId)
        return p.html || currentDefinition || ""
    }

    // 播放控制区域
    Flow {
        width: parent.width
        spacing: responsive.baseSpacing
        visible: currentWord.length > 0

        Button {
            id: speakButton
            text: voice.isSpeaking() ? "⏸️停止" : "🔊播放"
            enabled: currentWord.length > 0
            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onClicked: {
                if (voice.isSpeaking()) {
                    voice.stopSpeaking()
                } else {
                    voice.speakText(currentWord)
                }
            }
            // 监听播放状态变化
            Timer {
                interval: 200
                running: true
                repeat: true
                onTriggered: speakButton.text = voice.isSpeaking() ? "⏸️停止" : "🔊播放"
            }
        }

        Button {
            text: "📖定义"
            enabled: currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onClicked: voice.speakText(currentDefinition)
        }

        Label {
            text: currentWord
            font.bold: true
            font.pixelSize: responsive.normalFont
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    // 词典/笔记切换（有词头才出现；空态维持原纯文本占位）
    TabBar {
        id: contentTabs
        width: parent.width
        visible: currentWord.length > 0
        onCurrentIndexChanged: root.contentTab = currentIndex

        TabButton {
            objectName: "contentTabDict"
            text: "词典"
            font.pixelSize: responsive.normalFont
        }
        TabButton {
            objectName: "contentTabNote"
            text: "笔记"
            font.pixelSize: responsive.normalFont
        }
    }

    StackLayout {
        width: parent.width
        currentIndex: root.contentTab

        // 词典页：富文本 + 配图（presentEntry 重写 file:// 直出）
        Item {
            width: parent.width
            implicitHeight: definitionView.height

            TextArea {
                id: definitionView
                readOnly: true
                wrapMode: Text.Wrap
                width: parent.width
                height: responsive.isMobile ? 140 : 200
                text: root.dictHtml()
                font.pixelSize: responsive.normalFont
                textFormat: TextEdit.RichText
                onLinkActivated: function(link) { root._handleLink(link) }

                Rectangle {
                    anchors.fill: parent
                    color: "transparent"
                    border.color: Theme.divider
                    border.width: 1
                    radius: Theme.radiusS
                    z: -1
                }

                Text {
                    anchors.centerIn: parent
                    visible: !parent.text
                    text: "Definition will appear here"
                    color: Theme.textTertiary
                    font.pixelSize: responsive.smallFont
                }
            }
        }

        // 笔记页：荧光底衬展示 + 编辑入口（色板同桌面批二口径）
        Column {
            width: parent.width
            spacing: responsive.baseSpacing / 2

            Rectangle {
                width: parent.width
                height: noteDisplay.implicitHeight + 20
                visible: root.noteText.length > 0
                radius: Theme.radiusS
                color: root.noteColor > 0 ? Theme.noteHighlight(root.noteColor) : Theme.card
                border.color: Theme.divider
                border.width: 1

                Label {
                    id: noteDisplay
                    anchors.fill: parent
                    anchors.margins: 10
                    wrapMode: Text.Wrap
                    text: root.noteText
                    color: Theme.text
                    font.pixelSize: responsive.normalFont
                }
            }

            Label {
                visible: root.noteText.length === 0
                text: "No note yet — jot one down for this word."
                color: Theme.textTertiary
                font.pixelSize: responsive.smallFont
            }

            Row {
                spacing: responsive.baseSpacing / 2

                Button {
                    objectName: "noteEditButton"
                    text: root.noteText.length > 0 ? "编辑笔记" : "添加笔记"
                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                    font.pixelSize: responsive.normalFont
                    onClicked: {
                        noteEditor.noteColor = root.noteColor
                        noteArea.text = root.noteText
                        noteEditor.open()
                    }
                }

                Button {
                    objectName: "noteDeleteButton"
                    text: "删除笔记"
                    visible: root.noteText.length > 0
                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                    font.pixelSize: responsive.normalFont
                    onClicked: {
                        // 空文本落库即删（core 口径），色位随删
                        vocab.setVocabNoteColored(currentWord, "", 0)
                        root.reloadNote()
                    }
                }
            }
        }
    }

    // 笔记编辑弹层：文本 + 荧光色板（0=无色 + 四色），保存随文本落库
    Popup {
        id: noteEditor
        anchors.centerIn: parent
        width: Math.min(440, root.width * 0.92)
        height: 290
        padding: 16
        modal: true
        property int noteColor: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            Label {
                text: "笔记 · " + currentWord
                font.weight: Font.DemiBold
                color: Theme.text
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                TextArea {
                    id: noteArea
                    wrapMode: TextArea.Wrap
                    placeholderText: "为该词条记点什么（清空保存即删除）"
                    color: Theme.text
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Repeater {
                    model: 5
                    delegate: Rectangle {
                        objectName: "noteColorSwatch_" + index
                        implicitWidth: 24
                        implicitHeight: 24
                        radius: height / 2
                        color: index === 0 ? "transparent" : Theme.noteHighlight(index)
                        border.width: noteEditor.noteColor === index ? 2 : 1
                        border.color: noteEditor.noteColor === index
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
                            onClicked: noteEditor.noteColor = index
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignRight

                Button {
                    objectName: "noteCancelButton"
                    flat: true
                    text: "取消"
                    onClicked: noteEditor.close()
                }
                Button {
                    objectName: "noteSaveButton"
                    text: "保存"
                    onClicked: {
                        vocab.setVocabNoteColored(currentWord,
                                                  noteArea.text,
                                                  noteEditor.noteColor)
                        root.reloadNote()
                        noteEditor.close()
                    }
                }
            }
        }
    }
}
