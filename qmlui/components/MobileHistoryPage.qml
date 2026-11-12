import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15
import "../mobile/common"

// 移动端主壳·History 页（LAY-002 自 Main.qml 拆出）
// History Tab
//
// 窗口状态与根函数经 win 引用读写；responsive / suggestionModel
// 是 Main.qml 文件级 id，组件文件看不见父文件 id，故经本页同名字段
// 注入（实例化处同 id 名赋值——文档 id 优先于本组件同名字段，
// 与 SidebarPanel / Settings*Page 的 *Service 别名同律）；
// Theme / vocab / lookup 等 context 属性随引擎传播，无需注入。
Item {
    objectName: "mobileHistoryPage"

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
                onClicked: win.historyModel = vocab.searchHistory(200)
            }
            Button {
                text: "Clear"
                height: Math.max(responsive.buttonHeight, responsive.minTouchTarget)
                font.pixelSize: responsive.normalFont
                onClicked: { vocab.clearHistory(); win.historyModel = [] }
            }
        }

        TextField {
            id: historyFilter
            placeholderText: "Filter history..."
            width: parent.width
            height: Math.max(responsive.inputHeight, responsive.minTouchTarget)
            font.pixelSize: responsive.normalFont
            onTextChanged: {
                var all = vocab.searchHistory(200)
                if (historyFilter.text.length === 0) { win.historyModel = all; return }
                var filtered = []
                for (var i = 0; i < all.length; ++i) {
                    var w = all[i]
                    if (w.toLowerCase().indexOf(historyFilter.text.toLowerCase()) !== -1) filtered.push(w)
                }
                win.historyModel = filtered
            }
        }

        ListView {
            id: historyList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: historyFilter.bottom
            anchors.topMargin: responsive.baseSpacing
            model: win.historyModel
            delegate: ItemDelegate {
                width: parent.width
                height: Math.max(36, responsive.minTouchTarget)
                text: modelData
                font.pixelSize: responsive.normalFont
                onClicked: {
                    win.navigateTo(0)
                    input.text = modelData
                }
            }

            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: "No search history"
                color: Theme.textTertiary
                font.pixelSize: responsive.smallFont
            }
        }
    }
}
