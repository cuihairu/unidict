// 划词取词监控覆盖：qmlui/selection_monitor.cpp——文本源注入缝（默认源
// 钉 xcb，offscreen 恒空串，假源直驱）、配置钳制、start/stop 幂等与信号、
// 私有槽 checkSelection 经 moc invoke 直调（变化检测/长度界/排除表/去抖
// 分支全经检测流驱动）、真实定时器接线各一档。
// selection_monitor 没有库目标（qmlui 直接编译），照 clipboard_monitor_test
// 的做法把源码挂进本测试 target。

#include <QMetaObject>
#include <QSignalSpy>
#include <QtTest>

#include "selection_monitor.h"

class SelectionMonitorTest : public QObject {
    Q_OBJECT

private slots:
    // 平台支持面（offscreen 恒 false）+ 配置钳制 + start/stop 幂等与信号
    void config_and_lifecycle();
    // 检测流：变化/未变化/空串/长度界/排除表各分支（假源注入）
    void detection_flow();
    // 真实定时器接线：start 后由 QTimer::timeout 驱动 checkSelection
    void timer_wiring();
};

void SelectionMonitorTest::config_and_lifecycle() {
    SelectionMonitor monitor;
    QSignalSpy monSpy(&monitor, &SelectionMonitor::monitoringChanged);

    // offscreen 平台无独立 selection（默认源钉 xcb）
    QCOMPARE(monitor.isSupported(), false);
    QCOMPARE(monitor.isMonitoring(), false);
    QCOMPARE(monitor.getPollInterval(), 400);
    QCOMPARE(monitor.getMinLength(), 2);
    QCOMPARE(monitor.getMaxLength(), 80);

    // 边界钳制：轮询 [100, 5000]、下限 [1, 40]、上限 [20, 300]
    monitor.setPollInterval(50);
    QCOMPARE(monitor.getPollInterval(), 100);
    monitor.setPollInterval(99999);
    QCOMPARE(monitor.getPollInterval(), 5000);
    monitor.setPollInterval(250);
    QCOMPARE(monitor.getPollInterval(), 250);
    monitor.setMinLength(0);
    QCOMPARE(monitor.getMinLength(), 1);
    monitor.setMinLength(99);
    QCOMPARE(monitor.getMinLength(), 40);
    monitor.setMinLength(3);
    QCOMPARE(monitor.getMinLength(), 3);
    monitor.setMaxLength(10);
    QCOMPARE(monitor.getMaxLength(), 20);
    monitor.setMaxLength(9999);
    QCOMPARE(monitor.getMaxLength(), 300);
    monitor.setMaxLength(80);
    QCOMPARE(monitor.getMaxLength(), 80);

    // 未监控时 stop() 幂等（早退不发信号）
    monitor.stop();
    QCOMPARE(monSpy.count(), 0);

    monitor.start();
    QCOMPARE(monitor.isMonitoring(), true);
    QCOMPARE(monSpy.count(), 1);
    QCOMPARE(monSpy.first().first().toBool(), true);

    // 重复 start() 幂等
    monitor.start();
    QCOMPARE(monSpy.count(), 1);

    monitor.stop();
    QCOMPARE(monitor.isMonitoring(), false);
    QCOMPARE(monSpy.count(), 2);
    QCOMPARE(monSpy.last().first().toBool(), false);
}

void SelectionMonitorTest::detection_flow() {
    SelectionMonitor monitor;
    QSignalSpy spy(&monitor, &SelectionMonitor::selectionDetected);

    // 假源注入：脚本化划选内容序列
    QString selection;
    monitor.setSource([&selection]() { return selection; });

    monitor.start();
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 0);   // 首帧对齐基线不触发

    // 正常划选 → 命中并去首尾空白
    selection = QStringLiteral("  hello world  \n");
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("hello world"));

    // 同文本（未变化）→ 不重复触发
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 1);

    // 空串（点击空白处清选区）→ 变化但空内容不触发
    selection = QString();
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 1);

    // 长度下限：单字符多为误划
    selection = QStringLiteral("a");
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 1);

    // 长度上限：整段划选不是查词意图（81 触顶忽略，80 压线命中）
    selection = QString(81, QChar('x'));
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 1);
    selection = QString(80, QChar('x'));
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 2);

    // 排除表：网址不触发
    selection = QStringLiteral("https://example.com/dict");
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 2);
    selection = QStringLiteral("http://example.com/x");
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 2);
    selection = QStringLiteral("file:///tmp/notes.txt");
    QMetaObject::invokeMethod(&monitor, "checkSelection");
    QCOMPARE(spy.count(), 2);

    monitor.stop();
}

void SelectionMonitorTest::timer_wiring() {
    SelectionMonitor monitor;
    QSignalSpy spy(&monitor, &SelectionMonitor::selectionDetected);

    QString selection;
    monitor.setSource([&selection]() { return selection; });
    monitor.setPollInterval(50);
    monitor.start();

    selection = QStringLiteral("timer");
    // 真实定时器驱动：不直调 checkSelection，等 timeout 槽自跑
    QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    QCOMPARE(spy.first().first().toString(), QStringLiteral("timer"));

    monitor.stop();
    QSignalSpy monSpy(&monitor, &SelectionMonitor::monitoringChanged);
    monitor.setPollInterval(100);
    QCOMPARE(monitor.getPollInterval(), 100);   // 停止态改间隔只存不崩
}

QTEST_MAIN(SelectionMonitorTest)
#include "selection_monitor_test.moc"
