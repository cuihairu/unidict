#include "pcm_playback.h"

#include <QAudioDevice>
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

PcmPlayback::PcmPlayback(QObject* parent) : QObject(parent) {}

bool PcmPlayback::isPlaying() const {
    return sink_ != nullptr && sink_->state() == QAudio::ActiveState;
}

bool PcmPlayback::play(const std::vector<int16_t>& samples) {
    stop();
    if (samples.empty()) {
        return false;
    }
    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    const QAudioFormat format = pcmFormat();
    if (device.isNull() || !device.isFormatSupported(format)) {
        return false;
    }
    sink_ = new QAudioSink(device, format, this);
    connect(sink_, &QAudioSink::stateChanged, this, &PcmPlayback::onStateChanged);
    io_ = sink_->start();  // push 模式：write 完整段，Qt 内部消费
    if (!io_) {
        sink_->deleteLater();
        sink_ = nullptr;
        return false;
    }
    const QByteArray bytes(reinterpret_cast<const char*>(samples.data()),
                           static_cast<qsizetype>(samples.size() * sizeof(int16_t)));
    io_->write(bytes);
    return true;
}

void PcmPlayback::stop() {
    if (!sink_) {
        return;
    }
    disconnect(sink_, &QAudioSink::stateChanged, this, &PcmPlayback::onStateChanged);
    sink_->stop();
    cleanup();
}

void PcmPlayback::onStateChanged() {
    if (!sink_) {
        return;
    }
    if (sink_->state() == QAudio::IdleState) {
        // push 模式缓冲耗尽进入 IdleState ＝ 播完
        cleanup();
        emit finished();
    } else if (sink_->state() == QAudio::StoppedState &&
               sink_->error() != QAudio::NoError) {
        cleanup();
        emit finished();
    }
}

void PcmPlayback::cleanup() {
    if (sink_) {
        sink_->deleteLater();
        sink_ = nullptr;
    }
    io_ = nullptr;
}
