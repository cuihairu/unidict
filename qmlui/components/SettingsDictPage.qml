import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import QtQuick.Dialogs

// 设置抽屉·词典页（LAY-001 自 MainDesktop.qml 拆出）
// 词典（P-6 词典库正式面：目录导入 + 装载列表 + 隔离重试）
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
Item {
    objectName: "settingsdictPage"

    property var win: null
    property var lookup: null

    FolderDialog {
        id: importFolderDialog
        objectName: "importFolderDialog"
        onAccepted: {
            importDirField.text = selectedFolder.toString().replace("file://", "")
        }
    }
    ScrollView {
        // LAY-001：StackLayout 单元改经 Item 转交，铺满 wrapper（原由 StackLayout 直接撑满）
        anchors.fill: parent
        id: dictScroll
        clip: true
        // 只竖滚（Qt 官方口径）：contentWidth 钉视口宽。
        // 不钉的话 contentItem 宽随内容 implicitWidth 撑开
        // → 首行「浏览/导入」被推出抽屉外 clip 掉点不到，
        // 且 implicitWidth→contentWidth 反馈环使最终宽度
        // 依赖初始时序（sandbox 布局正常/audit 锁死溢出）
        contentWidth: availableWidth
        ColumnLayout {
            width: dictScroll.availableWidth
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: importDirField
                    objectName: "importDirField"
                    Layout.fillWidth: true
                    placeholderText: Theme.zh ? "词典目录路径" : "Dictionary folder path"
                    selectByMouse: true
                }
                Button {
                    objectName: "importBrowseButton"
                    text: Theme.zh ? "浏览" : "Browse"
                    onClicked: importFolderDialog.open()
                }
                Button {
                    id: importRunButton
                    objectName: "importRunButton"
                    text: lookup.isDirectoryImportRunning()
                        ? (Theme.zh ? "取消" : "Cancel")
                        : (Theme.zh ? "导入" : "Import")
                    onClicked: {
                        if (lookup.isDirectoryImportRunning()) {
                            lookup.cancelDirectoryImport()
                        } else if (importDirField.text.trim().length > 0) {
                            lookup.startDirectoryImport(importDirField.text.trim())
                        }
                    }
                }
            }
            ProgressBar {
                objectName: "importProgressBar"
                Layout.fillWidth: true
                visible: lookup.isDirectoryImportRunning()
                from: 0
                to: win.importProgressTotal
                value: win.importProgressDone + win.importProgressFailed
            }
            Label {
                objectName: "importStatusLabel"
                visible: win.importSummaryText.length > 0
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: Theme.textSecondary
                text: win.importSummaryText
            }

            Label {
                Layout.fillWidth: true
                text: lookup.loadedDictionaries().length > 0
                    ? (Theme.zh
                        ? ("已加载: " + lookup.loadedDictionaries().length + " 本")
                        : ("Loaded: " + lookup.loadedDictionaries().length))
                    : (Theme.zh
                        ? "未加载词典：请从上方目录导入"
                        : "No dictionaries loaded — import a folder above")
                wrapMode: Text.WordWrap
            }

            // 全文索引状态（UI 批二 元信息面板）：全文索引
            // 按需惰建（首次全文检索才建，core 口径），docs=0
            // = 尚未触发过，不是缺索引的错态。「重建」按钮
            // （F12）丢内存索引回到惰建态，下次全文检索重建
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    id: dictIndexStatusLabel
                    objectName: "dictIndexStatusLabel"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    color: Theme.textSecondary
                    text: (win.fulltextInfo.docs || 0) > 0
                        ? (Theme.zh
                            ? ("全文索引: " + win.fulltextInfo.docs + " 篇 · "
                               + win.fulltextInfo.terms + " 词项")
                            : ("Full-text index: " + win.fulltextInfo.docs
                               + " docs · " + win.fulltextInfo.terms + " terms"))
                        : (Theme.zh
                            ? "全文索引: 按需建（首次全文检索时）"
                            : "Full-text index: built on first search")
                }
                Button {
                    objectName: "dictIndexRebuildButton"
                    text: Theme.zh ? "重建" : "Rebuild"
                    enabled: lookup.loadedDictionaries().length > 0
                    onClicked: {
                        if (lookup.resetFulltextIndex()) {
                            win.statusText = Theme.zh
                                ? "已重置全文索引（下次全文检索时按需重建）"
                                : "Full-text index reset (rebuilt on next search)"
                        } else {
                            win.statusText = Theme.zh
                                ? "未加载词典，无可建索引"
                                : "No dictionaries loaded — nothing to index"
                        }
                    }
                }
            }

            Repeater {
                model: win.dictMetas
                delegate: Frame {
                    Layout.fillWidth: true
                    padding: 10
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            // 优先级标记（巡检续批 F2）：
                            // priority 降序可视位次，P 标签随移动归一
                            Label {
                                objectName: "dictPriorityLabel_" + index
                                text: "P" + (modelData.priority || 0)
                                color: Theme.textTertiary
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }

                            Label {
                                Layout.fillWidth: true
                                text: (modelData.name || "unknown") + " (" + (modelData.wordCount || 0) + ")"
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }

                            // 优先级 ↑↓（巡检续批 F2）：上移/下移整列
                            // 归一，边界项对应方向隐藏
                            ToolButton {
                                objectName: "dictPriorityUp_" + index
                                visible: index > 0
                                onClicked: lookup.moveDictionary(
                                    modelData.name || "", -1)
                                contentItem: Icon {
                                    name: "arrowUp"
                                    size: 14
                                    color: Theme.textSecondary
                                }
                            }
                            ToolButton {
                                objectName: "dictPriorityDown_" + index
                                visible: index < (win.dictMetas.length - 1)
                                onClicked: lookup.moveDictionary(
                                    modelData.name || "", 1)
                                contentItem: Icon {
                                    name: "arrowDown"
                                    size: 14
                                    color: Theme.textSecondary
                                }
                            }

                            // 启用勾选（UI 批二）：只影响查询
                            // 路径，装载清单照列（core 口径）
                            CheckBox {
                                objectName: "dictEnabledCheck_" + index
                                text: Theme.zh ? "启用" : "On"
                                font.pixelSize: 11
                                checked: modelData.enabled !== false
                                onToggled: lookup.setDictionaryEnabled(
                                    modelData.name || "", checked)
                            }

                            // 会话面移除（巡检续批 F8）：core
                            // remove_dictionary 摘词典+重建索引；
                            // qmlui 词典清单 env 驱动无状态文件，
                            // 重启随 UNIDICT_DICTS 回归
                            ToolButton {
                                objectName: "dictRemoveButton_" + index
                                opacity: hovered ? 1.0 : 0.4
                                ToolTip.visible: hovered
                                ToolTip.text: Theme.zh
                                    ? "移除该词典（重启后随来源回归）"
                                    : "Remove (returns after restart)"
                                onClicked: win.removeDictAndReport(
                                    modelData.name)
                                contentItem: Icon {
                                    name: "close"
                                    size: 14
                                    color: Theme.danger
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            // 格式展示名（StarDict/MDict/JSON…）
                            Label {
                                visible: (modelData.format || "").length > 0
                                text: modelData.format || ""
                                color: Theme.textTertiary
                                font.pixelSize: 11
                            }

                            Item { Layout.fillWidth: true }

                            // 打开目录（UI 批二）：开词典文件
                            // 所在目录（真实效果在外部文件管理器，
                            // 离屏审计止步接线态）
                            ToolButton {
                                objectName: "dictOpenFolderButton_" + index
                                visible: (modelData.filePath || "").length > 0
                                text: Theme.zh ? "打开目录" : "Show in folder"
                                font.pixelSize: 11
                                onClicked: {
                                    if (!win.platformService.openContainingFolder(modelData.filePath))
                                        win.statusText = Theme.zh
                                            ? ("打开目录失败 → " + modelData.filePath)
                                            : ("Couldn't open folder → " + modelData.filePath)
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: modelData.description || ""
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // 失败/隔离项（解析失败持久隔离档；文件丢失为
            // 运行期诊断不入档——std 口径）
            Label {
                visible: win.dictFailures.length > 0
                text: Theme.zh ? "导入失败（隔离档）"
                               : "Import failed (quarantined)"
                font.weight: Font.DemiBold
                color: Theme.warning
            }
            Repeater {
                model: win.dictFailures
                delegate: Frame {
                    Layout.fillWidth: true
                    padding: 10
                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: (modelData.filePath || "").split("/").pop()
                            elide: Text.ElideRight
                        }
                        Label {
                            Layout.fillWidth: true
                            text: modelData.reason || ""
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                            font.pixelSize: 11
                        }
                        Button {
                            objectName: "retryFailedButton"
                            text: Theme.zh ? "重试" : "Retry"
                            onClicked: {
                                var ok = lookup.retryFailedDictionary(modelData.filePath || "")
                                if (!ok) win.importSummaryText = Theme.zh
                                    ? "重试仍失败（已确认隔离）"
                                    : "Retry failed again (still quarantined)"
                            }
                        }
                    }
                }
            }
        }
    }
}
