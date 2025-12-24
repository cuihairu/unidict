#ifndef LOOKUP_ADAPTER_H
#define LOOKUP_ADAPTER_H

#include <QObject>
#include <QStringList>
#include <QVariant>
#include <QTextToSpeech>
#include <QMap>
#include <memory>
#include "lookup_service.h"

namespace UnidictCore { class LookupService; }

class LookupAdapter : public QObject {
    Q_OBJECT
public:
    explicit LookupAdapter(QObject* parent = nullptr);

    Q_INVOKABLE QString lookupDefinition(const QString& word);
    Q_INVOKABLE QStringList suggestPrefix(const QString& prefix, int maxResults = 20) const;
    Q_INVOKABLE QStringList suggestFuzzy(const QString& word, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchWildcard(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchRegex(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList loadedDictionaries() const;
    Q_INVOKABLE bool loadDictionariesFromEnv();
    Q_INVOKABLE void addToVocabulary(const QString& word, const QString& definition);
    Q_INVOKABLE QStringList searchHistory(int limit = 100) const;
    Q_INVOKABLE QVariantList vocabulary() const;
    Q_INVOKABLE QVariantList vocabularyMeta() const;
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

    // P0 专业词典功能
    // HTML 渲染和安全过滤
    Q_INVOKABLE QString sanitizeHtml(const QString& html) const;
    Q_INVOKABLE QString extractTextFromHtml(const QString& html) const;
    Q_INVOKABLE QString rewriteResourceUrls(const QString& html, const QString& dictionaryId) const;
    Q_INVOKABLE QString rewriteCrossReferenceLinks(const QString& html, const QString& dictionaryId) const;

    // 交叉引用导航
    Q_INVOKABLE bool canGoBack() const;
    Q_INVOKABLE bool canGoForward() const;
    Q_INVOKABLE QString goBack();
    Q_INVOKABLE QString goForward();
    Q_INVOKABLE void navigateToWord(const QString& word, const QString& dictionaryId = "");
    Q_INVOKABLE void clearNavigationHistory();
    Q_INVOKABLE int navigationHistorySize() const;

    // 多词典聚合查询
    Q_INVOKABLE QVariantList aggregateLookup(const QString& word, const QVariantMap& options = QVariantMap()) const;
    Q_INVOKABLE QVariantList getDictionariesByCategory(const QString& category) const;
    Q_INVOKABLE void setDictionaryPriority(const QString& dictionaryId, int priority);
    Q_INVOKABLE void setDictionaryEnabled(const QString& dictionaryId, bool enabled);

private:
    std::unique_ptr<UnidictCore::LookupService> m_service;
    std::unique_ptr<QTextToSpeech> m_tts;

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
};

#endif // LOOKUP_ADAPTER_H
