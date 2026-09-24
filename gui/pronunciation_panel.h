#pragma once

// 发音练习面板（发音练习 M1：录音基建）——录音、波形、回放。
// 跟读循环与评分在 M2/M3 接入；分层纪律见 docs/pronunciation-plan.md。
#include <QDialog>
#include <QLabel>
#include <QPushButton>

#include "audio_recorder.h"
#include "pcm_playback.h"
#include "waveform_widget.h"

class PronunciationPanel : public QDialog {
    Q_OBJECT
public:
    // word 为空时标题退化为“自由练习”（M1 不强制先查词）
    explicit PronunciationPanel(const QString& word, QWidget* parent = nullptr);

private slots:
    void toggleRecording();
    void refreshLiveWave();
    void onRecordingStopped();
    void onPlaybackFinished();

private:
    void setStatus(const QString& text);

    QString word_;
    AudioRecorder recorder_;
    PcmPlayback playback_;
    WaveformWidget* wave_ = nullptr;
    QPushButton* recordButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};
