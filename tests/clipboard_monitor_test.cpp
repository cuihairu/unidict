// Q-8 覆盖收口：qmlui/clipboard_monitor.cpp——剪贴板监听（offscreen 平台
// 插件提供内存剪贴板）。配置项钳制、start/stop 信号、私有槽 checkClipboard
// 经 moc invoke 直调（isValidWord/extractWord/isExcluded 三个私有 helper
// 全部经检测流间接驱动）、真实定时器接线各一档。
// clipboard_monitor 没有库目标（只有 qmlui 直接编译），照 test_global_hotkeys
// 的做法把源码挂进本测试 target。

#include <QClipboard>
#include <QGuiApplication>
#include <QMetaObject>
#include <QSignalSpy>
#include <QtTest>

#include "clipboard_monitor.h"

class ClipboardMonitorTest : public QObject {
    Q_OBJECT

private slots:
    // 配置项边界钳制 + start/stop 幂等与信号
    void q8_config_and_lifecycle();
    // 检测流：变化/未变化/空串/排除表/提词与成词校验各分支
    void q8_detection_flow();
    // 真实定时器接线：start 后由 QTimer::timeout 驱动 checkClipboard
    void q8_timer_wiring();
};

void ClipboardMonitorTest::q8_config_and_lifecycle() {
    ClipboardMonitor monitor;
    QSignalSpy monSpy(&monitor, &ClipboardMonitor::monitoringChanged);

    // 缺省配置
    QCOMPARE(monitor.isMonitoring(), false);
    QCOMPARE(monitor.getPollInterval(), 500);
    QCOMPARE(monitor.getMinWordLength(), 2);
    QCOMPARE(monitor.getMaxWordLength(), 50);

    // 边界钳制：轮询 [100, 5000]、词长下限 [1, 10]、上限 [10, 200]
    monitor.setPollInterval(50);
    QCOMPARE(monitor.getPollInterval(), 100);
    monitor.setPollInterval(99999);
    QCOMPARE(monitor.getPollInterval(), 5000);
    monitor.setPollInterval(250);
    QCOMPARE(monitor.getPollInterval(), 250);
    monitor.setMinWordLength(0);
    QCOMPARE(monitor.getMinWordLength(), 1);
    monitor.setMinWordLength(99);
    QCOMPARE(monitor.getMinWordLength(), 10);
    monitor.setMinWordLength(3);
    QCOMPARE(monitor.getMinWordLength(), 3);
    monitor.setMaxWordLength(1);
    QCOMPARE(monitor.getMaxWordLength(), 10);
    monitor.setMaxWordLength(9999);
    QCOMPARE(monitor.getMaxWordLength(), 200);
    monitor.setMaxWordLength(50);
    QCOMPARE(monitor.getMaxWordLength(), 50);

    // 未监控时 stop() 幂等（早退不发信号）
    monitor.stop();
    QCOMPARE(monSpy.count(), 0);

    // start：转监控 + monitoringChanged(true)；重复 start 早退
    monitor.start();
    QCOMPARE(monitor.isMonitoring(), true);
    QCOMPARE(monSpy.count(), 1);
    QCOMPARE(monSpy.at(0).at(0).toBool(), true);
    monitor.start();
    QCOMPARE(monSpy.count(), 1);

    // 监控中改轮询间隔 → 同步到已启动的定时器
    monitor.setPollInterval(100);
    QCOMPARE(monitor.getPollInterval(), 100);

    // stop：停止 + monitoringChanged(false)
    monitor.stop();
    QCOMPARE(monitor.isMonitoring(), false);
    QCOMPARE(monSpy.count(), 2);
    QCOMPARE(monSpy.at(1).at(0).toBool(), false);
}

void ClipboardMonitorTest::q8_detection_flow() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);
    clipboard->setText(QString());

    ClipboardMonitor monitor;
    QSignalSpy wordSpy(&monitor, &ClipboardMonitor::wordDetected);
    QSignalSpy textSpy(&monitor, &ClipboardMonitor::textChanged);

    // checkClipboard 是私有槽：经 moc invoke 直调（同线程直连）
    auto check = [&monitor] {
        QMetaObject::invokeMethod(&monitor, "checkClipboard");
    };

    // 变化 → textChanged + 提词成词 wordDetected（取首词）
    clipboard->setText(QStringLiteral("hello world"));
    check();
    QCOMPARE(textSpy.count(), 1);
    QCOMPARE(textSpy.at(0).at(0).toString(), QStringLiteral("hello world"));
    QCOMPARE(wordSpy.count(), 1);
    QCOMPARE(wordSpy.at(0).at(0).toString(), QStringLiteral("hello"));

    // 未变化 → 早退，无新信号
    check();
    QCOMPARE(textSpy.count(), 1);
    QCOMPARE(wordSpy.count(), 1);

    // 空串 → textChanged("")，提词为空 → 无 wordDetected
    clipboard->setText(QString());
    check();
    QCOMPARE(textSpy.count(), 2);
    QCOMPARE(wordSpy.count(), 1);

    // 缺省排除表：URL、纯数字 → 只有 textChanged
    clipboard->setText(QStringLiteral("https://example.com/dict"));
    check();
    QCOMPARE(textSpy.count(), 3);
    QCOMPARE(wordSpy.count(), 1);
    clipboard->setText(QStringLiteral("12345"));
    check();
    QCOMPARE(textSpy.count(), 4);
    QCOMPARE(wordSpy.count(), 1);

    // 清空排除表后：纯数字仍不成词（无字母被 isValidWord 拒）；
    // isExcluded 走"无匹配"出口
    monitor.clearExcludePatterns();
    clipboard->setText(QStringLiteral("6789"));
    check();
    QCOMPARE(textSpy.count(), 5);
    QCOMPARE(wordSpy.count(), 1);

    // 提词面：首尾标点剥离（含引号）
    clipboard->setText(QStringLiteral("\"apple\","));
    check();
    QCOMPARE(textSpy.count(), 6);
    QCOMPARE(wordSpy.count(), 2);
    QCOMPARE(wordSpy.at(1).at(0).toString(), QStringLiteral("apple"));

    // 词长边界（显式收紧到 min 2 / max 10）：过短、过长都只在 textChanged
    monitor.setMinWordLength(2);
    monitor.setMaxWordLength(10);
    clipboard->setText(QStringLiteral("a b c"));
    check();
    QCOMPARE(textSpy.count(), 7);
    QCOMPARE(wordSpy.count(), 2);
    clipboard->setText(QStringLiteral("abcdefghijkl"));
    check();
    QCOMPARE(textSpy.count(), 8);
    QCOMPARE(wordSpy.count(), 2);

    // 特殊字符 >30% 拒绝；恰好 ≤30% 通过（'-' 与 '\'' 不计特殊）
    clipboard->setText(QStringLiteral("ab@#d"));
    check();
    QCOMPARE(textSpy.count(), 9);
    QCOMPARE(wordSpy.count(), 2);
    clipboard->setText(QStringLiteral("ab@cd"));
    check();
    QCOMPARE(textSpy.count(), 10);
    QCOMPARE(wordSpy.count(), 3);
    QCOMPARE(wordSpy.at(2).at(0).toString(), QStringLiteral("ab@cd"));
    clipboard->setText(QStringLiteral("it's-fine"));
    check();
    QCOMPARE(textSpy.count(), 11);
    QCOMPARE(wordSpy.count(), 4);
    QCOMPARE(wordSpy.at(3).at(0).toString(), QStringLiteral("it's-fine"));

    // CJK 词形同样成词（字母判定含 一-鿿）
    clipboard->setText(QStringLiteral("中文"));
    check();
    QCOMPARE(textSpy.count(), 12);
    QCOMPARE(wordSpy.count(), 5);
    QCOMPARE(wordSpy.at(4).at(0).toString(), QStringLiteral("中文"));
}

void ClipboardMonitorTest::q8_timer_wiring() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    QVERIFY(clipboard);

    // start() 会先快照当前剪贴板，随后写入的新文本才会被下一次轮询看见
    clipboard->setText(QStringLiteral("中文"));

    ClipboardMonitor monitor;
    QSignalSpy wordSpy(&monitor, &ClipboardMonitor::wordDetected);
    QSignalSpy monSpy(&monitor, &ClipboardMonitor::monitoringChanged);
    monitor.setPollInterval(100);
    monitor.start();

    clipboard->setText(QStringLiteral("timerword"));
    QTRY_COMPARE(wordSpy.count(), 1);
    QCOMPARE(wordSpy.at(0).at(0).toString(), QStringLiteral("timerword"));

    monitor.stop();
    QCOMPARE(monSpy.count(), 2);
    QCOMPARE(monSpy.at(0).at(0).toBool(), true);
    QCOMPARE(monSpy.at(1).at(0).toBool(), false);
}

QTEST_MAIN(ClipboardMonitorTest)
#include "clipboard_monitor_test.moc"
