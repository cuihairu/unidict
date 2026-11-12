import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·外观页（LAY-001 自 MainDesktop.qml 拆出）
// 外观（F4 亮暗三态）：0=跟随系统（colorScheme 实时
// 跟随），1/2=手动锁定不再跟随；记忆到 ui/theme（gui
// 面同键同语义）。P1 text-norm 键归一化区并入后内容
// 超过抽屉固定高（535），走 ScrollView 收纳（语音页
// 同款）——不收纳时下半段在抽屉界外：渲染被裁，点击
// 落在 popup 外触发 CloseOnPressOutside（S38 实录）
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ScrollView {
    objectName: "settingsthemePage"

    property var win: null


    clip: true

    ColumnLayout {
        width: parent.width
        spacing: 8

        // 界面语言（中英双语）：同一布局、同一 token，
        // 仅文案语言不同——两套语言都按原型落地
        Label {
            text: Theme.zh ? "界面语言" : "Interface language"
            font.weight: Font.DemiBold
        }

        RowLayout {
            spacing: 12
            RadioButton {
                objectName: "langRadio0"
                text: "中文"
                checked: Theme.zh
                onToggled: win.setLang(true)
            }
            RadioButton {
                objectName: "langRadio1"
                text: "English"
                checked: !Theme.zh
                onToggled: win.setLang(false)
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh
                ? "切换界面文案语言；词条释义内容始终随词典原文。"
                : "Switches the interface copy language; dictionary "
                  + "content always stays in the source language."
            font.pixelSize: 12
            color: Theme.textTertiary
        }

        Label {
            text: Theme.zh ? "亮暗模式" : "Appearance"
            font.weight: Font.DemiBold
        }

        RadioButton {
            objectName: "themeModeRadio0"
            text: Theme.zh ? "跟随系统" : "Follow system"
            checked: Theme.mode === 0
            onToggled: win.setThemeMode(0)
        }
        RadioButton {
            objectName: "themeModeRadio1"
            text: Theme.zh ? "浅色" : "Light"
            checked: Theme.mode === 1
            onToggled: win.setThemeMode(1)
        }
        RadioButton {
            objectName: "themeModeRadio2"
            text: Theme.zh ? "深色" : "Dark"
            checked: Theme.mode === 2
            onToggled: win.setThemeMode(2)
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh
                ? "跟随系统=随系统亮暗实时切换；手动指定后不再"
                  + "跟随，重启后保持。"
                : "Follow system switches with your OS; a manual "
                  + "choice sticks across restarts."
            font.pixelSize: 12
            color: Theme.textTertiary
        }

        // 释义字号（F7）：档位即选即用，落盘
        // ui/def_font_size；decorateHtml 注入 +
        // applyDefFontSize 重建当前查询面。同页挂在
        // 主题区之后（StackLayout 单页，勿拆成平级
        // 子级——tab 栏只有 6 个，拆出去永远不可见）
        Label {
            text: Theme.zh ? "释义字号" : "Definition size"
            font.weight: Font.DemiBold
        }

        RowLayout {
            spacing: 12

            RadioButton {
                objectName: "defFontRadio12"
                text: "12"
                checked: win.defFontSize === 12
                onToggled: win.applyDefFontSize(12)
            }
            RadioButton {
                objectName: "defFontRadio14"
                text: "14"
                checked: win.defFontSize === 14
                onToggled: win.applyDefFontSize(14)
            }
            RadioButton {
                objectName: "defFontRadio16"
                text: "16"
                checked: win.defFontSize === 16
                onToggled: win.applyDefFontSize(16)
            }
            RadioButton {
                objectName: "defFontRadio18"
                text: "18"
                checked: win.defFontSize === 18
                onToggled: win.applyDefFontSize(18)
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh ? "释义区正文字号，重启后保持。"
                           : "Body size for definitions; kept after restart."
            font.pixelSize: 12
            color: Theme.textTertiary
        }

        // 查询键归一化（P1 text-norm）：查询词与词头怎么算
        // "同一个词"。宽容=大小写/重音/标点变体互通（默认）；
        // 谨慎=保留重音与组合记号（é 与 e 不再互撞，重音语言
        // 词形保真）；精确=原样字节逐位比对。换档即时生效
        // （core 重灌索引 + 作废解析器折叠缓存），落盘
        // textnorm/profile，启动装载前已按此档建好。
        // 同页挂在字号区之后（StackLayout 单页，勿拆平级
        // 子级——拆出去永远不可见，S38 实录）；radio 收进
        // RowLayout 独立互斥组——autoExclusive 按父分
        // 组，散在 ColumnLayout 会与亮暗/字号 radio 同组，
        // 点亮暗把本组 checked 连带打翻（S38 实录）
        Label {
            text: Theme.zh ? "查询键归一化" : "Query normalization"
            font.weight: Font.DemiBold
        }

        RowLayout {
            spacing: 12

            RadioButton {
                objectName: "textNormTolerant"
                text: Theme.zh ? "宽容" : "Tolerant"
                checked: win.normProfileValue === 0
                onToggled: win.applyNormProfile(0)
            }
            RadioButton {
                objectName: "textNormConservative"
                text: Theme.zh ? "谨慎" : "Careful"
                checked: win.normProfileValue === 1
                onToggled: win.applyNormProfile(1)
            }
            RadioButton {
                objectName: "textNormExact"
                text: Theme.zh ? "精确" : "Exact"
                checked: win.normProfileValue === 2
                onToggled: win.applyNormProfile(2)
            }
        }

        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh
                ? "宽容=大小写/重音/标点变体互通（默认）；" +
                  "谨慎=保留重音（é 与 e 不互撞）；" +
                  "精确=原样字节比对。即时生效，重启后保持。"
                : "Tolerant: case/accent/punctuation variants match " +
                  "(default). Careful: accents preserved (é ≠ e). " +
                  "Exact: byte-for-byte. Applies immediately."
            font.pixelSize: 12
            color: Theme.textTertiary
        }
    }
}
