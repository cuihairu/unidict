#include "lookup_service.h"

#include "unidict_core.h"

namespace UnidictCore {

QString LookupService::lookupDefinition(const QString& word, bool allowSuggest, int suggestMax) const {
    const DictionaryEntry e = DictionaryManager::instance().searchWord(word);
    if (!e.word.isEmpty()) return e.definition;
    if (!allowSuggest) return QString("Word not found: ") + word;
    const QStringList sug = DictionaryManager::instance().prefixSearch(word, suggestMax);
    if (sug.isEmpty()) return QString("Word not found: ") + word;
    return QString("Word not found: %1\nDid you mean:\n%2").arg(word, sug.join("\n"));
}

QStringList LookupService::suggestPrefix(const QString& prefix, int maxResults) const {
    return DictionaryManager::instance().prefixSearch(prefix, maxResults);
}

QStringList LookupService::suggestFuzzy(const QString& word, int maxResults) const {
    return DictionaryManager::instance().fuzzySearch(word, maxResults);
}

QStringList LookupService::searchWildcard(const QString& pattern, int maxResults) const {
    return DictionaryManager::instance().wildcardSearch(pattern, maxResults);
}

}

