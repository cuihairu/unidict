// Q-10 覆盖收口：gui/pronunciation_panel.cpp——发音练习面板的非设备逻辑
//（构造态/口音持久化/TTS 降级与 locale 切换/各守卫分支）。设备缠结部分
//（录音启动成功、回放成功、录音/回放回调、对比后半程）在门禁环境（无
// 任何音频设备）不可达，源码里以 GCOVR_EXCL 标注设备排除理由；本测试
// 不作任何音频设备假设，断言一律跟随面板构造期对
// AudioRecorder::hasInputDevice() 的落点与 QTextToSpeech::availableEngines()
// 的实际环境分支（不自行二次探测设备，避免与面板竞态）。
// pronunciation_panel 没有库目标（gui 直接编译），照 test_clipboard_monitor
// 的做法把源码挂进本测试 target；只编译 UNIDICT_GUI_PRON=OFF 形态
//（覆盖门禁树同口径，评分路径属 unidict_pron 域不在此测）。

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QTextToSpeech>
#include <QtTest>

#include <memory>

#include "audio_recorder.h"
#include "pronunciation_panel.h"

class PronunciationPanelTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    // 词条形态：构造态、TTS 示范与 ensureTts 幂等、口音切换持久化、
    // 对比/录音/回放的无样本失败路、超时守卫
    void q10_word_entry_flow();
    // 自由练习（无词条）：标题退化 + 口音行隐藏 + 示范/对比禁用
    void q10_free_practice_degrades();
    // 口音持久化回放：预存 en-US 构造恢复、非法存值回退缺省英音
    void q10_accent_persistence();

private:
    static QPushButton* buttonByText(const QWidget& panel, const QString& text);
    static QLabel* labelByText(const QWidget& panel, const QString& text);
    static bool anyLabelSays(const QWidget& panel, const QString& text);
};

void PronunciationPanelTest::initTestCase() {
    // QSettings 隔离到测试沙箱，不碰用户真实配置
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("unidict-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("pronunciation-panel-test"));
    // 注：无 speechd 守护进程的机器上，首次 say() 会打印一条
    // "Error loading text-to-speech plug-in"（Qt 惰性加载默认引擎失败，
    // QtTest 不因 qCritical 判失败）。本测试所有断言都不依赖引擎行为
    //（守卫分支经 moc 直调覆盖），该杂音无害，无需也无法用环境变量钉引擎
    //（Qt 6.10 的 QTextToSpeech() 默认构造不认 QT_TTS_ENGINE/QT_TTS_PLUGIN）
}

void PronunciationPanelTest::init() {
    QSettings().clear();
}

QPushButton* PronunciationPanelTest::buttonByText(const QWidget& panel,
                                                  const QString& text) {
    for (QPushButton* b : panel.findChildren<QPushButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

QLabel* PronunciationPanelTest::labelByText(const QWidget& panel,
                                            const QString& text) {
    for (QLabel* l : panel.findChildren<QLabel*>()) {
        if (l->text() == text) {
            return l;
        }
    }
    return nullptr;
}

bool PronunciationPanelTest::anyLabelSays(const QWidget& panel,
                                          const QString& text) {
    return labelByText(panel, text) != nullptr;
}

void PronunciationPanelTest::q10_word_entry_flow() {

    // 多态删除路径：面板在真实 UI 里由父级以 QDialog* 持有，析构走派生类
    // 的 D0 deleting 析构（栈对象的 D2 已由本 slot 及其余用例的作用域实例
    // 覆盖）——gcovr 要求该行的所有函数记录都执行过才计为覆盖（Q-7 同款）
    {
        std::unique_ptr<QDialog> polymorphic =
            std::make_unique<PronunciationPanel>(QStringLiteral("hello"));
        QVERIFY(polymorphic->findChild<QComboBox*>() != nullptr);
    }

    PronunciationPanel panel(QStringLiteral("hello"),
                             QStringLiteral("/həˈloʊ/"),
                             QStringLiteral("/həˈloʊ/"));
    QCOMPARE(panel.windowTitle(), QStringLiteral("发音练习"));

    // 口音行：缺省英音（QSettings 无存值）
    QComboBox* combo = panel.findChild<QComboBox*>();
    QVERIFY(combo);
    QVERIFY(combo->isVisibleTo(&panel));
    QCOMPARE(combo->count(), 2);
    QCOMPARE(combo->currentIndex(), 0);
    QCOMPARE(combo->currentData().toString(), QStringLiteral("en-GB"));

    // 按钮顺序即跟读流程；OFF 形态没有评分按钮
    QPushButton* say = buttonByText(panel, QStringLiteral("示范"));
    QPushButton* record = buttonByText(panel, QStringLiteral("开始录音"));
    QPushButton* play = buttonByText(panel, QStringLiteral("回放"));
    QPushButton* compare = buttonByText(panel, QStringLiteral("对比"));
    QVERIFY(say && record && play && compare);
    QVERIFY(buttonByText(panel, QStringLiteral("评分")) == nullptr);
    QVERIFY(say->isEnabled());  // 有词条即可示范

    // 录音入口的可用性与提示语跟随环境——测试不自带第二次设备探测
    // （macOS 26 runner 上连续两次 QMediaDevices::defaultAudioInput()
    // 可能给出不同答案，测试与面板各探一次会竞态），以面板构造期的
    // 落点为唯一事实源。构造期模型：record 的使能即设备探测结果；
    // play/compare 由"尚无录音样本"门控（onRecordingStopped 才打开），
    // 与设备无关，两种环境下都应为禁用
    const bool recordEnabled = record->isEnabled();
    if (recordEnabled) {
        QVERIFY(anyLabelSays(
            panel, QStringLiteral("先听「示范」，再录音跟读，「对比」人耳校准。")));
    } else {
        QVERIFY(anyLabelSays(panel,
                             QStringLiteral("未检测到麦克风输入设备，录音不可用。")));
    }
    QVERIFY(!play->isEnabled());
    QVERIFY(!compare->isEnabled());

    if (!QTextToSpeech::availableEngines().isEmpty()) {
        // 示范：惰性建引擎 + 播报提示；再点一次走 ttsChecked_ 幂等早退
        say->click();
        QVERIFY(anyLabelSays(panel, QStringLiteral("示范播报中…")));
        say->click();
        QVERIFY(anyLabelSays(panel, QStringLiteral("示范播报中…")));
        // onTtsStateChanged 在非对比流程下早退（不接管按钮状态）：
        // 私有槽经 moc invoke 直调，不依赖引擎的 stateChanged 时序
        QVERIFY(QMetaObject::invokeMethod(&panel, "onTtsStateChanged"));

        // 口音切换：持久化写回 + TTS locale 换算（en-US 分支）
        combo->setCurrentIndex(1);
        QCOMPARE(combo->currentData().toString(), QStringLiteral("en-US"));
        QCOMPARE(QSettings().value(QStringLiteral("pron/accent")).toString(),
                 QStringLiteral("en-US"));

        // 对比无样本：早退提示，不进对比流程
        QVERIFY(QMetaObject::invokeMethod(&panel, "playComparison"));
        QVERIFY(anyLabelSays(panel, QStringLiteral("先录一段自己的发音，再对比。")));
    }

    // 录音启动失败路（面板构造期无输入设备；有设备的机器绝不真开录音）
    if (!recordEnabled) {
        QVERIFY(QMetaObject::invokeMethod(&panel, "toggleRecording"));
        QVERIFY(anyLabelSays(
            panel, QStringLiteral("录音启动失败：输入设备不支持 16kHz 采集。")));
    }

    // 回放无样本：PcmPlayback::play 对空样本无条件 false（确定性分支，
    // 与有无输出设备无关），提示"没有可用的音频输出设备"
    play->setEnabled(true);  // 无设备构造期禁用，这里只测失败路，补点再点
    play->click();
    QVERIFY(anyLabelSays(panel,
                         QStringLiteral("回放失败：没有可用的音频输出设备。")));

    // 对比兜底定时器在非对比流程下是空操作：状态停在上一条
    QVERIFY(QMetaObject::invokeMethod(&panel, "onCompareTimeout"));
    QVERIFY(anyLabelSays(panel,
                         QStringLiteral("回放失败：没有可用的音频输出设备。")));
}

void PronunciationPanelTest::q10_free_practice_degrades() {
    PronunciationPanel panel(QStringLiteral(""));
    QCOMPARE(panel.windowTitle(), QStringLiteral("发音练习"));

    // 标题退化为"（自由练习）"（title 是构造的第一个 QLabel）
    QLabel* title = panel.findChild<QLabel*>();
    QVERIFY(title);
    QCOMPARE(title->text(), QStringLiteral("（自由练习）"));

    // 口音行整行隐藏（选择在自由练习下没有意义）
    QComboBox* combo = panel.findChild<QComboBox*>();
    QVERIFY(combo);
    QVERIFY(!combo->isVisibleTo(&panel));
    QLabel* accentLabel = labelByText(panel, QStringLiteral("口音"));
    QVERIFY(accentLabel);
    QVERIFY(!accentLabel->isVisibleTo(&panel));

    // 自由练习没有文本可播/无从对比：示范、对比禁用，示范说明原因
    QPushButton* say = buttonByText(panel, QStringLiteral("示范"));
    QPushButton* compare = buttonByText(panel, QStringLiteral("对比"));
    QVERIFY(say && compare);
    QVERIFY(!say->isEnabled());
    QCOMPARE(say->toolTip(), QStringLiteral("自由练习模式无词条可播报"));
    QVERIFY(!compare->isEnabled());
}

void PronunciationPanelTest::q10_accent_persistence() {
    // 预存美音：构造时恢复（connect 之前设，不触发持久化写回）
    QSettings().setValue(QStringLiteral("pron/accent"), QStringLiteral("en-US"));
    {
        PronunciationPanel panel(QStringLiteral("hello"));
        QComboBox* combo = panel.findChild<QComboBox*>();
        QVERIFY(combo);
        QCOMPARE(combo->currentIndex(), 1);
        QCOMPARE(combo->currentData().toString(), QStringLiteral("en-US"));
        QCOMPARE(QSettings().value(QStringLiteral("pron/accent")).toString(),
                 QStringLiteral("en-US"));
    }

    // 非法存值：findData 落空 → 维持缺省英音
    QSettings().setValue(QStringLiteral("pron/accent"), QStringLiteral("fr-FR"));
    {
        PronunciationPanel panel(QStringLiteral("hello"));
        QComboBox* combo = panel.findChild<QComboBox*>();
        QVERIFY(combo);
        QCOMPARE(combo->currentIndex(), 0);
        QCOMPARE(combo->currentData().toString(), QStringLiteral("en-GB"));
        // 用户手选美音：此刻 connect 已生效 → onAccentChanged 持久化写回
        combo->setCurrentIndex(1);
        QCOMPARE(QSettings().value(QStringLiteral("pron/accent")).toString(),
                 QStringLiteral("en-US"));
    }
}

QTEST_MAIN(PronunciationPanelTest)
#include "pronunciation_panel_test.moc"
