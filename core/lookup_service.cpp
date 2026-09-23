#include "lookup_service.h"

#include <QRegularExpression>

#include "unidict_core.h"

namespace UnidictCore {

QString LookupService::lookupDefinition(const QString& word, bool allowSuggest, int suggestMax) const {
    const LookupResult r = DictionaryManager::instance().searchWord(word);
    if (r.success) return r.entry.definition;
    if (!allowSuggest) return QString("Word not found: ") + word;
    QStringList sug = r.suggestions;
    if (sug.isEmpty()) sug = DictionaryManager::instance().searchSimilar(word, suggestMax);
    if (sug.isEmpty()) return QString("Word not found: ") + word;
    if (sug.size() > suggestMax) sug = sug.mid(0, suggestMax);
    return QString("Word not found: %1\nDid you mean:\n%2").arg(word, sug.join("\n"));
}

QStringList LookupService::suggestPrefix(const QString& prefix, int maxResults) const {
    return DictionaryManager::instance().searchSimilar(prefix, maxResults);
}

QStringList LookupService::suggestFuzzy(const QString& word, int maxResults) const {
    return DictionaryManager::instance().searchSimilar(word, maxResults);
}

QStringList LookupService::searchWildcard(const QString& pattern, int maxResults) const {
    // DictionaryManager exposes similarity search only; apply the wildcard as a
    // filter over its candidates.
    const QString trimmed = pattern.trimmed();
    if (trimmed.isEmpty()) return {};
    const QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(trimmed),
                                   QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) return {};
    const QStringList candidates = DictionaryManager::instance().searchSimilar(trimmed, maxResults * 4);
    QStringList results;
    for (const QString& candidate : candidates) {
        if (results.size() >= maxResults) break;
        const auto match = regex.match(candidate);
        if (match.hasMatch() && match.capturedLength() == candidate.size()) results.append(candidate);
    }
    return results;
}

}
