#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "core/lookup_service.h"
#include "core/unidict_core.h"

using namespace UnidictCore;

class LookupServiceTest : public QObject {
    Q_OBJECT
private slots:
    void not_found_message();
    void suggest_truncation();
};

void LookupServiceTest::not_found_message() {
    LookupService svc;
    const QString out = svc.lookupDefinition("__unlikely_word__", false);
    QVERIFY(out.startsWith("Word not found"));
}

// Q-6 覆盖收口：allowSuggest 路径的建议截断与 Did-you-mean 文案
void LookupServiceTest::suggest_truncation() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonArray entryArray;
    const QStringList words = {"hello", "help", "helmet"};
    for (const QString& word : words) {
        QJsonObject entryObject;
        entryObject.insert("word", word);
        entryObject.insert("definition", "def of " + word);
        entryArray.append(entryObject);
    }
    QJsonObject root;
    root.insert("name", QString("suggest"));
    root.insert("entries", entryArray);
    const QString jsonPath = QDir(dir.path()).filePath("suggest.json");
    QFile jsonFile(jsonPath);
    QVERIFY(jsonFile.open(QIODevice::WriteOnly | QIODevice::Text));
    jsonFile.write(QJsonDocument(root).toJson());
    jsonFile.close();

    QVERIFY(DictionaryManager::instance().addDictionary(jsonPath));
    LookupService svc;
    // suggestMax=1：无论上游给多少建议，正文里只保留 1 条
    const QString out = svc.lookupDefinition("hel", true, 1);
    QVERIFY(out.startsWith("Word not found: hel"));
    QVERIFY(out.contains("Did you mean:"));
    QCOMPARE(out.count(QLatin1Char('\n')), 2);

    QVERIFY(DictionaryManager::instance().removeDictionary(
        QFileInfo(jsonPath).canonicalFilePath().toLower()));
}

QTEST_MAIN(LookupServiceTest)
#include "lookup_service_test.moc"

