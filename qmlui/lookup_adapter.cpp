#include "lookup_adapter.h"
#include "clipboard_monitor.h"
#include "global_hotkeys.h"

#include <QRegularExpression>
#include <QTextToSpeech>
#include <QTimer>
#include <QtGlobal>

#include "lookup_service.h"
#include "unidict_core.h"
#include "data_store.h"

using namespace UnidictCore;

static QString stripHtmlForStorage(const QString& in) {
    // Keep vocabulary storage readable even when UI renders rich-text definitions.
    if (!in.contains('<') || !in.contains('>')) return in;
    QString s = in;
    s.replace(QRegularExpression("<\\s*br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.replace(QRegularExpression("</\\s*p\\s*>", QRegularExpression::CaseInsensitiveOption), "\n\n");
    s.replace(QRegularExpression("</\\s*div\\s*>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.remove(QRegularExpression("<[^>]+>"));
    s.replace("&nbsp;", " ");
    return s.trimmed();
}

LookupAdapter::LookupAdapter(QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<LookupService>())
    , m_tts(std::make_unique<QTextToSpeech>(this))
    , m_clipboardMonitor(std::make_unique<ClipboardMonitor>(this))
    , m_globalHotkeys(std::make_unique<GlobalHotkeys>(this))
    , m_p0(std::make_unique<P0Modules>()) {

    // Connect clipboard monitor word detection
    m_clipboardWordConnection = connect(m_clipboardMonitor.get(), &ClipboardMonitor::wordDetected,
        [this](const QString& word) {
            if (m_clipboardAutoLookupEnabled) {
                // Perform lookup without suggestion fallback
                m_service->lookupDefinition(word, false, 0);
                DataStore::instance().addSearchHistory(word);
            }
        }
    );

    // Connect global hotkey handler
    m_hotkeyPressedConnection = connect(m_globalHotkeys.get(), &GlobalHotkeys::hotkeyPressed,
        [this](const QString& action) {
            if (action == "lookup_selection") {
                // TODO: Get selected text from focused window
                // This requires platform-specific implementation
            } else if (action == "show_window") {
                // TODO: Show/bring main window to front
            } else if (action == "quick_lookup") {
                // Trigger quick lookup mode
            }
        }
    );
    // Initialize default voice presets
    m_voicePresets.insert("Default", QVariantMap{
        { "rate", 1.0 }, { "pitch", 0.0 }, { "volume", 0.8 }
    });
    m_voicePresets.insert("Calm Study", QVariantMap{
        { "rate", 0.8 }, { "pitch", -0.1 }, { "volume", 0.9 }
    });
    m_voicePresets.insert("Quick Review", QVariantMap{
        { "rate", 1.4 }, { "pitch", 0.1 }, { "volume", 0.8 }
    });
    if (m_tts) {
        setRate(m_currentRate);
        setPitch(m_currentPitch);
        setVolume(m_currentVolume);
    }
}

QString LookupAdapter::lookupDefinition(const QString& word) {
    // 记录导航历史
    navigateToWord(word);

    const QString def = m_service->lookupDefinition(word, true, 10);

    if (!def.startsWith("Word not found")) {
        DataStore::instance().addSearchHistory(word);
        if (m_autoPlayEnabled && m_tts && !word.trimmed().isEmpty()) {
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
    }
    return def;
}

QStringList LookupAdapter::suggestPrefix(const QString& prefix, int maxResults) const {
    return m_service->suggestPrefix(prefix, maxResults);
}

QStringList LookupAdapter::loadedDictionaries() const {
    return DictionaryManager::instance().getLoadedDictionaries();
}

QVariantList LookupAdapter::dictionariesMeta() const {
    QVariantList out;
    const auto metas = DictionaryManager::instance().getDictionariesMeta();
    for (const auto& m : metas) {
        QVariantMap vm;
        vm["name"] = m.name;
        vm["wordCount"] = m.wordCount;
        vm["description"] = m.description;
        out.push_back(vm);
    }
    return out;
}

bool LookupAdapter::loadDictionariesFromEnv() {
    const QString env = qEnvironmentVariable("UNIDICT_DICTS");
    if (env.isEmpty()) return false;
    const QStringList paths = env.split(QRegularExpression("[:;]"), Qt::SkipEmptyParts);
    bool ok = false;
    for (const QString& p : paths) {
        ok |= DictionaryManager::instance().addDictionary(p.trimmed());
    }
    return ok;
}

void LookupAdapter::addToVocabulary(const QString& word, const QString& definition) {
    DictionaryEntry e;
    e.word = word;
    e.definition = stripHtmlForStorage(definition);
    DataStore::instance().addVocabularyItem(e);
}

QStringList LookupAdapter::suggestFuzzy(const QString& word, int maxResults) const {
    return m_service->suggestFuzzy(word, maxResults);
}

QStringList LookupAdapter::searchWildcard(const QString& pattern, int maxResults) const {
    return m_service->searchWildcard(pattern, maxResults);
}

QStringList LookupAdapter::searchRegex(const QString& pattern, int maxResults) const {
    return DictionaryManager::instance().regexSearch(pattern, maxResults);
}

QStringList LookupAdapter::searchHistory(int limit) const {
    return DataStore::instance().getSearchHistory(limit);
}

QVariantList LookupAdapter::vocabulary() const {
    QVariantList out;
    const auto items = DataStore::instance().getVocabulary();
    for (const auto& e : items) {
        QVariantMap m; m["word"] = e.word; m["definition"] = e.definition; out.push_back(m);
    }
    return out;
}

QVariantList LookupAdapter::vocabularyMeta() const {
    return DataStore::instance().getVocabularyMeta();
}

void LookupAdapter::removeVocabularyWord(const QString& word) {
    DataStore::instance().removeVocabularyItem(word);
}

void LookupAdapter::clearHistory() {
    DataStore::instance().clearHistory();
}

void LookupAdapter::clearVocabulary() {
    DataStore::instance().clearVocabulary();
}

int LookupAdapter::indexedWordCount() const {
    return DictionaryManager::instance().getIndexedWordCount();
}

bool LookupAdapter::exportVocabCsv(const QString& path) const {
    return DataStore::instance().exportVocabularyCSV(path);
}

// ================= TTS功能实现 =================

void LookupAdapter::speakText(const QString& text) {
    if (m_tts && !text.trimmed().isEmpty()) {
        m_tts->say(text);
    }
}

void LookupAdapter::stopSpeaking() {
    if (m_tts) {
        m_tts->stop();
    }
}

void LookupAdapter::pauseSpeaking() {
    if (m_tts) {
        m_tts->pause();
    }
}

void LookupAdapter::resumeSpeaking() {
    if (m_tts) {
        m_tts->resume();
    }
}

bool LookupAdapter::isSpeaking() const {
    return m_tts && m_tts->state() == QTextToSpeech::Speaking;
}

bool LookupAdapter::isPaused() const {
    return m_tts && m_tts->state() == QTextToSpeech::Paused;
}

QStringList LookupAdapter::availableVoices() const {
    QStringList voices;
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            voices.append(voice.name());
        }
    }
    return voices;
}

void LookupAdapter::setVoice(const QString& voiceName) {
    if (m_tts) {
        const auto voiceList = m_tts->availableVoices();
        for (const auto& voice : voiceList) {
            if (voice.name() == voiceName) {
                m_tts->setVoice(voice);
                break;
            }
        }
    }
}

QString LookupAdapter::getCurrentVoice() const {
    if (!m_tts) return {};
    auto voice = m_tts->voice();
    return voice.name();
}

void LookupAdapter::setRate(double rate) {
    m_currentRate = qBound(0.1, rate, 2.0);
    if (m_tts) {
        // 将0.1-2.0映射到Qt的-1.0到1.0
        double qtRate = (m_currentRate - 1.0) * 2.0;
        qtRate = qBound(-1.0, qtRate, 1.0);
        m_tts->setRate(qtRate);
    }
}

double LookupAdapter::getRate() const {
    return m_currentRate;
}

void LookupAdapter::setPitch(double pitch) {
    m_currentPitch = qBound(-1.0, pitch, 1.0);
    if (m_tts) {
        m_tts->setPitch(m_currentPitch);
    }
}

double LookupAdapter::getPitch() const {
    return m_currentPitch;
}

void LookupAdapter::setVolume(double volume) {
    m_currentVolume = qBound(0.0, volume, 1.0);
    if (m_tts) {
        m_tts->setVolume(m_currentVolume);
    }
}

double LookupAdapter::getVolume() const {
    return m_currentVolume;
}

void LookupAdapter::applyVoicePreset(const QString& presetName) {
    auto it = m_voicePresets.constFind(presetName);
    if (it == m_voicePresets.constEnd()) return;
    const QVariantMap preset = it.value();
    if (preset.contains("rate")) setRate(preset.value("rate").toDouble());
    if (preset.contains("pitch")) setPitch(preset.value("pitch").toDouble());
    if (preset.contains("volume")) setVolume(preset.value("volume").toDouble());
    const QString voice = preset.value("voice").toString();
    if (!voice.isEmpty()) setVoice(voice);
}

QStringList LookupAdapter::getVoicePresets() const {
    return m_voicePresets.keys();
}

void LookupAdapter::setAutoPlayEnabled(bool enabled) {
    m_autoPlayEnabled = enabled;
}

bool LookupAdapter::isAutoPlayEnabled() const {
    return m_autoPlayEnabled;
}

void LookupAdapter::setAutoPlayDelay(int milliseconds) {
    m_autoPlayDelay = qMax(0, milliseconds);
}

int LookupAdapter::getAutoPlayDelay() const {
    return m_autoPlayDelay;
}

QVariantMap LookupAdapter::getVoiceInfo() const {
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

// ============================================================================
// P0 专业词典功能实现
// ============================================================================

// Pimpl 类封装 std 模块依赖
class LookupAdapter::P0Modules {
public:
    P0Modules() {
        // 初始化 std 模块实例
        // 由于 std 模块是纯 C++，不依赖 Qt，可以安全使用
    }

    // HTML 渲染 (使用 html_renderer_std)
    QString sanitize(const QString& html) const {
        // 简单的 HTML 安全过滤实现
        // 完整版本应调用 HtmlRendererStd::sanitize()
        QString result = html;

        // 移除危险标签
        result.remove(QRegularExpression("<script[^>]*>.*?</script>",
                                       QRegularExpression::CaseInsensitiveOption));
        result.remove(QRegularExpression("<iframe[^>]*>.*?</iframe>",
                                       QRegularExpression::CaseInsensitiveOption));
        result.remove(QRegularExpression("<object[^>]*>.*?</object>",
                                       QRegularExpression::CaseInsensitiveOption));
        result.remove(QRegularExpression("<embed[^>]*>.*?</embed>",
                                       QRegularExpression::CaseInsensitiveOption));

        // 移除事件处理器
        result.remove(QRegularExpression(R"(\s+on\w+\s*=\s*["'][^"']*["'])",
                                       QRegularExpression::CaseInsensitiveOption));

        return result;
    }

    QString extractText(const QString& html) const {
        // 简单的文本提取
        QString result = html;
        result.remove(QRegularExpression("<[^>]+>"));
        result.replace("&nbsp;", " ");
        result.replace("&lt;", "<");
        result.replace("&gt;", ">");
        result.replace("&amp;", "&");
        result.replace("&quot;", "\"");
        return result.simplified();
    }

    QString rewriteLinks(const QString& html, const QString& dictionaryId) const {
        QString result = html;

        // entry:// -> unidict://lookup?word=
        result.replace(QRegularExpression(R"(entry://([^<"\s]+))"),
                      R"(unidict://lookup?word=\1)");

        // bword:// -> unidict://lookup?word=
        result.replace(QRegularExpression(R"(bword://([^<"\s]+))"),
                      R"(unidict://lookup?word=\1)");

        // @@@LINK=word -> unidict://lookup?word=word
        result.replace(QRegularExpression(R"(@@@LINK=([^\s<>"']+))"),
                      R"(unidict://lookup?word=\1)");

        return result;
    }

    // 交叉引用导航状态
    QStringList backStack;
    QStringList forwardStack;
    QString currentWord;
    QString currentDictionary;
};

QString LookupAdapter::sanitizeHtml(const QString& html) const {
    if (!m_p0) return html;
    return m_p0->sanitize(html);
}

QString LookupAdapter::extractTextFromHtml(const QString& html) const {
    if (!m_p0) return html;
    return m_p0->extractText(html);
}

QString LookupAdapter::rewriteResourceUrls(const QString& html, const QString& dictionaryId) const {
    // 资源 URL 重写
    // 完整实现应调用 MddResourceManager
    Q_UNUSED(dictionaryId);
    return html;
}

QString LookupAdapter::rewriteCrossReferenceLinks(const QString& html, const QString& dictionaryId) const {
    if (!m_p0) return html;
    return m_p0->rewriteLinks(html, dictionaryId);
}

bool LookupAdapter::canGoBack() const {
    return m_p0 && !m_p0->backStack.isEmpty();
}

bool LookupAdapter::canGoForward() const {
    return m_p0 && !m_p0->forwardStack.isEmpty();
}

QString LookupAdapter::goBack() {
    if (!m_p0 || m_p0->backStack.isEmpty()) return QString();

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty()) {
        m_p0->forwardStack.push(m_p0->currentWord);
    }

    // 返回上一个
    QString word = m_p0->backStack.takeLast();
    m_p0->currentWord = word;
    return word;
}

QString LookupAdapter::goForward() {
    if (!m_p0 || m_p0->forwardStack.isEmpty()) return QString();

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty()) {
        m_p0->backStack.push(m_p0->currentWord);
    }

    // 前进到下一个
    QString word = m_p0->forwardStack.takeLast();
    m_p0->currentWord = word;
    return word;
}

void LookupAdapter::navigateToWord(const QString& word, const QString& dictionaryId) {
    if (!m_p0) return;

    // 保存当前状态
    if (!m_p0->currentWord.isEmpty() && m_p0->currentWord != word) {
        m_p0->backStack.push(m_p0->currentWord);
    }

    m_p0->currentWord = word;
    m_p0->currentDictionary = dictionaryId;

    // 清空 forward stack
    m_p0->forwardStack.clear();
}

void LookupAdapter::clearNavigationHistory() {
    if (m_p0) {
        m_p0->backStack.clear();
        m_p0->forwardStack.clear();
    }
}

int LookupAdapter::navigationHistorySize() const {
    if (!m_p0) return 0;
    return m_p0->backStack.size() + m_p0->forwardStack.size();
}

QVariantList LookupAdapter::aggregateLookup(const QString& word, const QVariantMap& options) const {
    // 多词典聚合查询
    // 简化实现：返回单词典查询结果
    // 完整实现应调用 DictionaryAggregator
    Q_UNUSED(options);

    QVariantList results;
    QString def = m_service->lookupDefinition(word, false, 10);

    QVariantMap entry;
    entry["word"] = word;
    entry["definition"] = def;
    entry["dictionary"] = "default";
    entry["relevance"] = 1.0;

    if (!def.startsWith("Word not found")) {
        results.append(entry);
    }

    return results;
}

QVariantList LookupAdapter::getDictionariesByCategory(const QString& category) const {
    // 按分类获取词典列表
    QVariantList results;
    auto dicts = DictionaryManager::instance().getLoadedDictionaries();

    for (const auto& dictId : dicts) {
        // 简化：所有词典返回，实际应按分类过滤
        QVariantMap info;
        info["id"] = dictId;
        info["category"] = category;
        results.append(info);
    }

    return results;
}

void LookupAdapter::setDictionaryPriority(const QString& dictionaryId, int priority) {
    // 设置词典优先级
    // 完整实现应调用 DictionaryAggregator::set_dictionary_priority()
    Q_UNUSED(dictionaryId);
    Q_UNUSED(priority);
}

void LookupAdapter::setDictionaryEnabled(const QString& dictionaryId, bool enabled) {
    // 设置词典启用状态
    // 完整实现应调用 DictionaryAggregator::set_dictionary_enabled()
    Q_UNUSED(dictionaryId);
    Q_UNUSED(enabled);
}

// ============================================================================
// P1 剪贴板监听功能实现
// ============================================================================

void LookupAdapter::startClipboardMonitoring() {
    m_clipboardMonitor->start();
}

void LookupAdapter::stopClipboardMonitoring() {
    m_clipboardMonitor->stop();
}

bool LookupAdapter::isClipboardMonitoring() const {
    return m_clipboardMonitor->isMonitoring();
}

void LookupAdapter::setClipboardPollInterval(int milliseconds) {
    m_clipboardMonitor->setPollInterval(milliseconds);
}

void LookupAdapter::setClipboardMinWordLength(int length) {
    m_clipboardMonitor->setMinWordLength(length);
}

void LookupAdapter::setClipboardMaxWordLength(int length) {
    m_clipboardMonitor->setMaxWordLength(length);
}

void LookupAdapter::addClipboardExcludePattern(const QString& pattern) {
    m_clipboardMonitor->addExcludePattern(pattern);
}

void LookupAdapter::clearClipboardExcludePatterns() {
    m_clipboardMonitor->clearExcludePatterns();
}

void LookupAdapter::setClipboardAutoLookupEnabled(bool enabled) {
    m_clipboardAutoLookupEnabled = enabled;
}

bool LookupAdapter::isClipboardAutoLookupEnabled() const {
    return m_clipboardAutoLookupEnabled;
}

// ============================================================================
// P1 全局热键功能实现
// ============================================================================

bool LookupAdapter::registerGlobalHotkey(const QString& action, const QString& keySequence) {
    return m_globalHotkeys->registerHotkey(action, keySequence);
}

void LookupAdapter::unregisterGlobalHotkey(const QString& action) {
    m_globalHotkeys->unregisterHotkey(action);
}

void LookupAdapter::unregisterAllGlobalHotkeys() {
    m_globalHotkeys->unregisterAllHotkeys();
}

QStringList LookupAdapter::registeredHotkeyActions() const {
    return m_globalHotkeys->registeredActions();
}

QString LookupAdapter::getHotkeyForAction(const QString& action) const {
    return m_globalHotkeys->getHotkeyForAction(action);
}

void LookupAdapter::setGlobalHotkeysEnabled(bool enabled) {
    m_globalHotkeys->setEnabled(enabled);
}

bool LookupAdapter::isGlobalHotkeysEnabled() const {
    return m_globalHotkeys->isEnabled();
}

bool LookupAdapter::isGlobalHotkeysSupported() {
    return GlobalHotkeys::isPlatformSupported();
}
