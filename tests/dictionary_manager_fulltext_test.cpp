// DictionaryManager 全文检索（Qt core 组合 std FullTextIndexStd）：
// 惰性建索引、命中定义含查询词的词条、来源 metadata、词典集合变化后的
// 索引失效重建，以及 JsonParser::allEntries 数据源。

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "core/json_parser.h"
#include "core/unidict_core.h"

using namespace UnidictCore;

namespace {

// 造一个最小 JSON 词典：greeting 的释义里含 hello——精确查 greeting
// 命中、查 hello 精确也命中，但全文查 greeting 应同时捞出 hello
// 与 greeting 两条（TF-IDF 相关度排序）
QByteArray dictJson(const QString& name) {
    QJsonObject root;
    root.insert("name", name);
    root.insert("description", "fulltext test dictionary");
    QJsonArray entries;
    const QList<QPair<QString, QString>> items = {
        {QStringLiteral("hello"), QStringLiteral("an expression of greeting")},
        {QStringLiteral("greeting"),
         QStringLiteral("a word of greeting, says hello when meeting")},
        {QStringLiteral("apple"), QStringLiteral("a round fruit with red or green skin")},
    };
    for (const auto& item : items) {
        QJsonObject entry;
        entry.insert("word", item.first);
        entry.insert("definition", item.second);
        entries.append(entry);
    }
    root.insert("entries", entries);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool writeDict(const QString& path, const QByteArray& content) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(content) == content.size();
}

} // namespace

class DictionaryManagerFulltextTest : public QObject {
    Q_OBJECT

private slots:
    void init(); // 每个用例前重建单例状态（DictionaryManager 是全局单例）

    void lazy_build_and_search();
    void empty_query_returns_empty();
    void invalidate_on_add();
    void invalidate_on_disable();
    void invalidate_on_remove();
    void json_parser_all_entries();

private:
    QTemporaryDir dir_;
    QString dictPath() const { return dir_.filePath("ft_dict.json"); }
};

void DictionaryManagerFulltextTest::init() {
    auto& manager = DictionaryManager::instance();
    manager.clear();
    QVERIFY(writeDict(dictPath(), dictJson(QStringLiteral("fulltext test dictionary"))));
    QVERIFY(manager.addDictionary(dictPath()));
    QVERIFY(!manager.isFulltextIndexBuilt()); // 新增词典后必须失效（惰性）
}

void DictionaryManagerFulltextTest::lazy_build_and_search() {
    auto& manager = DictionaryManager::instance();

    const auto hits = manager.fullTextSearch(QStringLiteral("greeting"), 20);
    QVERIFY(!hits.isEmpty());
    bool sawHello = false;
    bool sawGreeting = false;
    for (const auto& entry : hits) {
        if (entry.word == QStringLiteral("hello")) {
            sawHello = true;
        }
        if (entry.word == QStringLiteral("greeting")) {
            sawGreeting = true;
        }
        QCOMPARE(entry.metadata.value("dictionary").toString(),
                 QStringLiteral("fulltext test dictionary"));
    }
    QVERIFY(sawHello);
    QVERIFY(sawGreeting);
    QVERIFY(manager.isFulltextIndexBuilt());

    // 再次查询走已建索引
    const auto again = manager.fullTextSearch(QStringLiteral("greeting"), 1);
    QCOMPARE(again.size(), 1);
}

void DictionaryManagerFulltextTest::empty_query_returns_empty() {
    auto& manager = DictionaryManager::instance();
    QVERIFY(manager.fullTextSearch(QString(), 20).isEmpty());
    QVERIFY(manager.fullTextSearch(QStringLiteral("   "), 20).isEmpty());
    QVERIFY(manager.fullTextSearch(QStringLiteral("zzzznotaword"), 20).isEmpty());
}

void DictionaryManagerFulltextTest::invalidate_on_add() {
    auto& manager = DictionaryManager::instance();
    QVERIFY(manager.fullTextSearch(QStringLiteral("greeting"), 5).size() > 0);
    QVERIFY(manager.isFulltextIndexBuilt());

    // 新增词典（同名词条的另一份）后索引必须失效重建
    const QString second = dir_.filePath("second.json");
    QVERIFY(writeDict(second, dictJson(QStringLiteral("second dict"))));
    QVERIFY(manager.addDictionary(second));
    QVERIFY(!manager.isFulltextIndexBuilt());

    const auto hits = manager.fullTextSearch(QStringLiteral("greeting"), 20);
    QVERIFY(!hits.isEmpty());
    bool sawSecond = false;
    for (const auto& entry : hits) {
        if (entry.metadata.value("dictionary").toString() == QStringLiteral("second dict")) {
            sawSecond = true;
        }
    }
    QVERIFY(sawSecond);
}

void DictionaryManagerFulltextTest::invalidate_on_disable() {
    auto& manager = DictionaryManager::instance();
    QVERIFY(manager.fullTextSearch(QStringLiteral("greeting"), 5).size() > 0);
    QVERIFY(manager.isFulltextIndexBuilt());

    // 禁用唯一词典后全文检索应为空（索引失效重建后无文档）
    const QString id = manager.getLoadedDictionaryInfos().constFirst().id;
    QVERIFY(manager.setDictionaryEnabled(id, false));
    QVERIFY(!manager.isFulltextIndexBuilt());
    QVERIFY(manager.fullTextSearch(QStringLiteral("greeting"), 5).isEmpty());
}

void DictionaryManagerFulltextTest::invalidate_on_remove() {
    auto& manager = DictionaryManager::instance();
    QVERIFY(manager.fullTextSearch(QStringLiteral("greeting"), 5).size() > 0);
    QVERIFY(manager.isFulltextIndexBuilt());

    const QString id = manager.getLoadedDictionaryInfos().constFirst().id;
    QVERIFY(manager.removeDictionary(id));
    QVERIFY(!manager.isFulltextIndexBuilt());
    QVERIFY(manager.fullTextSearch(QStringLiteral("greeting"), 5).isEmpty());
}

void DictionaryManagerFulltextTest::json_parser_all_entries() {
    JsonParser parser;
    QVERIFY(parser.loadDictionary(dictPath()));

    const auto entries = parser.allEntries();
    QCOMPARE(entries.size(), 3);

    // QMap 按键排序：apple / greeting / hello
    QCOMPARE(entries.at(0).first, QStringLiteral("apple"));
    QCOMPARE(entries.at(0).second, QStringLiteral("a round fruit with red or green skin"));
    QCOMPARE(entries.at(1).first, QStringLiteral("greeting"));
    QCOMPARE(entries.at(2).first, QStringLiteral("hello"));

    // 未加载的 parser 返回空
    JsonParser unloaded;
    QVERIFY(unloaded.allEntries().isEmpty());
}

QTEST_MAIN(DictionaryManagerFulltextTest)
#include "dictionary_manager_fulltext_test.moc"
