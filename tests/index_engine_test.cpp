#include <QDir>
#include <QTemporaryDir>
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
    void remove_clear_exact_counts_and_persistence();
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

// Q-6 覆盖收口：门面上此前无测试的转发——removeWord/clearDictionary/
// exactMatch/getAllWords/getWordCount/saveIndex/loadIndex
void IndexEngineTest::remove_clear_exact_counts_and_persistence() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    IndexEngine engine;
    engine.addWord("hello", "dict1");
    engine.addWord("persist", "dict2");
    engine.buildIndex();

    engine.removeWord("hello", "dict1");
    engine.clearDictionary("dict2");
    engine.buildIndex();
    QVERIFY(engine.exactMatch("hello").isEmpty());
    QVERIFY(engine.getAllWords().isEmpty());
    QCOMPARE(engine.getWordCount(), 0);

    // 落盘/加载往返：存下的词重载后仍能精确命中
    engine.addWord("persist", "dict2");
    engine.buildIndex();
    const QString idxPath = QDir(dir.path()).filePath("index.bin");
    engine.saveIndex(idxPath);
    QVERIFY(engine.loadIndex(idxPath));
    QCOMPARE(engine.exactMatch("persist"), (QStringList{"persist"}));
}

QTEST_MAIN(IndexEngineTest)
#include "index_engine_test.moc"
