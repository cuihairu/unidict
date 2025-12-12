import QtQuick 2.15
import QtQuick.Controls 2.15

// 现代化动画效果库
Item {
    id: root

    // ========== 通用动画配置 ==========
    readonly property int fastAnimation: 150
    readonly property int normalAnimation: 250
    readonly property int slowAnimation: 350

    readonly property var easingOut: Easing.OutCubic
    readonly property var easingIn: Easing.InCubic
    readonly property var easingInOut: Easing.InOutCubic

    // ========== 淡入动画 ==========
    function fadeIn(target, duration) {
        fadeAnimation.target = target
        fadeAnimation.duration = duration || normalAnimation
        fadeAnimation.from = 0
        fadeAnimation.to = 1
        fadeAnimation.restart()
    }

    PropertyAnimation {
        id: fadeAnimation
        property: "opacity"
        easing.type: root.easingOut
    }

    // ========== 滑入动画 ==========
    function slideIn(target, direction, distance, duration) {
        slideAnimation.target = target
        slideAnimation.duration = duration || normalAnimation
        slideAnimation.property = direction === "up" || direction === "down" ? "y" : "x"

        var from = 0
        if (direction === "left") from = -distance
        else if (direction === "right") from = distance
        else if (direction === "up") from = -distance
        else if (direction === "down") from = distance

        slideAnimation.from = from
        slideAnimation.to = 0
        slideAnimation.restart()
    }

    PropertyAnimation {
        id: slideAnimation
        easing.type: root.easingOut
    }

    // ========== 缩放动画 ==========
    function scaleIn(target, fromScale, toScale, duration) {
        scaleAnimation.target = target
        scaleAnimation.duration = duration || normalAnimation
        scaleAnimation.from = fromScale || 0.8
        scaleAnimation.to = toScale || 1.0
        scaleAnimation.restart()
    }

    PropertyAnimation {
        id: scaleAnimation
        property: "scale"
        easing.type: root.easingOut
    }

    // ========== 弹跳动画 ==========
    function bounce(target, duration) {
        bounceAnimation.target = target
        bounceAnimation.duration = duration || normalAnimation
        bounceAnimation.restart()
    }

    SequentialAnimation {
        id: bounceAnimation
        property: "scale"

        PropertyAnimation { to: 1.1; duration: fastAnimation; easing.type: Easing.OutQuad }
        PropertyAnimation { to: 0.95; duration: fastAnimation; easing.type: Easing.InQuad }
        PropertyAnimation { to: 1.0; duration: fastAnimation; easing.type: Easing.OutQuad }
    }

    // ========== 脉冲动画 ==========
    function pulse(target, duration, repeat) {
        pulseAnimation.target = target
        pulseAnimation.duration = duration || normalAnimation
        pulseAnimation.loops = repeat ? Animation.Infinite : 1
        pulseAnimation.restart()
    }

    SequentialAnimation {
        id: pulseAnimation
        property: "scale"

        PropertyAnimation { to: 1.05; duration: fastAnimation; easing.type: Easing.OutCubic }
        PropertyAnimation { to: 1.0; duration: fastAnimation; easing.type: Easing.InCubic }
    }

    // ========== 震动动画（错误状态） ==========
    function shake(target, duration) {
        shakeAnimation.target = target
        shakeAnimation.duration = duration || fastAnimation
        shakeAnimation.restart()
    }

    SequentialAnimation {
        id: shakeAnimation
        property: "x"

        PropertyAnimation { to: -5; duration: 50 }
        PropertyAnimation { to: 5; duration: 50 }
        PropertyAnimation { to: -5; duration: 50 }
        PropertyAnimation { to: 5; duration: 50 }
        PropertyAnimation { to: 0; duration: 50 }
    }

    // ========== 加载动画 ==========
    function loadingSpin(target, duration) {
        loadingAnimation.target = target
        loadingAnimation.duration = duration || 2000
        loadingAnimation.loops = Animation.Infinite
        loadingAnimation.restart()
    }

    PropertyAnimation {
        id: loadingAnimation
        property: "rotation"
        from: 0
        to: 360
    }

    // ========== 逐字符显示动画 ==========
    function typewriter(target, text, charDuration) {
        typewriterAnimation.target = target
        typewriterAnimation.fullText = text
        typewriterAnimation.charDuration = charDuration || 50
        typewriterAnimation.restart()
    }

    Timer {
        id: typewriterTimer
        interval: 50
        repeat: true
        onTriggered: {
            var currentLength = typewriterAnimation.target.text.length
            if (currentLength < typewriterAnimation.fullText.length) {
                typewriterAnimation.target.text = typewriterAnimation.fullText.substring(0, currentLength + 1)
            } else {
                stop()
            }
        }
    }

    PropertyAnimation {
        id: typewriterAnimation
        property: "text"
        property string fullText: ""
        property int charDuration: 50

        onStarted: {
            target.text = ""
            typewriterTimer.interval = charDuration
            typewriterTimer.start()
        }
    }

    // ========== 页面转场动画 ==========
    function pageTransition(fromPage, toPage, direction) {
        // 并行动画：当前页面淡出，新页面淡入
        var outAnim = Qt.createQmlObject(
            'import QtQuick 2.15; PropertyAnimation { target: fromPage; property: "opacity"; to: 0; duration: ' + normalAnimation + '; easing.type: Easing.InCubic }',
            root, "outAnim"
        )

        var inAnim = Qt.createQmlObject(
            'import QtQuick 2.15; PropertyAnimation { target: toPage; property: "opacity"; to: 1; duration: ' + normalAnimation + '; easing.type: Easing.OutCubic }',
            root, "inAnim"
        )

        outAnim.finished.connect(function() {
            fromPage.visible = false
            inAnim.start()
        })

        fromPage.visible = true
        toPage.visible = true
        toPage.opacity = 0
        outAnim.start()
    }

    // ========== 列表项动画 ==========
    function listItemAppear(target, index, totalItems) {
        target.opacity = 0
        target.scale = 0.8

        var delay = index * 50 // 错开显示时间

        delayTimer.target = target
        delayTimer.delay = delay
        delayTimer.restart()
    }

    Timer {
        id: delayTimer
        property var target
        property int delay: 0

        onTriggered: {
            target.opacity = 1
            target.scale = 1.0

            // 添加弹性效果
            var bounceEffect = Qt.createQmlObject(
                'import QtQuick 2.15; SequentialAnimation { PropertyAnimation { target: target; property: "scale"; to: 1.05; duration: ' + fastAnimation + '; easing.type: Easing.OutQuad } PropertyAnimation { target: target; property: "scale"; to: 1.0; duration: ' + fastAnimation + '; easing.type: Easing.InQuad } }',
                root, "bounceEffect"
            )
            bounceEffect.start()
        }
    }

    // ========== 状态切换动画 ==========
    function stateChange(target, fromState, toState) {
        var stateAnim = Qt.createQmlObject(
            'import QtQuick 2.15; PropertyAnimation { properties: "scale,opacity"; duration: ' + normalAnimation + '; easing.type: Easing.OutCubic }',
            root, "stateAnim"
        )

        target.state = toState
    }

    // ========== 数值变化动画 ==========
    function numberChange(target, fromValue, toValue, duration, format) {
        numberAnim.target = target
        numberAnim.from = fromValue
        numberAnim.to = toValue
        numberAnim.duration = duration || normalAnimation
        numberAnim.format = format || "%d"
        numberAnim.restart()
    }

    PropertyAnimation {
        id: numberAnim
        property: "text"
        property string format: "%d"

        onStarted: {
            var startValue = from
            var endValue = to
            var currentValue = startValue

            valueUpdateTimer.interval = duration / 100
            valueUpdateTimer.startValue = startValue
            valueUpdateTimer.endValue = endValue
            valueUpdateTimer.currentValue = startValue
            valueUpdateTimer.format = format
            valueUpdateTimer.restart()
        }
    }

    Timer {
        id: valueUpdateTimer
        interval: 10
        repeat: true

        property real startValue: 0
        property real endValue: 0
        property real currentValue: 0
        property string format: "%d"

        onTriggered: {
            var progress = (currentValue - startValue) / (endValue - startValue)
            if (progress < 1.0) {
                currentValue += (endValue - startValue) / 100
                target.text = format.replace("%d", Math.round(currentValue))
            } else {
                stop()
                target.text = format.replace("%d", Math.round(endValue))
            }
        }
    }

    // ========== 背景模糊动画 ==========
    function backgroundBlur(target, blurRadius, duration) {
        blurAnim.target = target
        blurAnim.to = blurRadius
        blurAnim.duration = duration || normalAnimation
        blurAnim.restart()
    }

    PropertyAnimation {
        id: blurAnim
        property: "layer.effect.radius"
    }

    // ========== 发光效果动画 ==========
    function glowEffect(target, glowColor, duration) {
        glowAnim.target = target
        glowAnim.to = glowColor
        glowAnim.duration = duration || normalAnimation
        glowAnim.restart()
    }

    PropertyAnimation {
        id: glowAnim
        property: "layer.effect.color"
    }
}