// Unidict 设计语言 token 表（亮/暗两套，语义角色命名）
//
// 品牌推导：主色取自仓库 logo（docs/logo.svg，全库唯一色 #b11964 洋红）；
// 中性底与文字系带同色相的玫瑰调（不走无色相灰阶），亮/暗成对。
// 全部正文级配对按 WCAG AA 实测（门槛 4.5:1），实测值见
// tests/theme_tokens_contrast_test.cpp（同一组数值机器回归）：
//   light text/window 15.87:1 · secondary/card 6.03:1 · tertiary/card 4.80:1
//         accent/window 6.18:1 · accentText/accent 6.58:1 · danger/card 5.66:1
//   dark  text/window 15.39:1 · secondary/card 7.21:1 · tertiary/card 4.63:1
//         accent/window 7.00:1 · accentText/accent 6.98:1 · danger/card 7.24:1
// 凹陷面（surfaceSunken，搜索输入条等内嵌输入底）配对：
//   light text/sunken 15.16:1 · secondary/sunken 5.41:1 · accent/sunken 5.91:1
//   dark  text/sunken 14.74:1 · secondary/sunken 7.57:1 · accent/sunken 6.71:1
// （tertiary 在 sunken 上为 4.3:1 掉出门槛——凹陷面文字只用 text/secondary
// 两档，placeholder 归 secondary；textDisabled 按 WCAG 豁免不判定。）
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
    QString window;        // 窗体底
    QString card;          // 卡片/表面
    QString surfaceSunken; // 凹陷面（搜索输入条/内嵌输入底），比 card 低一档
    QString text;          // 主文字
    QString textSecondary; // 次要文字
    QString textTertiary;  // 三级文字（占位/说明）
    QString textDisabled;  // 禁用文字（WCAG 豁免档）
    QString accent;        // 品牌强调色（logo #b11964 衍生）
    QString accentHover;
    QString accentPressed;
    QString accentText;    // accent 底上的文字（暗主题 accent 变亮后文字转深）
    QString divider;       // 分隔线/边框
    QString hoverOverlay;  // 行/项悬停覆盖层（低对比中性，实体色——rgba()
                           // 浮点 alpha 串在 QML color 渲染路径会被解析成
                           // 不透明纯色：亮色出纯黑/暗色出纯白）
    QString danger;        // 危险操作
    QString link;          // 链接/选中态文字（查词面板：蓝色链接与高亮）
    QString success;       // 成功（移动端学习统计等）
    QString warning;
    QString info;
};

inline ThemeTokens tokensFor(bool dark) {
    ThemeTokens t;
    if (dark) {
        t.window        = "#1a1417";
        t.card          = "#251c21";
        t.surfaceSunken = "#1f181d";
        t.text          = "#f2eaef";
        t.textSecondary = "#b6a7af";
        t.textTertiary  = "#93838c";
        t.textDisabled  = "#67585f";
        t.accent        = "#ff6fa9";
        t.accentHover   = "#ff8bbc";
        t.accentPressed = "#e75490";
        t.accentText    = "#2b0a1a";
        t.divider       = "#3b2f35";
        t.hoverOverlay  = "#332830";
        t.danger        = "#ff8a7a";
        t.link          = "#82b1ff";
        t.success       = "#7ddc8a";
        t.warning       = "#ffb85c";
        t.info          = "#7fb5ff";
    } else {
        t.window        = "#faf7f9";
        t.card          = "#ffffff";
        t.surfaceSunken = "#f7f1f5";
        t.text          = "#241a20";
        t.textSecondary = "#6d5f67";
        t.textTertiary  = "#7e6e76";
        t.textDisabled  = "#b7aab1";
        t.accent        = "#b11964";
        t.accentHover   = "#971456";
        t.accentPressed = "#7c0f47";
        t.accentText    = "#ffffff";
        t.divider       = "#eadfe5";
        t.hoverOverlay  = "#f3ecef";
        t.danger        = "#c42b1c";
        t.link          = "#1b6ac9";
        t.success       = "#2e7d32";
        t.warning       = "#9a5b00";
        t.info          = "#2563eb";
    }
    return t;
}

} // namespace UnidictQml
