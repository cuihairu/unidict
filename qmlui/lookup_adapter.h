#ifndef LOOKUP_ADAPTER_H
#define LOOKUP_ADAPTER_H

#include <QObject>
#include <QStringList>
#include <QVariant>
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
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void clearVocabulary();
    Q_INVOKABLE int indexedWordCount() const;
    Q_INVOKABLE bool exportVocabCsv(const QString& path) const;

private:
    std::unique_ptr<UnidictCore::LookupService> m_service;
};

#endif // LOOKUP_ADAPTER_H
