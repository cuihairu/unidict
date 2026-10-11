import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

// 分组折叠卡（UI 批一）：D3 卡语言的公共壳——radiusL 圆角卡（card 底 +
// divider 描边），卡头 44px（旋转折叠箭头 + 标题 + 计数描边胶囊 chip），
// 悬停 ghost 面（inset3+radiusM），整头可点折叠。词典 tab 的分组卡与
// 例句/词组/近义联想 tab 的分区卡共用（卡内容由使用方以默认属性注入）。
// 折叠状态由使用方持有（EntryResultsPane 的 collapsed/contentCollapsed
// 映射，查词切换即复位），本组件只出 toggleRequested 信号。
Rectangle {
    id: card

    property string title: ""
    // 计数胶囊文案；空串不显示（词典 tab 沿用「>1 条才显示」口径由
    // 使用方决定传不传）
    property string badge: ""
    property bool collapsed: false
    // 卡头 objectName（audit 定位用，如 "groupHeader_0"）
    property string headerName: ""
    signal toggleRequested()

    default property alias body: bodyColumn.data
    implicitHeight: cardColumn.implicitHeight

    radius: Theme.radiusL
    color: Theme.card
    border.width: 1
    border.color: Theme.divider

    Column {
        id: cardColumn
        width: parent.width
        spacing: 0

        Rectangle {
            objectName: card.headerName
            width: parent.width
            height: 44
            color: "transparent"

            Rectangle {
                anchors.fill: parent
                anchors.margins: 3
                radius: Theme.radiusM
                color: Theme.hoverOverlay
                visible: headerMouse.containsMouse
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.divider
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Label {
                    text: "⌄"
                    color: Theme.textTertiary
                    font.pixelSize: 14
                    rotation: card.collapsed ? -90 : 0
                    Behavior on rotation {
                        NumberAnimation { duration: 120 }
                    }
                }

                Label {
                    text: card.title
                    font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    visible: card.badge.length > 0
                    radius: height / 2
                    color: "transparent"
                    border.width: 1
                    border.color: Theme.divider
                    implicitWidth: countChip.implicitWidth + 12
                    implicitHeight: countChip.implicitHeight + 4

                    Label {
                        id: countChip
                        anchors.centerIn: parent
                        text: card.badge
                        color: Theme.textSecondary
                        font.pixelSize: 11
                    }
                }
            }

            MouseArea {
                id: headerMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: card.toggleRequested()
            }
        }

        // 卡内容（使用方注入）；折叠时不参与布局，卡自然收短
        Column {
            id: bodyColumn
            width: parent.width
            visible: !card.collapsed
        }
    }
}
