// DictionaryManager::prefixSearch（QCompleter 前缀补全数据源）：
// 基类默认线性实现（JsonParser 路径）、StarDictParser 的 lowerBound
// 二分 override（有序小写键 + canonical 词形恢复），以及 manager 层的
// 多词典合并、去重与禁用过滤。

#include <QDataStream>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "core/json_parser.h"
#include "core/stardict_parser.h"
#include "core/unidict_core.h"

using namespace UnidictCore;

namespace {

QByteArray dictJson(const QString& name, const QList<QPair<QString, QString>>& items) {
    QJsonObject root;
    root.insert("name", name);
    QJsonArray entries;
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

// 仿 core_lookup_tests 的 StarDict 三件套（.ifo/.idx/.dict，无压缩）
struct StarEntry {
    QString word;
    QString definition;
};

bool writeStarDict(const QString& directoryPath,
                   const QString& dictionaryName,
                   const QList<StarEntry>& entries) {
    const QString basePath = QDir(directoryPath).filePath(dictionaryName);
    QFile dictFile(basePath + ".dict");
    QFile idxFile(basePath + ".idx");
    QFile ifoFile(basePath + ".ifo");
    if (!dictFile.open(QIODevice::WriteOnly) || !idxFile.open(QIODevice::WriteOnly) ||
        !ifoFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QByteArray dictData;
    QDataStream idxStream(&idxFile);
    idxStream.setByteOrder(QDataStream::BigEndian);
    quint32 offset = 0;
    for (const auto& entry : entries) {
        const QByteArray wordBytes = entry.word.toUtf8();
        const QByteArray definitionBytes = entry.definition.toUtf8();
        idxFile.write(wordBytes);
        idxFile.putChar('\0');
        idxStream << offset << static_cast<quint32>(definitionBytes.size());
        dictData.append(definitionBytes);
        offset += static_cast<quint32>(definitionBytes.size());
    }
    dictFile.write(dictData);

    const QByteArray ifoData =
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        "bookname=" + dictionaryName.toUtf8() + "\n"
        "wordcount=" + QByteArray::number(entries.size()) + "\n"
        "idxfilesize=" + QByteArray::number(idxFile.size()) + "\n";
    ifoFile.write(ifoData);
    return true;
}

} // namespace

class DictionaryManagerPrefixTest : public QObject {
    Q_OBJECT

private slots:
    void init(); // 每个用例前清单例状态（DictionaryManager 是全局单例）

    void json_parser_default_linear_path();
    void json_parser_lowerbound_path();
    void json_parser_large_dict_prefix_fast();
    void stardict_parser_lowerbound_path();
    void manager_merge_dedupe_and_disable();

private:
    QTemporaryDir dir_;
};

void DictionaryManagerPrefixTest::init() {
    DictionaryManager::instance().clear();
}

void DictionaryManagerPrefixTest::json_parser_default_linear_path() {
    const QString path = dir_.filePath("p.json");
    QVERIFY(writeDict(path, dictJson(QStringLiteral("p dict"), {
        {QStringLiteral("greeting"), QStringLiteral("hello words")},
        {QStringLiteral("growth"), QStringLiteral("process of growing")},
        {QStringLiteral("apple"), QStringLiteral("a fruit")},
        {QStringLiteral("Greet"), QStringLiteral("to greet someone")},
    })));

    JsonParser parser;
    QVERIFY(parser.loadDictionary(path));

    // 大小写不敏感前缀，返回原词形
    const auto hits = parser.prefixSearch(QStringLiteral("gr"), 10);
    QCOMPARE(hits.size(), 3);
    QVERIFY(hits.contains(QStringLiteral("greeting")));
    QVERIFY(hits.contains(QStringLiteral("growth")));
    QVERIFY(hits.contains(QStringLiteral("Greet")));

    // 大写前缀同样命中；maxResults 截断
    QCOMPARE(parser.prefixSearch(QStringLiteral("GR"), 10).size(), 3);
    QCOMPARE(parser.prefixSearch(QStringLiteral("gr"), 2).size(), 2);
    QCOMPARE(parser.prefixSearch(QStringLiteral("apple"), 10),
             QStringList{QStringLiteral("apple")});
    // 无命中与空查询
    QVERIFY(parser.prefixSearch(QStringLiteral("zzz"), 10).isEmpty());
    QVERIFY(parser.prefixSearch(QString(), 10).isEmpty());
    QVERIFY(parser.prefixSearch(QStringLiteral("gr"), 0).isEmpty());
}

void DictionaryManagerPrefixTest::stardict_parser_lowerbound_path() {
    QVERIFY(writeStarDict(dir_.path(), QStringLiteral("sd"), {
        {QStringLiteral("Apple"), QStringLiteral("a fruit")},
        {QStringLiteral("greeting"), QStringLiteral("hello words")},
        {QStringLiteral("Greeting"), QStringLiteral("hello words upper")},
        {QStringLiteral("growth"), QStringLiteral("process of growing")},
    }));

    StarDictParser parser;
    QVERIFY(parser.loadDictionary(dir_.filePath("sd.ifo")));

    // lowerBound 二分：前缀区段连续命中，越过区段即止；
    // "Greeting"/"greeting" 小写键相同（后写覆盖），canonical 取最后一个词形
    const auto hits = parser.prefixSearch(QStringLiteral("GR"), 10);
    QCOMPARE(hits.size(), 2);
    QVERIFY(hits.contains(QStringLiteral("Greeting")));
    QVERIFY(hits.contains(QStringLiteral("growth")));

    // 单命中 + canonical 词形（idx 里原词是 "Apple"）
    QCOMPARE(parser.prefixSearch(QStringLiteral("app"), 10),
             QStringList{QStringLiteral("Apple")});

    QVERIFY(parser.prefixSearch(QStringLiteral("zzz"), 10).isEmpty());
    QVERIFY(parser.prefixSearch(QString(), 10).isEmpty());
}

void DictionaryManagerPrefixTest::manager_merge_dedupe_and_disable() {
    auto& manager = DictionaryManager::instance();

    // 两部词典共享词条 "greeting"（小写判重去重），各自独有词条保留
    const QString first = dir_.filePath("d1.json");
    QVERIFY(writeDict(first, dictJson(QStringLiteral("dict one"), {
        {QStringLiteral("greeting"), QStringLiteral("hello words")},
        {QStringLiteral("growth"), QStringLiteral("process of growing")},
    })));
    const QString second = dir_.filePath("d2.json");
    QVERIFY(writeDict(second, dictJson(QStringLiteral("dict two"), {
        {QStringLiteral("greeting"), QStringLiteral("重复词条")},
        {QStringLiteral("grape"), QStringLiteral("a fruit")},
    })));
    QVERIFY(manager.addDictionary(first));
    QVERIFY(manager.addDictionary(second));

    const auto merged = manager.prefixSearch(QStringLiteral("gr"), 10);
    QCOMPARE(merged.size(), 3); // greeting 去重后 + growth + grape
    QVERIFY(merged.contains(QStringLiteral("greeting")));
    QVERIFY(merged.contains(QStringLiteral("growth")));
    QVERIFY(merged.contains(QStringLiteral("grape")));

    // 禁用一部词典后只余另一部的前缀命中
    const auto infos = manager.getLoadedDictionaryInfos();
    QCOMPARE(infos.size(), 2);
    const QString firstId = infos.constFirst().name == QStringLiteral("dict one")
                                ? infos.constFirst().id
                                : infos.at(1).id;
    QVERIFY(manager.setDictionaryEnabled(firstId, false));
    // dict one 禁用后余 dict two：greeting（与 d1 重复的词条仍在）+ grape。
    // JsonParser 走默认线性实现，保持词表插入序，断言用集合语义
    const auto remaining = manager.prefixSearch(QStringLiteral("gr"), 10);
    QCOMPARE(remaining.size(), 2);
    QVERIFY(remaining.contains(QStringLiteral("grape")));
    QVERIFY(remaining.contains(QStringLiteral("greeting")));

    // 空查询与空格前缀
    QVERIFY(manager.prefixSearch(QString(), 10).isEmpty());
    QVERIFY(manager.prefixSearch(QStringLiteral("  "), 10).isEmpty());
}

void DictionaryManagerPrefixTest::json_parser_lowerbound_path() {
    // JsonParser 已改用 m_lowerWords 小写有序键 lowerBound 二分（与
    // StarDictParser 同款模式），本用例钉住二分路径的行为语义
    QVERIFY(writeDict(dir_.filePath("lb.json"), dictJson(QStringLiteral("lb dict"), {
        {QStringLiteral("mango"), QStringLiteral("a fruit")},
        {QStringLiteral("Mint"), QStringLiteral("a herb")},
        {QStringLiteral("mint"), QStringLiteral("a herb lower dup")},
        {QStringLiteral("banana"), QStringLiteral("a fruit too")},
        {QStringLiteral("Cherry"), QStringLiteral("a stone fruit")},
    })));

    JsonParser parser;
    QVERIFY(parser.loadDictionary(dir_.filePath("lb.json")));

    // 大小写不敏感前缀，返回原词形，按键字母序输出
    const auto hits = parser.prefixSearch(QStringLiteral("m"), 10);
    QCOMPARE(hits.size(), 2); // "Mint"/"mint" 同小写键合并，canonical 取后写者
    QCOMPARE(hits.constFirst(), QStringLiteral("mango"));
    QCOMPARE(hits.last(), QStringLiteral("mint"));

    // 大写前缀同样命中
    QCOMPARE(parser.prefixSearch(QStringLiteral("M"), 10), hits);

    // maxResults 截断取区段头部
    QCOMPARE(parser.prefixSearch(QStringLiteral("m"), 1), QStringList{QStringLiteral("mango")});

    // 无命中、空查询、零上限
    QVERIFY(parser.prefixSearch(QStringLiteral("zzz"), 10).isEmpty());
    QVERIFY(parser.prefixSearch(QString(), 10).isEmpty());
    QVERIFY(parser.prefixSearch(QStringLiteral("m"), 0).isEmpty());
}

void DictionaryManagerPrefixTest::json_parser_large_dict_prefix_fast() {
    // 性能守护：5 万词 JSON 词典的无命中前缀查询（基类线性路径的最坏场景）
    // 依赖 m_lowerWords 二分。压测基准：10 万词线性 13ms/次，二分 <0.05ms；
    // 5 万词线性约 6.5ms 会撞 5ms 阈值，二分离阈值三个数量级，CI 抖动不误伤
    const int total = 50000;
    QJsonArray entries;
    for (int i = 0; i < total; ++i) {
        QJsonObject entry;
        entry.insert(QStringLiteral("word"), QStringLiteral("bench%1").arg(i, 5, 10, QLatin1Char('0')));
        entry.insert(QStringLiteral("definition"), QStringLiteral("bench entry %1 for prefix timing").arg(i));
        entries.append(entry);
    }
    QJsonObject root;
    root.insert(QStringLiteral("name"), QStringLiteral("large"));
    root.insert(QStringLiteral("entries"), entries);
    QVERIFY(writeDict(dir_.filePath("large.json"),
                      QJsonDocument(root).toJson(QJsonDocument::Compact)));

    JsonParser parser;
    QVERIFY(parser.loadDictionary(dir_.filePath("large.json")));

    QElapsedTimer timer;
    timer.start();
    const auto hits = parser.prefixSearch(QStringLiteral("zzzz"), 10);
    const qint64 elapsedMs = timer.elapsed();
    QVERIFY(hits.isEmpty());
    // 宽阈值只拦"退回线性扫全表"级别的回归，不做精确定时断言
    QVERIFY2(elapsedMs < 5, qPrintable(QStringLiteral("prefixSearch took %1 ms").arg(elapsedMs)));
}

QTEST_MAIN(DictionaryManagerPrefixTest)
#include "dictionary_manager_prefix_test.moc"
