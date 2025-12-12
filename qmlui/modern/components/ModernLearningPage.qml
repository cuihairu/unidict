import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Charts
import "../Theme.qml"

// 现代化学习进度页面
Item {
    id: root

    // ========== 公共属性 ==========
    property alias lookupAdapter: internal.lookupAdapter

    // 学习数据
    property int totalWords: 150
    property int masteredWords: 45
    property int learningWords: 60
    property int newWords: 30
    property int reviewedWords: 15

    // 学习统计
    property var dailyProgress: []  // 每日学习进度
    property var weeklyStats: []    // 周统计数据
    property var monthlyStats: []   // 月统计数据

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 内部状态 ==========
    QtObject {
        id: internal
        property var lookupAdapter
        property string selectedTimeRange: "week" // day, week, month, year
        property string selectedChart: "progress" // progress, retention, frequency
    }

    // ========== 页面布局 ==========
    Column {
        anchors.fill: parent
        spacing: theme.spacingMD

        // 页面头部
        Rectangle {
            width: parent.width
            height: 80
            color: "transparent"

            Row {
                anchors.fill: parent
                anchors.margins: theme.spacingMD
                spacing: theme.spacingMD

                // 标题区域
                Row {
                    spacing: theme.spacingSM

                    Text {
                        text: "📈"
                        font.pixelSize: 18
                        color: theme.primary
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Column {
                        spacing: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: "学习进度"
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 18
                            font.weight: Font.SemiBold
                            color: theme.textPrimary
                        }

                        Text {
                            text: "持续学习 ${getLearningDays()} 天"
                            font.family: "SF Pro Display"
                            font.pixelSize: 12
                            color: theme.textSecondary
                        }
                    }
                }

                // 弹性空间
                Item { Layout.fillWidth: true }

                // 时间范围选择
                Row {
                    spacing: theme.spacingXS

                    Repeater {
                        model: [
                            { value: "day", label: "今日" },
                            { value: "week", label: "本周" },
                            { value: "month", label: "本月" },
                            { value: "year", label: "本年" }
                        ]

                        Rectangle {
                            width: labelText.implicitWidth + 16
                            height: 32
                            radius: 16
                            color: internal.selectedTimeRange === modelData.value ? theme.primary : theme.overlay

                            Text {
                                id: labelText
                                anchors.centerIn: parent
                                text: modelData.label
                                font.family: "SF Pro Display"
                                font.pixelSize: 13
                                color: internal.selectedTimeRange === modelData.value ? "#FFFFFF" : theme.textPrimary
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    internal.selectedTimeRange = modelData.value
                                    updateStatistics()
                                }
                            }
                        }
                    }
                }
            }
        }

        // 学习概览卡片
        Row {
            width: parent.width
            spacing: theme.spacingMD

            Repeater {
                model: [
                    { title: "总词汇", value: totalWords, color: theme.primary, icon: "📚", change: "+12", trend: "up" },
                    { title: "已掌握", value: masteredWords, color: theme.success, icon: "✅", change: "+8", trend: "up" },
                    { title: "学习中", value: learningWords, color: theme.warning, icon: "📖", change: "-3", trend: "down" },
                    { title: "复习数", value: reviewedWords, color: theme.info, icon: "🔄", change: "+15", trend: "up" }
                ]

                ModernCard {
                    width: parent.width / 4 - theme.spacingMD * 0.75
                    height: 120

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingLG
                        spacing: theme.spacingSM

                        // 图标和变化趋势
                        Row {
                            width: parent.width
                            spacing: theme.spacingXS

                            Text {
                                text: modelData.icon || ""
                                font.pixelSize: 18
                            }

                            Item { Layout.fillWidth: true }

                            Row {
                                spacing: 2

                                Text {
                                    text: modelData.change || ""
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: modelData.trend === "up" ? theme.success : theme.error
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Text {
                                    text: modelData.trend === "up" ? "↑" : "↓"
                                    font.pixelSize: 10
                                    color: modelData.trend === "up" ? theme.success : theme.error
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }

                        // 数值
                        Text {
                            width: parent.width
                            text: modelData.value || 0
                            font.family: "SF Pro Display, -apple-system, BlinkMacSystemFont"
                            font.pixelSize: 24
                            font.weight: Font.Bold
                            color: modelData.color || theme.textPrimary
                        }

                        // 标题
                        Text {
                            width: parent.width
                            text: modelData.title || ""
                            font.family: "SF Pro Display"
                            font.pixelSize: 12
                            color: theme.textSecondary
                        }
                    }

                    // 动画效果
                    Component.onCompleted: {
                        scale = 0.8
                        opacity = 0
                        cardEnterAnim.delay = index * 100
                        cardEnterAnim.target = parent
                        cardEnterAnim.restart()
                    }
                }
            }
        }

        // 主要内容区域
        Row {
            width: parent.width
            Layout.fillHeight: true
            spacing: theme.spacingMD

            // 左侧 - 图表区域
            Column {
                width: parent.width * 0.6
                height: parent.height
                spacing: theme.spacingMD

                // 学习进度图表
                ModernCard {
                    width: parent.width
                    height: 320
                    title: "学习进度"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingMD

                        // 图表类型选择
                        Row {
                            width: parent.width
                            spacing: theme.spacingXS

                            Repeater {
                                model: [
                                    { value: "progress", label: "进度" },
                                    { value: "retention", label: "记忆率" },
                                    { value: "frequency", label: "频率" }
                                ]

                                Rectangle {
                                    width: chartLabel.implicitWidth + 12
                                    height: 28
                                    radius: 14
                                    color: internal.selectedChart === modelData.value ? theme.primary : theme.border

                                    Text {
                                        id: chartLabel
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        font.family: "SF Pro Display"
                                        font.pixelSize: 12
                                        color: internal.selectedChart === modelData.value ? "#FFFFFF" : theme.textSecondary
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            internal.selectedChart = modelData.value
                                        }
                                    }
                                }
                            }
                        }

                        // 图表容器
                        Rectangle {
                            width: parent.width
                            height: 200
                            color: theme.overlay
                            radius: theme.radiusMedium

                            // 简化的进度条图表
                            Column {
                                anchors.fill: parent
                                anchors.margins: theme.spacingMD
                                spacing: theme.spacingSM

                                Repeater {
                                    model: 7 // 7天的数据

                                    Row {
                                        width: parent.width
                                        height: parent.height / 7
                                        spacing: theme.spacingSM

                                        // 日期标签
                                        Text {
                                            width: 40
                                            text: getDayLabel(index)
                                            font.family: "SF Pro Display"
                                            font.pixelSize: 10
                                            color: theme.textTertiary
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        // 进度条
                                        Rectangle {
                                            width: parent.width - 40 - theme.spacingSM - 40
                                            height: 20
                                            radius: 10
                                            color: theme.background
                                            anchors.verticalCenter: parent.verticalCenter

                                            Rectangle {
                                                width: parent.width * getProgressValue(index)
                                                height: parent.height
                                                radius: 10
                                                color: theme.primary

                                                Behavior on width {
                                                    NumberAnimation {
                                                        duration: 500
                                                        easing.type: Easing.OutCubic
                                                    }
                                                }
                                            }
                                        }

                                        // 数值标签
                                        Text {
                                            width: 40
                                            text: `${Math.round(getProgressValue(index) * 100)}%`
                                            font.family: "SF Pro Display"
                                            font.pixelSize: 10
                                            color: theme.textSecondary
                                            horizontalAlignment: Text.AlignRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }

                        // 图表说明
                        Text {
                            width: parent.width
                            text: getChartDescription()
                            font.family: "SF Pro Display"
                            font.pixelSize: 11
                            color: theme.textTertiary
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // 学习日历
                ModernCard {
                    width: parent.width
                    Layout.fillHeight: true
                    title: "学习日历"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        // 简化的日历视图
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            // 星期标题
                            Row {
                                width: parent.width
                                spacing: theme.spacingXS

                                Repeater {
                                    model: ["日", "一", "二", "三", "四", "五", "六"]

                                    Text {
                                        width: parent.width / 7
                                        text: modelData || ""
                                        font.family: "SF Pro Display"
                                        font.pixelSize: 11
                                        font.weight: Font.Medium
                                        color: theme.textSecondary
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                }
                            }

                            // 日期网格
                            Repeater {
                                model: 30 // 显示30天的日历

                                Row {
                                    width: parent.width
                                    spacing: theme.spacingXS
                                    visible: index % 7 === 0 // 每行开始

                                    Repeater {
                                        model: Math.min(7, 30 - index)
                                        visible: index + modelIndex < 30

                                        Rectangle {
                                            width: parent.width / 7
                                            height: 36
                                            radius: 18
                                            color: getLearningStatus(index + modelIndex).color
                                            border.color: theme.border
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: (index + modelIndex + 1).toString()
                                                font.family: "SF Pro Display"
                                                font.pixelSize: 12
                                                color: getLearningStatus(index + modelIndex).textColor
                                            }

                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                hoverEnabled: true

                                                onEntered: {
                                                    parent.scale = 1.1
                                                }

                                                onExited: {
                                                    parent.scale = 1.0
                                                }

                                                onClicked: {
                                                    showDayDetails(index + modelIndex + 1)
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
                                }
                            }
                        }

                        // 图例
                        Row {
                            anchors.horizontalCenter: parent
                            spacing: theme.spacingMD
                            anchors.topMargin: theme.spacingMD

                            Repeater {
                                model: [
                                    { label: "未学习", color: theme.background },
                                    { label: "少量学习", color: theme.info },
                                    { label: "正常学习", color: theme.warning },
                                    { label: "大量学习", color: theme.success }
                                ]

                                Row {
                                    spacing: theme.spacingXS

                                    Rectangle {
                                        width: 12
                                        height: 12
                                        radius: 2
                                        color: modelData.color
                                        border.color: theme.border
                                        border.width: 1
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        text: modelData.label
                                        font.family: "SF Pro Display"
                                        font.pixelSize: 10
                                        color: theme.textSecondary
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 右侧 - 学习统计和目标
            Column {
                width: parent.width * 0.4
                height: parent.height
                spacing: theme.spacingMD

                // 学习目标
                ModernCard {
                    width: parent.width
                    height: 280
                    title: "学习目标"

                    Column {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL
                        spacing: theme.spacingMD

                        // 当前目标进度
                        Column {
                            width: parent.width
                            spacing: theme.spacingSM

                            Text {
                                text: "本月目标"
                                font.family: "SF Pro Display"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                color: theme.textPrimary
                            }

                            Text {
                                text: `${getMonthlyProgress()} / 100 个新词汇`
                                font.family: "SF Pro Display"
                                font.pixelSize: 24
                                font.weight: Font.Bold
                                color: theme.primary
                            }

                            ProgressBar {
                                width: parent.width
                                value: getMonthlyProgress() / 100

                                Material.accent: theme.primary
                            }

                            Text {
                                text: `还需 ${Math.max(0, 100 - getMonthlyProgress())} 个词汇达成目标`
                                font.family: "SF Pro Display"
                                font.pixelSize: 12
                                color: theme.textSecondary
                            }
                        }

                        // 快速开始学习
                        ModernButton {
                            width: parent.width
                            text: "🚀 开始今日学习"
                            buttonType: ModernButton.Primary

                            onClicked: {
                                startDailyLearning()
                            }
                        }

                        // 学习提醒
                        Rectangle {
                            width: parent.width
                            height: 60
                            color: theme.primaryLight
                            radius: theme.radiusSmall

                            Row {
                                anchors.fill: parent
                                anchors.margins: theme.spacingMD
                                spacing: theme.spacingMD

                                Text {
                                    text: "⏰"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                }

                                Column {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 2

                                    Text {
                                        text: "学习提醒"
                                        font.family: "SF Pro Display"
                                        font.pixelSize: 12
                                        font.weight: Font.Medium
                                        color: theme.primary
                                    }

                                    Text {
                                        text: "每日 20:00"
                                        font.family: "SF Pro Display"
                                        font.pixelSize: 11
                                        color: theme.textSecondary
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: "设置"
                                    font.family: "SF Pro Display"
                                    font.pixelSize: 12
                                    color: theme.primary
                                    anchors.verticalCenter: parent.verticalCenter

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            showReminderSettings()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 成就徽章
                ModernCard {
                    width: parent.width
                    height: 320
                    title: "成就徽章"

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: theme.spacingMD
                        anchors.topMargin: theme.spacingXL

                        Column {
                            width: parent.width
                            spacing: theme.spacingMD

                            Repeater {
                                model: [
                                    { name: "初学者", description: "完成第一个词汇", icon: "🌟", earned: true, date: "2024-01-01" },
                                    { name: "坚持者", description: "连续学习7天", icon: "🔥", earned: true, date: "2024-01-08" },
                                    { name: "词汇达人", description: "掌握100个词汇", icon: "👑", earned: true, date: "2024-01-15" },
                                    { name: "学习冠军", description: "连续学习30天", icon: "🏆", earned: false, date: "" },
                                    { name: "记忆大师", description: "掌握500个词汇", icon: "🧠", earned: false, date: "" },
                                    { name: "终身学者", description: "掌握1000个词汇", icon: "🎓", earned: false, date: "" }
                                ]

                                Rectangle {
                                    width: parent.width
                                    height: 60
                                    color: modelData.earned ? theme.cardBackground : "transparent"
                                    border.color: modelData.earned ? theme.border : theme.textTertiary
                                    border.width: 1
                                    radius: theme.radiusSmall

                                    Row {
                                        anchors.fill: parent
                                        anchors.margins: theme.spacingMD
                                        spacing: theme.spacingMD

                                        // 徽章图标
                                        Rectangle {
                                            width: 40
                                            height: 40
                                            radius: 20
                                            color: modelData.earned ? theme.primary : theme.textTertiary
                                            anchors.verticalCenter: parent.verticalCenter

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.icon || ""
                                                font.pixelSize: 16
                                                color: modelData.earned ? "#FFFFFF" : theme.cardBackground
                                            }
                                        }

                                        // 徽章信息
                                        Column {
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: parent.width - 40 - theme.spacingMD * 2
                                            spacing: 1

                                            Text {
                                                text: modelData.name || ""
                                                font.family: "SF Pro Display"
                                                font.pixelSize: 14
                                                font.weight: Font.Medium
                                                color: modelData.earned ? theme.textPrimary : theme.textTertiary
                                            }

                                            Text {
                                                text: modelData.description || ""
                                                font.family: "SF Pro Display"
                                                font.pixelSize: 12
                                                color: modelData.earned ? theme.textSecondary : theme.textTertiary
                                            }

                                            Text {
                                                text: modelData.date || "未解锁"
                                                font.family: "SF Pro Display"
                                                font.pixelSize: 10
                                                color: modelData.earned ? theme.textTertiary : theme.textTertiary
                                                visible: modelData.date !== ""
                                            }
                                        }
                                    }

                                    // 获得动画
                                    SequentialAnimation {
                                        running: modelData.earned && index === 2 // 模拟新获得的徽章
                                        loops: 1

                                        PropertyAnimation {
                                            target: parent
                                            property: "scale"
                                            to: 1.1
                                            duration: 300
                                        }
                                        PropertyAnimation {
                                            target: parent
                                            property: "scale"
                                            to: 1.0
                                            duration: 300
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ========== 辅助方法 ==========
    function getLearningDays() {
        // 模拟计算学习天数
        return 45
    }

    function getDayLabel(dayIndex) {
        var days = ["周一", "周二", "周三", "周四", "周五", "周六", "周日"]
        return days[dayIndex % 7]
    }

    function getProgressValue(dayIndex) {
        // 模拟每日学习进度
        return Math.random() * 0.8 + 0.2
    }

    function getChartDescription() {
        switch(internal.selectedChart) {
            case "progress": return "过去7天的学习进度，显示每日掌握的词汇数量"
            case "retention": return "词汇记忆保持率，展示长期记忆效果"
            case "frequency": return "学习频率统计，反映学习规律性"
            default: return "学习数据分析"
        }
    }

    function getLearningStatus(dayIndex) {
        // 模拟每日学习状态
        var status = Math.random()
        if (status < 0.2) {
            return { color: theme.background, textColor: theme.textTertiary } // 未学习
        } else if (status < 0.5) {
            return { color: theme.info, textColor: "#FFFFFF" } // 少量学习
        } else if (status < 0.8) {
            return { color: theme.warning, textColor: "#FFFFFF" } // 正常学习
        } else {
            return { color: theme.success, textColor: "#FFFFFF" } // 大量学习
        }
    }

    function getMonthlyProgress() {
        // 模拟月度学习进度
        return 67
    }

    function showDayDetails(day) {
        console.log("Show day details:", day)
        // 显示特定日期的详细学习信息
    }

    function startDailyLearning() {
        console.log("Start daily learning")
        // 开始每日学习模式
    }

    function showReminderSettings() {
        console.log("Show reminder settings")
        // 显示学习提醒设置
    }

    function updateStatistics() {
        console.log("Update statistics for time range:", internal.selectedTimeRange)
        // 根据选择的时间范围更新统计数据
    }

    // ========== 动画效果 ==========
    PropertyAnimation {
        id: cardEnterAnim
        property: "scale"
        from: 0.8
        to: 1.0
        duration: 300
        easing.type: Easing.OutCubic

        onStarted: {
            if (target) {
                opacityAnim.target = target
                opacityAnim.from = 0
                opacityAnim.to = 1
                opacityAnim.start()
            }
        }
    }

    PropertyAnimation {
        id: opacityAnim
        property: "opacity"
        duration: 300
        easing.type: Easing.OutCubic
    }

    // ========== 信号 ==========
    signal learningStarted()
    signal goalUpdated(int newGoal)
    signal reminderSet(string time)
    signal achievementUnlocked(string achievementName)
}