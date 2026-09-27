#include "pronunciation_panel.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLocale>
#include <QSettings>
#include <QTextToSpeech>
#include <QVBoxLayout>

#ifdef UNIDICT_GUI_PRON
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>

#include "data_store.h"
#include "model_downloader.h"
#include "onnx_pron_scorer.h"
#include "std/ctc_gop_std.h"
#include "std/ipa_to_arpabet_std.h"
#include "std/pron_history_std.h"
#include "std/pron_model_source_std.h"
#include "std/pron_review_std.h"
#endif

namespace {
// 实时波形只画最近 1 秒的尾窗（16k 样本），更早的等停止后全量铺开
constexpr qint64 kLiveWindowSamples = PronAudio::kSampleRate;
constexpr int kWaveColumns = 120;
// 对比流程等示范播完的兜底：引擎哑火（state 永不变化）时不吊死按钮
constexpr int kCompareFallbackMs = 5000;
// 短于这个时长的录音不进评分：连一个音素都切不出来，分数没有意义
constexpr qint64 kMinScoreSamples = PronAudio::kSampleRate / 2;
// M9 练习清单在面板上显示的条数：全量可能几十条，面板不是复习队列
// （下钻仍是收藏面板按「发音不稳」标签过滤）
constexpr int kPracticeListMax = 5;

#ifdef UNIDICT_GUI_PRON
// 现在（epoch 秒）。历史记录与清单排序都用它；取不到时传 0 让规则走
// "时刻未知"分支而不是猜
long long now_seconds() {
    return static_cast<long long>(QDateTime::currentSecsSinceEpoch());
}
#endif

// M5：口音数据 ↔ TTS locale（引擎没有对应语音时 Qt 自行回落默认 voice）
QLocale locale_for_accent(const QString& accent) {
    if (accent == QLatin1String("en-US")) {
        return QLocale(QLocale::English, QLocale::UnitedStates);
    }
    return QLocale(QLocale::English, QLocale::UnitedKingdom);
}

#ifdef UNIDICT_GUI_PRON
// 评分结果展示：词分 + 逐音素 GOP（低分音素带"发成了什么"箭头，
// 地道变体带 ≈ 实读标记）+ 最弱音素点名（M4 差异高亮 → M6 混淆
// 定位 → M7 位置感知变体——不撒糖，指出该练哪、错在哪；被容忍的
// 地道读法也不隐瞒）。纯格式化，无状态。
QString format_score(const UnidictCoreStd::WordGopResult& r) {
    const UnidictCoreStd::PhoneGopResult* weakest = nullptr;
    bool anyRealized = false;
    QString line = QStringLiteral("词分 %1：").arg(QString::number(r.word_score, 'f', 2));
    for (const auto& p : r.phones) {
        line += QStringLiteral(" %1 %2").arg(QString::fromStdString(p.arpabet),
                                             QString::number(p.score, 'f', 2));
        if (!p.confused_with.empty()) {
            // 混淆定位：T 0.05→ER = "这个 T 听起来是 ER"（错读，已扣分）
            line += QStringLiteral("→%1").arg(
                QString::fromStdString(p.confused_with));
        } else if (!p.realized_as.empty()) {
            // 地道变体：G 0.93≈NG = 词尾 g 实读 ŋ（容忍，按它记分）
            line += QStringLiteral("≈%1").arg(
                QString::fromStdString(p.realized_as));
            anyRealized = true;
        }
        line += QStringLiteral(" ·");
        if (!weakest || p.score < weakest->score) {
            weakest = &p;
        }
    }
    if (!r.phones.empty()) {
        line.chop(2);  // 去掉尾分隔符 " ·"
    }
    if (weakest) {
        const QString heard = weakest->confused_with.empty()
                                  ? QString()
                                  : QStringLiteral("，听起来像 %1").arg(
                                        QString::fromStdString(
                                            weakest->confused_with));
        line += QStringLiteral("\n最弱：%1（%2）%3——对着示范多跟几遍。")
                    .arg(QString::fromStdString(weakest->arpabet),
                         QString::number(weakest->score, 'f', 2), heard);
    }
    if (anyRealized) {
        // ≈ 不解释会被读成"错了一点"——必须说明它没扣分
        line += QStringLiteral("\n≈ = 地道变体（按它记分，未扣分）");
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

#ifdef UNIDICT_GUI_PRON
    // M10 模型资产自举下载：缺资产时才出现的一行。600MB 的 fp16 模型不
    // 进仓库，过去这一步要用户自己去 HuggingFace 手动下、放到约定目录、
    // 还得自己确认没下坏——错一步看到的就只是一句加载失败。现在按钮
    // 直接把事情做完：断点续传 + SHA-256 校验，不合格不落地
    downloader_ = new ModelDownloader(this);
    connect(downloader_, &ModelDownloader::progress, this,
            &PronunciationPanel::onModelProgress);
    connect(downloader_, &ModelDownloader::finished, this,
            &PronunciationPanel::onModelDownloadFinished);
    downloadButton_ = new QPushButton(
        QStringLiteral("下载发音模型（%1）").arg(pronModelSizeText()), this);
    downloadButton_->setToolTip(
        QStringLiteral("评分用的离线声学模型（%1），不进仓库。下载后按 SHA-256 "
                       "校验，校验不过不落地；中断了重试会从断点续传。")
            .arg(pronModelSizeText()));
    connect(downloadButton_, &QPushButton::clicked, this,
            &PronunciationPanel::startModelDownload);
    downloadButton_->hide();
    layout->addWidget(downloadButton_);
#endif

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

    // M9 练习清单：读历史里"还没练稳"的词，低分/久未练优先。有内容才
    // 出现（没练过任何词时整行隐藏，不占地方也不误导）
    practiceLabel_ = new QLabel(this);
    practiceLabel_->setWordWrap(true);
    QFont small = practiceLabel_->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    practiceLabel_->setFont(small);
    practiceLabel_->setToolTip(
        QStringLiteral("跟自己的进步比：这里只列最近一次仍未练稳的词，"
                       "按词分低、久未练习排序（完整清单见收藏面板的"
                       "「发音不稳」标签过滤）"));
    practiceLabel_->hide();
    layout->addWidget(practiceLabel_);

    connect(&scoreWatcher_, &QFutureWatcher<ScoreOutcome>::finished, this,
            &PronunciationPanel::onScoreFinished);
    connect(scoreButton_, &QPushButton::clicked, this,
            &PronunciationPanel::scoreRecording);

    retargetScoring();  // 按初始口音换算目标音素并设置按钮态
    refreshPracticeList();
    refreshModelAvailability();  // M10：缺资产才亮下载按钮并禁用评分
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
        // 缺模型时明说：评分要等资产下回来（按钮就在上面一行），别让
        // 用户录完音才发现"评分"点不动
        setStatus(assetsReady_
                      ? QStringLiteral("先听「示范」，再录音跟读；「评分」逐音素对照。")
                      : QStringLiteral("评分要先下载发音模型（%1），点上方按钮；"
                                       "下回来之前可以先「示范」跟读。")
                            .arg(pronModelSizeText()));
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
    scoreButton_->setEnabled(!scoringDead_ && assetsReady_ &&
                             !targetPhones_.empty() &&
                             !recorder_.isRecording() &&
                             recorder_.sampleCount() >= kMinScoreSamples &&
                             !scoreWatcher_.isRunning());
}

void PronunciationPanel::refreshModelAvailability() {
    if (!downloadButton_) {
        return;
    }
    // 判据与 CLI 同一套（存在且尺寸对）：正式文件名只由校验过的断点改名
    // 产生，所以"文件在对目录里"本身就是过过校验的证据
    assetsReady_ = UnidictCoreStd::missing_pron_assets(
                       UnidictCoreStd::pron_model_dir()).empty();
    downloadButton_->setVisible(!assetsReady_);
    retargetScoring();  // tooltip 要跟着"缺模型/齐了"换说法
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
    } else if (!assetsReady_) {
        // 缺模型：说清是哪个前置条件缺，而不是让用户点了等一次加载失败
        scoreButton_->setToolTip(
            QStringLiteral("评分需要先下载发音模型（%1）").arg(pronModelSizeText()));
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
    // 模型路径：core/std pron_model_source_std 是单一真源（约定目录 + env
    // 覆盖），与 CLI --pron-fetch-model 取到的是同一份资产——过去 GUI 与
    // CLI 各硬编码一份 ~/.cache 路径，布局一改漏一处就是"命令行能评、
    // 面板不能评"。路径在 UI 线程解析好，后台任务只做加载与推理
    const std::string dir = UnidictCoreStd::pron_model_dir();
    const auto assetPath = [&dir](const char* key) {
        const UnidictCoreStd::ModelAsset* asset =
            UnidictCoreStd::find_pron_model_asset(key);
        // 清单里必有 model/vocab 两项（测试钉住）；nullptr 兜底给空串，
        // 让 onnxruntime 报"路径为空"而不是崩在解引用上
        return asset ? UnidictCoreStd::pron_asset_path(*asset, dir) : std::string();
    };
    UnidictPron::PronScorerOnnx::Config cfg;
    const QString model = qEnvironmentVariable("UNIDICT_PRON_MODEL");
    const QString vocab = qEnvironmentVariable("UNIDICT_PRON_VOCAB");
    cfg.model_path = (model.isEmpty() ? assetPath("model") : model.toStdString());
    cfg.vocab_path = (vocab.isEmpty() ? assetPath("vocab") : vocab.toStdString());

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
                                              "指定模型路径；文件损坏的话删掉模型目录后"
                                              "重开面板，会重新下载并校验。")
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
            out.word_score = result->word_score;  // M8 联动判稳用
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
        // 状态栏一次说全：进步/持平（M9 相对化基线）+ 生词本标签（M8）。
        // 两段都是"片段"，可拼可缺——都无话可说时才回默认文案
        QString msg = QStringLiteral("评分完成。");
        const QString hist = recordAttempt(out.word_score);
        const QString link = linkVocabularyTag(out.word_score);
        if (!hist.isEmpty()) {
            msg += hist;
        }
        if (!link.isEmpty()) {
            msg += link;
        }
        if (hist.isEmpty() && link.isEmpty()) {
            msg += QStringLiteral("分低的音素就是该练的地方。");
        }
        setStatus(msg);
        refreshPracticeList();
    } else {
        if (out.fatal) {
            scoringDead_ = true;  // 模型缺失不反复试：回退 M2 无评分跟读
            scoreButton_->setToolTip(out.text);
        }
        setStatus(out.text);
    }
    refreshScoreButton();
}

QString PronunciationPanel::linkVocabularyTag(double wordScore) {
    // M8 生词本联动：词分不稳给词条打「发音不稳」标签、回升自动摘
    // （规则在 core/std pron_review_std，这里只做生词本读写与文案）。
    // 只动已在生词本的词——生词本是用户收藏语义，不替用户收词；未收藏
    // 时状态栏点一句，收藏后下次评分即自动生效。
    // 返回的是状态栏**片段**（不带"评分完成。"前缀，那句由调用方统一
    // 拼，好和 M9 的进步文案排在同一句里）
    if (word_.isEmpty()) {
        return {};  // 自由练习无词条
    }
    const QString tag = QString::fromUtf8(UnidictCoreStd::kPronReviewTag);
    auto& store = UnidictCore::DataStore::instance();
    const QString target = word_.trimmed();
    QStringList current;
    bool found = false;
    for (const QVariant& v : store.getVocabularyMeta()) {
        const QVariantMap item = v.toMap();
        if (QString::compare(item.value(QStringLiteral("word")).toString().trimmed(),
                             target, Qt::CaseInsensitive) == 0) {
            found = true;
            current = item.value(QStringLiteral("tags")).toStringList();
            break;
        }
    }
    const bool unstable = UnidictCoreStd::word_score_unstable(wordScore);
    if (!found) {
        return unstable ? QStringLiteral("发音不稳——收藏该词后会自动打上「%1」标签。")
                                  .arg(tag)
                        : QString();
    }
    std::vector<std::string> tags;
    tags.reserve(static_cast<size_t>(current.size()));
    for (const QString& t : current) {
        tags.push_back(t.toStdString());
    }
    const auto next = UnidictCoreStd::next_pron_review_tags(tags, wordScore);
    if (!next) {
        return {};  // 状态没变（或分数无效）：不重写、不打扰
    }
    QStringList updated;
    updated.reserve(next->size());
    for (const std::string& t : *next) {
        updated << QString::fromStdString(t);
    }
    // 词条刚查到，命中必然为真；极端并发（此刻被移除）返回假 → 不改口
    if (!store.setVocabularyItemTags(target, updated)) {
        return {};
    }
    return unstable ? QStringLiteral("生词本已打「%1」标签，收藏面板按分组可筛出练习。")
                              .arg(tag)
                    : QStringLiteral("「%1」标签已移除（发音稳了）。").arg(tag);
}

QString PronunciationPanel::recordAttempt(double wordScore) {
    // M9 跟自己的进步比：把这次练习累计进历史，返回"比上次 +0.12"这类
    // 相对化文案（规则在 core/std pron_history_std，这里只做读写与文案）。
    // 与 M8 的区别：历史不限是否收藏——练过就是自己的练习轨迹，标签才
    // 只挂在生词本词条上（生词本是用户收藏语义，不替用户收词）
    if (word_.isEmpty()) {
        return {};  // 自由练习无词条，记不进按词组织的历史
    }
    auto& store = UnidictCore::DataStore::instance();
    const QString target = word_.trimmed();
    UnidictCoreStd::PronRecordStd prev;
    const QVariantMap old = store.getPronRecord(target);
    const bool has_old = !old.isEmpty();
    if (has_old) {
        prev.word = old.value(QStringLiteral("word")).toString().toStdString();
        prev.last_score = old.value(QStringLiteral("last_score")).toDouble();
        prev.best_score = old.value(QStringLiteral("best_score")).toDouble();
        prev.attempts = old.value(QStringLiteral("attempts")).toInt();
        prev.last_at =
            static_cast<long long>(old.value(QStringLiteral("last_at")).toLongLong());
    }
    const auto attempt = UnidictCoreStd::next_pron_attempt(
        target.toStdString(), has_old ? &prev : nullptr, wordScore, now_seconds());
    if (!attempt) {
        return {};  // 分数无效（域外/NaN 哨兵值）：不污染基线
    }
    const UnidictCoreStd::PronRecordStd& rec = attempt->record;
    store.setPronRecord(target, rec.last_score, rec.best_score, rec.attempts,
                        static_cast<qlonglong>(rec.last_at));
    const QString last = QString::number(rec.last_score, 'f', 2);
    switch (UnidictCoreStd::pron_trend(*attempt)) {
        case UnidictCoreStd::PronTrend::kFirstTime:
            return QStringLiteral("首次记录 %1（第 %2 次）。").arg(last).arg(rec.attempts);
        case UnidictCoreStd::PronTrend::kUp:
            return QStringLiteral("比上次 +%1（%2）。")
                .arg(QString::number(attempt->delta, 'f', 2), last);
        case UnidictCoreStd::PronTrend::kDown:
            return QStringLiteral("比上次 -%1（%2）。")
                .arg(QString::number(-attempt->delta, 'f', 2), last);
        case UnidictCoreStd::PronTrend::kFlat:
            break;
    }
    return QStringLiteral("与上次持平（%1）。").arg(last);
}

void PronunciationPanel::refreshPracticeList() {
    // 练习清单 = 历史里"最近一次仍未练稳"的词（低分 → 久未练优先）。
    // 只是提醒，不是复习队列：点不开、也不改评分目标——完整清单在
    // 收藏面板按「发音不稳」标签过滤
    if (!practiceLabel_) {
        return;
    }
    std::vector<UnidictCoreStd::PronRecordStd> records;
    for (const QVariant& v : UnidictCore::DataStore::instance().getPronRecords()) {
        const QVariantMap item = v.toMap();
        UnidictCoreStd::PronRecordStd rec;
        rec.word = item.value(QStringLiteral("word")).toString().toStdString();
        rec.last_score = item.value(QStringLiteral("last_score")).toDouble();
        rec.best_score = item.value(QStringLiteral("best_score")).toDouble();
        rec.attempts = item.value(QStringLiteral("attempts")).toInt();
        rec.last_at =
            static_cast<long long>(item.value(QStringLiteral("last_at")).toLongLong());
        records.push_back(std::move(rec));
    }
    const auto queue = UnidictCoreStd::pron_practice_queue(
        records, now_seconds(), static_cast<size_t>(kPracticeListMax));
    if (queue.empty()) {
        practiceLabel_->hide();
        return;
    }
    QString text = QStringLiteral("待练：");
    for (size_t i = 0; i < queue.size(); ++i) {
        if (i) {
            text += QStringLiteral(" · ");
        }
        text += QStringLiteral("%1 %2")
                    .arg(QString::fromStdString(queue[i].word),
                         QString::number(queue[i].last_score, 'f', 2));
    }
    practiceLabel_->setText(text);
    practiceLabel_->show();
}
#endif
