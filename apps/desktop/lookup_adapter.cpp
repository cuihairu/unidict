#include "lookup_adapter.h"

#include <QRegularExpression>
#include <QTextToSpeech>

#include "lookup_service.h"
#include "unidict_core.h"
#include "data_store.h"

using namespace UnidictCore;

LookupAdapter::LookupAdapter(QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<LookupService>())
    , m_tts(std::make_unique<QTextToSpeech>(this)) {
}

QString LookupAdapter::lookupDefinition(const QString& word) {
    const QString def = m_service->lookupDefinition(word, true, 10);
    if (!def.startsWith("Word not found")) {
        DataStore::instance().addSearchHistory(word);
    }
    return def;
}

QStringList LookupAdapter::suggestPrefix(const QString& prefix, int maxResults) const {
    return m_service->suggestPrefix(prefix, maxResults);
}

QStringList LookupAdapter::loadedDictionaries() const {
    return DictionaryManager::instance().getLoadedDictionaries();
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
    DictionaryEntry e; e.word = word; e.definition = definition;
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

bool LookupAdapter::isSpeaking() const {
    return m_tts && m_tts->state() == QTextToSpeech::Speaking;
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

void LookupAdapter::setRate(double rate) {
    if (m_tts) {
        // 将0.1-2.0映射到Qt的-1.0到1.0
        double qtRate = (rate - 1.0) * 2.0;
        qtRate = qBound(-1.0, qtRate, 1.0);
        m_tts->setRate(qtRate);
    }
}

void LookupAdapter::setPitch(double pitch) {
    if (m_tts) {
        double qtPitch = qBound(-1.0, pitch, 1.0);
        m_tts->setPitch(qtPitch);
    }
}

void LookupAdapter::setVolume(double volume) {
    if (m_tts) {
        double qtVolume = qBound(0.0, volume, 1.0);
        m_tts->setVolume(qtVolume);
    }
}
