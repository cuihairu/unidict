#include "vocab_store_adapter.h"

#include <QRegularExpression>

#include "unidict_core.h"
#include "data_store_qt.h"

using namespace UnidictCore;
using UnidictAdaptersQt::DataStoreQt;

namespace {
// Keep vocabulary storage readable even when UI renders rich-text definitions.
QString stripHtmlForStorage(const QString& in) {
    if (!in.contains('<') || !in.contains('>')) return in;
    QString s = in;
    s.replace(QRegularExpression("<\\s*br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.replace(QRegularExpression("</\\s*p\\s*>", QRegularExpression::CaseInsensitiveOption), "\n\n");
    s.replace(QRegularExpression("</\\s*div\\s*>", QRegularExpression::CaseInsensitiveOption), "\n");
    s.remove(QRegularExpression("<[^>]+>"));
    s.replace("&nbsp;", " ");
    return s.trimmed();
}
}  // namespace

VocabStoreAdapter::VocabStoreAdapter(QObject* parent) : QObject(parent) {}

void VocabStoreAdapter::addToVocabulary(const QString& word, const QString& definition) {
    DictionaryEntry e;
    e.word = word;
    e.definition = stripHtmlForStorage(definition);
    DataStoreQt::instance().addVocabularyItem(e);
}

QStringList VocabStoreAdapter::searchHistory(int limit) const {
    return DataStoreQt::instance().getSearchHistory(limit);
}

QVariantList VocabStoreAdapter::searchHistoryEntries(int limit) const {
    QVariantList out;
    const auto entries = DataStoreQt::instance().getSearchHistoryEntries(limit);
    for (const auto& e : entries) {
        QVariantMap m;
        m["word"] = e.query;
        m["count"] = e.hitCount;
        m["pinned"] = e.pinned;
        out.push_back(m);
    }
    return out;
}

QVariantList VocabStoreAdapter::vocabulary() const {
    QVariantList out;
    const auto items = DataStoreQt::instance().getVocabulary();
    for (const auto& e : items) {
        QVariantMap m; m["word"] = e.word; m["definition"] = e.definition; out.push_back(m);
    }
    return out;
}

QVariantList VocabStoreAdapter::vocabularyMeta() const {
    return DataStoreQt::instance().getVocabularyMeta();
}

// —— M3-B 生词本编辑：标签/笔记（core DataStore M3 口径转发，标签与
// 笔记操作 core 内部自带落盘，这里不重复 save） ——

QVariantList VocabStoreAdapter::vocabularyByTag(const QString& tag) const {
    return DataStoreQt::instance().getVocabularyByTag(tag);
}

bool VocabStoreAdapter::addVocabTag(const QString& word, const QString& tag) {
    return DataStoreQt::instance().addVocabularyItemTag(word, tag);
}

bool VocabStoreAdapter::removeVocabTag(const QString& word, const QString& tag) {
    return DataStoreQt::instance().removeVocabularyItemTag(word, tag);
}

void VocabStoreAdapter::setVocabNote(const QString& word, const QString& text) {
    DataStoreQt::instance().setNote(word, text);
}

void VocabStoreAdapter::setVocabNoteColored(const QString& word, const QString& text, int color) {
    DataStoreQt::instance().setNoteWithColor(word, text, color);
}

QString VocabStoreAdapter::getVocabNote(const QString& word) const {
    return DataStoreQt::instance().getNote(word);
}

int VocabStoreAdapter::getVocabNoteColor(const QString& word) const {
    return DataStoreQt::instance().getNoteColor(word);
}

QVariantList VocabStoreAdapter::searchNotes(const QString& query) const {
    return DataStoreQt::instance().getNotesByText(query);
}

bool VocabStoreAdapter::setVocabularySkill(const QString& word, const QString& skill, int level) {
    return DataStoreQt::instance().setVocabularyItemSkill(word, skill, level);
}

void VocabStoreAdapter::removeVocabularyWord(const QString& word) {
    DataStoreQt::instance().removeVocabularyItem(word);
}

void VocabStoreAdapter::clearHistory() {
    DataStoreQt::instance().clearHistory();
}

void VocabStoreAdapter::clearVocabulary() {
    DataStoreQt::instance().clearVocabulary();
}

bool VocabStoreAdapter::exportVocabCsv(const QString& path) const {
    return DataStoreQt::instance().exportVocabularyCSV(path);
}

QVariantMap VocabStoreAdapter::importVocabCsv(const QString& path) {
    return DataStoreQt::instance().importVocabularyCSV(path);
}

bool VocabStoreAdapter::exportNotesHtml(const QString& path) const {
    return DataStoreQt::instance().exportNotesHtml(path);
}

bool VocabStoreAdapter::exportNotesPdf(const QString& path) const {
    return DataStoreQt::instance().exportNotesPdf(path);
}
