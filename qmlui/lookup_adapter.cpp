#include "lookup_adapter.h"

#include <QRegularExpression>

#include "lookup_service.h"
#include "unidict_core.h"
#include "data_store.h"

using namespace UnidictCore;

LookupAdapter::LookupAdapter(QObject* parent)
    : QObject(parent)
    , m_service(std::make_unique<LookupService>()) {
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
