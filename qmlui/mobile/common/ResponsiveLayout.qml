import QtQuick 2.15
import QtQuick.Controls 2.15

// 响应式布局组件 - 统一移动端适配逻辑
Item {
    id: root

    // 设备类型枚举
    enum DeviceType {
        Desktop,
        Tablet,
        Phone
    }

    // 屏幕尺寸断点
    readonly property int phoneBreakpoint: 600
    readonly property int tabletBreakpoint: 900

    // 当前设备类型
    readonly property int deviceType: {
        if (width < phoneBreakpoint) return ResponsiveLayout.DeviceType.Phone
        if (width < tabletBreakpoint) return ResponsiveLayout.DeviceType.Tablet
        return ResponsiveLayout.DeviceType.Desktop
    }

    // 响应式尺寸
    readonly property int baseMargin: deviceType === ResponsiveLayout.DeviceType.Phone ? 8 : 12
    readonly property int baseSpacing: deviceType === ResponsiveLayout.DeviceType.Phone ? 6 : 8
    readonly property int buttonHeight: deviceType === ResponsiveLayout.DeviceType.Phone ? 48 : 36
    readonly property int inputHeight: deviceType === ResponsiveLayout.DeviceType.Phone ? 44 : 32
    readonly property int tabHeight: deviceType === ResponsiveLayout.DeviceType.Phone ? 56 : 48

    // 触控优化
    readonly property int minTouchTarget: 44  // iOS和Android推荐最小触控目标
    readonly property bool isMobile: deviceType !== ResponsiveLayout.DeviceType.Desktop

    // 字体大小
    readonly property int smallFont: deviceType === ResponsiveLayout.DeviceType.Phone ? 12 : 10
    readonly property int normalFont: deviceType === ResponsiveLayout.DeviceType.Phone ? 16 : 14
    readonly property int largeFont: deviceType === ResponsiveLayout.DeviceType.Phone ? 20 : 18

    // 布局方向（竖屏/横屏）
    readonly property bool isPortrait: height > width
    readonly property bool isLandscape: !isPortrait

    // 安全区域（为状态栏和底部区域预留空间）
    readonly property int safeAreaTop: isMobile ? 24 : 0
    readonly property int safeAreaBottom: isMobile ? 24 : 0
    readonly property int safeAreaLeft: 0
    readonly property int safeAreaRight: 0

    // 设备类型变化处理 - 移除显式信号处理避免冲突
    Component.onCompleted: {
        console.log("ResponsiveLayout initialized. Device type:",
                   deviceType === ResponsiveLayout.DeviceType.Phone ? "Phone" :
                   deviceType === ResponsiveLayout.DeviceType.Tablet ? "Tablet" : "Desktop")
    }
}