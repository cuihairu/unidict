import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Theme.qml"

// 现代化设置页面
Item {
    id: root

    // ========== 公共属性 ==========
    property alias lookupAdapter: internal.lookupAdapter

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
        property string currentCategory: "general" // general, appearance, voice, learning, sync, advanced
        property var settings: {
            "general": {
                "language": "zh-CN",
                "autoUpdate": true,
                "startupBehavior": "dashboard",
                "crashReports": false
            },
            "appearance": {
                "theme": "light",
                "fontSize": "medium",
                "fontFamily": "system",
                "animations": true,
                "compactMode": false
            },
            "voice": {
                "defaultVoice": "",
                "speechRate": 1.0,
                "speechPitch": 1.0,
                "autoPlay": true
            },
            "learning": {
                "dailyGoal": 20,
                "reminderTime": "20:00",
                "reminderEnabled": true,
                "reviewInterval": "smart",
                "difficulty": "adaptive"
            },
            "sync": {
                "autoSync": true,
                "syncInterval": 30,
                "backupEnabled": true,
                "cloudProvider": "local"
            },
            "advanced": {
                "debugMode": false,
                "logLevel": "info",
                "cacheSize": 100,
                "networkTimeout": 10000
            }
        }
    }

    // ========== 页面布局 ==========
    Row {
        anchors.fill: parent
        spacing: 0

        // 左侧导航
        Rectangle {
            width: 260
            height: parent.height
            color: theme.background

            Column {
                anchors.fill: parent
                anchors.topMargin: theme.spacingXL
                spacing: theme.spacingSM

                // 设置标题
                Rectangle {
                    width: parent.width
                    height: 60
                    color: "transparent"

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: theme.spacingMD
                        spacing: theme.spacingMD
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: "⚙️"
                            font.pixelSize: 18
                            color: theme.primary
                        }

                        Text {
                            text: "设置"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 18
                            font.weight: Font.SemiBold
                            color: theme.textPrimary
                        }
                    }
                }

                // 分类导航
                Repeater {
                    model: [
                        { value: "general", label: "通用设置", icon: "🔧" },
                        { value: "appearance", label: "外观主题", icon: "🎨" },
                        { value: "voice", label: "语音设置", icon: "🔊" },
                        { value: "learning", label: "学习设置", icon: "📚" },
                        { value: "sync", label: "同步备份", icon: "☁️" },
                        { value: "advanced", label: "高级设置", icon: "⚡" }
                    ]

                    Rectangle {
                        width: parent.width
                        height: 48
                        color: "transparent"

                        // 选中状态背景
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.leftMargin: theme.spacingMD
                            anchors.rightMargin: theme.spacingMD
                            color: internal.currentCategory === modelData.value ? theme.primaryLight : "transparent"
                            radius: theme.radiusSmall

                            Behavior on color {
                                ColorAnimation {
                                    duration: theme.animationFast
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        // 选中指示器
                        Rectangle {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: 3
                            height: 20
                            color: theme.primary
                            visible: internal.currentCategory === modelData.value

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: theme.animationFast
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        // 导航项内容
                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: internal.currentCategory === modelData.value ? theme.spacingMD + 12 : theme.spacingMD + 16
                            anchors.rightMargin: theme.spacingMD
                            spacing: theme.spacingMD

                            Text {
                                text: modelData.icon
                                font.pixelSize: 16
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: modelData.label
                                font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                                font.pixelSize: 14
                                font.weight: internal.currentCategory === modelData.value ? Font.Medium : Font.Normal
                                color: internal.currentCategory === modelData.value ? theme.primary : theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Item { Layout.fillWidth: true }

                            // 设置状态指示器
                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: theme.info
                                anchors.verticalCenter: parent.verticalCenter
                                visible: hasUnsavedChanges(modelData.value)

                                SequentialAnimation on opacity {
                                    running: hasUnsavedChanges(modelData.value)
                                    loops: Animation.Infinite
                                    PropertyAnimation { to: 0.3; duration: 800 }
                                    PropertyAnimation { to: 1.0; duration: 800 }
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor

                            onClicked: {
                                internal.currentCategory = modelData.value
                                categoryChangeAnimation.restart()
                            }

                            onEntered: {
                                hoverAnimation.target = parent
                                hoverAnimation.restart()
                            }
                        }

                        Behavior on scale {
                            NumberAnimation {
                                duration: 100
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }

                // 底部空间
                Item {
                    Layout.fillHeight: true
                }

                // 重置按钮
                ModernButton {
                    anchors.horizontalCenter: parent
                    text: "🔄 重置所有"
                    buttonType: ModernButton.Outline
                    size: ModernButton.Small
                    anchors.bottomMargin: theme.spacingMD

                    onClicked: {
                        showResetConfirmDialog()
                    }
                }
            }
        }

        // 分割线
        Rectangle {
            width: 1
            height: parent.height
            color: theme.border
        }

        // 右侧内容
        Rectangle {
            width: parent.width - 260 - 1
            height: parent.height
            color: theme.background

            ScrollView {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                contentWidth: parent.width - theme.spacingMD * 2

                // 通用设置
                Column {
                    width: parent.width
                    visible: internal.currentCategory === "general"
                    spacing: theme.spacingMD

                    ModernCard {
                        width: parent.width
                        title: "应用设置"

                        Column {
                            anchors.fill: parent
                            anchors.margins: theme.spacingMD
                            anchors.topMargin: theme.spacingXL
                            spacing: theme.spacingMD

                            // 语言设置
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "界面语言"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                ComboBox {
                                    id: languageComboBox
                                    width: parent.width
                                    model: ["简体中文", "繁體中文", "English", "日本語", "한국어"]
                                    currentIndex: 0

                                    Material.background: theme.surface
                                    Material.foreground: theme.textPrimary

                                    onActivated: function(index) {
                                        var languages = ["zh-CN", "zh-TW", "en", "ja", "ko"]
                                        internal.settings.general.language = languages[index]
                                        saveSetting("general", "language", languages[index])
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: "更改界面显示语言"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.textTertiary
                                }
                            }

                            // 自动更新
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "自动更新"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Switch {
                                    id: autoUpdateSwitch
                                    checked: internal.settings.general.autoUpdate

                                    Material.accent: theme.primary

                                    onCheckedChanged: {
                                        internal.settings.general.autoUpdate = checked
                                        saveSetting("general", "autoUpdate", checked)
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: "自动检查并安装应用更新"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.textTertiary
                                }
                            }

                            // 启动行为
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "启动行为"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                ComboBox {
                                    id: startupComboBox
                                    width: parent.width
                                    model: ["显示仪表板", "显示搜索页", "显示上次页面", "最小化启动"]
                                    currentIndex: 0

                                    Material.background: theme.surface
                                    Material.foreground: theme.textPrimary

                                    onActivated: function(index) {
                                        var behaviors = ["dashboard", "search", "last", "minimize"]
                                        internal.settings.general.startupBehavior = behaviors[index]
                                        saveSetting("general", "startupBehavior", behaviors[index])
                                    }
                                }
                            }
                        }
                    }

                    ModernCard {
                        width: parent.width
                        title: "数据隐私"

                        Column {
                            anchors.fill: parent
                            anchors.margins: theme.spacingMD
                            anchors.topMargin: theme.spacingXL
                            spacing: theme.spacingMD

                            // 崩溃报告
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "发送崩溃报告"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Switch {
                                    id: crashReportsSwitch
                                    checked: internal.settings.general.crashReports

                                    Material.accent: theme.primary

                                    onCheckedChanged: {
                                        internal.settings.general.crashReports = checked
                                        saveSetting("general", "crashReports", checked)
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: "帮助开发者改善应用稳定性"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.textTertiary
                                }
                            }

                            // 清除数据
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "数据管理"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Row {
                                    width: parent.width
                                    spacing: theme.spacingSM

                                    ModernButton {
                                        text: "🗑️ 清除缓存"
                                        buttonType: ModernButton.Outline
                                        size: ModernButton.Small
                                        Layout.fillWidth: true

                                        onClicked: {
                                            clearCache()
                                        }
                                    }

                                    ModernButton {
                                        text: "📊 使用统计"
                                        buttonType: ModernButton.Outline
                                        size: ModernButton.Small
                                        Layout.fillWidth: true

                                        onClicked: {
                                            showUsageStats()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 外观主题设置
                Column {
                    width: parent.width
                    visible: internal.currentCategory === "appearance"
                    spacing: theme.spacingMD

                    ModernCard {
                        width: parent.width
                        title: "主题设置"

                        Column {
                            anchors.fill: parent
                            anchors.margins: theme.spacingMD
                            anchors.topMargin: theme.spacingXL
                            spacing: theme.spacingMD

                            // 主题选择
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "界面主题"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Row {
                                    width: parent.width
                                    spacing: theme.spacingSM

                                    Repeater {
                                        model: [
                                            { value: "light", label: "浅色", icon: "☀️" },
                                            { value: "dark", label: "深色", icon: "🌙" },
                                            { value: "auto", label: "自动", icon: "🌓" }
                                        ]

                                        Rectangle {
                                            width: parent.width / 3 - theme.spacingSM * 2/3
                                            height: 60
                                            color: internal.settings.appearance.theme === modelData.value ? theme.primary : theme.overlay
                                            border.color: internal.settings.appearance.theme === modelData.value ? theme.primary : theme.border
                                            border.width: internal.settings.appearance.theme === modelData.value ? 2 : 1
                                            radius: theme.radiusSmall

                                            Column {
                                                anchors.centerIn: parent
                                                spacing: 2

                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.icon
                                                    font.pixelSize: 16
                                                }

                                                Text {
                                                    anchors.horizontalCenter: parent.horizontalCenter
                                                    text: modelData.label
                                                    font.family: "SF Pro Display"
                                                    font.pixelSize: 12
                                                    color: internal.settings.appearance.theme === modelData.value ? "#FFFFFF" : theme.textPrimary
                                                }
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor

                                                onClicked: {
                                                    internal.settings.appearance.theme = modelData.value
                                                    saveSetting("appearance", "theme", modelData.value)
                                                }

                                                onEntered: {
                                                    parent.scale = 1.02
                                                }

                                                onExited: {
                                                    parent.scale = 1.0
                                                }

                                                Behavior on scale {
                                                    NumberAnimation {
                                                        duration: 100
                                                        easing.type: Easing.OutCubic
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // 字体设置
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "字体大小"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Row {
                                    width: parent.width
                                    spacing: theme.spacingSM

                                    Repeater {
                                        model: [
                                            { value: "small", label: "小" },
                                            { value: "medium", label: "中" },
                                            { value: "large", label: "大" }
                                        ]

                                        Rectangle {
                                            width: parent.width / 3 - theme.spacingSM * 2/3
                                            height: 36
                                            color: internal.settings.appearance.fontSize === modelData.value ? theme.primary : theme.overlay
                                            radius: theme.radiusSmall

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.label
                                                font.family: "SF Pro Display"
                                                font.pixelSize: 13
                                                font.weight: Font.Medium
                                                color: internal.settings.appearance.fontSize === modelData.value ? "#FFFFFF" : theme.textPrimary
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor

                                                onClicked: {
                                                    internal.settings.appearance.fontSize = modelData.value
                                                    saveSetting("appearance", "fontSize", modelData.value)
                                                }
                                            }
                                        }
                                    }
                                }

                                // 字体选择
                                ComboBox {
                                    id: fontFamilyComboBox
                                    width: parent.width
                                    model: ["系统默认", "SF Pro Display", "PingFang SC", "Microsoft YaHei", "Arial"]
                                    currentIndex: 0

                                    Material.background: theme.surface
                                    Material.foreground: theme.textPrimary

                                    onActivated: function(index) {
                                        var fonts = ["system", "SFProDisplay", "PingFangSC", "MicrosoftYaHei", "Arial"]
                                        internal.settings.appearance.fontFamily = fonts[index]
                                        saveSetting("appearance", "fontFamily", fonts[index])
                                    }
                                }
                            }

                            // 动画效果
                            Column {
                                width: parent.width
                                spacing: theme.spacingSM

                                Text {
                                    text: "界面动画"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    color: theme.textPrimary
                                }

                                Switch {
                                    id: animationsSwitch
                                    checked: internal.settings.appearance.animations

                                    Material.accent: theme.primary

                                    onCheckedChanged: {
                                        internal.settings.appearance.animations = checked
                                        saveSetting("appearance", "animations", checked)
                                    }
                                }

                                Text {
                                    width: parent.width
                                    text: "启用界面动画效果和过渡"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.textTertiary
                                }
                            }
                        }
                    }
                }

                // 其他设置分类...
                // 这里可以根据需要继续添加 voice, learning, sync, advanced 等设置页面
                // 为了简化，这里只展示通用和外观设置

            }
        }
    }

    // ========== 辅助方法 ==========
    function hasUnsavedChanges(category) {
        // 检查是否有未保存的更改
        return false // 简化实现
    }

    function saveSetting(category, key, value) {
        // 保存设置到配置文件
        console.log("Save setting:", category, key, value)

        // 显示保存提示
        showToast(`已保存 ${key} 设置`)
    }

    function clearCache() {
        console.log("Clear cache")
        showToast("正在清除缓存...")
        // 实现缓存清除功能
    }

    function showUsageStats() {
        console.log("Show usage stats")
        // 显示使用统计对话框
    }

    function showResetConfirmDialog() {
        console.log("Show reset confirm dialog")
        // 显示重置确认对话框
    }

    function showToast(message) {
        console.log("Toast:", message)
        // 显示提示消息
    }

    // ========== 动画效果 ==========
    SequentialAnimation {
        id: categoryChangeAnimation
        running: false

        PropertyAnimation {
            target: contentArea
            property: "opacity"
            to: 0
            duration: 100
        }
        PropertyAnimation {
            target: contentArea
            property: "opacity"
            to: 1
            duration: 200
        }
    }

    PropertyAnimation {
        id: hoverAnimation
        property: "scale"
        to: 1.02
        duration: 100
        easing.type: Easing.OutCubic

        onStopped: {
            if (target) {
                target.scale = 1.0
            }
        }
    }

    // ========== 信号 ==========
    signal settingChanged(string category, string key, var value)
    signal settingsReset()
    signal settingsExported(string filePath)
    signal settingsImported(string filePath)
}