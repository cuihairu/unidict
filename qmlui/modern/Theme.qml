import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15

// 现代化设计主题配置
Item {
    id: theme

    // ========== 现代化配色方案 ==========

    // 主色调 - 基于现代蓝紫色调
    readonly property color primary: "#6366F1"           // 现代蓝紫色
    readonly property color primaryLight: "#818CF8"     // 亮主色
    readonly property color primaryDark: "#4F46E5"      // 暗主色

    // 次要色调
    readonly property color secondary: "#8B5CF6"        // 紫色
    readonly property color accent: "#EC4899"           // 粉红色强调色

    // 中性色系
    readonly property color background: "#FAFBFC"       // 主背景 - 更柔和的白色
    readonly property color surface: "#FFFFFF"          // 卡片表面
    readonly property color cardBackground: "#FFFFFF"   // 卡片背景
    readonly property color overlay: "rgba(0, 0, 0, 0.04)" // 覆盖层

    // 文本色系
    readonly property color textPrimary: "#1F2937"      // 主要文本
    readonly property color textSecondary: "#6B7280"    // 次要文本
    readonly property color textTertiary: "#9CA3AF"     // 三级文本
    readonly property color textDisabled: "#D1D5DB"     // 禁用文本

    // 边框和分割线
    readonly property color border: "#E5E7EB"           // 标准边框
    readonly property color borderLight: "#F3F4F6"      // 轻边框
    readonly property color divider: "#E5E7EB"           // 分割线

    // 状态色系
    readonly property color success: "#10B981"          // 成功绿
    readonly property color warning: "#F59E0B"          // 警告橙
    readonly property color error: "#EF4444"             // 错误红
    readonly property color info: "#3B82F6"              // 信息蓝

    // ========== 阴影系统 ==========

    readonly property var shadow1: [
        Qt.rect(0, 1, 3, 0, "rgba(0, 0, 0, 0.1)"),
        Qt.rect(0, 1, 2, 0, "rgba(0, 0, 0, 0.06)")
    ]

    readonly property var shadow2: [
        Qt.rect(0, 4, 6, -1, "rgba(0, 0, 0, 0.1)"),
        Qt.rect(0, 2, 4, -1, "rgba(0, 0, 0, 0.06)")
    ]

    readonly property var shadow3: [
        Qt.rect(0, 10, 15, -3, "rgba(0, 0, 0, 0.1)"),
        Qt.rect(0, 4, 6, -2, "rgba(0, 0, 0, 0.05)")
    ]

    readonly property var shadow4: [
        Qt.rect(0, 20, 25, -5, "rgba(0, 0, 0, 0.1)"),
        Qt.rect(0, 10, 10, -5, "rgba(0, 0, 0, 0.04)")
    ]

    // ========== 圆角系统 ==========

    readonly property int radiusSmall: 4
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int radiusXLarge: 16
    readonly property int radiusXXLarge: 24

    // ========== 间距系统 ==========

    readonly property int spacingXS: 4
    readonly property int spacingSM: 8
    readonly property int spacingMD: 16
    readonly property int spacingLG: 24
    readonly property int spacingXL: 32
    readonly property int spacingXXL: 48

    // ========== 字体系统 ==========

    readonly property font fontWeightLight: Qt.font({ weight: Font.Light })
    readonly property font fontWeightRegular: Qt.font({ weight: Font.Normal })
    readonly property font fontWeightMedium: Qt.font({ weight: Font.Medium })
    readonly property font fontWeightSemibold: Qt.font({ weight: Font.DemiBold })
    readonly property font fontWeightBold: Qt.font({ weight: Font.Bold })

    // ========== 动画配置 ==========

    readonly property int animationFast: 150
    readonly property int animationNormal: 250
    readonly property int animationSlow: 350

    // ========== Material Design 3 主题 ==========

    property MaterialTheme materialTheme: MaterialTheme {
        primary: theme.primary
        accent: theme.accent
        background: theme.background
        surface: theme.surface

        // Material 3 风格的色调映射
        primaryDark: theme.primaryDark
        primaryLight: theme.primaryLight
    }
}