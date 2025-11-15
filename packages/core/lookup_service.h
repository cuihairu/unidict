#ifndef LOOKUP_SERVICE_H
#define LOOKUP_SERVICE_H

#include <QString>
#include <QStringList>

namespace UnidictCore {

// Thin service to encapsulate lookup strategy and fallbacks
class LookupService {
public:
    // Exact lookup; if not found and allowSuggest = true, returns suggestion list joined by newlines
    QString lookupDefinition(const QString& word, bool allowSuggest = true, int suggestMax = 10) const;
    QStringList suggestPrefix(const QString& prefix, int maxResults = 10) const;
    QStringList suggestFuzzy(const QString& word, int maxResults = 10) const;
    QStringList searchWildcard(const QString& pattern, int maxResults = 10) const;
};

}

#endif // LOOKUP_SERVICE_H

