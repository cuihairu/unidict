#include "data_store.h"
#include "data_store_qt.h"

namespace UnidictCore {

DataStore& DataStore::instance() { static DataStore ds; return ds; }

DataStore::DataStore() = default;

void DataStore::setStoragePath(const QString& filePath) { ::UnidictAdaptersQt::DataStoreQt::instance().setStoragePath(filePath); }
QString DataStore::storagePath() const { return ::UnidictAdaptersQt::DataStoreQt::instance().storagePath(); }

void DataStore::addSearchHistory(const QString& word) { ::UnidictAdaptersQt::DataStoreQt::instance().addSearchHistory(word); }
QStringList DataStore::getSearchHistory(int limit) const { return ::UnidictAdaptersQt::DataStoreQt::instance().getSearchHistory(limit); }
void DataStore::clearHistory() { ::UnidictAdaptersQt::DataStoreQt::instance().clearHistory(); }
void DataStore::restoreSearchHistory(const QStringList& queries) { ::UnidictAdaptersQt::DataStoreQt::instance().restoreSearchHistory(queries); }

void DataStore::addVocabularyItem(const DictionaryEntry& entry) { ::UnidictAdaptersQt::DataStoreQt::instance().addVocabularyItem(entry); }
void DataStore::addVocabularyItemWithTime(const QString& word, const QString& definition, qlonglong addedAt) { ::UnidictAdaptersQt::DataStoreQt::instance().addVocabularyItemWithTime(word, definition, addedAt); }
void DataStore::removeVocabularyItem(const QString& word) { ::UnidictAdaptersQt::DataStoreQt::instance().removeVocabularyItem(word); }
bool DataStore::setVocabularyItemTags(const QString& word, const QStringList& tags) { return ::UnidictAdaptersQt::DataStoreQt::instance().setVocabularyItemTags(word, tags); }
bool DataStore::addVocabularyItemTag(const QString& word, const QString& tag) { return ::UnidictAdaptersQt::DataStoreQt::instance().addVocabularyItemTag(word, tag); }
bool DataStore::removeVocabularyItemTag(const QString& word, const QString& tag) { return ::UnidictAdaptersQt::DataStoreQt::instance().removeVocabularyItemTag(word, tag); }
QVariantList DataStore::getVocabularyByTag(const QString& tag) const { return ::UnidictAdaptersQt::DataStoreQt::instance().getVocabularyByTag(tag); }
bool DataStore::setVocabularyItemSkill(const QString& word, const QString& skill, int level) { return ::UnidictAdaptersQt::DataStoreQt::instance().setVocabularyItemSkill(word, skill, level); }
QList<DictionaryEntry> DataStore::getVocabulary() const { return ::UnidictAdaptersQt::DataStoreQt::instance().getVocabulary(); }
QVariantList DataStore::getVocabularyMeta() const { return ::UnidictAdaptersQt::DataStoreQt::instance().getVocabularyMeta(); }
void DataStore::clearVocabulary() { ::UnidictAdaptersQt::DataStoreQt::instance().clearVocabulary(); }
bool DataStore::exportVocabularyCSV(const QString& filePath) const { return ::UnidictAdaptersQt::DataStoreQt::instance().exportVocabularyCSV(filePath); }

void DataStore::setNote(const QString& word, const QString& text) { ::UnidictAdaptersQt::DataStoreQt::instance().setNote(word, text); }
QString DataStore::getNote(const QString& word) const { return ::UnidictAdaptersQt::DataStoreQt::instance().getNote(word); }
QVariantList DataStore::getNotes() const { return ::UnidictAdaptersQt::DataStoreQt::instance().getNotes(); }
QVariantList DataStore::getNotesByText(const QString& query) const { return ::UnidictAdaptersQt::DataStoreQt::instance().getNotesByText(query); }
bool DataStore::exportNotesHtml(const QString& filePath) const { return ::UnidictAdaptersQt::DataStoreQt::instance().exportNotesHtml(filePath); }
bool DataStore::exportNotesPdf(const QString& filePath) const { return ::UnidictAdaptersQt::DataStoreQt::instance().exportNotesPdf(filePath); }

void DataStore::setPronRecord(const QString& word, double lastScore, double bestScore,
                              int attempts, qlonglong lastAt) {
    ::UnidictAdaptersQt::DataStoreQt::instance().setPronRecord(word, lastScore, bestScore, attempts, lastAt);
}
QVariantMap DataStore::getPronRecord(const QString& word) const { return ::UnidictAdaptersQt::DataStoreQt::instance().getPronRecord(word); }
QVariantList DataStore::getPronRecords() const { return ::UnidictAdaptersQt::DataStoreQt::instance().getPronRecords(); }
void DataStore::clearPronRecords() { ::UnidictAdaptersQt::DataStoreQt::instance().clearPronRecords(); }

}
