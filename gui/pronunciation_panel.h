#pragma once

// 发音练习面板（M1 录音基建 + M2 跟读循环 + M4 评分接线）——TTS 示范、
// 跟读录音、波形、回放、对比（示范→录音依次播）、逐音素 GOP 评分。
// 分层纪律见 docs/pronunciation-plan.md：TTS/录音/回放/推理全是平台壳，
// 不进任何测试；评分路径只在 UNIDICT_GUI_PRON（UNIDICT_BUILD_PRON）
// 构建下存在，OFF 时面板退回 M2 无评分跟读。
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <memory>
#include <string>
#include <vector>

#include "audio_recorder.h"
#include "pcm_playback.h"
#include "waveform_widget.h"

#ifdef UNIDICT_GUI_PRON
#include <QFutureWatcher>

namespace UnidictPron {
class PronScorerOnnx;
}
#endif

class QComboBox;
class QTextToSpeech;

class PronunciationPanel : public QDialog {
    Q_OBJECT
public:
    // word 为空时标题退化为“自由练习”（此时 TTS 示范/对比禁用——没内容可说）；
    // phoneticsBrE/AmE 是词条英/美音标（IPA/ARPAbet 文本，词典惯例英先
    // 美后，由 core/std extract_phonetic_variants 提取）。评分参考随口音
    // 选择切换：选中口音的字段缺失时退用另一字段，两者都空则评分不可用
    explicit PronunciationPanel(const QString& word,
                                const QString& phoneticsBrE = {},
                                const QString& phoneticsAmE = {},
                                QWidget* parent = nullptr);
    // 析构必须在 .cpp（QTextToSpeech 完整处）实例化：unique_ptr 成员
    // 的隐式析构在调用方展开会报 incomplete type
    ~PronunciationPanel() override;

private slots:
    void toggleRecording();
    void refreshLiveWave();
    void onRecordingStopped();
    void onPlaybackFinished();
    // TTS 播报词条（示范）；对比流程共用
    void speakExample();
    // 对比 = 示范播完 → 自动接用户录音回放；TTS 哑火 5 秒兜底直进录音
    void playComparison();
    void onTtsStateChanged();
    void onCompareTimeout();
    // M5：口音切换 = 示范 TTS 换 locale + 评分换参考字段；选择持久化
    void onAccentChanged();
#ifdef UNIDICT_GUI_PRON
    // M4 评分：对刚录的音频逐音素 GOP 评分（QtConcurrent 后台跑，
    // 模型加载/推理不冻 UI）
    void scoreRecording();
    void onScoreFinished();
#endif

private:
    // 惰性构造 QTextToSpeech：引擎插件缺失（CI offscreen/精简部署）时
    // 保持空并把示范/对比按钮禁用——构造一次的成本也不该让面板打开变慢
    void ensureTts();
    void finishComparison();
    void setStatus(const QString& text);
    // 把当前口音应用到 TTS（引擎没有对应语音时 Qt 自行回落默认 voice）
    void applyTtsLocale();
#ifdef UNIDICT_GUI_PRON
    // 录音条件（有完整一段录音、不在录音/评分中、音标可解析、模型
    // 未判死）齐备时启用评分按钮
    void refreshScoreButton();
    // 按当前口音重算评分目标音素（选中字段缺失时退用另一字段），
    // 并同步评分按钮的可用状态与 tooltip
    void retargetScoring();
#endif

    QString word_;
    QString phoneticsBrE_;  // 词典英音字段（原文，换算评分目标时再解析）
    QString phoneticsAmE_;  // 美音字段；单字段词典为空
    AudioRecorder recorder_;
    PcmPlayback playback_;
    WaveformWidget* wave_ = nullptr;
    QPushButton* recordButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* sayButton_ = nullptr;
    QPushButton* compareButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QComboBox* accentCombo_ = nullptr;
    QLabel* accentLabel_ = nullptr;
    std::unique_ptr<QTextToSpeech> tts_;
    bool ttsChecked_ = false;
    bool comparePending_ = false;  // 对比流程中：等示范播完接录音回放
    QTimer compareFallback_;
#ifdef UNIDICT_GUI_PRON
    // 评分后台任务的返回：成功带结果文本与可复用的 scorer（下次点击
    // 不重载模型）；fatal=模型缺失/加载失败，评分永久禁用（回退 M2）
    struct ScoreOutcome {
        bool ok = false;
        bool fatal = false;
        QString text;
        std::shared_ptr<UnidictPron::PronScorerOnnx> scorer;
    };
    std::vector<std::string> targetPhones_;  // 非空 = 音标可换算 ARPAbet
    bool scoringDead_ = false;               // 模型加载失败后不再尝试
    QPushButton* scoreButton_ = nullptr;
    QLabel* scoreLabel_ = nullptr;
    std::shared_ptr<UnidictPron::PronScorerOnnx> scorer_;
    QFutureWatcher<ScoreOutcome> scoreWatcher_;
#endif
};
