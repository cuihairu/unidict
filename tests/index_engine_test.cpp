#include <QtTest>

#include "index_engine.h"

using namespace UnidictCore;

class IndexEngineTest : public QObject {
    Q_OBJECT

private slots:
    void prefixSearch_basic();
    void fuzzySearch_basic();
    void wildcardSearch_basic();
    void regexSearch_basic();
    void dictionariesForWord();
};

void IndexEngineTest::prefixSearch_basic() {
    IndexEngine engine;
    engine.addWord("hello", "dict1");
    engine.addWord("hell", "dict1");
    engine.addWord("world", "dict1");
    engine.buildIndex();

    const QStringList res = engine.prefixSearch("he", 10);
    QVERIFY(res.contains("hell"));
    QVERIFY(res.contains("hello"));
}

void IndexEngineTest::fuzzySearch_basic() {
    IndexEngine engine;
    engine.addWord("hello", "dict1");
    engine.addWord("world", "dict1");
    engine.buildIndex();

    const QStringList res = engine.fuzzySearch("hellp", 10);
    QVERIFY(res.contains("hello"));
}

void IndexEngineTest::wildcardSearch_basic() {
    IndexEngine engine;
    engine.addWord("hello", "dict1");
    engine.addWord("help", "dict1");
    engine.buildIndex();

    const QStringList res = engine.wildcardSearch("he*o", 10);
    QVERIFY(res.contains("hello"));
}

void IndexEngineTest::regexSearch_basic() {
    IndexEngine engine;
    engine.addWord("alpha", "dict1");
    engine.addWord("beta", "dict1");
    engine.buildIndex();
    const QStringList res = engine.regexSearch("^a.*a$", 10);
    QVERIFY(res.contains("alpha"));
    QVERIFY(!res.contains("beta"));
}

void IndexEngineTest::dictionariesForWord() {
    IndexEngine engine;
    engine.addWord("hello", "dict1");
    engine.addWord("hello", "dict2");
    engine.buildIndex();
    const QStringList ds = engine.getDictionariesForWord("hello");
    QVERIFY(ds.contains("dict1"));
    QVERIFY(ds.contains("dict2"));
}

QTEST_MAIN(IndexEngineTest)
#include "index_engine_test.moc"
