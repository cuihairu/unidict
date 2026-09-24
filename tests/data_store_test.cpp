#include <QtTest>

#include "core/data_store.h"

using namespace UnidictCore;

class DataStoreTest : public QObject {
    Q_OBJECT
private slots:
    void history_add_dedupe_order();
    void vocab_add_and_clear();
    void vocab_tags_persist_and_meta();
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

QTEST_MAIN(DataStoreTest)
#include "data_store_test.moc"

