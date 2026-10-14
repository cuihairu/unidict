// F13b 焦点窗选区读取（quick_lookup 热键的 macOS 文本源）+ TCC 授权交付面。
// 与 F13a（Windows UIA）同型：热键按下时优先读焦点窗口当前选区（AX API），
// 空则由 QML 回落剪贴板路径（原语义不变）。读取走辅助功能接口，macOS 的
// TCC 授权无程序化授予路径（须用户在系统设置里手动开）——本类除读取外，
// 另给出「是否已授权 / 触发系统提示 / 系统设置深链」三件事，供 QML 侧按
// 方案 d（首次取词弹系统提示 + 设置页内联引导直到授权）交付。
// 非 macOS 平台：读取恒空串、恒未授权、触发提示为 no-op（调用方零分支，
// 不假装工作）。
#ifndef MAC_SELECTION_READER_H
#define MAC_SELECTION_READER_H

#include <QString>

class MacSelectionReader {
public:
    // 焦点窗口当前选区文本；未授权/不支持/失败/无选区 → 空串
    static QString readFocusedSelection();
    // 是否已获辅助功能授权（非 macOS 恒 false，不假装）
    static bool isAccessibilityTrusted();
    // 触发 macOS 系统授权提示（AXIsProcessTrustedWithOptions + prompt，
    // 系统框异步、本调用立即返回）；非 macOS no-op
    static void requestAccessibilityPrompt();
    // 「辅助功能」系统设置深链（各平台同值常量，QML 侧据此开系统设置）
    static QString accessibilitySettingsUrl();
};

#endif // MAC_SELECTION_READER_H
