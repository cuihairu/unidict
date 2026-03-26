#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>

#include "qmlui/lookup_adapter.h"
#include "core/unidict_core.h"

using namespace UnidictCore;

class LookupAdapterTest : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void reload_replaces_dictionaries_from_env();

private:
    static QString writeJsonDict(const QString& dirPath,
                                 const QString& fileName,
                                 const QString& dictName,
                                 const QList<QPair<QString, QString>>& entries);
};

void LookupAdapterTest::init() {
    DictionaryManager::instance().clearDictionaries();
    qunsetenv("UNIDICT_DICTS");
    qunsetenv("UNIDICT_MDICT_PASSWORD");
    qunsetenv("UNIDICT_PASSWORD");
}

void LookupAdapterTest::cleanup() {
    DictionaryManager::instance().clearDictionaries();
    qunsetenv("UNIDICT_DICTS");
    qunsetenv("UNIDICT_MDICT_PASSWORD");
    qunsetenv("UNIDICT_PASSWORD");
}

QString LookupAdapterTest::writeJsonDict(const QString& dirPath,
                                         const QString& fileName,
                                         const QString& dictName,
                                         const QList<QPair<QString, QString>>& entries) {
    const QString path = dirPath + "/" + fileName;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }

    QByteArray json;
    json += "{\n";
    json += "  \"name\": \"" + dictName.toUtf8() + "\",\n";
    json += "  \"description\": \"test dictionary\",\n";
    json += "  \"entries\": [\n";
    for (int i = 0; i < entries.size(); ++i) {
        const auto& entry = entries.at(i);
        json += "    {\"word\":\"" + entry.first.toUtf8() + "\",\"definition\":\"" + entry.second.toUtf8() + "\"}";
        if (i + 1 < entries.size()) {
            json += ",";
        }
        json += "\n";
    }
    json += "  ]\n";
    json += "}\n";

    file.write(json);
    file.close();
    return path;
}

void LookupAdapterTest::reload_replaces_dictionaries_from_env() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dictA = writeJsonDict(
        tempDir.path(),
        "dict_a.json",
        "Dict A",
        {{"hello", "from dict a"}, {"alpha", "entry a"}});
    const QString dictB = writeJsonDict(
        tempDir.path(),
        "dict_b.json",
        "Dict B",
        {{"world", "from dict b"}, {"beta", "entry b"}});

    QVERIFY(!dictA.isEmpty());
    QVERIFY(!dictB.isEmpty());

    LookupAdapter adapter;
    QSignalSpy stampSpy(&adapter, &LookupAdapter::dictionariesStampChanged);

    qputenv("UNIDICT_DICTS", dictA.toUtf8());
    QVERIFY(adapter.loadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries(), QStringList({"Dict A"}));
    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));
    QVERIFY(adapter.lookupDefinition("world").startsWith("Word not found"));

    qputenv("UNIDICT_DICTS", dictB.toUtf8());
    QVERIFY(adapter.reloadDictionariesFromEnv());

    QCOMPARE(adapter.loadedDictionaries(), QStringList({"Dict B"}));
    QCOMPARE(adapter.lookupDefinition("world"), QString("from dict b"));
    QVERIFY(adapter.lookupDefinition("hello").startsWith("Word not found"));
    QCOMPARE(adapter.indexedWordCount(), 2);
    QCOMPARE(adapter.dictionariesStamp(), 2);
    QCOMPARE(stampSpy.count(), 2);
}

QTEST_MAIN(LookupAdapterTest)
#include "lookup_adapter_test.moc"
