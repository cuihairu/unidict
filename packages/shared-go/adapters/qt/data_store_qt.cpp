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

QList<UnidictCore::DictionaryEntry> DataStoreQt::getVocabulary() const {
    QList<UnidictCore::DictionaryEntry> out;
    for (const auto& it : impl_->get_vocabulary()) {
        UnidictCore::DictionaryEntry e; e.word = qs(it.word); e.definition = qs(it.definition); out.push_back(e);
    }
    return out;
}

void DataStoreQt::clearVocabulary() { impl_->clear_vocabulary(); }
bool DataStoreQt::exportVocabularyCSV(const QString& filePath) const { return impl_->export_vocabulary_csv(cs(filePath)); }

} // namespace UnidictAdaptersQt

