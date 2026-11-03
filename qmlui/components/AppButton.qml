import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

// 原型按钮语言（prototype .btn / .btn.primary）：控件级 8px 圆角、32px 高、
// 13px 文案；primary = 玫瑰实底 + accentText，次级 = hairline 描边 ghost。
//
// 为什么不直接用 Material Button.highlighted：Material 样式的 buttonColor()
// 在本主题下不给强调色实底（且是胶囊圆角 + 投影，与 Linear 式扁平冲突），
// 这里自绘 background/contentItem，颜色全部走 Theme token。
Button {
    id: control

    property bool primary: false

    implicitHeight: 32
    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    horizontalPadding: 14
    verticalPadding: 0
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: control.primary ? Font.DemiBold : Font.Normal

    background: Rectangle {
        radius: Theme.radiusM
        color: control.primary
            ? (control.pressed ? Theme.accentPressed
               : control.hovered ? Theme.accentHover : Theme.accent)
            : (control.hovered ? Theme.hoverOverlay : "transparent")
        border.width: control.primary ? 0 : 1
        border.color: Theme.divider
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? Theme.accentText
             : control.enabled ? Theme.textSecondary : Theme.textDisabled
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
