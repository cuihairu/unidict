// Compatibility implementation: delegates to the Qt adapter, which wraps the
// std-only engine. The old in-file Trie implementation was removed when the
// engine moved to core/std (see index_engine.h).

#include "index_engine.h"

#include "index_engine_qt.h"

namespace UnidictCore {

IndexEngine::IndexEngine() : impl_(std::make_unique<UnidictAdaptersQt::IndexEngineQt>()) {}

IndexEngine::~IndexEngine() = default;

void IndexEngine::addWord(const QString& word, const QString& dictionaryId) {
    impl_->addWord(word, dictionaryId);
}

void IndexEngine::removeWord(const QString& word, const QString& dictionaryId) {
    impl_->removeWord(word, dictionaryId);
}

void IndexEngine::clearDictionary(const QString& dictionaryId) {
    impl_->clearDictionary(dictionaryId);
}

QStringList IndexEngine::exactMatch(const QString& word) const {
    return impl_->exactMatch(word);
}

QStringList IndexEngine::prefixSearch(const QString& prefix, int maxResults) const {
    return impl_->prefixSearch(prefix, maxResults);
}

QStringList IndexEngine::fuzzySearch(const QString& word, int maxResults) const {
    return impl_->fuzzySearch(word, maxResults);
}

QStringList IndexEngine::wildcardSearch(const QString& pattern, int maxResults) const {
    return impl_->wildcardSearch(pattern, maxResults);
}

QStringList IndexEngine::regexSearch(const QString& pattern, int maxResults) const {
    return impl_->regexSearch(pattern, maxResults);
}

QStringList IndexEngine::getAllWords() const {
    return impl_->getAllWords();
}

int IndexEngine::getWordCount() const {
    return impl_->getWordCount();
}

QStringList IndexEngine::getDictionariesForWord(const QString& word) const {
    return impl_->getDictionariesForWord(word);
}

void IndexEngine::buildIndex() {
    impl_->buildIndex();
}

void IndexEngine::saveIndex(const QString& filePath) const {
    impl_->saveIndex(filePath);
}

bool IndexEngine::loadIndex(const QString& filePath) {
    return impl_->loadIndex(filePath);
}

}
