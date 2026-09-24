#pragma once

// 麦克风采集薄壳（发音练习 M1）：QAudioSource pull 模式 + 定时器搬运，
// 锁死 16k/16bit/单声道。仅薄壳，不做测试——CI offscreen 无音频设备，
// 可测的纯逻辑都在 pcm_util.h（docs/pronunciation-plan.md 分层纪律）。
#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <vector>

#include "pcm_util.h"

class QAudioSource;
class QIODevice;

class AudioRecorder : public QObject {
    Q_OBJECT
public:
    explicit AudioRecorder(QObject* parent = nullptr);

    // 无默认输入设备（CI offscreen / 台式机没插麦）时 UI 应禁用录音入口
    static bool hasInputDevice();

    bool isRecording() const { return source_ != nullptr; }
    // 停止后的全量样本；录音中读取亦可（实时波形取尾窗用）
    const std::vector<int16_t>& samples() const { return samples_; }
    qint64 sampleCount() const { return static_cast<qint64>(samples_.size()); }

public slots:
    // 设备缺失/格式不被支持时返回 false，不抛异常
    bool start();
    void stop();

signals:
    // 录音中有新数据落账（实时波形/电平刷新时机）
    void samplesAppended();
    void recordingStopped();

private slots:
    void drainInput();

private:
    void appendPcm(const QByteArray& bytes);

    QAudioSource* source_ = nullptr;
    QIODevice* io_ = nullptr;
    QTimer* pullTimer_ = nullptr;
    std::vector<int16_t> samples_;
};
