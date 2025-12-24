#include "lookup_adapter.h"

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
    , m_tts(std::make_unique<QTextToSpeech>(this)) {
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
