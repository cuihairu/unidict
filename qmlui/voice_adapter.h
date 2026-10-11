#ifndef VOICE_ADAPTER_H
#define VOICE_ADAPTER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QTextToSpeech>
#include <memory>

// Forward declarations
class QNetworkAccessManager;
class QMediaPlayer;
class QAudioOutput;

namespace UnidictCoreStd {
class PronunciationSourceStd;
}

// 语音面（TD-121 拆桥第二刀，从 LookupAdapter 整体迁出）：本地 TTS +
// 在线发音（三态源切换、口音偏好、语速/音调/音量、预设）+ 自动播放。
// 查词命中后的自动朗读经 LookupAdapter::entryLookedUp 信号接入
// speakAfterLookup（main.cpp 装配），查词与朗读两侧互不感知内部实现。
class VoiceAdapter : public QObject {
    Q_OBJECT
public:
    explicit VoiceAdapter(QObject* parent = nullptr);
    // 网络/播放/TTS 成员只前向声明保持头文件精简；析构定义在 .cpp
    // （那里已含真实类型），否则只持有不拆构的地方（dev 审计/沙箱夹具）
    // 会踩不完整类型
    ~VoiceAdapter() override;

    // TTS功能
    Q_INVOKABLE void speakText(const QString& text);
    // BUG-010：音标行英/美喇叭分口音——accent 用发音源口音枚举
    // （0=自动 1=美音 2=英音 3=澳音），-1 表示按设置页的口音偏好
    Q_INVOKABLE void speakWordWithAccent(const QString& word, int accent);
    // 本地 TTS 是否真正可用（引擎存在且有可用语音）；不可用时朗读入口
    // 会给明确状态行而不是静默
    Q_INVOKABLE bool hasLocalTts() const;
    Q_INVOKABLE void stopSpeaking();
    Q_INVOKABLE void pauseSpeaking();
    Q_INVOKABLE void resumeSpeaking();
    Q_INVOKABLE bool isSpeaking() const;
    Q_INVOKABLE bool isPaused() const;
    Q_INVOKABLE QStringList availableVoices() const;
    Q_INVOKABLE void setVoice(const QString& voiceName);
    Q_INVOKABLE QString getCurrentVoice() const;
    Q_INVOKABLE void setRate(double rate);    // 语速: 0.1 - 2.0
    Q_INVOKABLE double getRate() const;
    Q_INVOKABLE void setPitch(double pitch);  // 音调: -1.0 - 1.0
    Q_INVOKABLE double getPitch() const;
    Q_INVOKABLE void setVolume(double volume); // 音量: 0.0 - 1.0
    Q_INVOKABLE double getVolume() const;

    // 语音品质预设
    Q_INVOKABLE void applyVoicePreset(const QString& presetName);
    Q_INVOKABLE QStringList getVoicePresets() const;

    // 自动播放设置
    Q_INVOKABLE void setAutoPlayEnabled(bool enabled);
    Q_INVOKABLE bool isAutoPlayEnabled() const;
    Q_INVOKABLE void setAutoPlayDelay(int milliseconds);
    Q_INVOKABLE int getAutoPlayDelay() const;

    // 语音状态信息
    Q_INVOKABLE QVariantMap getVoiceInfo() const;

    // 在线发音：三态（0=本地 TTS 1=在线发音 2=自动：在线优先失败回落
    // 本地）与口音偏好（0=自动 1=美音 2=英音 3=澳音）。持久化在
    // pron/* 键，默认本地——隐私口径：开在线是显式动作
    Q_INVOKABLE void setPronSourceMode(int mode);
    Q_INVOKABLE int pronSourceMode() const;
    Q_INVOKABLE void setPronAccent(int accent);
    Q_INVOKABLE int pronAccent() const;

signals:
    // 在线发音链路的状态行（请求中/播放中/失败回落），设置页与朗读处可显
    void pronOnlineStatus(const QString& message);

public slots:
    // 查词命中后的自动朗读入口（LookupAdapter::entryLookedUp 接入）：
    // 开关关/词空白跳过，延时 >0 走 QTimer::singleShot
    void speakAfterLookup(const QString& word);

private:
    // 在线发音取片段：拉 request_url → 容忍式解析 → 口音挑选 → 播放。
    // fallbackLocal 为真（自动态）失败时回落本地 TTS；为假（在线态）
    // 只报状态。accentOverride >= 0 时覆盖口音偏好（音标行英/美喇叭），
    // 否则用 m_pronAccent。唯一外发内容是查询词（core 层保证）
    void fetchOnlinePron(const QString& word, bool fallbackLocal, int accentOverride = -1);
    void ttsSay(const QString& text);

    std::unique_ptr<QTextToSpeech> m_tts;

    // 在线发音：网络与播放（QMediaPlayer 需挂 QAudioOutput 出声）
    std::unique_ptr<QNetworkAccessManager> m_net;
    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audioOut;
    // 发音源（core 纯逻辑层；换源=换实现，平台壳不感知协议细节）
    std::unique_ptr<UnidictCoreStd::PronunciationSourceStd> m_pronSource;
    int m_pronSourceMode = 0;   // 0 本地 / 1 在线 / 2 自动
    int m_pronAccent = 0;       // 0 自动 / 1 美音 / 2 英音 / 3 澳音
    bool m_pronFetchActive = false;  // 防并发请求（同一时刻只取一条）

    // 语音设置
    double m_currentRate = 1.0;
    double m_currentPitch = 0.0;
    double m_currentVolume = 0.8;
    bool m_autoPlayEnabled = false;
    int m_autoPlayDelay = 1000; // 毫秒

    // 语音预设配置
    QMap<QString, QVariantMap> m_voicePresets;
};

#endif // VOICE_ADAPTER_H
