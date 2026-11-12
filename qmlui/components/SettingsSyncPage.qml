import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·同步页（LAY-001 自 MainDesktop.qml 拆出）
// 同步备份（B7：同步默认关闭、显式开启明示范围；
// 口令加密备份自救口。传输绑定归 B5 剩余，先落开关与设置）
// 内容高于抽屉固定高，同语音 tab 走 ScrollView 收纳
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ScrollView {
    objectName: "settingssyncPage"

    property var win: null
    property var syncManager: null


    // 同步开关状态回读（LAY-001 拆页时随迁自 MainDesktop Connections）：
    // 开启走「确认开启」按钮（onToggled 曾把 checked 打回），
    // enabledChanged 时以管理器实际状态为准；同步轮次结束回读
    // 状态行（位点/待发/错误）
    Connections {
        target: syncManager
        function onEnabledChanged() { syncSwitch.checked = syncManager.syncEnabled }
        function onSyncStateChanged() {
            if (syncState.visible) syncState.text = syncManager.syncStatusText()
        }
    }

    clip: true
    ColumnLayout {
        width: parent.width
        spacing: 10

    Switch {
        id: syncSwitch
        objectName: "syncSwitch"
        text: Theme.zh ? "多设备同步" : "Multi-device sync"
        checked: syncManager.syncEnabled
        onToggled: {
            if (checked) {
                // 开启须经「确认开启」按钮——范围明示常驻上方
                checked = false
                syncState.text = Theme.zh
                    ? "先确认同步范围，再点「确认开启」"
                    : "Review the sync scope, then hit Confirm"
            } else {
                syncManager.disable()
                syncState.text = Theme.zh
                    ? "已关闭同步（默认不连接任何服务器）"
                    : "Sync off (no servers contacted by default)"
            }
        }
    }

    Label {
        id: syncScopeLabel
        objectName: "syncScopeLabel"
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: syncManager.scopeText()
        font.pixelSize: 12
        color: Theme.textSecondary
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: Theme.zh ? "组 ID" : "Group ID" }
        TextField {
            id: syncGidField
            objectName: "syncGidField"
            Layout.fillWidth: true
            text: syncManager.groupId
            placeholderText: Theme.zh ? "16-64 位字母/数字/_/-"
                                       : "16-64 letters/digits/_/-"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: Theme.zh ? "同步形态" : "Transport" }
        ComboBox {
            id: syncFormCombo
            Layout.fillWidth: true
            textRole: "label"
            model: Theme.zh ? [
                { label: "自建中转（自有服务器）", value: "selfhost" },
                { label: "官方托管（Worker 地址）", value: "hosted" },
                { label: "局域网直传（同网直连）", value: "lan" }
            ] : [
                { label: "Self-hosted relay", value: "selfhost" },
                { label: "Hosted (Worker URL)", value: "hosted" },
                { label: "LAN direct (same network)", value: "lan" }
            ]
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: syncFormCombo.currentIndex === 0

        Label { text: Theme.zh ? "中转地址" : "Relay URL" }
        TextField {
            id: syncRelayField
            objectName: "syncRelayField"
            Layout.fillWidth: true
            text: syncManager.relayUrl
            placeholderText: Theme.zh ? "如 http://192.168.1.10:8788"
                                       : "e.g. http://192.168.1.10:8788"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: syncFormCombo.currentIndex === 1

        Label { text: Theme.zh ? "托管地址" : "Hosted URL" }
        TextField {
            id: syncHostedField
            objectName: "syncHostedField"
            Layout.fillWidth: true
            text: syncManager.hostedUrl
            placeholderText: Theme.zh
                ? "https://unidict-sync.<你的子域>.workers.dev"
                : "https://unidict-sync.<your-sub>.workers.dev"
        }
    }

    // 局域网直传：本机宿主开关（默认关）+ 扫描 + 直连点选
    ColumnLayout {
        Layout.fillWidth: true
        spacing: 8
        visible: syncFormCombo.currentIndex === 2

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Switch {
                id: lanHostSwitch
                objectName: "lanHostSwitch"
                text: Theme.zh ? "本机作为同步宿主"
                               : "This device as sync host"
                checked: syncManager.lanHostRunning
                onToggled: {
                    if (checked) {
                        if (!syncManager.lanHostStart())
                            syncState.text = syncManager.lastError()
                    } else {
                        syncManager.lanHostStop()
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: syncManager.lanHostInfo()
                font.pixelSize: 12
                color: Theme.textSecondary
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                objectName: "lanScanButton"
                text: Theme.zh ? "扫描局域网" : "Scan LAN"
                onClicked: {
                    var peers = syncManager.lanScan()
                    lanPeersModel.clear()
                    for (var i = 0; i < peers.length; ++i)
                        lanPeersModel.append(peers[i])
                    if (peers.length === 0)
                        syncState.text = Theme.zh
                            ? "未发现同网宿主（对方需先开启「本机作为同步宿主」）"
                            : "No host found (peer must enable hosting first)"
                    else
                        syncState.text = Theme.zh
                            ? ("发现 " + peers.length + " 台宿主，点选以直连")
                            : ("Found " + peers.length + " host(s) — pick one")
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.textSecondary
                text: syncManager.lanPeerAddr
                      ? (Theme.zh
                            ? "已选直连：" + syncManager.lanPeerAddr + ":" + syncManager.lanPeerPort
                            : "Direct peer: " + syncManager.lanPeerAddr + ":" + syncManager.lanPeerPort)
                      : (Theme.zh ? "尚未选择直连设备" : "No peer selected")
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 96)
            visible: count > 0
            clip: true
            model: ListModel { id: lanPeersModel }
            delegate: Button {
                objectName: "lanPeerButton"
                width: ListView.view.width
                text: name + " · " + addr + ":" + port
                font.pixelSize: 12
                onClicked: syncManager.setLanPeer(addr, port)
            }
        }
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: (Theme.zh ? "本机设备标识：" : "Device ID: ") + syncManager.deviceId
        font.pixelSize: 12
        color: Theme.textTertiary
    }

    Button {
        objectName: "syncApplyButton"
        text: syncManager.syncEnabled
              ? (Theme.zh ? "重新保存设置" : "Save settings")
              : (Theme.zh ? "确认开启（我已了解同步范围）"
                          : "Confirm & enable")
        onClicked: {
            if (syncManager.enable(syncGidField.text.trim())) {
                syncManager.transportForm =
                    syncFormCombo.model[syncFormCombo.currentIndex].value
                if (syncFormCombo.currentIndex === 0)
                    syncManager.relayUrl = syncRelayField.text.trim()
                if (syncFormCombo.currentIndex === 1)
                    syncManager.hostedUrl = syncHostedField.text.trim()
                syncState.text = Theme.zh
                    ? "同步设置已保存，可点「立即同步」"
                    : "Sync settings saved — hit Sync now"
            } else {
                syncState.text = syncManager.lastError()
            }
        }
    }

    Button {
        objectName: "syncNowButton"
        text: Theme.zh ? "立即同步" : "Sync now"
        enabled: syncManager.syncEnabled
        onClicked: {
            if (syncManager.syncNow())
                syncState.text = syncManager.syncStatusText()
            else
                syncState.text = syncManager.lastError()
        }
    }

    Label {
        id: syncState
        objectName: "syncStateLabel"
        visible: text.length > 0
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        font.pixelSize: 12
        color: Theme.textSecondary
    }

    Label {
        text: Theme.zh ? "加密备份（防设备全丢）"
                       : "Encrypted backup"
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: Theme.text
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        text: Theme.zh
            ? "备份生词本（词/分组标签/笔记）：口令加密、不经过任何服务器。"
              + "恢复时缺的补上、已有的不动；口令遗失无法解密，请牢记。"
            : "Backs up your wordbook (words/tags/notes), encrypted "
              + "locally and never sent to a server. Restore fills in "
              + "what's missing; there is no password recovery."
        font.pixelSize: 12
        color: Theme.textTertiary
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: Theme.zh ? "口令" : "Password" }
        TextField {
            id: backupPassField
            objectName: "backupPassField"
            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: Theme.zh ? "备份口令" : "Backup password"
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Label { text: Theme.zh ? "文件" : "File" }
        TextField {
            id: backupPathField
            objectName: "backupPathField"
            Layout.fillWidth: true
            text: documentsPath + "/Unidict/unidict-backup.udbk"
        }
    }

    RowLayout {
        spacing: 8

        Button {
            objectName: "backupExportButton"
            text: Theme.zh ? "导出备份" : "Export"
            onClicked: {
                if (backupPassField.text.length === 0) {
                    syncState.text = Theme.zh ? "请先输入备份口令"
                                              : "Enter a password first"
                    return
                }
                syncState.text = syncManager.exportBackup(
                    backupPathField.text, backupPassField.text)
                    ? (Theme.zh
                        ? ("备份已导出：" + backupPathField.text)
                        : ("Backup exported: " + backupPathField.text))
                    : syncManager.lastError()
            }
        }

        Button {
            objectName: "backupRestoreButton"
            text: Theme.zh ? "恢复备份" : "Restore"
            onClicked: {
                if (backupPassField.text.length === 0) {
                    syncState.text = Theme.zh ? "请先输入备份口令"
                                              : "Enter a password first"
                    return
                }
                var r = syncManager.restoreBackup(
                    backupPathField.text, backupPassField.text)
                syncState.text = r.ok
                    ? (Theme.zh
                        ? ("已恢复 " + r.words + " 词 / " + r.notes
                           + " 笔记 / " + r.tags + " 标签")
                        : ("Restored " + r.words + " words / "
                           + r.notes + " notes / " + r.tags + " tags"))
                    : (r.error || syncManager.lastError())
            }
        }
    }
}
}
