#pragma once

// 发音练习面板（M1 录音基建 + M2 跟读循环）——TTS 示范、跟读录音、
// 波形、回放、对比（示范→录音依次播）。评分在 M3 接入；分层纪律见
// docs/pronunciation-plan.md：TTS/录音/回放全是平台壳，不进任何测试。
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <memory>
#include <vector>

#include "audio_recorder.h"
#include "pcm_playback.h"
#include "waveform_widget.h"

class QTextToSpeech;

class PronunciationPanel : public QDialog {
    Q_OBJECT
public:
    // word 为空时标题退化为“自由练习”（此时 TTS 示范/对比禁用——没内容可说）
    explicit PronunciationPanel(const QString& word, QWidget* parent = nullptr);
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

private:
    // 惰性构造 QTextToSpeech：引擎插件缺失（CI offscreen/精简部署）时
    // 保持空并把示范/对比按钮禁用——构造一次的成本也不该让面板打开变慢
    void ensureTts();
    void finishComparison();
    void setStatus(const QString& text);

    QString word_;
    AudioRecorder recorder_;
    PcmPlayback playback_;
    WaveformWidget* wave_ = nullptr;
    QPushButton* recordButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* sayButton_ = nullptr;
    QPushButton* compareButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    std::unique_ptr<QTextToSpeech> tts_;
    bool ttsChecked_ = false;
    bool comparePending_ = false;  // 对比流程中：等示范播完接录音回放
    QTimer compareFallback_;
};
