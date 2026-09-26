#include "pronunciation_panel.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLocale>
#include <QSettings>
#include <QTextToSpeech>
#include <QVBoxLayout>

#ifdef UNIDICT_GUI_PRON
#include <QDir>
#include <QtConcurrent/QtConcurrentRun>

#include "onnx_pron_scorer.h"
#include "std/ctc_gop_std.h"
#include "std/ipa_to_arpabet_std.h"
#endif

namespace {
// 实时波形只画最近 1 秒的尾窗（16k 样本），更早的等停止后全量铺开
constexpr qint64 kLiveWindowSamples = PronAudio::kSampleRate;
constexpr int kWaveColumns = 120;
// 对比流程等示范播完的兜底：引擎哑火（state 永不变化）时不吊死按钮
constexpr int kCompareFallbackMs = 5000;
// 短于这个时长的录音不进评分：连一个音素都切不出来，分数没有意义
constexpr qint64 kMinScoreSamples = PronAudio::kSampleRate / 2;

// M5：口音数据 ↔ TTS locale（引擎没有对应语音时 Qt 自行回落默认 voice）
QLocale locale_for_accent(const QString& accent) {
    if (accent == QLatin1String("en-US")) {
        return QLocale(QLocale::English, QLocale::UnitedStates);
    }
    return QLocale(QLocale::English, QLocale::UnitedKingdom);
}

#ifdef UNIDICT_GUI_PRON
// 评分结果展示：词分 + 逐音素 GOP + 最弱音素点名（M4 差异高亮的
// 最小可用形态——不撒糖，指出该练哪）。纯格式化，无状态。
QString format_score(const UnidictCoreStd::WordGopResult& r) {
    const UnidictCoreStd::PhoneGopResult* weakest = nullptr;
    QString line = QStringLiteral("词分 %1：").arg(QString::number(r.word_score, 'f', 2));
    for (const auto& p : r.phones) {
        line += QStringLiteral(" %1 %2 ·")
                    .arg(QString::fromStdString(p.arpabet),
                         QString::number(p.score, 'f', 2));
        if (!weakest || p.score < weakest->score) {
            weakest = &p;
        }
    }
    if (!r.phones.empty()) {
        line.chop(2);  // 去掉尾分隔符 " ·"
    }
    if (weakest) {
        line += QStringLiteral("\n最弱：%1（%2）——对着示范多跟几遍。")
                    .arg(QString::fromStdString(weakest->arpabet),
                         QString::number(weakest->score, 'f', 2));
    }
    return line;
}
#endif
}  // namespace

PronunciationPanel::~PronunciationPanel() = default;

PronunciationPanel::PronunciationPanel(const QString& word,
                                       const QString& phoneticsBrE,
                                       const QString& phoneticsAmE,
                                       QWidget* parent)
    : QDialog(parent),
      word_(word),
      phoneticsBrE_(phoneticsBrE),
      phoneticsAmE_(phoneticsAmE) {
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

    // M5 口音选择：示范层切 TTS locale，评分层切英/美音标字段（词典
    // "英先美后"惯例由 core/std extract_phonetic_variants 提取）。选择
    // 持久化在 QSettings；自由练习（无词条）下口音没有意义，整行隐藏
    accentLabel_ = new QLabel(QStringLiteral("口音"), this);
    accentCombo_ = new QComboBox(this);
    accentCombo_->addItem(QStringLiteral("英音 (BrE)"), QStringLiteral("en-GB"));
    accentCombo_->addItem(QStringLiteral("美音 (AmE)"), QStringLiteral("en-US"));
    accentCombo_->setToolTip(
        QStringLiteral("示范口音按引擎可用语音近似；评分按所选口音取词典"
                       "英/美音标字段（单字段词典不区分口音）"));
    const QString savedAccent = QSettings()
                                    .value(QStringLiteral("pron/accent"),
                                           QStringLiteral("en-GB"))
                                    .toString();
    const int accentIdx = accentCombo_->findData(savedAccent);
    if (accentIdx >= 0) {
        accentCombo_->setCurrentIndex(accentIdx);  // connect 之前设，不触发槽
    }
    auto* accentRow = new QHBoxLayout;
    accentRow->addWidget(accentLabel_);
    accentRow->addWidget(accentCombo_);
    accentRow->addStretch();
    layout->addLayout(accentRow);
    if (word_.isEmpty()) {
        accentLabel_->hide();
        accentCombo_->hide();
    }

    wave_ = new WaveformWidget(this);
    wave_->setMinimumHeight(120);
    layout->addWidget(wave_, /*stretch=*/1);

    auto* buttons = new QHBoxLayout;
    // 按钮顺序即跟读流程：示范 → 录音 → 回放 → 对比（→ 评分）
    sayButton_ = new QPushButton(QStringLiteral("示范"), this);
    sayButton_->setToolTip(QStringLiteral("TTS 播报当前词条"));
    recordButton_ = new QPushButton(QStringLiteral("开始录音"), this);
    recordButton_->setToolTip(QStringLiteral("最长 30 秒，再次点击结束"));
    playButton_ = new QPushButton(QStringLiteral("回放"), this);
    playButton_->setEnabled(false);
    compareButton_ = new QPushButton(QStringLiteral("对比"), this);
    compareButton_->setToolTip(QStringLiteral("示范播完自动接你的录音，人耳对比"));
    compareButton_->setEnabled(false);
#ifdef UNIDICT_GUI_PRON
    scoreButton_ = new QPushButton(QStringLiteral("评分"), this);
    scoreButton_->setEnabled(false);
#endif
    buttons->addWidget(sayButton_);
    buttons->addWidget(recordButton_);
    buttons->addWidget(playButton_);
    buttons->addWidget(compareButton_);
#ifdef UNIDICT_GUI_PRON
    buttons->addWidget(scoreButton_);
#endif
    buttons->addStretch();
    layout->addLayout(buttons);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

#ifdef UNIDICT_GUI_PRON
    // 评分目标音素随口音换算：解析失败（空/非英语 IPA）就静默退回
    // M2 跟读，按钮 tooltip 说明原因
    scoreLabel_ = new QLabel(this);
    scoreLabel_->setWordWrap(true);
    scoreLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont mono = scoreLabel_->font();
    mono.setStyleHint(QFont::TypeWriter);
    scoreLabel_->setFont(mono);
    scoreLabel_->hide();
    layout->addWidget(scoreLabel_);

    connect(&scoreWatcher_, &QFutureWatcher<ScoreOutcome>::finished, this,
            &PronunciationPanel::onScoreFinished);
    connect(scoreButton_, &QPushButton::clicked, this,
            &PronunciationPanel::scoreRecording);

    retargetScoring();  // 按初始口音换算目标音素并设置按钮态
#endif

    if (!AudioRecorder::hasInputDevice()) {
        recordButton_->setEnabled(false);
        playButton_->setEnabled(false);
        compareButton_->setEnabled(false);
        setStatus(QStringLiteral("未检测到麦克风输入设备，录音不可用。"));
    } else {
        setStatus(QStringLiteral("先听「示范」，再录音跟读，「对比」人耳校准。"));
    }
#ifdef UNIDICT_GUI_PRON
    if (!targetPhones_.empty() && AudioRecorder::hasInputDevice()) {
        setStatus(QStringLiteral("先听「示范」，再录音跟读；「评分」逐音素对照。"));
    }
#endif
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
    connect(accentCombo_, &QComboBox::currentIndexChanged, this,
            &PronunciationPanel::onAccentChanged);
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
    applyTtsLocale();  // 创建时就把持久化的口音落到引擎
}

void PronunciationPanel::applyTtsLocale() {
    if (tts_ && accentCombo_) {
        tts_->setLocale(locale_for_accent(accentCombo_->currentData().toString()));
    }
}

void PronunciationPanel::onAccentChanged() {
    QSettings().setValue(QStringLiteral("pron/accent"),
                         accentCombo_->currentData().toString());
    applyTtsLocale();
#ifdef UNIDICT_GUI_PRON
    scoreLabel_->hide();  // 口音换了：上一份评分基于旧参考，不再适用
    retargetScoring();
#endif
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
#ifdef UNIDICT_GUI_PRON
        refreshScoreButton();  // 上一段的评分结果对新录音失效，先禁用
#endif
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
#ifdef UNIDICT_GUI_PRON
    refreshScoreButton();
#endif
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

#ifdef UNIDICT_GUI_PRON
void PronunciationPanel::refreshScoreButton() {
    scoreButton_->setEnabled(!scoringDead_ && !targetPhones_.empty() &&
                             !recorder_.isRecording() &&
                             recorder_.sampleCount() >= kMinScoreSamples &&
                             !scoreWatcher_.isRunning());
}

void PronunciationPanel::retargetScoring() {
    const bool amE = accentCombo_->currentData().toString() == QLatin1String("en-US");
    const QString& chosen = amE ? phoneticsAmE_ : phoneticsBrE_;
    const QString& other = amE ? phoneticsBrE_ : phoneticsAmE_;
    const QString accentName = amE ? QStringLiteral("美") : QStringLiteral("英");

    // 选中口音的字段缺失/解析失败时退用另一字段：单字段词典或不分
    // 英美的拼写里，切口音不该把评分整个关掉
    targetPhones_.clear();
    const QString* effective = nullptr;
    const auto parse = [this](const QString& text) {
        if (text.isEmpty()) {
            return false;
        }
        auto phones = UnidictCoreStd::phonetic_text_to_arpabet(text.toStdString());
        if (!phones) {
            return false;
        }
        targetPhones_ = std::move(*phones);
        return true;
    };
    if (parse(chosen)) {
        effective = &chosen;
    } else if (parse(other)) {
        effective = &other;
    }

    if (targetPhones_.empty()) {
        scoreButton_->setToolTip(
            word_.isEmpty() ? QStringLiteral("自由练习无词条音标，不可评分")
                            : QStringLiteral("词条音标不是可识别的英语 IPA/ARPAbet"));
    } else {
        // tooltip 带上实际生效的字段原文：跨口音回退/提取结果对用户可见
        scoreButton_->setToolTip(
            QStringLiteral("按%1音标评分（GOP，需录音 ≥0.5 秒）：%2")
                .arg(accentName, *effective));
    }
    refreshScoreButton();
}

void PronunciationPanel::scoreRecording() {
    if (scoreWatcher_.isRunning() || scoringDead_ || targetPhones_.empty()) {
        return;
    }
    if (recorder_.sampleCount() < kMinScoreSamples) {
        setStatus(QStringLiteral("录音太短（至少 0.5 秒），先跟读一段。"));
        return;
    }
    // 模型路径：环境变量可覆盖；默认数据目录约定（模型资产不进 git，
    // 与 CLI --pron-model 指向同一份资产）。路径在 UI 线程解析好，
    // 后台任务只做加载与推理。
    const QDir modelDir = QDir(QDir::home().filePath(
        QStringLiteral(".cache/unidict-models/wav2vec2-espeak-ctc")));
    UnidictPron::PronScorerOnnx::Config cfg;
    const QString model = qEnvironmentVariable("UNIDICT_PRON_MODEL");
    const QString vocab = qEnvironmentVariable("UNIDICT_PRON_VOCAB");
    cfg.model_path = (model.isEmpty() ? modelDir.filePath(QStringLiteral("model.onnx"))
                                      : model)
                         .toStdString();
    cfg.vocab_path = (vocab.isEmpty() ? modelDir.filePath(QStringLiteral("vocab.json"))
                                      : vocab)
                         .toStdString();

    // 值拷贝进后台任务：评分期间面板可能随时被关掉，lambda 不得碰 this
    std::vector<int16_t> pcm = recorder_.samples();
    std::vector<std::string> phones = targetPhones_;
    std::shared_ptr<UnidictPron::PronScorerOnnx> scorer = scorer_;
    scoreButton_->setEnabled(false);
    setStatus(QStringLiteral("评分中…（首次需加载模型，稍等）"));
    scoreWatcher_.setFuture(QtConcurrent::run(
        [pcm = std::move(pcm), phones = std::move(phones),
         scorer = std::move(scorer), cfg]() mutable -> ScoreOutcome {
            ScoreOutcome out;
            std::string err;
            if (!scorer) {
                // mutable：首次点击在任务里惰性加载，unique_ptr 直接
                // 移交进本次任务的 shared_ptr 拷贝
                scorer = UnidictPron::PronScorerOnnx::load(cfg, err);
                if (!scorer) {
                    out.fatal = true;
                    out.text = QStringLiteral("评分模型加载失败（%1）。已保持跟读模式，"
                                              "可用 UNIDICT_PRON_MODEL/UNIDICT_PRON_VOCAB "
                                              "指定模型路径。")
                                   .arg(QString::fromStdString(err));
                    return out;
                }
            }
            auto result = scorer->score(pcm, phones, err);
            if (!result) {
                out.text = QStringLiteral("评分失败：%1").arg(
                    QString::fromStdString(err));
                return out;
            }
            out.scorer = scorer;  // 回传 UI 线程复用，下次点击不重载模型
            out.ok = true;
            out.text = format_score(*result);
            return out;
        }));
}

void PronunciationPanel::onScoreFinished() {
    const ScoreOutcome out = scoreWatcher_.result();
    if (out.ok) {
        scorer_ = out.scorer;
        scoreLabel_->setText(out.text);
        scoreLabel_->show();
        setStatus(QStringLiteral("评分完成。分低的音素就是该练的地方。"));
    } else {
        if (out.fatal) {
            scoringDead_ = true;  // 模型缺失不反复试：回退 M2 无评分跟读
            scoreButton_->setToolTip(out.text);
        }
        setStatus(out.text);
    }
    refreshScoreButton();
}
#endif
