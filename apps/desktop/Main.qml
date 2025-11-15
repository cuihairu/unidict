import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
// import QtQuick.Dialogs 6.3  // 暂时注释掉，使用自定义文件对话框
import "mobile/common"

ApplicationWindow {
    id: win
    visible: true
    // 响应式窗口尺寸
    width: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.width : 640
    height: Qt.platform.os === "android" || Qt.platform.os === "ios" ? Screen.height : 480
    title: "Unidict (QML)"

    // 移动端全屏显示
    visibility: (Qt.platform.os === "android" || Qt.platform.os === "ios") ?
                ApplicationWindow.FullScreen : ApplicationWindow.Windowed

    property string currentWord: ""
    property string currentDefinition: ""

    // 响应式布局配置
    ResponsiveLayout {
        id: responsive
        anchors.fill: parent
    }

    Column {
        anchors.fill: parent
        anchors.margins: responsive.baseMargin
        anchors.topMargin: responsive.safeAreaTop + responsive.baseMargin
        anchors.bottomMargin: responsive.safeAreaBottom + responsive.baseMargin
        spacing: responsive.baseSpacing

        TabBar {
            id: tabs
            width: parent.width
            height: responsive.tabHeight
            TabButton {
                text: "Search"
                font.pixelSize: responsive.normalFont
                height: responsive.tabHeight
            }
            TabButton {
                text: "History"
                font.pixelSize: responsive.normalFont
                height: responsive.tabHeight
            }
            TabButton {
                text: "Vocab"
                font.pixelSize: responsive.normalFont
                height: responsive.tabHeight
            }
            TabButton {
                text: "🔊语音"
                font.pixelSize: responsive.normalFont
                height: responsive.tabHeight
            }
            TabButton {
                text: "📊学习"
                font.pixelSize: responsive.normalFont
                height: responsive.tabHeight
            }
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
                            onTextChanged: suggestions.model = input.text.length > 0 ? lookup.suggestPrefix(input.text, 20) : []
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
                                currentWord = input.text
                                if (modeBox.currentText === "Exact") {
                                    currentDefinition = lookup.lookupDefinition(currentWord)
                                    resultsView.model = []
                                    // 记录学习数据
                                    learningManager.recordLookup(currentWord, currentDefinition)
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
                    GroupBox {
                        title: "Full-Text Index"
                        width: parent.width
                        font.pixelSize: responsive.normalFont

                        Column {
                            width: parent.width
                            spacing: responsive.baseSpacing

                            // 按钮行 - 响应式流布局
                            Flow {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                Button {
                                    text: "Save Index"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: ftSaveDialog.open()
                                }
                                Button {
                                    text: "Load Index"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: ftLoadDialog.open()
                                }
                                ComboBox {
                                    id: ftCompat
                                    model: ["strict","auto","loose"]
                                    currentIndex: 1
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    width: Math.min(100, parent.width * 0.25)
                                }
                                Button {
                                    text: "Stats"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: ftStatsDialog.open()
                                }
                                Button {
                                    text: "Upgrade"
                                    height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                    font.pixelSize: responsive.smallFont
                                    onClicked: ftInDialog.open()
                                }
                            }

                            TextArea {
                                id: ftInfo
                                readOnly: true
                                wrapMode: Text.Wrap
                                width: parent.width
                                height: responsive.isMobile ? 60 : 80
                                font.pixelSize: responsive.smallFont
                            }
                        }

                        // 使用移动端优化的文件对话框
                        MobileFileDialog {
                            id: ftSaveDialog
                            title: "Save Full-Text Index"
                            selectExisting: false
                            nameFilters: ["Index (*.index)","All (*)"]
                            onAccepted: {
                                fulltext.saveIndex(ftSaveDialog.fileUrl.toString().replace("file://",""))
                            }
                        }

                        MobileFileDialog {
                            id: ftLoadDialog
                            title: "Load Full-Text Index"
                            selectExisting: true
                            nameFilters: ["Index (*.index)","All (*)"]
                            onAccepted: {
                                var err = fulltext.loadIndex(ftLoadDialog.fileUrl.toString().replace("file://",""), ftCompat.currentText)
                                ftInfo.text = err.length===0? "Loaded ("+ftCompat.currentText+")" : ("Load failed: "+err)
                            }
                        }

                        MobileFileDialog {
                            id: ftStatsDialog
                            title: "Index Stats"
                            selectExisting: true
                            nameFilters: ["Index (*.index)","All (*)"]
                            onAccepted: {
                                var m = fulltext.statsFromFile(ftStatsDialog.fileUrl.toString().replace("file://",""))
                                var s = ""
                                for (var k in m) s += k+": "+m[k]+"\n"
                                ftInfo.text = s
                            }
                        }

                        MobileFileDialog {
                            id: ftInDialog
                            title: "Upgrade From"
                            selectExisting: true
                            nameFilters: ["Index (*.index)","All (*)"]
                            onAccepted: ftOutDialog.open()
                        }

                        MobileFileDialog {
                            id: ftOutDialog
                            title: "Upgrade To"
                            selectExisting: false
                            nameFilters: ["Index (*.index)","All (*)"]
                            onAccepted: {
                                fulltext.upgrade(ftInDialog.fileUrl.toString().replace("file://",""), ftOutDialog.fileUrl.toString().replace("file://",""))
                            }
                        }
                    }
                    Column {
                        spacing: responsive.baseSpacing / 2
                        Label {
                            text: lookup.loadedDictionaries().length > 0 ? "Loaded: " + lookup.loadedDictionaries().join(", ") :
                                  "No dictionaries loaded. Set UNIDICT_DICTS to paths (':' or ';' separated)."
                            wrapMode: Text.Wrap
                            width: parent.width
                            font.pixelSize: responsive.smallFont
                        }
                        Label {
                            text: "Indexed words: " + lookup.indexedWordCount()
                            font.pixelSize: responsive.smallFont
                        }
                    }

                    // 结果显示区域 - 移动端垂直布局，桌面端水平布局
                    Item {
                        width: parent.width
                        height: responsive.isMobile ? 400 : 260

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
                                model: []
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: Math.max(36, responsive.minTouchTarget)
                                    text: modelData
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        input.text = modelData
                                        searchBtn.clicked()
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "Suggestions will appear here"
                                    color: "gray"
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
                                    color: "gray"
                                    font.pixelSize: responsive.smallFont
                                }
                            }

                            ScrollView {
                                width: parent.width
                                height: 160
                                DefinitionContent {}
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
                                model: suggestions.model
                                delegate: ItemDelegate {
                                    text: modelData
                                    onClicked: {
                                        input.text = modelData
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
                                DefinitionContent {}
                            }
                        }
                    }
                    Button {
                        text: "Save to Vocab"
                        enabled: currentWord.length > 0 && currentDefinition.length > 0 && !currentDefinition.startsWith("Word not found")
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
                        width: responsive.isMobile ? parent.width : implicitWidth
                        onClicked: lookup.addToVocabulary(currentWord, currentDefinition)
                    }
                }
            }

            // History Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: responsive.baseSpacing

                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        Button {
                            text: "Refresh"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: historyModel = lookup.searchHistory(200)
                        }
                        Button {
                            text: "Clear"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: { lookup.clearHistory(); historyModel = [] }
                        }
                    }

                    TextField {
                        id: historyFilter
                        placeholderText: "Filter history..."
                        width: parent.width
                        height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
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
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.top: historyFilter.bottom
                        anchors.topMargin: responsive.baseSpacing
                        model: historyModel
                        delegate: ItemDelegate {
                            width: parent.width
                            height: Math.max(36, responsive.minTouchTarget)
                            text: modelData
                            font.pixelSize: responsive.normalFont
                            onClicked: {
                                tabs.currentIndex = 0
                                input.text = modelData
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.count === 0
                            text: "No search history"
                            color: "gray"
                            font.pixelSize: responsive.smallFont
                        }
                    }
                }
                property var historyModel: lookup.searchHistory(200)
            }

            // Vocab Tab
            Item {
                Column {
                    anchors.fill: parent
                    spacing: responsive.baseSpacing

                    Flow {
                        width: parent.width
                        spacing: responsive.baseSpacing

                        Button {
                            text: "Refresh"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: vocabModel = lookup.vocabulary()
                        }
                        Button {
                            text: "Clear"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: { lookup.clearVocabulary(); vocabModel = [] }
                        }
                        Button {
                            text: "Export CSV"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.normalFont
                            onClicked: saveDialog.open()
                        }
                    }

                    TextField {
                        id: vocabFilter
                        placeholderText: "Filter vocabulary..."
                        width: parent.width
                        height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.normalFont
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
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.top: vocabFilter.bottom
                        anchors.topMargin: responsive.baseSpacing
                        model: vocabModel
                        delegate: ItemDelegate {
                            width: parent.width
                            height: Math.max(60, responsive.minTouchTarget + 16)

                            Label {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.margins: responsive.baseSpacing
                                text: modelData.word + ": " + modelData.definition
                                font.pixelSize: responsive.normalFont
                                wrapMode: Text.WordWrap
                                maximumLineCount: 3
                                elide: Text.ElideRight
                            }

                            onClicked: {
                                tabs.currentIndex = 0
                                input.text = modelData.word
                            }
                        }

                        Text {
                            anchors.centerIn: parent
                            visible: parent.count === 0
                            text: "No vocabulary saved"
                            color: "gray"
                            font.pixelSize: responsive.smallFont
                        }
                    }
                }
                property var vocabModel: lookup.vocabulary()

                MobileFileDialog {
                    id: saveDialog
                    title: "Export Vocabulary CSV"
                    selectExisting: false
                    nameFilters: ["CSV files (*.csv)", "All files (*)"]
                    onAccepted: {
                        if (lookup.exportVocabCsv(saveDialog.fileUrl.toString().replace("file://", ""))) {
                            // 成功导出
                        }
                    }
                }
            }

            // 语音设置 Tab
            Item {
                ScrollView {
                    anchors.fill: parent
                    contentHeight: voiceColumn.implicitHeight

                    Column {
                        id: voiceColumn
                        width: parent.width
                        anchors.margins: responsive.baseMargin
                        spacing: responsive.baseSpacing

                        GroupBox {
                            title: "🎵 语音设置"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                // 语音引擎状态
                                Row {
                                    spacing: responsive.baseSpacing
                                    Label {
                                        text: "引擎状态:"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Label {
                                        text: lookup.availableVoices().length > 0 ? "✅ 可用" : "❌ 不可用"
                                        color: lookup.availableVoices().length > 0 ? "green" : "red"
                                        font.pixelSize: responsive.normalFont
                                    }
                                }

                                // 音量控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "音量: " + Math.round(volumeSlider.value * 100) + "%"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: volumeSlider
                                        from: 0.0
                                        to: 1.0
                                        value: 0.8
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setVolume(value)
                                    }
                                }

                                // 语速控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "语速: " + rateSlider.value.toFixed(1) + "x"
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: rateSlider
                                        from: 0.1
                                        to: 2.0
                                        value: 1.0
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setRate(value)
                                    }
                                }

                                // 音调控制
                                Column {
                                    width: parent.width
                                    spacing: responsive.baseSpacing / 2

                                    Label {
                                        text: "音调: " + pitchSlider.value.toFixed(1)
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Slider {
                                        id: pitchSlider
                                        from: -1.0
                                        to: 1.0
                                        value: 0.0
                                        stepSize: 0.1
                                        width: parent.width
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        onValueChanged: lookup.setPitch(value)
                                    }
                                }
                            }
                        }

                        GroupBox {
                            title: "🎤 可用语音"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            ListView {
                                width: parent.width
                                height: Math.min(200, count * responsive.minTouchTarget)
                                clip: true
                                model: lookup.availableVoices()
                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: Math.max(36, responsive.minTouchTarget)
                                    text: modelData
                                    font.pixelSize: responsive.normalFont
                                    onClicked: {
                                        lookup.setVoice(modelData)
                                        voiceStatus.text = "当前语音: " + modelData
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "No voices available"
                                    color: "gray"
                                    font.pixelSize: responsive.smallFont
                                }
                            }
                        }

                        Label {
                            id: voiceStatus
                            text: "当前语音: 系统默认"
                            font.italic: true
                            font.pixelSize: responsive.normalFont
                        }

                        GroupBox {
                            title: "🧪 测试语音"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                TextField {
                                    id: testText
                                    width: parent.width
                                    height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
                                    placeholderText: "输入要测试的文本..."
                                    text: "Hello, this is a pronunciation test."
                                    font.pixelSize: responsive.normalFont
                                }

                                Flow {
                                    width: parent.width
                                    spacing: responsive.baseSpacing

                                    Button {
                                        text: "🎯 测试播放"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.normalFont
                                        onClicked: lookup.speakText(testText.text)
                                    }
                                    Button {
                                        text: "⏹️ 停止"
                                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                                        font.pixelSize: responsive.normalFont
                                        onClicked: lookup.stopSpeaking()
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 📊学习统计 Tab
            Item {
                ScrollView {
                    anchors.fill: parent
                    contentHeight: learningColumn.implicitHeight

                    Column {
                        id: learningColumn
                        width: parent.width
                        anchors.margins: responsive.baseMargin
                        spacing: responsive.baseSpacing

                        // 今日学习统计
                        GroupBox {
                            title: "📈 今日学习"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            property var dailyStats: learningManager.getDailyStats()

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                Row {
                                    spacing: responsive.baseSpacing * 2
                                    width: parent.width

                                    Column {
                                        Label {
                                            text: parent.parent.parent.dailyStats.newWords || 0
                                            font.pixelSize: responsive.largeFont
                                            font.bold: true
                                            color: "#2196F3"
                                        }
                                        Label {
                                            text: "新单词"
                                            font.pixelSize: responsive.smallFont
                                            color: "gray"
                                        }
                                    }

                                    Column {
                                        Label {
                                            text: parent.parent.parent.dailyStats.lookups || 0
                                            font.pixelSize: responsive.largeFont
                                            font.bold: true
                                            color: "#4CAF50"
                                        }
                                        Label {
                                            text: "查询次数"
                                            font.pixelSize: responsive.smallFont
                                            color: "gray"
                                        }
                                    }

                                    Column {
                                        Label {
                                            text: parent.parent.parent.dailyStats.reviews || 0
                                            font.pixelSize: responsive.largeFont
                                            font.bold: true
                                            color: "#FF9800"
                                        }
                                        Label {
                                            text: "复习次数"
                                            font.pixelSize: responsive.smallFont
                                            color: "gray"
                                        }
                                    }
                                }

                                ProgressBar {
                                    width: parent.width
                                    from: 0
                                    to: parent.parent.dailyStats.target || 10
                                    value: parent.parent.dailyStats.newWords || 0
                                }

                                Label {
                                    text: "目标：" + (parent.parent.dailyStats.target || 10) + " 个新单词/天"
                                    font.pixelSize: responsive.smallFont
                                    color: parent.parent.dailyStats.targetMet ? "green" : "orange"
                                }
                            }
                        }

                        // 学习进度
                        GroupBox {
                            title: "🎯 学习进度"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            property var progressStats: learningManager.getProgressStats()

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                Row {
                                    spacing: responsive.baseSpacing
                                    Label {
                                        text: "总词汇量："
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Label {
                                        text: parent.parent.parent.progressStats.totalWords || 0
                                        font.pixelSize: responsive.normalFont
                                        font.bold: true
                                        color: "#2196F3"
                                    }
                                }

                                Row {
                                    spacing: responsive.baseSpacing
                                    Label {
                                        text: "已掌握："
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Label {
                                        text: parent.parent.parent.progressStats.masteredWords || 0
                                        font.pixelSize: responsive.normalFont
                                        font.bold: true
                                        color: "#4CAF50"
                                    }
                                    Label {
                                        text: "(" + Math.round(parent.parent.parent.progressStats.masteryRate || 0) + "%)"
                                        font.pixelSize: responsive.normalFont
                                        color: "gray"
                                    }
                                }

                                Row {
                                    spacing: responsive.baseSpacing
                                    Label {
                                        text: "待加强："
                                        font.pixelSize: responsive.normalFont
                                    }
                                    Label {
                                        text: parent.parent.parent.progressStats.weakWords || 0
                                        font.pixelSize: responsive.normalFont
                                        font.bold: true
                                        color: "#f44336"
                                    }
                                }
                            }
                        }

                        // 复习提醒
                        GroupBox {
                            title: "⏰ 待复习单词"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            property var dueReviews: learningManager.getDueReviews()

                            Column {
                                width: parent.width
                                spacing: responsive.baseSpacing

                                Label {
                                    text: "需要复习：" + (parent.parent.dueReviews.length || 0) + " 个单词"
                                    font.pixelSize: responsive.normalFont
                                    color: parent.parent.dueReviews.length > 0 ? "#FF9800" : "gray"
                                }

                                ListView {
                                    width: parent.width
                                    height: Math.min(150, (parent.parent.dueReviews.length || 0) * 40)
                                    clip: true
                                    model: parent.parent.dueReviews

                                    delegate: ItemDelegate {
                                        width: parent.width
                                        height: 40

                                        Row {
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: responsive.baseSpacing

                                            Label {
                                                text: modelData.word || ""
                                                font.pixelSize: responsive.normalFont
                                                font.bold: true
                                            }

                                            Label {
                                                text: "优先级: " + (modelData.priority || 1)
                                                font.pixelSize: responsive.smallFont
                                                color: "gray"
                                            }
                                        }

                                        onClicked: {
                                            tabs.currentIndex = 0 // 切换到搜索页面
                                            input.text = modelData.word || ""
                                            searchBtn.clicked()
                                        }
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        visible: parent.count === 0
                                        text: "暂无需要复习的单词 🎉"
                                        color: "gray"
                                        font.pixelSize: responsive.smallFont
                                    }
                                }
                            }
                        }

                        // 薄弱单词
                        GroupBox {
                            title: "⚠️ 薄弱单词"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            property var weakWords: learningManager.getWeakWords(5)

                            ListView {
                                width: parent.width
                                height: Math.min(150, (parent.parent.weakWords.length || 0) * 40)
                                clip: true
                                model: parent.parent.weakWords

                                delegate: ItemDelegate {
                                    width: parent.width
                                    height: 40

                                    Row {
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: responsive.baseSpacing

                                        Label {
                                            text: modelData.word || ""
                                            font.pixelSize: responsive.normalFont
                                            font.bold: true
                                        }

                                        Label {
                                            text: "弱项指数: " + Math.round((modelData.weakness || 0) * 100) + "%"
                                            font.pixelSize: responsive.smallFont
                                            color: "#f44336"
                                        }
                                    }

                                    onClicked: {
                                        tabs.currentIndex = 0
                                        input.text = modelData.word || ""
                                        searchBtn.clicked()
                                    }
                                }

                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.count === 0
                                    text: "没有明显的薄弱单词 💪"
                                    color: "gray"
                                    font.pixelSize: responsive.smallFont
                                }
                            }
                        }

                        // 激励信息
                        GroupBox {
                            title: "💪 学习激励"
                            width: parent.width
                            font.pixelSize: responsive.normalFont

                            Label {
                                width: parent.width
                                text: learningManager.getMotivationalMessage()
                                font.pixelSize: responsive.normalFont
                                wrapMode: Text.WordWrap
                                horizontalAlignment: Text.AlignHCenter
                                color: "#2196F3"
                            }
                        }
                    }
                }
            }
        }
    }
}
