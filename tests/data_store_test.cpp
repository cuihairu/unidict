#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include "core/data_store.h"

using namespace UnidictCore;

class DataStoreTest : public QObject {
    Q_OBJECT
private slots:
    void history_add_dedupe_order();
    void vocab_add_and_clear();
    void vocab_tags_persist_and_meta();
    void notes_upsert_remove_and_persist();
    void pron_records_and_gates();
};

void DataStoreTest::history_add_dedupe_order() {
    auto& ds = DataStore::instance();
    ds.setStoragePath("data/unidict_test.json");
    ds.clearHistory();
    ds.addSearchHistory("hello");
    ds.addSearchHistory("world");
    ds.addSearchHistory("hello"); // move to end
    const QStringList h = ds.getSearchHistory(10);
    QCOMPARE(h.size(), 2);
    QCOMPARE(h.at(0), QString("world"));
    QCOMPARE(h.at(1), QString("hello"));
}

void DataStoreTest::vocab_add_and_clear() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry e; e.word = "foo"; e.definition = "bar"; ds.addVocabularyItem(e);
    auto items = ds.getVocabulary();
    QVERIFY(items.size() >= 1);
    bool found = false;
    for (const auto& it : items) if (it.word == "foo" && it.definition == "bar") { found = true; break; }
    QVERIFY(found);
    ds.clearVocabulary();
}

// 标签链路（Qt 门面）：setVocabularyItemTags 命中/未命中 + getVocabularyMeta 带 tags
void DataStoreTest::vocab_tags_persist_and_meta() {
    auto& ds = DataStore::instance();
    ds.clearVocabulary();
    DictionaryEntry e; e.word = "persist"; e.definition = "to last"; ds.addVocabularyItem(e);

    QVERIFY(!ds.setVocabularyItemTags("missing-word", {"x"}));
    QVERIFY(ds.setVocabularyItemTags("persist", {"exam", "high-freq"}));

    bool found = false;
    for (const auto& meta : ds.getVocabularyMeta()) {
        const QVariantMap m = meta.toMap();
        if (m.value("word").toString() == QLatin1String("persist")) {
            found = true;
            const QVariantList tags = m.value("tags").toList();
            QCOMPARE(tags.size(), 2);
            QCOMPARE(tags.at(0).toString(), QString("exam"));
            QCOMPARE(tags.at(1).toString(), QString("high-freq"));
        }
    }
    QVERIFY(found);
    ds.clearVocabulary();
}

// 笔记链路（Qt 门面）：upsert/大小写不敏感/空串删除 + getNotes 元数据
void DataStoreTest::notes_upsert_remove_and_persist() {
    auto& ds = DataStore::instance();
    ds.setNote("note_word", "first");
    ds.setNote("note_word", "second");           // upsert 覆盖
    ds.setNote("NOTE_WORD", "case-insensitive"); // 同一条
    QCOMPARE(ds.getNote("note_word"), QString("case-insensitive"));

    bool found = false;
    for (const auto& meta : ds.getNotes()) {
        const QVariantMap m = meta.toMap();
        if (m.value("word").toString() == QLatin1String("note_word")) {
            found = true;
            QCOMPARE(m.value("text").toString(), QString("case-insensitive"));
            QVERIFY(m.value("updated_at").toLongLong() > 0);
        }
    }
    QVERIFY(found);

    ds.setNote("note_word", ""); // 空串删除
    QCOMPARE(ds.getNote("note_word"), QString());
    QVERIFY(ds.getNotes().isEmpty());
}

// Q-6 覆盖收口：门面上此前无测试的转发——storagePath 读写、发音练习记录
// 增查清、load/save/ensureLoaded 的兼容桩
void DataStoreTest::pron_records_and_gates() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    auto& ds = DataStore::instance();
    const QString storage = QDir(dir.path()).filePath("pron_store.json");
    ds.setStoragePath(storage);
    QCOMPARE(ds.storagePath(), storage);

    ds.clearPronRecords();
    QVERIFY(ds.getPronRecord("never").isEmpty());

    ds.setPronRecord("apple", 71.5, 88.0, 3, 1234567890);
    const QVariantMap rec = ds.getPronRecord("apple");
    QCOMPARE(rec.value("word").toString(), QString("apple"));
    QCOMPARE(rec.value("attempts").toInt(), 3);
    QVERIFY(qFuzzyCompare(rec.value("best_score").toDouble(), 88.0));
    QVERIFY(qFuzzyCompare(rec.value("last_score").toDouble(), 71.5));
    QCOMPARE(rec.value("last_at").toLongLong(), qlonglong(1234567890));

    const QVariantList all = ds.getPronRecords();
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.at(0).toMap().value("word").toString(), QString("apple"));

    ds.clearPronRecords();
    QVERIFY(ds.getPronRecords().isEmpty());

    // 兼容桩：Qt 门面实时落盘，load/save 恒真；ensureLoaded 是私有零调用
    // 桩，已在 core/data_store.cpp 标注不可达
    QVERIFY(ds.load());
    QVERIFY(ds.save());
}

QTEST_MAIN(DataStoreTest)
#include "data_store_test.moc"

