#ifndef LOOKUP_ADAPTER_H
#define LOOKUP_ADAPTER_H

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <QTextToSpeech>
#include <QMap>
#include <memory>
#include "lookup_service.h"

// Forward declarations
class ClipboardMonitor;
class GlobalHotkeys;
class QNetworkAccessManager;
class QMediaPlayer;
class QAudioOutput;

namespace UnidictCore { class LookupService; }
namespace UnidictCoreStd { class PronunciationSourceStd; }

class LookupAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int dictionariesStamp READ dictionariesStamp NOTIFY dictionariesStampChanged)
public:
    explicit LookupAdapter(QObject* parent = nullptr);
    ~LookupAdapter() override;

    int dictionariesStamp() const { return dictionariesStamp_; }

    Q_INVOKABLE QString lookupDefinition(const QString& word);
    Q_INVOKABLE QStringList suggestPrefix(const QString& prefix, int maxResults = 20) const;
    Q_INVOKABLE QStringList suggestFuzzy(const QString& word, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchWildcard(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchRegex(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList loadedDictionaries() const;
    Q_INVOKABLE bool loadDictionariesFromEnv();
    Q_INVOKABLE bool reloadDictionariesFromEnv();
    Q_INVOKABLE bool setMdictPassword(const QString& password);
    Q_INVOKABLE void clearMdictPassword();
    Q_INVOKABLE bool hasMdictPassword() const;
    Q_INVOKABLE void addToVocabulary(const QString& word, const QString& definition);
    Q_INVOKABLE QStringList searchHistory(int limit = 100) const;
    Q_INVOKABLE QVariantList vocabulary() const;
    Q_INVOKABLE QVariantList vocabularyMeta() const; // [{word,definition,added_at,tags}]
    // M3-B 生词本编辑（qmlui 生词本页）：标签单条增删（词大小写不敏感、
    // add 幂等、空标签拒、remove 双命中才真）、按标签筛选（保持存储序）、
    // 笔记 upsert（空串即删）——core DataStore M3 口径的转发
    Q_INVOKABLE QVariantList vocabularyByTag(const QString& tag) const;
    Q_INVOKABLE bool addVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE bool removeVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE void setVocabNote(const QString& word, const QString& text);
    Q_INVOKABLE QString getVocabNote(const QString& word) const;
    Q_INVOKABLE void removeVocabularyWord(const QString& word);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void clearVocabulary();
    Q_INVOKABLE int indexedWordCount() const;
    Q_INVOKABLE bool exportVocabCsv(const QString& path) const;
    Q_INVOKABLE QVariantList dictionariesMeta() const;

    // TTS功能
    Q_INVOKABLE void speakText(const QString& text);
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

    // P0 专业词典功能
    // HTML 渲染和安全过滤
    Q_INVOKABLE QString sanitizeHtml(const QString& html) const;
    Q_INVOKABLE QString extractTextFromHtml(const QString& html) const;
    Q_INVOKABLE QString rewriteResourceUrls(const QString& html, const QString& dictionaryId) const;
    Q_INVOKABLE QString rewriteCrossReferenceLinks(const QString& html, const QString& dictionaryId) const;

    // .mdd 资源直取：rewriteResourceUrls 负责把 HTML 里的相对 src 换成
    // file:// 缓存路径（图片/内嵌音视频够用了）。下面三个给"需要原始字节"
    // 的场景——发音音频要喂给 QML MediaPlayer、调用方要自己判断资源是否存在。
    // key 用 .mdd 里的原始键（如 "sounds/hello.mp3"），大小写与前导斜杠
    // 由 core/std 的 normalize_key 归一。
    Q_INVOKABLE bool hasDictionaryResource(const QString& dictionaryId, const QString& key) const;
    Q_INVOKABLE QByteArray loadDictionaryResourceData(const QString& dictionaryId, const QString& key) const;
    // 资源在本地缓存里的 file:// URL；未命中返回空串
    Q_INVOKABLE QString dictionaryResourceUrl(const QString& dictionaryId, const QString& key) const;

    // 一站式词条呈现管线：上面四步合成一次调用，顺序固定为
    // 清洗 → 交叉引用链接 → 资源 URL 重写。返回：
    //   html       —— 可直接交给富文本组件的 HTML
    //   text       —— 纯文本回退（搜索高亮/朗读/剪贴板用）
    //   resources  —— [{key, url, found}]，found=false 表示 .mdd 里没这张图
    Q_INVOKABLE QVariantMap presentEntry(const QString& rawDefinition,
                                         const QString& dictionaryId) const;

    // 交叉引用导航
    Q_INVOKABLE bool canGoBack() const;
    Q_INVOKABLE bool canGoForward() const;
    Q_INVOKABLE QString goBack();
    Q_INVOKABLE QString goForward();
    Q_INVOKABLE void navigateToWord(const QString& word, const QString& dictionaryId = "");
    Q_INVOKABLE void clearNavigationHistory();
    Q_INVOKABLE int navigationHistorySize() const;

    // 多词典聚合查询（分组形态：[{dictionary, dictionaryId, entries:[…]}]，
    // core searchGrouped 三层降级 + 组内 headword 去重）
    Q_INVOKABLE QVariantList aggregateLookup(const QString& word, const QVariantMap& options = QVariantMap());
    // 词条卡头英/美音标：从释义开头的"英 […] 美 […]"惯例提取（{british, american}）
    Q_INVOKABLE QVariantMap extractPhonetics(const QString& definition) const;
    // 全文检索 tab：释义包含目标词的词条（[{word, definition, dictionary}]）
    Q_INVOKABLE QVariantList fullTextLookup(const QString& word, int maxResults = 20) const;
    // 内容 tab：phrases=以查询词开头的词组条目（带释义）；
    // related=近义/联想候选词表（[{word}]，词头链接形态）
    Q_INVOKABLE QVariantList relatedLookup(const QString& word, const QString& kind) const;
    Q_INVOKABLE QVariantList getDictionariesByCategory(const QString& category) const;
    Q_INVOKABLE void setDictionaryPriority(const QString& dictionaryId, int priority);
    Q_INVOKABLE void setDictionaryEnabled(const QString& dictionaryId, bool enabled);

    // P1 剪贴板监听功能
    Q_INVOKABLE void startClipboardMonitoring();
    Q_INVOKABLE void stopClipboardMonitoring();
    Q_INVOKABLE bool isClipboardMonitoring() const;
    Q_INVOKABLE void setClipboardPollInterval(int milliseconds);
    Q_INVOKABLE void setClipboardMinWordLength(int length);
    Q_INVOKABLE void setClipboardMaxWordLength(int length);
    Q_INVOKABLE void addClipboardExcludePattern(const QString& pattern);
    Q_INVOKABLE void clearClipboardExcludePatterns();

    // 剪贴板自动查词开关
    Q_INVOKABLE void setClipboardAutoLookupEnabled(bool enabled);
    Q_INVOKABLE bool isClipboardAutoLookupEnabled() const;

    // P1 全局热键功能
    Q_INVOKABLE bool registerGlobalHotkey(const QString& action, const QString& keySequence);
    Q_INVOKABLE void unregisterGlobalHotkey(const QString& action);
    Q_INVOKABLE void unregisterAllGlobalHotkeys();
    Q_INVOKABLE QStringList registeredHotkeyActions() const;
    Q_INVOKABLE QString getHotkeyForAction(const QString& action) const;
    Q_INVOKABLE void setGlobalHotkeysEnabled(bool enabled);
    Q_INVOKABLE bool isGlobalHotkeysEnabled() const;
    Q_INVOKABLE static bool isGlobalHotkeysSupported();

signals:
    void clipboardWordDetected(const QString& word);
    void dictionariesStampChanged();
    // 在线发音链路的状态行（请求中/播放中/失败回落），设置页与朗读处可显
    void pronOnlineStatus(const QString& message);

private:
    // 在线发音取片段：拉 request_url → 容忍式解析 → 口音挑选 → 播放。
    // fallbackLocal 为真（自动态）失败时回落本地 TTS；为假（在线态）
    // 只报状态。唯一外发内容是查询词（core 层保证）
    void fetchOnlinePron(const QString& word, bool fallbackLocal);
    void ttsSay(const QString& text);

    int dictionariesStamp_ = 0;
    std::unique_ptr<UnidictCore::LookupService> m_service;
    std::unique_ptr<QTextToSpeech> m_tts;
    std::unique_ptr<ClipboardMonitor> m_clipboardMonitor;
    std::unique_ptr<GlobalHotkeys> m_globalHotkeys;

    // 在线发音：网络与播放（QMediaPlayer 需挂 QAudioOutput 出声）
    std::unique_ptr<QNetworkAccessManager> m_net;
    std::unique_ptr<QMediaPlayer> m_player;
    std::unique_ptr<QAudioOutput> m_audioOut;
    // 发音源（core 纯逻辑层；换源=换实现，平台壳不感知协议细节）
    std::unique_ptr<UnidictCoreStd::PronunciationSourceStd> m_pronSource;
    int m_pronSourceMode = 0;   // 0 本地 / 1 在线 / 2 自动
    int m_pronAccent = 0;       // 0 自动 / 1 美音 / 2 英音 / 3 澳音
    bool m_pronFetchActive = false;  // 防并发请求（同一时刻只取一条）

    // P0 模块实例 (forward declared, 使用 std 模块)
    // 为了避免 Qt 依赖 std 模块，这里使用 pimpl 模式
    class P0Modules;
    std::unique_ptr<P0Modules> m_p0;

    // 语音设置
    double m_currentRate = 1.0;
    double m_currentPitch = 0.0;
    double m_currentVolume = 0.8;
    bool m_autoPlayEnabled = false;
    int m_autoPlayDelay = 1000; // 毫秒

    // 语音预设配置
    QMap<QString, QVariantMap> m_voicePresets;

    // 剪贴板自动查词
    bool m_clipboardAutoLookupEnabled = false;

    // 信号连接
    QMetaObject::Connection m_clipboardWordConnection;
    QMetaObject::Connection m_hotkeyPressedConnection;
};

#endif // LOOKUP_ADAPTER_H
