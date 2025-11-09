import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Dialogs 1.3

ApplicationWindow {
    id: win
    visible: true
    width: 640
    height: 480
    title: "Unidict (QML)"

    property string currentWord: ""
    property string currentDefinition: ""

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        TabBar {
            id: tabs
            width: parent.width
            TabButton { text: "Search" }
            TabButton { text: "History" }
            TabButton { text: "Vocab" }
        }

        StackLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: tabs.bottom
            currentIndex: tabs.currentIndex

            // Search Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: 8
                    Row {
                        spacing: 8
                        TextField {
                            id: input
                            width: parent.width - searchBtn.implicitWidth - modeBox.implicitWidth - 24
                            placeholderText: "Enter a word..."
                            onTextChanged: suggestions.model = input.text.length > 0 ? lookup.suggestPrefix(input.text, 20) : []
                            onAccepted: searchBtn.clicked()
                        }
                        ComboBox {
                            id: modeBox
                            model: ["Exact", "Prefix", "Fuzzy", "Wildcard", "Regex"]
                            currentIndex: 0
                        }
                        Button {
                            id: searchBtn
                            text: "Search"
                            onClicked: {
                                currentWord = input.text
                                if (modeBox.currentText === "Exact") {
                                    currentDefinition = lookup.lookupDefinition(currentWord)
                                    resultsView.model = []
                                } else if (modeBox.currentText === "Prefix") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.suggestPrefix(currentWord, 100)
                                } else if (modeBox.currentText === "Fuzzy") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.suggestFuzzy(currentWord, 100)
                                } else if (modeBox.currentText === "Wildcard") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.searchWildcard(currentWord, 100)
                                } else if (modeBox.currentText === "Regex") {
                                    currentDefinition = ""
                                    resultsView.model = lookup.searchRegex(currentWord, 100)
                                }
                            }
                        }
                    }
                    Column {
                        spacing: 4
                        Label {
                            text: lookup.loadedDictionaries().length > 0 ? "Loaded: " + lookup.loadedDictionaries().join(", ") :
                                  "No dictionaries loaded. Set UNIDICT_DICTS to paths (':' or ';' separated)."
                            wrapMode: Text.Wrap
                            width: parent.width
                        }
                        Label {
                            text: "Indexed words: " + lookup.indexedWordCount()
                        }
                    }
                    Row {
                        spacing: 8
                        ListView {
                            id: suggestions
                            width: parent.width * 0.33
                            height: 260
                            clip: true
                            model: []
                            delegate: ItemDelegate {
                                text: modelData
                                onClicked: {
                                    input.text = modelData
                                    searchBtn.clicked()
                                }
                            }
                        }
                        ListView {
                            id: resultsView
                            width: parent.width * 0.33
                            height: 260
                            clip: true
                            model: []
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
                            width: parent.width - suggestions.width - resultsView.width - 16
                            height: 260
                            TextArea {
                                id: definitionView
                                readOnly: true
                                wrapMode: Text.Wrap
                                text: currentDefinition
                            }
                        }
                    }
                    Button {
                        text: "Save to Vocab"
                        enabled: currentWord.length > 0 && currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
                        onClicked: lookup.addToVocabulary(currentWord, currentDefinition)
                    }
                }
            }

            // History Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: 8
                    Row {
                        spacing: 8
                        Button { text: "Refresh"; onClicked: historyModel = lookup.searchHistory(200) }
                        Button { text: "Clear"; onClicked: { lookup.clearHistory(); historyModel = [] } }
                    }
                    TextField {
                        id: historyFilter
                        placeholderText: "Filter history..."
                        onTextChanged: {
                            var all = lookup.searchHistory(200)
                            if (historyFilter.text.length === 0) { historyModel = all; return }
                            var filtered = []
                            for (var i = 0; i < all.length; ++i) {
                                var w = all[i]
                                if (w.toLowerCase().indexOf(historyFilter.text.toLowerCase()) !== -1) filtered.push(w)
                            }
                            historyModel = filtered
                        }
                    }
                    ListView {
                        id: historyList
                        anchors.fill: parent
                        model: historyModel
                        delegate: ItemDelegate {
                            text: modelData
                            onClicked: {
                                tabs.currentIndex = 0
                                input.text = modelData
                            }
                        }
                    }
                }
                property var historyModel: lookup.searchHistory(200)
            }

            // Vocab Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: 8
                    Row {
                        spacing: 8
                        Button { text: "Refresh"; onClicked: vocabModel = lookup.vocabulary() }
                        Button { text: "Clear"; onClicked: { lookup.clearVocabulary(); vocabModel = [] } }
                        Button { text: "Export CSV"; onClicked: saveDialog.open() }
                    }
                    TextField {
                        id: vocabFilter
                        placeholderText: "Filter vocabulary..."
                        onTextChanged: {
                            var all = lookup.vocabulary()
                            if (vocabFilter.text.length === 0) { vocabModel = all; return }
                            var filtered = []
                            for (var i = 0; i < all.length; ++i) {
                                var item = all[i]
                                if (item.word.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1 ||
                                    item.definition.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1) {
                                    filtered.push(item)
                                }
                            }
                            vocabModel = filtered
                        }
                    }
                    ListView {
                        id: vocabList
                        anchors.fill: parent
                        model: vocabModel
                        delegate: ItemDelegate {
                            width: parent.width
                            text: modelData.word + ": " + modelData.definition
                            onClicked: {
                                tabs.currentIndex = 0
                                input.text = modelData.word
                            }
                        }
                    }
                }
                property var vocabModel: lookup.vocabulary()
                FileDialog {
                    id: saveDialog
                    title: "Export Vocabulary CSV"
                    selectExisting: false
                    nameFilters: ["CSV files (*.csv)", "All files (*)"]
                    onAccepted: {
                        if (lookup.exportVocabCsv(saveDialog.fileUrl.toString().replace("file://", ""))) {
                            // no-op
                        }
                    }
                }
            }
        }
    }
}
