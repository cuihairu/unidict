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
    void mdict_password_env_roundtrip();
    void reload_replaces_dictionaries_from_env();
    void multiple_paths_split_by_platform_separator();

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

void LookupAdapterTest::mdict_password_env_roundtrip() {
    LookupAdapter adapter;

    QVERIFY(!adapter.hasMdictPassword());
    QVERIFY(!adapter.setMdictPassword(QString()));

    QVERIFY(adapter.setMdictPassword("secret"));
    QVERIFY(adapter.hasMdictPassword());
    QCOMPARE(qEnvironmentVariable("UNIDICT_MDICT_PASSWORD"), QString("secret"));
    QVERIFY(qEnvironmentVariable("UNIDICT_PASSWORD").isEmpty());

    qputenv("UNIDICT_PASSWORD", QByteArray("legacy"));
    QVERIFY(adapter.hasMdictPassword());

    adapter.clearMdictPassword();
    QVERIFY(!adapter.hasMdictPassword());
    QVERIFY(qEnvironmentVariable("UNIDICT_MDICT_PASSWORD").isEmpty());
    QVERIFY(qEnvironmentVariable("UNIDICT_PASSWORD").isEmpty());
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

// 回归：UNIDICT_DICTS 的多路径分隔符必须跟平台 PATH 惯例（Windows ';',
// POSIX ':'），且不能把另一平台的分隔符也当成分隔符——否则 Windows 的
// 盘符 'C:\...' 自带冒号会被劈碎，字典全部加载失败（曾致 Windows CI 独挂）
void LookupAdapterTest::multiple_paths_split_by_platform_separator() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString dictA = writeJsonDict(
        tempDir.path(), "dict_a.json", "Dict A", {{"hello", "from dict a"}});
    const QString dictB = writeJsonDict(
        tempDir.path(), "dict_b.json", "Dict B", {{"world", "from dict b"}});
    QVERIFY(!dictA.isEmpty());
    QVERIFY(!dictB.isEmpty());

    LookupAdapter adapter;

    QByteArray joined = dictA.toUtf8();
#if defined(Q_OS_WIN)
    joined += ';';
#else
    joined += ':';
#endif
    joined += dictB.toUtf8();
    qputenv("UNIDICT_DICTS", joined);

    QVERIFY(adapter.loadDictionariesFromEnv());
    QCOMPARE(adapter.loadedDictionaries(), QStringList({"Dict A", "Dict B"}));
    QCOMPARE(adapter.lookupDefinition("hello"), QString("from dict a"));
    QCOMPARE(adapter.lookupDefinition("world"), QString("from dict b"));
}

QTEST_MAIN(LookupAdapterTest)
#include "lookup_adapter_test.moc"
