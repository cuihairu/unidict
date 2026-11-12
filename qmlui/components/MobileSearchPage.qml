import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"

// 移动端主壳·Search 页（LAY-002 自 Main.qml 拆出）
// Search tab（page 0）——ca8f835 重排事故把输入区与结果区副本
// 粘残：此处只剩引用不存在 id 的孤块（input/searchBtn/modeBox
// 无定义、resultsView 与真身重复 id），按 4370c36 骨架恢复输入
// 区；结果显示区真身见下方「结果显示区域」
//
// 窗口状态与根函数经 win 引用读写；responsive / suggestionModel
// 是 Main.qml 文件级 id，组件文件看不见父文件 id，故经本页同名字段
// 注入（实例化处同 id 名赋值——文档 id 优先于本组件同名字段，
// 与 SidebarPanel / Settings*Page 的 *Service 别名同律）；
// Theme / vocab / lookup 等 context 属性随引擎传播，无需注入。
Item {
    objectName: "mobileSearchPage"

    property var win: null
    property var responsive: null
    property var suggestionModel: null

    Column {
        width: parent.width
        spacing: responsive.baseSpacing

        // 搜索输入区域 - 响应式布局
        Flow {
            width: parent.width
            spacing: responsive.baseSpacing

            TextField {
                id: input
                width: responsive.isMobile ? parent.width : Math.max(200, parent.width * 0.5)
                height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                placeholderText: "Enter a word..."
                font.pixelSize: responsive.normalFont
                onTextChanged: win.reloadSuggestions(input.text)
                onAccepted: searchBtn.clicked()
            }

            ComboBox {
                id: modeBox
                model: ["Exact", "Prefix", "Fuzzy", "Wildcard", "Regex"]
                currentIndex: 0
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                width: responsive.isMobile ? Math.min(120, parent.width * 0.3) : 120
            }

            Button {
                id: searchBtn
                text: "Search"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                width: responsive.isMobile ? Math.min(80, parent.width * 0.2) : implicitWidth
                onClicked: {
                    win.currentWord = input.text
                    if (modeBox.currentText === "Exact") {
                        // UI 批三：富文本命中走 aggregateLookup 首
                        // 分组（清洗+交叉引用已做），纯文本口径留给
                        // 存生词/TTS/AI；无分组回落 lookupDefinition
                        win.currentDefinitionHtml = ""
                        win.currentDictId = ""
                        var groups = lookup.aggregateLookup(win.currentWord, {
                            "maxTotalResults": 20,
                            "sanitizeHtml": true,
                            "rewriteCrossRefs": true
                        })
                        if (groups && groups.length > 0
                                && groups[0].entries
                                && groups[0].entries.length > 0) {
                            var firstDef = groups[0].entries[0].definition || ""
                            win.currentDefinitionHtml = firstDef
                            win.currentDefinition = lookup.extractTextFromHtml(firstDef)
                            win.currentDictId = groups[0].dictionaryId || ""
                        } else {
                            win.currentDefinition = lookup.lookupDefinition(win.currentWord)
                        }
                        resultsView.model = []
                    } else if (modeBox.currentText === "Prefix") {
                        win.currentDefinition = ""
                        resultsView.model = lookup.suggestPrefix(win.currentWord, 100)
                    } else if (modeBox.currentText === "Fuzzy") {
                        win.currentDefinition = ""
                        resultsView.model = lookup.suggestFuzzy(win.currentWord, 100)
                    } else if (modeBox.currentText === "Wildcard") {
                        win.currentDefinition = ""
                        resultsView.model = lookup.searchWildcard(win.currentWord, 100)
                    } else if (modeBox.currentText === "Regex") {
                        win.currentDefinition = ""
                        resultsView.model = lookup.searchRegex(win.currentWord, 100)
                    }
                }
            }

            Button {
                text: "Save to Vocabulary"
                enabled: win.currentWord.length > 0 && win.currentDefinition.length > 0 && !win.currentDefinition.startsWith("Word not found")
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: vocab.addToVocabulary(win.currentWord, win.currentDefinition)
            }
        }

        Column {
            spacing: responsive.baseSpacing / 2
            // 顶部加密提示横幅（若存在加密词典）
            Label {
                id: encryptedBanner
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: responsive.smallFont
                color: Theme.danger
                text: {
                    var _stamp = lookup.dictionariesStamp
                    var metas = lookup.dictionariesMeta()
                    var hasEncrypted = false
                    for (var i = 0; i < metas.length; ++i) {
                        var m = metas[i]
                        var desc = m.description ? m.description : ""
                        if (desc.indexOf("[encrypted]") !== -1) { hasEncrypted = true; break }
                    }
                    return hasEncrypted ? "Warning: Some dictionaries are encrypted. Content may require UNIDICT_MDICT_PASSWORD or may be unsupported depending on encryption type." : ''
                }
                visible: text.length > 0
            }
            RowLayout {
                width: parent.width
                spacing: responsive.baseSpacing / 2
                visible: encryptedBanner.visible

                TextField {
                    id: mdictPasswordField
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: lookup.hasMdictPassword()
                        ? "MDict password is set (enter to replace)"
                        : "Enter MDict password"
                }
                Button {
                    text: "Apply & Reload"
                    enabled: mdictPasswordField.text.length > 0
                    onClicked: {
                        var okPw = lookup.setMdictPassword(mdictPasswordField.text)
                        if (!okPw) { win.showToast("Password empty", 1500); return }
                        var ok = lookup.reloadDictionariesFromEnv()
                        win.showToast(ok ? "Reloaded dictionaries" : "Reload failed", 1500)
                        mdictPasswordField.text = ""
                    }
                }
                Button {
                    text: "Clear"
                    onClicked: {
                        lookup.clearMdictPassword()
                        mdictPasswordField.text = ""
                        win.showToast("Password cleared", 1500)
                    }
                }
            }
            // 词典勾选管理（UI 批三）：逐词典启用开关，stamp 绑定
            // 重读；启用只影响查询路径，装载清单照列（core 口径）
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                font.pixelSize: responsive.smallFont
                visible: lookup.dictionariesMeta().length === 0
                text: "No dictionaries loaded. Set UNIDICT_DICTS to paths (':' or ';' separated)."
            }
            Repeater {
                model: {
                    var _stamp = lookup.dictionariesStamp
                    return lookup.dictionariesMeta()
                }
                delegate: RowLayout {
                    required property var modelData
                    required property int index
                    width: parent.width
                    spacing: responsive.baseSpacing / 2

                    CheckBox {
                        objectName: "mobileDictEnabledCheck_" + index
                        checked: modelData.enabled !== false
                        onToggled: lookup.setDictionaryEnabled(
                            modelData.name || "", checked)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: (modelData.name || "unknown") + " (" + (modelData.wordCount || 0) + ")"
                        font.pixelSize: responsive.smallFont
                        color: Theme.text
                        elide: Text.ElideRight
                    }
                    Label {
                        text: modelData.format || ""
                        font.pixelSize: responsive.smallFont
                        color: Theme.textTertiary
                    }
                }
            }
            Label {
                text: "Indexed words: " + lookup.indexedWordCount()
                font.pixelSize: responsive.smallFont
            }
        }

        // 结果显示区域 - 移动端垂直布局，桌面端水平布局
        Item {
            width: parent.width
            height: responsive.isMobile ? 500 : 260

            // 移动端垂直布局
            Column {
                anchors.fill: parent
                spacing: responsive.baseSpacing
                visible: responsive.isMobile

                ListView {
                    id: suggestions
                    width: parent.width
                    height: 120
                    clip: true
                    model: suggestionModel

                    // 建议分组（UI 批三，桌面同款）：段头小灰标
                    section {
                        property: "section"
                        delegate: ItemDelegate {
                            required property string section
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
                        width: parent.width
                        height: Math.max(36, responsive.minTouchTarget)
                        text: model.word
                        font.pixelSize: responsive.normalFont
                        onClicked: {
                            input.text = model.word
                            searchBtn.clicked()
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: parent.count === 0
                        text: "Suggestions will appear here"
                        color: Theme.textTertiary
                        font.pixelSize: responsive.smallFont
                    }
                }

                ListView {
                    id: resultsView
                    width: parent.width
                    height: 120
                    clip: true
                    model: []
                    delegate: ItemDelegate {
                        width: parent.width
                        height: Math.max(36, responsive.minTouchTarget)
                        text: modelData
                        font.pixelSize: responsive.normalFont
                        onClicked: {
                            input.text = modelData
                            modeBox.currentIndex = 0
                            searchBtn.clicked()
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: parent.count === 0
                        text: "Search results will appear here"
                        color: Theme.textTertiary
                        font.pixelSize: responsive.smallFont
                    }
                }

                ScrollView {
                    width: parent.width
                    height: responsive.isMobile ? 240 : 260
                    DefinitionContent {
                        onLookupRequested: function(w) {
                            input.text = w
                            searchBtn.clicked()
                        }
                    }
                }
            }

            // 桌面端水平布局
            Row {
                anchors.fill: parent
                spacing: responsive.baseSpacing
                visible: !responsive.isMobile

                ListView {
                    id: suggestionsDesktop
                    width: parent.width * 0.33
                    height: 260
                    clip: true
                    model: suggestionModel
                    delegate: ItemDelegate {
                        text: model.word
                        onClicked: {
                            input.text = model.word
                            searchBtn.clicked()
                        }
                    }
                }
                ListView {
                    id: resultsViewDesktop
                    width: parent.width * 0.33
                    height: 260
                    clip: true
                    model: resultsView.model
                    delegate: ItemDelegate {
                        text: modelData
                        onClicked: {
                            input.text = modelData
                            modeBox.currentIndex = 0
                            searchBtn.clicked()
                        }
                    }
                }
                ScrollView {
                    width: parent.width - suggestionsDesktop.width - resultsViewDesktop.width - 16
                    height: 260
                    DefinitionContent {
                        onLookupRequested: function(w) {
                            input.text = w
                            searchBtn.clicked()
                        }
                    }
                }
            }
        }
        Button {
            text: "Save to Vocab"
            enabled: win.currentWord.length > 0 && win.currentDefinition.length > 0 && !win.currentDefinition.startsWith("Word not found")
            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            width: responsive.isMobile ? parent.width : implicitWidth
            onClicked: vocab.addToVocabulary(win.currentWord, win.currentDefinition)
        }

        // AI Tools
        GroupBox {
            title: "AI Tools"
            width: parent.width
            Column {
                width: parent.width
                spacing: responsive.baseSpacing / 2
                Flow {
                    width: parent.width
                    spacing: responsive.baseSpacing
                    Button {
                        text: "Translate Definition → Chinese"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        onClicked: {
                            var txt = win.currentDefinition.length>0 ? win.currentDefinition : input.text
                            if (!txt || txt.length===0) { win.showToast("Nothing to translate"); return }
                            aiOut.text = ai.translate(txt, "zh")
                        }
                    }
                    Button {
                        text: "Grammar Check"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        onClicked: {
                            var txt = win.currentDefinition.length>0 ? win.currentDefinition : input.text
                            if (!txt || txt.length===0) { win.showToast("Nothing to check"); return }
                            aiOut.text = ai.grammarCheck(txt)
                        }
                    }
                }
                TextArea {
                    id: aiOut
                    readOnly: true
                    wrapMode: Text.Wrap
                    width: parent.width
                    height: Math.max(80, implicitHeight)
                    font.pixelSize: responsive.smallFont
                    placeholderText: "AI output will appear here"
                }
            }
        }
    }
}
