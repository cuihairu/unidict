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

// 结构化面（P-7 双存储合一：legacy manager 的查词记录转此单源）
void DataStoreQt::addSearchHistoryEntry(const QString& query, bool success,
                                        const QString& dictionaryName) {
    impl_->add_search_history_entry({cs(query), success, cs(dictionaryName), false});
}

QList<SearchHistoryEntry> DataStoreQt::getSearchHistoryEntries(int limit) const {
    QList<SearchHistoryEntry> out;
    const auto v = impl_->get_search_history_entries(limit);
    out.reserve((int)v.size());
    for (const auto& e : v) {
        SearchHistoryEntry entry;
        entry.query = qs(e.query);
        entry.success = e.success;
        entry.dictionaryName = qs(e.dictionary_name);
        entry.pinned = e.pinned;
        out.append(entry);
    }
    return out;
}

bool DataStoreQt::setSearchHistoryPinned(const QString& query, bool pinned) {
    return impl_->set_search_history_pinned(cs(query), pinned);
}

bool DataStoreQt::removeSearchHistoryItem(const QString& query) {
    return impl_->remove_search_history(cs(query));
}

void DataStoreQt::restoreSearchHistory(const QStringList& queries) {
    std::vector<UnidictCoreStd::SearchHistoryEntryStd> entries;
    entries.reserve(queries.size());
    for (const QString& q : queries) {
        entries.push_back({cs(q), true, std::string(), false});
    }
    impl_->set_search_history(std::move(entries));
}

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

namespace {

// VocabItemStd → [{word,definition,added_at,tags}]（getVocabularyMeta/ByTag 共用）
QVariantMap vocab_meta_map(const UnidictCoreStd::VocabItemStd& it) {
    QVariantMap m;
    m["word"] = qs(it.word);
    m["definition"] = qs(it.definition);
    m["added_at"] = static_cast<qlonglong>(it.added_at);
    QVariantList tags;
    for (const auto& tag : it.tags) tags.append(qs(tag));
    m["tags"] = tags;
    return m;
}

} // namespace

QVariantList DataStoreQt::getVocabularyMeta() const {
    QVariantList out;
    for (const auto& it : impl_->get_vocabulary()) {
        out.push_back(vocab_meta_map(it));
    }
    return out;
}

bool DataStoreQt::setVocabularyItemTags(const QString& word, const QStringList& tags) {
    std::vector<std::string> t;
    t.reserve(static_cast<size_t>(tags.size()));
    for (const QString& tag : tags) t.push_back(cs(tag));
    return impl_->set_vocabulary_item_tags(cs(word), t);
}

bool DataStoreQt::addVocabularyItemTag(const QString& word, const QString& tag) {
    return impl_->add_vocabulary_item_tag(cs(word), cs(tag));
}

bool DataStoreQt::removeVocabularyItemTag(const QString& word, const QString& tag) {
    return impl_->remove_vocabulary_item_tag(cs(word), cs(tag));
}

QVariantList DataStoreQt::getVocabularyByTag(const QString& tag) const {
    QVariantList out;
    for (const auto& it : impl_->get_vocabulary_by_tag(cs(tag))) {
        out.push_back(vocab_meta_map(it));
    }
    return out;
}

void DataStoreQt::removeVocabularyItem(const QString& word) {
    impl_->remove_vocabulary_item(cs(word));
}

void DataStoreQt::clearVocabulary() { impl_->clear_vocabulary(); }
bool DataStoreQt::exportVocabularyCSV(const QString& filePath) const { return impl_->export_vocabulary_csv(cs(filePath)); }

void DataStoreQt::setNote(const QString& word, const QString& text) { impl_->set_note(cs(word), cs(text)); }
QString DataStoreQt::getNote(const QString& word) const { return qs(impl_->get_note(cs(word))); }

QVariantList DataStoreQt::getNotes() const {
    QVariantList out;
    for (const auto& it : impl_->get_notes()) {
        QVariantMap m;
        m["word"] = qs(it.word);
        m["text"] = qs(it.text);
        m["updated_at"] = static_cast<qlonglong>(it.updated_at);
        out.push_back(m);
    }
    return out;
}

void DataStoreQt::setPronRecord(const QString& word, double lastScore, double bestScore,
                                int attempts, qlonglong lastAt) {
    UnidictCoreStd::PronRecordStd r;
    r.word = cs(word);
    r.last_score = lastScore;
    r.best_score = bestScore;
    r.attempts = attempts;
    r.last_at = static_cast<long long>(lastAt);
    impl_->set_pron_record(r);
}

QVariantMap DataStoreQt::getPronRecord(const QString& word) const {
    QVariantMap m;
    const auto rec = impl_->get_pron_record(cs(word));
    if (!rec) {
        return m;  // 空 map = 没练过（面板据此走"首次记录"分支）
    }
    m["word"] = qs(rec->word);
    m["last_score"] = rec->last_score;
    m["best_score"] = rec->best_score;
    m["attempts"] = rec->attempts;
    m["last_at"] = static_cast<qlonglong>(rec->last_at);
    return m;
}

QVariantList DataStoreQt::getPronRecords() const {
    QVariantList out;
    for (const auto& it : impl_->get_pron_records()) {
        QVariantMap m;
        m["word"] = qs(it.word);
        m["last_score"] = it.last_score;
        m["best_score"] = it.best_score;
        m["attempts"] = it.attempts;
        m["last_at"] = static_cast<qlonglong>(it.last_at);
        out.push_back(m);
    }
    return out;
}

void DataStoreQt::clearPronRecords() { impl_->clear_pron_records(); }

} // namespace UnidictAdaptersQt
