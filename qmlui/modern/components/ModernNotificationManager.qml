import QtQuick 2.15
import QtQuick.Controls 2.15
import "../Theme.qml"

// 现代化通知管理器
Item {
    id: root

    // ========== 主题 ==========
    Theme { id: theme }

    // ========== 通知队列 ==========
    property var notificationQueue: []
    property var activeNotifications: []
    property int maxConcurrentNotifications: 3
    property int maxNotificationHeight: 120

    // ========== 通知管理器实例 ==========
    property var managerInstance: root

    // ========== 通知容器 ==========
    Column {
        id: notificationContainer
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: theme.spacingLG
        anchors.rightMargin: theme.spacingLG
        spacing: theme.spacingSM
        z: 1000
    }

    // ========== 静态方法 ==========
    function show(message, title, type, duration, options) {
        var notification = createNotification(message, title, type, duration, options)
        addNotificationToQueue(notification)
        return notification
    }

    function showSuccess(message, title, duration, options) {
        return show(message, title, ModernToast.Success, duration || 3000, options)
    }

    function showError(message, title, duration, options) {
        return show(message, title, ModernToast.Error, duration || 5000, options)
    }

    function showWarning(message, title, duration, options) {
        return show(message, title, ModernToast.Warning, duration || 4000, options)
    }

    function showInfo(message, title, duration, options) {
        return show(message, title, ModernToast.Info, duration || 3000, options)
    }

    function showLoading(message, title, options) {
        return show(message, title, ModernToast.Loading, 0, options)
    }

    function showProgress(message, title, initialProgress, options) {
        var notification = createNotification(message, title, ModernToast.Info, 0, options)
        notification.showProgress = true
        notification.progress = initialProgress || 0
        addNotificationToQueue(notification)
        return notification
    }

    // ========== 通知创建 ==========
    function createNotification(message, title, type, duration, options) {
        options = options || {}

        // 创建通知组件
        var component = Qt.createComponent("ModernToast.qml")
        if (component.status === Component.Ready) {
            var notification = component.createObject(root, {
                text: message || "",
                title: title || "",
                type: type || ModernToast.Info,
                duration: duration || 3000,
                autoHide: options.autoHide !== false,
                canDismiss: options.canDismiss !== false,
                showIcon: options.showIcon !== false,
                position: options.position || ModernToast.Bottom
            })

            // 设置特定选项
            if (options.icon) notification.icon = options.icon
            if (options.showProgress) notification.showProgress = options.showProgress
            if (options.progress !== undefined) notification.progress = options.progress

            // 连接信号
            notification.dismissed.connect(function() {
                removeNotification(notification)
            })

            return notification
        } else {
            console.error("Failed to create notification:", component.errorString())
            return null
        }
    }

    // ========== 队列管理 ==========
    function addNotificationToQueue(notification) {
        if (notification) {
            notificationQueue.push(notification)
            processQueue()
        }
    }

    function processQueue() {
        if (activeNotifications.length < maxConcurrentNotifications && notificationQueue.length > 0) {
            var notification = notificationQueue.shift()
            if (notification) {
                showNotification(notification)
            }
        }
    }

    function showNotification(notification) {
        if (notification) {
            activeNotifications.push(notification)
            notificationContainer.insert(0, notification)

            // 调整通知位置
            adjustNotificationPositions()

            // 显示通知
            notification.show()

            // 监听通知完成事件
            notification.dismissed.connect(function() {
                removeNotification(notification)
            })
        }
    }

    function removeNotification(notification) {
        // 从活动列表中移除
        var index = activeNotifications.indexOf(notification)
        if (index !== -1) {
            activeNotifications.splice(index, 1)
        }

        // 从容器中移除
        if (notificationContainer.children.indexOf(notification) !== -1) {
            notification.visible = false
            notificationContainer.children = notificationContainer.children.filter(function(child) {
                return child !== notification
            })
        }

        // 调整位置
        adjustNotificationPositions()

        // 处理队列中的下一个通知
        processQueue()

        // 销毁通知
        if (notification.destroy) {
            notification.destroy()
        }
    }

    function adjustNotificationPositions() {
        var yOffset = 0
        for (var i = notificationContainer.children.length - 1; i >= 0; i--) {
            var notification = notificationContainer.children[i]
            if (notification && notification.visible) {
                notification.anchors.topMargin = yOffset
                yOffset += notification.height + theme.spacingSM

                // 限制最大显示高度
                if (yOffset > maxNotificationHeight) {
                    notification.opacity = 0
                    notification.visible = false
                }
            }
        }
    }

    // ========== 批量操作 ==========
    function clearAllNotifications() {
        // 复制当前活动的通知以避免迭代时修改数组
        var notifications = activeNotifications.slice()

        notifications.forEach(function(notification) {
            if (notification && notification.dismiss) {
                notification.dismiss()
            }
        })

        // 清空队列
        notificationQueue = []
    }

    function pauseAllNotifications() {
        activeNotifications.forEach(function(notification) {
            if (notification && notification.pause) {
                notification.pause()
            }
        })
    }

    function resumeAllNotifications() {
        activeNotifications.forEach(function(notification) {
            if (notification && notification.resume) {
                notification.resume()
            }
        })
    }

    // ========== 通知历史 ==========
    property var notificationHistory: []

    function addToHistory(notification) {
        var historyItem = {
            message: notification.text,
            title: notification.title,
            type: notification.type,
            timestamp: new Date(),
            id: Math.random().toString(36).substr(2, 9)
        }

        notificationHistory.unshift(historyItem)

        // 限制历史记录数量
        if (notificationHistory.length > 100) {
            notificationHistory = notificationHistory.slice(0, 100)
        }

        // 触发历史更新事件
        historyUpdated(historyItem)
    }

    function getNotificationHistory(limit) {
        limit = limit || 50
        return notificationHistory.slice(0, limit)
    }

    function clearHistory() {
        notificationHistory = []
        historyCleared()
    }

    // ========== 预设通知 ==========
    function showNetworkError(error) {
        return showError(
            "网络连接失败，请检查网络设置",
            "网络错误",
            5000
        )
    }

    function showSaveSuccess() {
        return showSuccess(
            "数据已成功保存",
            "保存成功"
        )
    }

    function showLoadError(resource) {
        return showError(
            `无法加载 ${resource || "资源"}`,
            "加载失败"
        )
    }

    function showSyncProgress(progress) {
        if (progress >= 100) {
            return showSuccess("同步完成", "数据同步")
        } else {
            return showProgress(
                "正在同步数据...",
                "数据同步",
                progress / 100
            )
        }
    }

    function showLearningReminder() {
        return showInfo(
            "今日学习目标还未完成，快来学习吧！",
            "学习提醒",
            5000
        )
    }

    function showUpdateAvailable(version) {
        return showInfo(
            `发现新版本 ${version}，点击查看详情`,
            "更新可用",
            0, // 不自动隐藏
            {
                canDismiss: true,
                showProgress: false
            }
        )
    }

    // ========== 信号 ==========
    signal notificationAdded(var notification)
    signal notificationRemoved(var notification)
    signal queueProcessed()
    signal historyUpdated(var historyItem)
    signal historyCleared()
    signal allNotificationsCleared()

    // ========== 工具方法 ==========
    function getActiveNotificationCount() {
        return activeNotifications.length
    }

    function getQueueLength() {
        return notificationQueue.length
    }

    function hasActiveNotifications() {
        return activeNotifications.length > 0
    }

    function hasPendingNotifications() {
        return notificationQueue.length > 0
    }

    function getNotificationById(id) {
        for (var i = 0; i < activeNotifications.length; i++) {
            if (activeNotifications[i].id === id) {
                return activeNotifications[i]
            }
        }
        return null
    }

    // ========== 调试功能 ==========
    function showTestNotifications() {
        showSuccess("这是成功消息", "测试成功")
        showInfo("这是信息消息", "测试信息")
        showWarning("这是警告消息", "测试警告")
        showError("这是错误消息", "测试错误")
        showLoading("这是加载消息", "测试加载")
        showProgress("处理中...", "测试进度", 0.7)
    }

    // ========== 初始化 ==========
    Component.onCompleted: {
        console.log("ModernNotificationManager initialized")
    }

    // ========== 全局访问点 ==========
    function getGlobalInstance() {
        return managerInstance
    }

    // 设置全局实例
    managerInstance = root

    // 导出到全局作用域（可选）
    if (typeof global !== 'undefined') {
        global.NotificationManager = root
    }
}