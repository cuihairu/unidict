import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"

// 移动端主壳·Vocab 页（LAY-002 自 Main.qml 拆出）
// Vocab Tab
//
// 窗口状态与根函数经 win 引用读写；responsive / suggestionModel
// 是 Main.qml 文件级 id，组件文件看不见父文件 id，故经本页同名字段
// 注入（实例化处同 id 名赋值——文档 id 优先于本组件同名字段，
// 与 SidebarPanel / Settings*Page 的 *Service 别名同律）；
// Theme / vocab / lookup 等 context 属性随引擎传播，无需注入。
Item {
    objectName: "mobileVocabPage"

    property var win: null
    property var responsive: null
    property var suggestionModel: null

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
                onClicked: win.vocabModel = vocab.vocabulary()
            }
            Button {
                text: "Sort A→Z"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: {
                    var all = vocab.vocabulary()
                    all.sort(function(a,b){ return a.word.toLowerCase() < b.word.toLowerCase() ? -1 : (a.word.toLowerCase() > b.word.toLowerCase() ? 1 : 0) })
                    win.vocabModel = all
                }
            }
            Button {
                text: "Sort Z→A"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: {
                    var all = vocab.vocabulary()
                    all.sort(function(a,b){ return a.word.toLowerCase() > b.word.toLowerCase() ? -1 : (a.word.toLowerCase() < b.word.toLowerCase() ? 1 : 0) })
                    win.vocabModel = all
                }
            }
            Button {
                text: "Sort by Time"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: {
                    var all = vocab.vocabularyMeta()
                    // Descending by added_at; missing treated as 0
                    all.sort(function(a,b){ var ta = a.added_at || 0; var tb = b.added_at || 0; return tb - ta })
                    // Map back to simple modelData used by list
                    var simple = []
                    for (var i = 0; i < all.length; ++i) { simple.push({word: all[i].word, definition: all[i].definition}) }
                    win.vocabModel = simple
                }
            }
            Button {
                text: "Clear"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: { vocab.clearVocabulary(); win.vocabModel = [] }
            }
            Button {
                text: "Export CSV"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: saveDialog.open()
            }
            Button {
                text: "Choose Sync File"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: syncChooseDialog.open()
            }
            Button {
                objectName: "syncPreviewButton"
                text: "Preview Sync"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: {
                    if (sync.syncFile().length === 0) { win.showToast("Please choose a sync file first"); return }
                    var m = sync.previewDiff()
                    if (!m.ok) {
                        win.lastError = ({ context: "sync_preview", path: sync.syncFile(), error: (m.error||"") })
                        win.showToast("Preview failed. See Error Details.", 2200)
                        return
                    }
                    var msg = "Preview: "
                    msg += "localOnly=" + (m.localOnly ? m.localOnly.length : 0)
                    msg += ", remoteOnly=" + (m.remoteOnly ? m.remoteOnly.length : 0)
                    msg += ", remoteNewer=" + (m.remoteNewer ? m.remoteNewer.length : 0)
                    msg += ", localNewer=" + (m.localNewer ? m.localNewer.length : 0)
                    syncStatus.text = msg
                    win.showToast(msg)
                    // Populate details dialog
                    previewDetails.fill(m)
                    win.lastError = {}
                    previewDialog.open()
                }
            }
            Button {
                text: "Sync Now"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: {
                    if (sync.syncFile().length === 0) { syncStatus.text = "Please choose a sync file first"; return }
                    var ok = sync.syncNow()
                    syncStatus.text = ok ? ("Synced: " + sync.syncFile()) : "Sync failed"
                    if (!ok) {
                        win.lastError = ({ context: "sync_now", path: sync.syncFile(), error: (sync.lastError ? sync.lastError() : "unknown error") })
                        win.showToast("Sync failed. See Error Details.", 2200)
                    } else {
                        win.lastError = {}
                        win.showToast("Synced with " + sync.syncFile())
                    }
                    // Refresh local view
                    win.vocabModel = vocab.vocabulary()
                    win.historyModel = vocab.searchHistory(200)
                }
            }
        }

        MobileFileDialog {
            id: syncChooseDialog
            title: "Choose Sync File"
            selectExisting: false
            nameFilters: ["JSON files (*.json)", "All files (*)"]
            onAccepted: {
                var p = syncChooseDialog.fileUrl.toString().replace("file://","")
                sync.setSyncFile(p)
                syncStatus.text = "Sync file: " + p
            }
        }

        // 显示同步状态
        Label {
            id: syncStatus
            width: parent.width
            text: ""
            font.pixelSize: responsive.smallFont
            wrapMode: Text.Wrap
            color: Theme.textTertiary
        }
        // 预览详情对话框
        Dialog {
            id: previewDialog
            objectName: "previewDialog"
            modal: true
            title: "Sync Preview Details"
            standardButtons: Dialog.Ok
            // 尺寸钉在 Dialog 本体：contentItem 显式绑 win 尺寸会
            // 与 Popup 定位器构成 implicitWidth 死环，open() 即冻结
            width: Math.min(win.width*0.9, 600)
            height: Math.min(win.height*0.7, 400)
            contentItem: ScrollView {
                id: previewScroll
                clip: true
                // 只竖滚（MainDesktop dictScroll 同口径）：contentWidth
                // 钉视口宽，断 implicitWidth→contentWidth 反馈环
                // （不钉则 open() 即布局自旋死锁）
                contentWidth: availableWidth
                Column {
                    id: previewDetails
                    objectName: "previewDetails"
                    width: previewScroll.availableWidth
                    spacing: 8
                    padding: 8
                    property var payload: ({})
                    function fill(m) {
                        previewDetails.payload = m
                        localOnlyLbl.text = "Local Only (" + (m.localOnly?m.localOnly.length:0) + "): " + (m.localOnly? m.localOnly.join(", "):"")
                        remoteOnlyLbl.text = "Remote Only (" + (m.remoteOnly?m.remoteOnly.length:0) + "): " + (m.remoteOnly? m.remoteOnly.join(", "):"")
                        var rn = m.remoteNewer || []
                        var ln = m.localNewer || []
                        var rnStr = []
                        for (var i=0;i<rn.length;++i) rnStr.push(rn[i].word + " (local:" + rn[i].local_ts + ", remote:" + rn[i].remote_ts + ")")
                        var lnStr = []
                        for (var j=0;j<ln.length;++j) lnStr.push(ln[j].word + " (local:" + ln[j].local_ts + ", remote:" + ln[j].remote_ts + ")")
                        remoteNewerLbl.text = "Remote Newer (" + rn.length + "): " + rnStr.join(", ")
                        localNewerLbl.text = "Local Newer (" + ln.length + "): " + lnStr.join(", ")
                    }
                    CheckBox {
                        id: cbRemoteOnly; text: "Pull remote-only into local";
                        checked: settings.getBool("sync/preview/includeRemoteOnly", true)
                        onToggled: {
                            settings.setBool("sync/preview/includeRemoteOnly", checked)
                            win.lastPreviewIncludeRemoteOnly = checked
                        }
                    }
                    CheckBox {
                        id: cbLocalOnly;  text: "Push local-only to remote";
                        checked: settings.getBool("sync/preview/includeLocalOnly", true)
                        onToggled: {
                            settings.setBool("sync/preview/includeLocalOnly", checked)
                            win.lastPreviewIncludeLocalOnly = checked
                        }
                    }
                    CheckBox {
                        id: cbRemoteNewer; text: "Take remote newer into local";
                        checked: settings.getBool("sync/preview/takeRemoteNewer", true)
                        onToggled: {
                            settings.setBool("sync/preview/takeRemoteNewer", checked)
                            win.lastPreviewTakeRemoteNewer = checked
                        }
                    }
                    CheckBox {
                        id: cbLocalNewer;  text: "Take local newer into remote";
                        checked: settings.getBool("sync/preview/takeLocalNewer", true)
                        onToggled: {
                            settings.setBool("sync/preview/takeLocalNewer", checked)
                            win.lastPreviewTakeLocalNewer = checked
                        }
                    }
                    Label { id: localOnlyLbl; wrapMode: Text.Wrap; width: parent.width }
                    Label { id: remoteOnlyLbl; wrapMode: Text.Wrap; width: parent.width }
                    Label { id: remoteNewerLbl; wrapMode: Text.Wrap; width: parent.width }
                    Label { id: localNewerLbl; wrapMode: Text.Wrap; width: parent.width }
                    // Selection lists
                    GroupBox {
                        title: "Select Words"
                        width: parent.width
                        Column {
                            width: parent.width
                            spacing: 4
                            // Remote Only list
                            Label { text: "Remote Only (select to pull into local)"; font.pixelSize: responsive.smallFont }
                            ListView {
                                id: lvRemoteOnly
                                width: parent.width; height: 100; clip: true
                                model: previewDetails.payload.remoteOnly || []
                                delegate: Row {
                                    spacing: 6
                                    CheckBox { id: cb; checked: true }
                                    Label { text: modelData; elide: Text.ElideRight; width: parent.width - 60 }
                                }
                                // model 是 JS 数组（QStringList/QVariantList），
                                // ListModel 专用的 model.get() 不适用（运行时报
                                // "Property 'get' of ... is not a function"）；
                                // 改读 delegate 已渲染的 Label 文本（string 列表
                                // =modelData，object 列表=modelData.word，与原
                                // push 值等价）
                                property var selected: function() {
                                    var out = []
                                    for (var i=0;i<count;++i) {
                                        var v = itemAtIndex(i)
                                        if (v && v.children[0].checked) out.push(v.children[1].text)
                                    }
                                    return out
                                }
                            }
                            // Local Only list
                            Label { text: "Local Only (select to push to remote)"; font.pixelSize: responsive.smallFont }
                            ListView {
                                id: lvLocalOnly; width: parent.width; height: 100; clip: true
                                model: previewDetails.payload.localOnly || []
                                delegate: Row {
                                    spacing: 6
                                    CheckBox { id: cb2; checked: true }
                                    Label { text: modelData; elide: Text.ElideRight; width: parent.width - 60 }
                                }
                                // model 是 JS 数组（QStringList/QVariantList），
                                // ListModel 专用的 model.get() 不适用（运行时报
                                // "Property 'get' of ... is not a function"）；
                                // 改读 delegate 已渲染的 Label 文本（string 列表
                                // =modelData，object 列表=modelData.word，与原
                                // push 值等价）
                                property var selected: function() {
                                    var out = []
                                    for (var i=0;i<count;++i) {
                                        var v = itemAtIndex(i)
                                        if (v && v.children[0].checked) out.push(v.children[1].text)
                                    }
                                    return out
                                }
                            }
                            // Remote Newer list
                            Label { text: "Remote Newer (select to override local with remote)"; font.pixelSize: responsive.smallFont }
                            ListView {
                                id: lvRemoteNewer; width: parent.width; height: 100; clip: true
                                model: previewDetails.payload.remoteNewer || []
                                delegate: Row {
                                    spacing: 6
                                    CheckBox { id: cb3; checked: true }
                                    Label { text: modelData.word; elide: Text.ElideRight; width: parent.width - 60 }
                                }
                                // model 是 JS 数组（QStringList/QVariantList），
                                // ListModel 专用的 model.get() 不适用（运行时报
                                // "Property 'get' of ... is not a function"）；
                                // 改读 delegate 已渲染的 Label 文本（string 列表
                                // =modelData，object 列表=modelData.word，与原
                                // push 值等价）
                                property var selected: function() {
                                    var out = []
                                    for (var i=0;i<count;++i) {
                                        var v = itemAtIndex(i)
                                        if (v && v.children[0].checked) out.push(v.children[1].text)
                                    }
                                    return out
                                }
                            }
                            // Local Newer list
                            Label { text: "Local Newer (select to override remote with local)"; font.pixelSize: responsive.smallFont }
                            ListView {
                                id: lvLocalNewer; width: parent.width; height: 100; clip: true
                                model: previewDetails.payload.localNewer || []
                                delegate: Row {
                                    spacing: 6
                                    CheckBox { id: cb4; checked: true }
                                    Label { text: modelData.word; elide: Text.ElideRight; width: parent.width - 60 }
                                }
                                // model 是 JS 数组（QStringList/QVariantList），
                                // ListModel 专用的 model.get() 不适用（运行时报
                                // "Property 'get' of ... is not a function"）；
                                // 改读 delegate 已渲染的 Label 文本（string 列表
                                // =modelData，object 列表=modelData.word，与原
                                // push 值等价）
                                property var selected: function() {
                                    var out = []
                                    for (var i=0;i<count;++i) {
                                        var v = itemAtIndex(i)
                                        if (v && v.children[0].checked) out.push(v.children[1].text)
                                    }
                                    return out
                                }
                            }
                        }
                    }
                    Button {
                        objectName: "previewApplyButton"
                        text: "Apply"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.smallFont
                        onClicked: {
                            var sel = {
                                remoteOnly: (lvRemoteOnly.selected ? lvRemoteOnly.selected() : []),
                                localOnly: (lvLocalOnly.selected ? lvLocalOnly.selected() : []),
                                remoteNewer: (lvRemoteNewer.selected ? lvRemoteNewer.selected() : []),
                                localNewer: (lvLocalNewer.selected ? lvLocalNewer.selected() : [])
                            }
                            // Show confirmation summary
                            confirmApplyDialog.selection = sel
                            var cnt = (sel.remoteOnly?sel.remoteOnly.length:0) + (sel.localOnly?sel.localOnly.length:0) + (sel.remoteNewer?sel.remoteNewer.length:0) + (sel.localNewer?sel.localNewer.length:0)
                            confirmApplyDialog.summaryText = "Apply selected changes?\n" +
                                "+ pull remote-only: " + (sel.remoteOnly?sel.remoteOnly.length:0) + "\n" +
                                "+ push local-only: " + (sel.localOnly?sel.localOnly.length:0) + "\n" +
                                "* update local from remote: " + (sel.remoteNewer?sel.remoteNewer.length:0) + "\n" +
                                "* update remote from local: " + (sel.localNewer?sel.localNewer.length:0) + "\n" +
                                "Total: " + cnt
                            confirmApplyDialog.open()
                        }
                    }
                    Button {
                        text: "Show Last Sync Changes"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.smallFont
                        onClicked: {
                            var r = sync.lastChanges()
                            if (!r.ok) { win.showToast("No changes or error: " + (r.error||"")); return }
                            var c = r.changes || {}
                            var parts = []
                            if (c.pulled_remote_only) parts.push("pulled_remote_only: " + c.pulled_remote_only.length)
                            if (c.pushed_local_only) parts.push("pushed_local_only: " + c.pushed_local_only.length)
                            if (c.updated_local_from_remote) parts.push("updated_local_from_remote: " + c.updated_local_from_remote.length)
                            if (c.updated_remote_from_local) parts.push("updated_remote_from_local: " + c.updated_remote_from_local.length)
                            var txt = "Last changes: " + parts.join(", ")
                            win.showToast(txt)
                        }
                    }
                    Button {
                        text: "Copy Preview"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        font.pixelSize: responsive.smallFont
                        onClicked: {
                            var txt = localOnlyLbl.text + "\n" + remoteOnlyLbl.text + "\n" + remoteNewerLbl.text + "\n" + localNewerLbl.text
                            clip.setText(txt)
                            win.showToast("Copied preview to clipboard")
                        }
                    }
                    Row {
                        spacing: responsive.baseSpacing/2
                        Button {
                            text: "Export Selection..."
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: previewExportSelDialog.open()
                        }
                        Button {
                            text: "Import Selection..."
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: previewImportSelDialog.open()
                        }
                    }
                }
            }
        }
        // Confirm apply dialog
        Dialog {
            id: confirmApplyDialog
            objectName: "previewConfirmDialog"
            modal: true
            title: "Confirm Apply"
            standardButtons: Dialog.Ok | Dialog.Cancel
            width: Math.min(win.width*0.8, 520)
            property var selection: ({})
            property string summaryText: ""
            onAccepted: {
                var ok = sync.applySelection(selection)
                if (!ok) {
                    win.lastError = ({ context: "sync_apply", path: sync.syncFile(), error: (sync.lastError ? sync.lastError() : "unknown error"), selection: selection })
                } else {
                    win.lastError = {}
                }
                win.showToast(ok ? "Applied selected changes" : "Apply failed. See Error Details.")
                if (ok) {
                    previewDialog.close()
                    // Refresh local view models
                    win.vocabModel = vocab.vocabulary()
                    win.historyModel = vocab.searchHistory(200)
                }
            }
            contentItem: Column {
                width: parent.width
                spacing: responsive.baseSpacing
                padding: responsive.baseSpacing
                Label { text: confirmApplyDialog.summaryText; wrapMode: Text.Wrap; width: parent.width }
            }
        }
        // Sync 错误详情弹窗
        Dialog {
            id: syncErrorDialog
            modal: true
            title: "Sync Error Details"
            standardButtons: Dialog.Ok
            width: Math.min(win.width*0.9, 600)
            height: Math.min(win.height*0.6, 420)
            contentItem: ScrollView {
                id: syncErrScroll
                clip: true
                contentWidth: availableWidth
                Column {
                    width: syncErrScroll.availableWidth
                    spacing: 6
                    padding: 8
                    Label { text: "Context: " + (win.lastError.context || ""); wrapMode: Text.Wrap }
                    Label { text: "Sync File: " + (sync.syncFile() || ""); wrapMode: Text.Wrap }
                    TextArea {
                        text: win.lastError.error || ""
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        width: Math.min(win.width*0.9, 560)
                        height: 160
                    }
                    Label {
                        text: "Hints: Ensure the JSON file is valid and writable, choose a proper sync file, or export selection to a new file. You can also retry preview below."
                        wrapMode: Text.Wrap
                        width: Math.min(win.width*0.9, 560)
                        color: Theme.textTertiary
                    }
                    Row {
                        spacing: responsive.baseSpacing/2
                        Button {
                            text: "Copy"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: {
                                var txt = "Context: " + (win.lastError.context||"") + "\n" +
                                          "Sync File: " + (sync.syncFile()||"") + "\n" +
                                          (win.lastError.error||"")
                                clip.setText(txt)
                                win.showToast("Copied error details")
                            }
                        }
                        Button {
                            text: "Choose Sync File..."
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: syncChooseDialog.open()
                        }
                        Button {
                            text: "Open Containing Folder"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: {
                                var p = sync.syncFile() || ""
                                if (p.length > 0) {
                                    var pp = p.replace(/\\\\/g,'/'); var idx = pp.lastIndexOf('/'); var dir = (idx>0) ? pp.substring(0, idx) : pp
                                    Qt.openUrlExternally("file://" + dir)
                                }
                            }
                        }
                        Button {
                            text: "Retry Preview"
                            height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                            font.pixelSize: responsive.smallFont
                            onClicked: {
                                if (sync.syncFile().length === 0) { win.showToast("Please choose a sync file first"); return }
                                var m = sync.previewDiff()
                                if (!m.ok) {
                                    win.lastError = ({ context: "sync_preview", path: sync.syncFile(), error: (m.error||"") })
                                    win.showToast("Preview failed", 1500)
                                } else {
                                    previewDetails.fill(m)
                                    win.lastError = {}
                                    syncErrorDialog.close()
                                    previewDialog.open()
                                }
                            }
                        }
                    }
                }
            }
        }
        // Export/Import selection dialogs
        MobileFileDialog {
            id: previewExportSelDialog
            title: "Export Selection"
            selectExisting: false
            nameFilters: ["JSON (*.json)","All (*)"]
            onAccepted: {
                var path = previewExportSelDialog.fileUrl.toString().replace("file://","")
                var sel = {
                    remoteOnly: (lvRemoteOnly.selected ? lvRemoteOnly.selected() : []),
                    localOnly: (lvLocalOnly.selected ? lvLocalOnly.selected() : []),
                    remoteNewer: (lvRemoteNewer.selected ? lvRemoteNewer.selected() : []),
                    localNewer: (lvLocalNewer.selected ? lvLocalNewer.selected() : [])
                }
                var ok = sync.exportSelection(sel, path)
                win.showToast(ok ? "Exported selection" : "Export selection failed")
            }
        }
        MobileFileDialog {
            id: previewImportSelDialog
            title: "Import Selection"
            selectExisting: true
            nameFilters: ["JSON (*.json)","All (*)"]
            onAccepted: {
                var path = previewImportSelDialog.fileUrl.toString().replace("file://","")
                var r = sync.importSelection(path)
                if (!r.ok) { win.showToast("Import failed: " + (r.error||"")); return }
                var sel = r.selection || {}
                // Apply to views (set checkboxes)
                var toLowerSet = function(lst) { var s={}; for (var i=0;i<lst.length;++i) s[lst[i].toLowerCase()] = true; return s }
                var ro = toLowerSet(sel.remoteOnly || [])
                var lo = toLowerSet(sel.localOnly || [])
                var rn = toLowerSet(sel.remoteNewer || [])
                var ln = toLowerSet(sel.localNewer || [])
                // For each list, set checked based on presence
                // （同 selected()：JS 数组 model 无 model.get()，读 delegate Label）
                if (lvRemoteOnly && lvRemoteOnly.count !== undefined) {
                    for (var i=0;i<lvRemoteOnly.count;++i) { var item = lvRemoteOnly.itemAtIndex(i); if (item) item.children[0].checked = !!ro[(item.children[1].text||"").toLowerCase()] }
                }
                if (lvLocalOnly && lvLocalOnly.count !== undefined) {
                    for (var j=0;j<lvLocalOnly.count;++j) { var item2 = lvLocalOnly.itemAtIndex(j); if (item2) item2.children[0].checked = !!lo[(item2.children[1].text||'').toLowerCase()] }
                }
                if (lvRemoteNewer && lvRemoteNewer.count !== undefined) {
                    for (var k=0;k<lvRemoteNewer.count;++k) { var item3 = lvRemoteNewer.itemAtIndex(k); if (item3) item3.children[0].checked = !!rn[(item3.children[1].text||'').toLowerCase()] }
                }
                if (lvLocalNewer && lvLocalNewer.count !== undefined) {
                    for (var k2=0;k2<lvLocalNewer.count;++k2) { var item4 = lvLocalNewer.itemAtIndex(k2); if (item4) item4.children[0].checked = !!ln[(item4.children[1].text||'').toLowerCase()] }
                }
                win.showToast("Imported selection (not yet applied)")
            }
        }

        // 删除确认弹窗（移动端友好）
        Dialog {
            id: vocabRemoveDialog
            modal: true
            title: "Remove Word"
            standardButtons: Dialog.Ok | Dialog.Cancel
            property string pendingWord: ""
            onAccepted: {
                if (pendingWord.length > 0) {
                    vocab.removeVocabularyWord(pendingWord)
                    win.vocabModel = vocab.vocabulary()
                    pendingWord = ""
                }
            }
            onRejected: pendingWord = ""
            contentItem: Column {
                spacing: responsive.baseSpacing
                padding: responsive.baseSpacing
                Label {
                    text: "Remove from vocabulary: " + vocabRemoveDialog.pendingWord + " ?"
                    wrapMode: Text.WordWrap
                    font.pixelSize: responsive.normalFont
                }
            }
        }

        TextField {
            id: vocabFilter
            placeholderText: "Filter vocabulary..."
            width: parent.width
            height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onTextChanged: {
                var all = vocab.vocabulary()
                if (vocabFilter.text.length === 0) { win.vocabModel = all; return }
                var filtered = []
                for (var i = 0; i < all.length; ++i) {
                    var item = all[i]
                    if (item.word.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1 ||
                        item.definition.toLowerCase().indexOf(vocabFilter.text.toLowerCase()) !== -1) {
                        filtered.push(item)
                    }
                }
                // 笔记内容也参与匹配（Search within notes）：core
                // 大小写不敏感子串命中词去重并入
                var noteHits = vocab.searchNotes(vocabFilter.text)
                for (var n = 0; n < noteHits.length; ++n) {
                    var nw = noteHits[n].word
                    var dup = false
                    for (var f = 0; f < filtered.length; ++f) {
                        if (filtered[f].word === nw) { dup = true; break }
                    }
                    if (!dup) {
                        for (var a = 0; a < all.length; ++a) {
                            if (all[a].word === nw) { filtered.push(all[a]); break }
                        }
                    }
                }
                win.vocabModel = filtered
            }
        }

        ListView {
            id: vocabList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: vocabFilter.bottom
            anchors.topMargin: responsive.baseSpacing
            model: win.vocabModel
            delegate: ItemDelegate {
                width: parent.width
                height: Math.max(60, responsive.minTouchTarget + 16)

                Row {
                    anchors.fill: parent
                    anchors.margins: responsive.baseSpacing
                    spacing: responsive.baseSpacing

                    Label {
                        id: vocabLabel
                        width: parent.width - removeBtn.width - 3*responsive.baseSpacing
                        text: modelData.word + ": " + modelData.definition
                        font.pixelSize: responsive.normalFont
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    Button {
                        id: removeBtn
                        text: "Remove"
                        height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                        onClicked: {
                            vocabRemoveDialog.pendingWord = modelData.word
                            vocabRemoveDialog.open()
                        }
                    }
                }

                onClicked: {
                    win.navigateTo(0)
                    input.text = modelData.word
                }
                onPressAndHold: {
                    // Show confirm dialog on long-press (mobile friendly)
                    vocabRemoveDialog.pendingWord = modelData.word
                    vocabRemoveDialog.open()
                }
            }

            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: "No vocabulary saved"
                color: Theme.textTertiary
                font.pixelSize: responsive.smallFont
            }
        }
    }

    MobileFileDialog {
        id: saveDialog
        title: "Export Vocabulary CSV"
        selectExisting: false
        nameFilters: ["CSV files (*.csv)", "All files (*)"]
        onAccepted: {
            var p = saveDialog.fileUrl.toString().replace("file://", "")
            var ok = vocab.exportVocabCsv(p)
            win.showToast(ok ? ("Exported CSV: " + p) : "Export failed")
        }
    }
}
