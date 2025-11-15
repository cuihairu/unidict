#include "index_engine_qt.h"

namespace UnidictAdaptersQt {

static inline QStringList to_qsl(const std::vector<std::string>& v) {
    QStringList out; out.reserve((int)v.size());
    for (const auto& s : v) out.append(QString::fromUtf8(s.c_str()));
    return out;
}

IndexEngineQt::IndexEngineQt() : impl_(new UnidictCoreStd::IndexEngineStd) {}

void IndexEngineQt::addWord(const QString& word, const QString& dictionaryId) {
    impl_->add_word(word.toUtf8().constData(), dictionaryId.toUtf8().constData());
}

void IndexEngineQt::removeWord(const QString& word, const QString& dictionaryId) {
    impl_->remove_word(word.toUtf8().constData(), dictionaryId.toUtf8().constData());
}

void IndexEngineQt::clearDictionary(const QString& dictionaryId) {
    impl_->clear_dictionary(dictionaryId.toUtf8().constData());
}

void IndexEngineQt::buildIndex() { impl_->build_index(); }

QStringList IndexEngineQt::exactMatch(const QString& word) const {
    return to_qsl(impl_->exact_match(word.toUtf8().constData()));
}

QStringList IndexEngineQt::prefixSearch(const QString& prefix, int maxResults) const {
    return to_qsl(impl_->prefix_search(prefix.toUtf8().constData(), maxResults));
}

QStringList IndexEngineQt::fuzzySearch(const QString& word, int maxResults) const {
    return to_qsl(impl_->fuzzy_search(word.toUtf8().constData(), maxResults));
}

QStringList IndexEngineQt::wildcardSearch(const QString& pattern, int maxResults) const {
    return to_qsl(impl_->wildcard_search(pattern.toUtf8().constData(), maxResults));
}

QStringList IndexEngineQt::regexSearch(const QString& pattern, int maxResults) const {
    return to_qsl(impl_->regex_search(pattern.toUtf8().constData(), maxResults));
}

QStringList IndexEngineQt::getAllWords() const { return to_qsl(impl_->all_words()); }

QStringList IndexEngineQt::getDictionariesForWord(const QString& word) const {
    return to_qsl(impl_->dictionaries_for_word(word.toUtf8().constData()));
}

int IndexEngineQt::getWordCount() const { return impl_->word_count(); }

bool IndexEngineQt::saveIndex(const QString& filePath) const {
    return impl_->save_index(filePath.toUtf8().constData());
}

bool IndexEngineQt::loadIndex(const QString& filePath) {
    return impl_->load_index(filePath.toUtf8().constData());
}

} // namespace UnidictAdaptersQt

