#pragma once

// PCM 回放薄壳（发音练习 M1）：QAudioSink push 模式，写完整段后由
// stateChanged(IdleState) 判定播完。仅薄壳不进测试，理由同 audio_recorder。
#include <QObject>
#include <QAudioSink>
#include <vector>

#include "pcm_util.h"

class QIODevice;

class PcmPlayback : public QObject {
    Q_OBJECT
public:
    explicit PcmPlayback(QObject* parent = nullptr);

    bool isPlaying() const;

public slots:
    // 空数据/无输出设备返回 false；重复调用会先停掉上一次
    bool play(const std::vector<int16_t>& samples);
    void stop();

signals:
    // 播完或出错收场（外部 stop() 不触发）
    void finished();

private slots:
    void onStateChanged();

private:
    void cleanup();

    QAudioSink* sink_ = nullptr;
    QIODevice* io_ = nullptr;
};
