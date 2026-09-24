#include "pronunciation_panel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {
// 实时波形只画最近 1 秒的尾窗（16k 样本），更早的等停止后全量铺开
constexpr qint64 kLiveWindowSamples = PronAudio::kSampleRate;
constexpr int kWaveColumns = 120;
}  // namespace

PronunciationPanel::PronunciationPanel(const QString& word, QWidget* parent)
    : QDialog(parent), word_(word) {
    setWindowTitle(QStringLiteral("发音练习"));
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* title = new QLabel(word_.isEmpty() ? QStringLiteral("（自由练习）") : word_, this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.6);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setWordWrap(true);
    layout->addWidget(title);

    wave_ = new WaveformWidget(this);
    wave_->setMinimumHeight(120);
    layout->addWidget(wave_, /*stretch=*/1);

    auto* buttons = new QHBoxLayout;
    recordButton_ = new QPushButton(QStringLiteral("开始录音"), this);
    recordButton_->setToolTip(QStringLiteral("最长 30 秒，再次点击结束"));
    playButton_ = new QPushButton(QStringLiteral("回放"), this);
    playButton_->setEnabled(false);
    buttons->addWidget(recordButton_);
    buttons->addWidget(playButton_);
    buttons->addStretch();
    layout->addLayout(buttons);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    if (!AudioRecorder::hasInputDevice()) {
        recordButton_->setEnabled(false);
        playButton_->setEnabled(false);
        setStatus(QStringLiteral("未检测到麦克风输入设备，录音不可用。"));
    } else {
        setStatus(QStringLiteral("点击「开始录音」跟读上方词条，录完可回放对比。"));
    }

    connect(recordButton_, &QPushButton::clicked, this,
            &PronunciationPanel::toggleRecording);
    connect(&recorder_, &AudioRecorder::samplesAppended, this,
            &PronunciationPanel::refreshLiveWave);
    connect(&recorder_, &AudioRecorder::recordingStopped, this,
            &PronunciationPanel::onRecordingStopped);
    connect(playButton_, &QPushButton::clicked, this, [this] {
        if (playback_.play(recorder_.samples())) {
            playButton_->setEnabled(false);
            setStatus(QStringLiteral("回放中…"));
        } else {
            setStatus(QStringLiteral("回放失败：没有可用的音频输出设备。"));
        }
    });
    connect(&playback_, &PcmPlayback::finished, this,
            &PronunciationPanel::onPlaybackFinished);
}

void PronunciationPanel::toggleRecording() {
    if (!recorder_.isRecording()) {
        wave_->setWave({});
        playButton_->setEnabled(false);
        if (recorder_.start()) {
            recordButton_->setText(QStringLiteral("停止录音"));
            setStatus(QStringLiteral("录音中…（最长 30 秒）"));
        } else {
            setStatus(QStringLiteral("录音启动失败：输入设备不支持 16kHz 采集。"));
        }
        return;
    }
    recorder_.stop();
}

void PronunciationPanel::refreshLiveWave() {
    const auto& samples = recorder_.samples();
    const size_t begin = samples.size() > static_cast<size_t>(kLiveWindowSamples)
                             ? samples.size() - static_cast<size_t>(kLiveWindowSamples)
                             : 0;
    const std::vector<int16_t> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                                      samples.end());
    wave_->setWave(PronAudio::downsample_wave(window, kWaveColumns));
}

void PronunciationPanel::onRecordingStopped() {
    recordButton_->setText(QStringLiteral("开始录音"));
    wave_->setWave(PronAudio::downsample_wave(recorder_.samples(), kWaveColumns));
    playButton_->setEnabled(!recorder_.samples().empty());
    setStatus(QStringLiteral("已录 %1 秒，可回放对比。")
                  .arg(QString::number(recorder_.sampleCount() / PronAudio::kSampleRate,
                                       'f', 1)));
}

void PronunciationPanel::onPlaybackFinished() {
    playButton_->setEnabled(!recorder_.samples().empty());
    setStatus(QStringLiteral("回放完毕。"));
}

void PronunciationPanel::setStatus(const QString& text) {
    statusLabel_->setText(text);
}
