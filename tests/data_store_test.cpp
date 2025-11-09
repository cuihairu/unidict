#include <QtTest>

#include "core/data_store.h"

using namespace UnidictCore;

class DataStoreTest : public QObject {
    Q_OBJECT
private slots:
    void history_add_dedupe_order();
    void vocab_add_and_clear();
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

QTEST_MAIN(DataStoreTest)
#include "data_store_test.moc"

