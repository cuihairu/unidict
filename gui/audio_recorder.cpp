#include "audio_recorder.h"

#include <QAudioDevice>
#include <QAudioSource>
#include <QIODevice>
#include <QMediaDevices>

namespace {

QAudioFormat pcmFormat() {
    QAudioFormat format;
    format.setSampleRate(PronAudio::kSampleRate);
    format.setChannelCount(PronAudio::kChannels);
    format.setSampleFormat(QAudioFormat::Int16);
    return format;
}

}  // namespace

AudioRecorder::AudioRecorder(QObject* parent) : QObject(parent) {
    // 40ms 拉一次 ≈ 640 样本/次，够实时波形顺滑，又不至于太密
    pullTimer_ = new QTimer(this);
    pullTimer_->setInterval(40);
    connect(pullTimer_, &QTimer::timeout, this, &AudioRecorder::drainInput);
}

bool AudioRecorder::hasInputDevice() {
    return !QMediaDevices::defaultAudioInput().isNull();
}

bool AudioRecorder::start() {
    if (source_) {
        return false;  // 已在录
    }
    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    const QAudioFormat format = pcmFormat();
    if (device.isNull() || !device.isFormatSupported(format)) {
        return false;
    }
    samples_.clear();
    source_ = new QAudioSource(device, format, this);
    io_ = source_->start();  // pull 模式：内部缓冲，定时器来读
    if (!io_) {
        source_->deleteLater();
        source_ = nullptr;
        return false;
    }
    pullTimer_->start();
    return true;
}

void AudioRecorder::stop() {
    if (!source_) {
        return;
    }
    pullTimer_->stop();
    drainInput();  // 停设备前把残余缓冲收干净
    source_->stop();
    io_ = nullptr;
    source_->deleteLater();
    source_ = nullptr;
    emit recordingStopped();
}

void AudioRecorder::drainInput() {
    if (!io_) {
        return;
    }
    while (true) {
        const qint64 avail = io_->bytesAvailable();
        if (avail <= 0) {
            break;
        }
        const QByteArray chunk = io_->read(avail);
        if (chunk.isEmpty()) {
            break;
        }
        appendPcm(chunk);
    }
    if (samples_.size() >= PronAudio::kMaxSamples) {
        // 上限熔断：忘点停止不至于吃穿内存
        stop();
        return;
    }
    emit samplesAppended();
}

void AudioRecorder::appendPcm(const QByteArray& bytes) {
    // x86/ARM 均为 little-endian，int16 直接转译；奇数字节（不应出现）自然丢弃
    const auto* p = reinterpret_cast<const int16_t*>(bytes.constData());
    const size_t n = static_cast<size_t>(bytes.size()) / sizeof(int16_t);
    samples_.insert(samples_.end(), p, p + n);
}
