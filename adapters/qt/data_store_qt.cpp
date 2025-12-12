#include "data_store_qt.h"

namespace UnidictAdaptersQt {

static inline std::string cs(const QString& s) { return std::string(s.toUtf8().constData()); }
static inline QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }

DataStoreQt& DataStoreQt::instance() { static DataStoreQt ds; return ds; }

DataStoreQt::DataStoreQt() : impl_(new UnidictCoreStd::DataStoreStd) {}

void DataStoreQt::setStoragePath(const QString& filePath) { impl_->set_storage_path(cs(filePath)); }
QString DataStoreQt::storagePath() const { return qs(impl_->storage_path()); }

void DataStoreQt::addSearchHistory(const QString& word) { impl_->add_search_history(cs(word)); }
QStringList DataStoreQt::getSearchHistory(int limit) const {
    QStringList out; auto v = impl_->get_search_history(limit); out.reserve((int)v.size());
    for (auto& s : v) out.append(qs(s)); return out;
}
void DataStoreQt::clearHistory() { impl_->clear_history(); }

void DataStoreQt::addVocabularyItem(const UnidictCore::DictionaryEntry& entry) {
    impl_->add_vocabulary_item({ cs(entry.word), cs(entry.definition) });
}

void DataStoreQt::addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt) {
    UnidictCoreStd::VocabItemStd vi;
    vi.word = cs(word);
    vi.definition = cs(definition);
    vi.added_at = static_cast<long long>(addedAt);
    impl_->add_vocabulary_item(vi);
}

QList<UnidictCore::DictionaryEntry> DataStoreQt::getVocabulary() const {
    QList<UnidictCore::DictionaryEntry> out;
    for (const auto& it : impl_->get_vocabulary()) {
        UnidictCore::DictionaryEntry e; e.word = qs(it.word); e.definition = qs(it.definition); out.push_back(e);
    }
    return out;
}

QVariantList DataStoreQt::getVocabularyMeta() const {
    QVariantList out;
    for (const auto& it : impl_->get_vocabulary()) {
        QVariantMap m;
        m["word"] = qs(it.word);
        m["definition"] = qs(it.definition);
        m["added_at"] = static_cast<qlonglong>(it.added_at);
        out.push_back(m);
    }
    return out;
}

void DataStoreQt::removeVocabularyItem(const QString& word) {
    impl_->remove_vocabulary_item(cs(word));
}

void DataStoreQt::clearVocabulary() { impl_->clear_vocabulary(); }
bool DataStoreQt::exportVocabularyCSV(const QString& filePath) const { return impl_->export_vocabulary_csv(cs(filePath)); }

} // namespace UnidictAdaptersQt
