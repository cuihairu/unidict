import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

// 设置抽屉·AI页（LAY-001 自 MainDesktop.qml 拆出）
// AI（L99 LLM 集成面）：provider auth 属用户凭据/计费，
// 不入仓不接真服务——本页暴露可做的面：结果缓存开关 +
// 提示词模板预览（P-9 外部命令桥/启发式兜底仍是活路径）。
// 抽屉高度 535 不足时走 ScrollView 收纳（外观页同款）。
//
// 窗口状态经 win 引用读写；C++ 服务经同名字段注入（实例化处经
// 根作用域 *Service 别名中转——裸名自引用陷阱，SidebarPanel 实例化
// 处注释同款）；Theme 单例走 context——components/ 现有组件同口径。
ScrollView {
    objectName: "settingsaiPage"

    property var win: null
    property var ai: null
    property var settings: null


    clip: true

    ColumnLayout {
        width: parent.width
        spacing: 10

        Label {
            text: Theme.zh ? "AI 工具" : "AI tools"
            font.weight: Font.DemiBold
        }

        Switch {
            objectName: "aiCacheSwitch"
            text: Theme.zh ? "缓存 AI 结果" : "Cache AI results"
            checked: settings.getBool("ai/cacheEnabled", true)
            onToggled: {
                ai.setCacheEnabled(checked)
                settings.setBool("ai/cacheEnabled", checked)
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh
                ? "相同输入的 AI 结果在会话内复用，避免重复调用"
                  + "外部命令；关闭即每次重新求值。"
                : "Identical inputs reuse the in-session result "
                  + "instead of re-running the external command."
            font.pixelSize: 12
            color: Theme.textTertiary
        }

        Label {
            text: Theme.zh ? "当前 provider 链" : "Provider chain"
            font.weight: Font.DemiBold
        }
        Label {
            objectName: "aiProviderNames"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: ai.providerNames().join(" → ")
            font.pixelSize: 12
            color: Theme.textSecondary
        }
        Label {
            text: Theme.zh ? "是否支持流式输出" : "Streaming"
            font.weight: Font.DemiBold
        }
        Label {
            objectName: "aiStreamingSupport"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: ai.streamingSupported()
                ? (Theme.zh ? "支持" : "Supported")
                : (Theme.zh ? "暂不支持（真 LLM/SSE provider 未接入）"
                            : "Not yet (no live LLM/SSE provider)")
            font.pixelSize: 12
            color: Theme.textSecondary
        }

        Label {
            text: Theme.zh ? "提示词模板（只读预览）"
                           : "Prompt templates (read-only)"
            font.weight: Font.DemiBold
        }
        Label {
            objectName: "aiPromptTranslate"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: (Theme.zh ? "翻译: " : "Translate: ") + ai.promptFor("translate")
            font.pixelSize: 12
            color: Theme.textTertiary
        }
        Label {
            objectName: "aiPromptGrammar"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: (Theme.zh ? "语法: " : "Grammar: ") + ai.promptFor("grammar")
            font.pixelSize: 12
            color: Theme.textTertiary
        }
        Label {
            objectName: "aiPromptSentences"
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: (Theme.zh ? "造句: " : "Sentences: ") + ai.promptFor("sentences")
            font.pixelSize: 12
            color: Theme.textTertiary
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: Theme.zh
                ? "提示词模板可经设置面调整；provider auth 需外部"
                  + "服务凭据与计费口径，属用户私有/财务面，未接入。"
                : "Templates are adjustable in settings; provider auth "
                  + "(credentials/billing) stays private and is not wired."
            font.pixelSize: 12
            color: Theme.textTertiary
        }
    }
}
