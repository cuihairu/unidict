#include "pronunciation_panel.h"

#include <QFont>
#include <QHBoxLayout>
#include <QTextToSpeech>
#include <QVBoxLayout>

namespace {
// 实时波形只画最近 1 秒的尾窗（16k 样本），更早的等停止后全量铺开
constexpr qint64 kLiveWindowSamples = PronAudio::kSampleRate;
constexpr int kWaveColumns = 120;
// 对比流程等示范播完的兜底：引擎哑火（state 永不变化）时不吊死按钮
constexpr int kCompareFallbackMs = 5000;
}  // namespace

PronunciationPanel::~PronunciationPanel() = default;

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
    // 按钮顺序即跟读流程：示范 → 录音 → 回放 → 对比
    sayButton_ = new QPushButton(QStringLiteral("示范"), this);
    sayButton_->setToolTip(QStringLiteral("TTS 播报当前词条"));
    recordButton_ = new QPushButton(QStringLiteral("开始录音"), this);
    recordButton_->setToolTip(QStringLiteral("最长 30 秒，再次点击结束"));
    playButton_ = new QPushButton(QStringLiteral("回放"), this);
    playButton_->setEnabled(false);
    compareButton_ = new QPushButton(QStringLiteral("对比"), this);
    compareButton_->setToolTip(QStringLiteral("示范播完自动接你的录音，人耳对比"));
    compareButton_->setEnabled(false);
    buttons->addWidget(sayButton_);
    buttons->addWidget(recordButton_);
    buttons->addWidget(playButton_);
    buttons->addWidget(compareButton_);
    buttons->addStretch();
    layout->addLayout(buttons);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    if (!AudioRecorder::hasInputDevice()) {
        recordButton_->setEnabled(false);
        playButton_->setEnabled(false);
        compareButton_->setEnabled(false);
        setStatus(QStringLiteral("未检测到麦克风输入设备，录音不可用。"));
    } else {
        setStatus(QStringLiteral("先听「示范」，再录音跟读，「对比」人耳校准。"));
    }
    if (word_.isEmpty()) {
        // 自由练习没有文本可播
        sayButton_->setEnabled(false);
        compareButton_->setEnabled(false);
        sayButton_->setToolTip(QStringLiteral("自由练习模式无词条可播报"));
    }

    compareFallback_.setSingleShot(true);
    compareFallback_.setInterval(kCompareFallbackMs);
    connect(&compareFallback_, &QTimer::timeout, this,
            &PronunciationPanel::onCompareTimeout);

    connect(sayButton_, &QPushButton::clicked, this,
            &PronunciationPanel::speakExample);
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
    connect(compareButton_, &QPushButton::clicked, this,
            &PronunciationPanel::playComparison);
    connect(&playback_, &PcmPlayback::finished, this,
            &PronunciationPanel::onPlaybackFinished);
}

void PronunciationPanel::ensureTts() {
    if (ttsChecked_) {
        return;
    }
    ttsChecked_ = true;
    if (QTextToSpeech::availableEngines().isEmpty()) {
        sayButton_->setEnabled(false);
        compareButton_->setEnabled(false);
        sayButton_->setToolTip(QStringLiteral("未安装 TTS 语音引擎"));
        return;
    }
    tts_ = std::make_unique<QTextToSpeech>(this);
    connect(tts_.get(), &QTextToSpeech::stateChanged, this,
            &PronunciationPanel::onTtsStateChanged);
}

void PronunciationPanel::speakExample() {
    ensureTts();
    if (!tts_) {
        return;
    }
    tts_->say(word_);
    setStatus(QStringLiteral("示范播报中…"));
}

void PronunciationPanel::playComparison() {
    ensureTts();
    if (!tts_) {
        return;
    }
    if (recorder_.samples().empty()) {
        setStatus(QStringLiteral("先录一段自己的发音，再对比。"));
        return;
    }
    comparePending_ = true;
    sayButton_->setEnabled(false);
    compareButton_->setEnabled(false);
    compareFallback_.start();
    tts_->say(word_);
    setStatus(QStringLiteral("对比：示范 → 你的录音"));
}

void PronunciationPanel::onTtsStateChanged() {
    if (!tts_ || !comparePending_) {
        return;  // 普通示范不接管按钮状态
    }
    // 示范还在播（Speaking）继续等；到 Ready/Error 即接录音
    if (tts_->state() == QTextToSpeech::Speaking ||
        tts_->state() == QTextToSpeech::Paused) {
        return;
    }
    finishComparison();
}

void PronunciationPanel::onCompareTimeout() {
    if (comparePending_) {
        finishComparison();
    }
}

void PronunciationPanel::finishComparison() {
    comparePending_ = false;
    compareFallback_.stop();
    sayButton_->setEnabled(true);
    if (!playback_.play(recorder_.samples())) {
        setStatus(QStringLiteral("回放失败：没有可用的音频输出设备。"));
    }
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
    compareButton_->setEnabled(!recorder_.samples().empty());
    setStatus(QStringLiteral("已录 %1 秒，可回放或对比。")
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
