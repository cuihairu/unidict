#include "voice_adapter.h"

#include <QAudioOutput>
#include <QCoreApplication>
#include <QMediaPlayer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QTimer>
#include <QUrl>

#include "unidict_core.h"
#include "std/online_pron_std.h"

using namespace UnidictCore;

VoiceAdapter::~VoiceAdapter() = default;

VoiceAdapter::VoiceAdapter(QObject* parent)
    : QObject(parent)
    , m_tts(std::make_unique<QTextToSpeech>(this))
    , m_net(std::make_unique<QNetworkAccessManager>(this))
    , m_player(std::make_unique<QMediaPlayer>(this))
    , m_audioOut(std::make_unique<QAudioOutput>(this))
    , m_pronSource(std::make_unique<UnidictCoreStd::FreeDictionarySource>()) {

    // Initialize default voice presets
    // 续行 25/28/30 是 gcov 在 -O2 下的归属假象：insert 首行命中、initializer_list
    // 临时量的构造块被内联进 <initializer_list>/<map> 头文件而挂在这三行上，
    // 实测构造函数跑 16 次这三行仍恒 0（预设内容本身有测试断言兜底）。
    m_voicePresets.insert("Default", QVariantMap{
        { "rate", 1.0 }, { "pitch", 0.0 }, { "volume", 0.8 }  // GCOVR_EXCL_LINE
    });
    m_voicePresets.insert("Calm Study", QVariantMap{
        { "rate", 0.8 }, { "pitch", -0.1 }, { "volume", 0.9 }  // GCOVR_EXCL_LINE
    });
    m_voicePresets.insert("Quick Review", QVariantMap{
        { "rate", 1.4 }, { "pitch", 0.1 }, { "volume", 0.8 }  // GCOVR_EXCL_LINE
    });
    if (m_tts) {
        setRate(m_currentRate);
        setPitch(m_currentPitch);
        setVolume(m_currentVolume);
    }

    // 在线发音播放管线：QMediaPlayer 必须挂 QAudioOutput 才出声；
    // 播放器错误/播完都推进状态行，用户知道链路走到了哪一步
    if (m_player && m_audioOut) {
        m_player->setAudioOutput(m_audioOut.get());
        connect(m_player.get(), &QMediaPlayer::errorOccurred, this,
                [this](QMediaPlayer::Error, const QString& errorString) {
                    emit pronOnlineStatus(
                        QStringLiteral("在线发音播放失败：%1").arg(errorString));
                });
        connect(m_player.get(), &QMediaPlayer::mediaStatusChanged, this,
                [this](QMediaPlayer::MediaStatus status) {
                    if (status == QMediaPlayer::EndOfMedia) {
                        emit pronOnlineStatus(QStringLiteral("在线发音播放完毕"));
                    }
                });
    }

    // 发音源三态与口音偏好持久化（与 SettingsQt 同一 ini 文件）。
    // 默认本地：隐私口径——开在线是显式动作
    const QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                             QCoreApplication::organizationName(),
                             QCoreApplication::applicationName());
    m_pronSourceMode =
        qBound(0, settings.value("pron/sourceMode", 0).toInt(), 2);
    m_pronAccent = qBound(0, settings.value("pron/accent", 0).toInt(), 3);
}

// 查词命中后的自动朗读：开关关/词空白跳过；延时 >0 走 singleShot
void VoiceAdapter::speakAfterLookup(const QString& word) {
    if (!m_autoPlayEnabled || !m_tts || word.trimmed().isEmpty()) return;
    const QString spokenWord = word;
    const int delay = qMax(0, m_autoPlayDelay);
    if (delay <= 0) {
        speakText(spokenWord);
    } else {
        QTimer::singleShot(delay, this, [this, spokenWord]() {
            speakText(spokenWord);
        });
    }
}

// 朗读统一入口（结果页/生词本/自动播放都汇到这里）：按发音源三态
// 分发——本地直接 TTS；在线与自动走在线片段，自动态失败回落本地
void VoiceAdapter::speakText(const QString& text) {
    const QString word = text.trimmed();
    if (word.isEmpty()) return;
    if (m_pronSourceMode == 0) {
        ttsSay(word);
        return;
    }
    fetchOnlinePron(word, /*fallbackLocal=*/ m_pronSourceMode == 2);
}

// BUG-010：音标行英/美喇叭分口音。本地 TTS 无口音概念（原样朗读）；
// 在线/自动态走 fetchOnlinePron 并覆盖口音偏好
void VoiceAdapter::speakWordWithAccent(const QString& word, int accent) {
    const QString w = word.trimmed();
    if (w.isEmpty()) return;
    if (m_pronSourceMode == 0) {
        ttsSay(w);
        return;
    }
    fetchOnlinePron(w, /*fallbackLocal=*/ m_pronSourceMode == 2,
                    qBound(-1, accent, 3));
}

bool VoiceAdapter::hasLocalTts() const {
    return m_tts && !m_tts->availableVoices().isEmpty();
}

void VoiceAdapter::ttsSay(const QString& text) {
    if (hasLocalTts()) {
        m_tts->say(text);
        return;
    }
    // BUG-010：本地语音不可用不再静默——朗读点击必须有可观测反馈，
    // 否则用户视角就是「点了没反应」
    emit pronOnlineStatus(QStringLiteral("本地语音不可用（未检测到系统 TTS 语音），"
                                         "可在 设置→语音 换用在线发音"));
}

void VoiceAdapter::fetchOnlinePron(const QString& word, bool fallbackLocal,
                                   int accentOverride) {
    if (!m_net || !m_pronSource) {
        if (fallbackLocal) ttsSay(word);
        return;
    }
    if (m_pronFetchActive) {
        emit pronOnlineStatus(QStringLiteral("上一个在线发音请求还在进行，稍后再试"));
        if (fallbackLocal) ttsSay(word);
        return;
    }
    // 唯一外发内容是查询词（core 层拼装保证，无历史/生词本等附加信息）
    const std::string url = m_pronSource->request_url(word.toStdString());
    if (url.empty()) {
        if (fallbackLocal) ttsSay(word);
        return;
    }
    m_pronFetchActive = true;
    emit pronOnlineStatus(QStringLiteral("正在获取在线发音（%1）…")
                              .arg(QString::fromLatin1(m_pronSource->name())));

    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    request.setTransferTimeout(8000);
    QNetworkReply* reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, word, fallbackLocal, accentOverride]() {
        reply->deleteLater();
        m_pronFetchActive = false;

        const QVariant httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        std::vector<UnidictCoreStd::PronClip> clips;
        if (reply->error() == QNetworkReply::NoError && httpStatus.toInt() == 200) {
            clips = m_pronSource->parse_response(
                QString::fromUtf8(reply->readAll()).toStdString());
        }
        // 口音偏好：覆盖参（音标行英/美喇叭）优先，其次设置页偏好
        // 0=自动（不指定），1/2/3 对应美/英/澳
        const int accentSetting = accentOverride >= 0 ? accentOverride : m_pronAccent;
        UnidictCoreStd::PronAccent prefer = UnidictCoreStd::PronAccent::Unknown;
        if (accentSetting == 1) prefer = UnidictCoreStd::PronAccent::US;
        else if (accentSetting == 2) prefer = UnidictCoreStd::PronAccent::UK;
        else if (accentSetting == 3) prefer = UnidictCoreStd::PronAccent::AU;
        const UnidictCoreStd::PronClip* clip =
            UnidictCoreStd::pick_clip(clips, prefer);
        if (!clip || !m_player) {
            // 失败口径：在线态只报状态；自动态回落本地 TTS
            if (fallbackLocal) {
                emit pronOnlineStatus(QStringLiteral("在线发音失败，已回落本地语音"));
                ttsSay(word);
            } else if (reply->error() != QNetworkReply::NoError) {
                emit pronOnlineStatus(QStringLiteral("在线发音失败：%1")
                                          .arg(reply->errorString()));
            } else {
                emit pronOnlineStatus(QStringLiteral("该词没有在线发音片段"));
            }
            return;
        }
        m_player->setSource(QUrl(QString::fromStdString(clip->url)));
        m_player->play();
        emit pronOnlineStatus(QStringLiteral("正在播放在线发音（%1）…")
                                  .arg(QString::fromLatin1(m_pronSource->name())));
    });
}

void VoiceAdapter::setPronSourceMode(int mode) {
    const int v = qBound(0, mode, 2);
    if (v == m_pronSourceMode) return;
    m_pronSourceMode = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    settings.setValue("pron/sourceMode", v);
    settings.sync();
}

int VoiceAdapter::pronSourceMode() const { return m_pronSourceMode; }

void VoiceAdapter::setPronAccent(int accent) {
    const int v = qBound(0, accent, 3);
    if (v == m_pronAccent) return;
    m_pronAccent = v;
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QCoreApplication::organizationName(),
                       QCoreApplication::applicationName());
    settings.setValue("pron/accent", v);
    settings.sync();
}

int VoiceAdapter::pronAccent() const { return m_pronAccent; }

void VoiceAdapter::stopSpeaking() {
    if (m_tts) {
        m_tts->stop();
    }
    if (m_player) {
        m_player->stop();
    }
}

void VoiceAdapter::pauseSpeaking() {
    if (m_tts) {
        m_tts->pause();
    }
}

void VoiceAdapter::resumeSpeaking() {
    if (m_tts) {
        m_tts->resume();
    }
}

bool VoiceAdapter::isSpeaking() const {
    if (m_player &&
        m_player->playbackState() == QMediaPlayer::PlayingState) {
        return true;
    }
    return m_tts && m_tts->state() == QTextToSpeech::Speaking;
}

bool VoiceAdapter::isPaused() const {
    return m_tts && m_tts->state() == QTextToSpeech::Paused;
}

QStringList VoiceAdapter::availableVoices() const {
    QStringList voices;
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            // 无引擎的 headless 环境（offscreen CI 无 speech-dispatcher，
            // mock 引擎只认 QTextToSpeech("mock") 显式构造，adapter 走默认
            // 探测）恒空表，循环体不可确定性到达——仓库纪律也禁止对音频
            // 设备/引擎做假设。
            voices.append(voice.name());  // GCOVR_EXCL_LINE
        }
    }
    return voices;
}

void VoiceAdapter::setVoice(const QString& voiceName) {
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            // 同上：headless 无语音后端时 availableVoices() 恒空，匹配体
            // 不可确定性到达（循环本身的扫描行已由测试命中）。
            if (voice.name() == voiceName) {  // GCOVR_EXCL_LINE
                m_tts->setVoice(voice);       // GCOVR_EXCL_LINE
                break;                        // GCOVR_EXCL_LINE
            }
        }
    }
}

QString VoiceAdapter::getCurrentVoice() const {
    if (!m_tts) return {};
    auto voice = m_tts->voice();
    return voice.name();
}

void VoiceAdapter::setRate(double rate) {
    m_currentRate = qBound(0.1, rate, 2.0);
    if (m_tts) {
        // 将0.1-2.0映射到Qt的-1.0到1.0
        double qtRate = (m_currentRate - 1.0) * 2.0;
        qtRate = qBound(-1.0, qtRate, 1.0);
        m_tts->setRate(qtRate);
    }
}

double VoiceAdapter::getRate() const {
    return m_currentRate;
}

void VoiceAdapter::setPitch(double pitch) {
    m_currentPitch = qBound(-1.0, pitch, 1.0);
    if (m_tts) {
        m_tts->setPitch(m_currentPitch);
    }
}

double VoiceAdapter::getPitch() const {
    return m_currentPitch;
}

void VoiceAdapter::setVolume(double volume) {
    m_currentVolume = qBound(0.0, volume, 1.0);
    if (m_tts) {
        m_tts->setVolume(m_currentVolume);
    }
}

double VoiceAdapter::getVolume() const {
    return m_currentVolume;
}

void VoiceAdapter::applyVoicePreset(const QString& presetName) {
    auto it = m_voicePresets.constFind(presetName);
    if (it == m_voicePresets.constEnd()) return;
    const QVariantMap preset = it.value();
    if (preset.contains("rate")) setRate(preset.value("rate").toDouble());
    if (preset.contains("pitch")) setPitch(preset.value("pitch").toDouble());
    if (preset.contains("volume")) setVolume(preset.value("volume").toDouble());
    const QString voice = preset.value("voice").toString();
    if (!voice.isEmpty()) setVoice(voice);
}

QStringList VoiceAdapter::getVoicePresets() const {
    return m_voicePresets.keys();
}

void VoiceAdapter::setAutoPlayEnabled(bool enabled) {
    m_autoPlayEnabled = enabled;
}

bool VoiceAdapter::isAutoPlayEnabled() const {
    return m_autoPlayEnabled;
}

void VoiceAdapter::setAutoPlayDelay(int milliseconds) {
    m_autoPlayDelay = qMax(0, milliseconds);
}

int VoiceAdapter::getAutoPlayDelay() const {
    return m_autoPlayDelay;
}

QVariantMap VoiceAdapter::getVoiceInfo() const {
    QVariantMap info;
    info["speaking"] = isSpeaking();
    info["paused"] = isPaused();
    info["voice"] = getCurrentVoice();
    info["availableVoices"] = availableVoices();
    info["rate"] = m_currentRate;
    info["pitch"] = m_currentPitch;
    info["volume"] = m_currentVolume;
    info["autoPlayEnabled"] = m_autoPlayEnabled;
    info["autoPlayDelay"] = m_autoPlayDelay;
    return info;
}
