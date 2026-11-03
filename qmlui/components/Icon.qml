import QtQuick 2.15
import QtQuick.Shapes 1.15

// SVG 描边图标系统（界面重设计批，替代 emoji/tofu 字形）。
//
// 规格与原型一致（/tmp/unidict-ui/prototype.html 内联 <svg>）：
//   24×24 viewBox · fill:none · stroke-width 1.7 · round cap/join。
// 路径表与设计资产 assets/icons/ui/*.svg 同一几何（同一份 path d 生成，
// 该目录供文档/打包侧取用）；运行时渲染走 QtQuick.Shapes 的 PathSvg，
// strokeColor 绑定 Theme 语义色，亮暗/悬停换色零额外资源。
//
// 用法：Icon { name: "volume"; size: 16; color: Theme.textTertiary }
//   name 查下面 paths 表；未登记的 name 渲染为空（不画占位框）。
Item {
    id: root

    property string name: ""
    // 显示边长（px）；路径按 24 网格缩放，描边随之等比（1.7@24 → 16px≈1.13）
    property real size: 16
    property color color: Theme.text
    property real strokeWidth: 1.7

    implicitWidth: size
    implicitHeight: size

    // 24 网格路径表（name → d）。与 assets/icons/ui/*.svg 同源。
    readonly property var paths: ({
        "search":    "M11 4a7 7 0 1 0 0 14 7 7 0 0 0 0-14z M20 20l-4.2-4.2",
        "book":      "M4 19.5A2.5 2.5 0 0 1 6.5 17H20 M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z",
        "bookOpen":  "M12 6.6C12 5.2 10.6 4.2 8 4.2H3v13.2h5c2.6 0 4 1 4 2.4 M12 6.6c0-1.4 1.4-2.4 4-2.4h5v13.2h-5c-2.6 0-4 1-4 2.4 M12 6.6v13.2",
        "volume":    "M11 5 6 9H2v6h4l5 4V5z M15.5 8.5a5 5 0 0 1 0 7 M18.5 5.5a9.5 9.5 0 0 1 0 13",
        "pencil":    "M12 20h9 M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z",
        "copy":      "M9 9h10a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H9a2 2 0 0 1-2-2v-8a2 2 0 0 1 2-2z M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1",
        "sparkles":  "M12 3l1.8 4.7L18.5 9.5l-4.7 1.8L12 16l-1.8-4.7L5.5 9.5l4.7-1.8L12 3z M18.5 15.5l.8 1.9 1.9.8-1.9.8-.8 1.9-.8-1.9-1.9-.8 1.9-.8.8-1.9z",
        "pin":       "M9 3h6l-1.2 5.6L15 12v2H9v-2l1.2-3.4L9 3z M12 14v7",
        "close":     "M18 6 6 18 M6 6l12 12",
        "chevronDown":  "m6 9 6 6 6-6",
        "chevronUp":    "m18 15-6-6-6 6",
        "arrowLeft": "M19 12H5 M12 19l-7-7 7-7",
        "arrowRight":"M5 12h14 M12 5l7 7-7 7",
        "arrowUp":   "M12 19V5 M5 12l7-7 7 7",
        "arrowDown": "M12 5v14 M19 12l-7 7-7-7",
        "folder":    "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V7z",
        "check":     "m5 13 4 4L19 7",
        "globe":     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18z M3 12h18 M12 3a14.5 14.5 0 0 1 0 18 M12 3a14.5 14.5 0 0 0 0 18",
        "clock":     "M12 21a9 9 0 1 0 0-18 9 9 0 0 0 0 18z M12 7.5V12l3 2",
        "settings":  "M19.4 15a1.7 1.7 0 0 0 .34 1.87l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.7 1.7 0 0 0-1.87-.34 1.7 1.7 0 0 0-1 1.55V21a2 2 0 1 1-4 0v-.09a1.7 1.7 0 0 0-1-1.55 1.7 1.7 0 0 0-1.87.34l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.7 1.7 0 0 0 .34-1.87 1.7 1.7 0 0 0-1.55-1H3a2 2 0 1 1 0-4h.09a1.7 1.7 0 0 0 1.55-1 1.7 1.7 0 0 0-.34-1.87l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.7 1.7 0 0 0 1.87.34h.01a1.7 1.7 0 0 0 1-1.55V3a2 2 0 1 1 4 0v.09a1.7 1.7 0 0 0 1 1.55 1.7 1.7 0 0 0 1.87-.34l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.7 1.7 0 0 0-.34 1.87v.01a1.7 1.7 0 0 0 1.55 1H21a2 2 0 1 1 0 4h-.09a1.7 1.7 0 0 0-1.55 1Z"
    })

    visible: name.length > 0 && paths.hasOwnProperty(name)

    Shape {
        width: 24
        height: 24
        scale: root.size / 24
        transformOrigin: Item.TopLeft

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            fillColor: "transparent"

            PathSvg {
                path: root.paths.hasOwnProperty(root.name) ? root.paths[root.name] : ""
            }
        }
    }
}
