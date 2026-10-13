// F13a 焦点窗选区读取（quick_lookup 热键的 Windows 文本源）。
// 热键按下时优先读焦点窗口当前选区（UIA TextPattern），空则由 QML 回落
// 剪贴板路径（原语义不变）。这是**触发式读取**——与 SelectionMonitor 的
// 持续监控是两条线：Win/mac 无独立 selection，监控回落剪贴板会双触发
// （已否，见 CURRENT_FEATURE_MATRIX 划词行），监控在这两个平台保持
// isSupported=false。读取走 UI Automation 无障碍接口，无选区 / 焦点元素
// 不支持 TextPattern / COM 失败一律返回空串，诚实不假装。非 Windows
// 平台编译为恒空实现（调用方零分支）。
#ifndef WIN_SELECTION_READER_H
#define WIN_SELECTION_READER_H

#include <QString>

class WinSelectionReader {
public:
    // 焦点窗口当前选区文本；不支持/失败/无选区 → 空串
    static QString readFocusedSelection();
};

#endif // WIN_SELECTION_READER_H
