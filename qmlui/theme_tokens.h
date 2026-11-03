// Unidict 设计语言 token 表（亮/暗两套，语义角色命名）
//
// 品牌推导：主色取自仓库 logo（docs/logo.svg，全库唯一色 #b11964 洋红），
// 暗色交互态提亮为 #ff6fa9（Linear 式暗色精密世界，原型
// /tmp/unidict-ui/prototype.html 同值）。中性底与文字为**近无色相深灰阶**
// （仅极微暖），让品牌玫瑰成为唯一彩色——曾是「玫瑰调灰阶 + 蓝色 link」
// 双强调分裂，界面重设计批（2026-11-03）收敛为单色玫瑰：
//   link = accent（词条链接/内容 tab 选中/高亮标注全部走品牌玫瑰）
//   window/card/sunken/divider/hover 全部去色相（近无色灰阶）
// 语义色（success/warning/danger/info）与笔记荧光四色不属强调色系，保留。
// 全部正文级配对按 WCAG AA 实测（门槛 4.5:1），机器回归见
// tests/theme_tokens_contrast_test.cpp（同一组数值）：
//   light text/window 15.79:1 · secondary/card 7.04:1 · tertiary/card 5.27:1
//         accent/window 6.11:1 · accentText/accent 6.58:1 · danger/card 5.66:1
//   dark  text/window 16.3:1 · secondary/card 7.4:1 · tertiary/card 5.1:1
//         accent/window 7.3:1 · accentText/accent 7.0:1 · danger/card 7.2:1
// 凹陷面（surfaceSunken，搜索输入条等内嵌输入底）配对：
//   light text/sunken 15.0:1 · secondary/sunken 6.2:1 · accent/sunken 5.8:1
//   dark  text/sunken 15.4:1 · secondary/sunken 7.9:1 · accent/sunken 7.2:1
// （tertiary 在 sunken 上为 4.3:1 掉出门槛——凹陷面文字只用 text/secondary
// 两档，placeholder 归 secondary；textDisabled 按 WCAG 豁免不判定。）
// 笔记荧光笔四色（UI 批二，文字档固定用 text）配对：
//   light text/noteYellow 14.05:1 · noteGreen 13.72:1 · noteBlue 13.55:1 · notePink 13.14:1
//   dark  text/noteYellow  8.90:1 · noteGreen  8.60:1 · noteBlue  9.73:1 · notePink 10.17:1
//
// 取值唯一来源：QML（Theme 上下文对象）与任何 HTML 插值都从这里出，
// 禁止在 QML/页面代码里散落硬编码颜色。

#pragma once

#include <QString>

namespace UnidictQml {

// 形状单一规则（按 Qt 尺度定档），与色值同源出此文件：
//   小件（标签/徽标/浮层）4 · 控件级（按钮/输入）8 · 卡片/面板 12
inline constexpr int kRadiusS = 4;
inline constexpr int kRadiusM = 8;
inline constexpr int kRadiusL = 12;

struct ThemeTokens {
    QString window;        // 窗体底（主内容画布）
    QString panel;         // 面板面（顶栏/侧栏/状态栏——比 window 高一档，
                           // 原型 --panel；卡片之外的结构面）
    QString card;          // 卡片/表面
    QString surfaceSunken; // 凹陷面（搜索输入条/内嵌输入底），比 card 低一档
    QString text;          // 主文字
    QString textSecondary; // 次要文字
    QString textTertiary;  // 三级文字（占位/说明）
    QString textDisabled;  // 禁用文字（WCAG 豁免档）
    QString accent;        // 品牌强调色（logo #b11964 衍生，暗色提亮 #ff6fa9）
    QString accentHover;
    QString accentPressed;
    QString accentText;    // accent 底上的文字（暗主题 accent 变亮后文字转深）
    QString divider;       // 分隔线/边框（近无色相）
    QString hoverOverlay;  // 行/项悬停覆盖层（低对比中性，实体色——rgba()
                           // 浮点 alpha 串在 QML color 渲染路径会被解析成
                           // 不透明纯色：亮色出纯黑/暗色出纯白）
    QString danger;        // 危险操作
    QString link;          // 链接/选中态文字（词条蓝链、内容 tab 选中——
                           // 重设计批起 = accent，消灭蓝/玫瑰双强调分裂）
    QString success;       // 成功（词典状态点、发音源可用等）
    QString warning;
    QString info;
    // 笔记荧光笔四色（UI 批二）：文字档固定用 text，配对实测见文件头。
    // 位次→色值映射在 Theme::noteHighlight（QML 唯一入口）；core/std 的
    // NoteItemStd.color 只存位次不藏主题色表
    QString noteYellow;
    QString noteGreen;
    QString noteBlue;
    QString notePink;
};

inline ThemeTokens tokensFor(bool dark) {
    ThemeTokens t;
    if (dark) {
        // 近无色相深灰阶（Linear 式，原型 :root 暗色同值）
        t.window        = "#101013";
        t.panel         = "#151518";
        t.card          = "#1a1a1e";
        t.surfaceSunken = "#131316";
        t.text          = "#f4f4f5";
        t.textSecondary = "#a8a8b0";
        t.textTertiary  = "#8b8b95";
        t.textDisabled  = "#6b6b73";
        t.accent        = "#ff6fa9";
        t.accentHover   = "#ff8bbc";
        t.accentPressed = "#e75490";
        t.accentText    = "#2b0a1a";
        t.divider       = "#26262b";
        t.hoverOverlay  = "#212126";
        t.danger        = "#ff8a7a";
        t.link          = "#ff6fa9";
        t.success       = "#7ddc8a";
        t.warning       = "#ffb85c";
        t.info          = "#7fb5ff";
        t.noteYellow    = "#4d3d1a";
        t.noteGreen     = "#1f4a28";
        t.noteBlue      = "#1f3a5f";
        t.notePink      = "#5a2340";
    } else {
        // 近无色相亮灰阶（原型 light 同值）
        t.window        = "#f7f7f8";
        t.panel         = "#fbfbfc";
        t.card          = "#ffffff";
        t.surfaceSunken = "#f0f0f2";
        t.text          = "#1c1a1e";
        t.textSecondary = "#5c5760";
        t.textTertiary  = "#6f6a73";
        t.textDisabled  = "#b5b5bc";
        t.accent        = "#b11964";
        t.accentHover   = "#971456";
        t.accentPressed = "#7c0f47";
        t.accentText    = "#ffffff";
        t.divider       = "#e3e3e6";
        t.hoverOverlay  = "#f2f2f4";
        t.danger        = "#c42b1c";
        t.link          = "#b11964";
        t.success       = "#2e7d32";
        t.warning       = "#9a5b00";
        t.info          = "#2563eb";
        t.noteYellow    = "#ffe9a8";
        t.noteGreen     = "#d3efd3";
        t.noteBlue      = "#d6e8ff";
        t.notePink      = "#ffd9e8";
    }
    return t;
}

} // namespace UnidictQml
