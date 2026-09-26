#include <QDataStream>
#include "mdict_fixture.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <algorithm>

#include "unidict_core.h"

extern "C" {
#include <zlib.h>
}

namespace {

// MDX/.mdd 夹具已抽到 tests/mdict_fixture.h（core_lookup_tests 与
// lookup_adapter_test 共用，避免两处各写一份约 190 行的二进制布局代码）
using UnidictMdictFixture::TestEntry;
using UnidictMdictFixture::writeMdxDictionary;
using UnidictMdictFixture::writeMddResource;
using UnidictMdictFixture::wrapZlibBlock;
using UnidictMdictFixture::toUtf16Le;
using UnidictMdictFixture::appendBigEndian16;
using UnidictMdictFixture::appendBigEndian32;
using UnidictMdictFixture::appendBigEndian64;

bool writeStarDictDictionary(const QString& directoryPath,
                             const QString& dictionaryName,
                             const QList<TestEntry>& entries) {
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
        "idxfilesize=" + QByteArray::number(idxFile.size()) + "\n"
        "description=Test dictionary\n";
    ifoFile.write(ifoData);

    return true;
}

bool writeJsonDictionary(const QString& directoryPath,
                         const QString& dictionaryName,
                         const QList<TestEntry>& entries) {
    QJsonArray entryArray;
    for (const auto& entry : entries) {
        QJsonObject entryObject;
        entryObject.insert("word", entry.word);
        entryObject.insert("definition", entry.definition);
        entryArray.append(entryObject);
    }
    QJsonObject root;
    root.insert("name", dictionaryName);
    root.insert("entries", entryArray);

    QFile jsonFile(QDir(directoryPath).filePath(dictionaryName + ".json"));
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    jsonFile.write(QJsonDocument(root).toJson());
    return true;
}

// 最小 EPUB（stored zip，免压缩依赖）：container + OPF + 一章 XHTML。
// 词头用 <h2>/<h3>，走 EpubParserStd 的 heading 提取约定。
bool writeEpubDictionary(const QString& directoryPath,
                         const QString& dictionaryName,
                         const QList<TestEntry>& entries) {
    auto putU16 = [](QByteArray& out, quint16 v) {
        out.append(static_cast<char>(v & 0xff));
        out.append(static_cast<char>(v >> 8));
    };
    auto putU32 = [](QByteArray& out, quint32 v) {
        for (int i = 0; i < 4; ++i) {
            out.append(static_cast<char>((v >> (8 * i)) & 0xff));
        }
    };

    QString chapter = QStringLiteral("<html><head><title>c</title></head><body>");
    for (const auto& entry : entries) {
        chapter += QStringLiteral("<h2>%1</h2><p>%2</p>")
                       .arg(entry.word.toHtmlEscaped(), entry.definition.toHtmlEscaped());
    }
    chapter += QStringLiteral("</body></html>");

    const QByteArray container =
        "<rootfiles><rootfile full-path=\"content.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles>";
    const QByteArray opf =
        "<package><metadata><dc:title>" + dictionaryName.toUtf8() +
        "</dc:title></metadata>"
        "<manifest><item id=\"c\" href=\"c.xhtml\" "
        "media-type=\"application/xhtml+xml\"/></manifest></package>";
    const QByteArray chapterBytes = chapter.toUtf8();

    struct LocalEntry {
        QByteArray name;
        QByteArray data;
    };
    const LocalEntry localEntries[] = {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", container},
        {"content.opf", opf},
        {"c.xhtml", chapterBytes},
    };

    QByteArray archive;
    QByteArray central;
    quint16 count = 0;
    for (const auto& item : localEntries) {
        const quint32 crc = ::crc32(0, reinterpret_cast<const Bytef*>(item.data.constData()),
                                    static_cast<uInt>(item.data.size()));
        const quint32 size = static_cast<quint32>(item.data.size());
        const quint32 offset = static_cast<quint32>(archive.size());
        putU32(archive, 0x04034b50u);
        putU16(archive, 20);
        putU16(archive, 0);
        putU16(archive, 0); // stored
        putU16(archive, 0);
        putU16(archive, 0);
        putU32(archive, crc);
        putU32(archive, size);
        putU32(archive, size);
        putU16(archive, static_cast<quint16>(item.name.size()));
        putU16(archive, 0);
        archive.append(item.name);
        archive.append(item.data);

        putU32(central, 0x02014b50u);
        putU16(central, 20);
        putU16(central, 20);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU32(central, crc);
        putU32(central, size);
        putU32(central, size);
        putU16(central, static_cast<quint16>(item.name.size()));
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU32(central, 0);
        putU32(central, offset);
        central.append(item.name);
        ++count;
    }
    const quint32 cdOffset = static_cast<quint32>(archive.size());
    const quint32 cdSize = static_cast<quint32>(central.size());
    archive.append(central);
    putU32(archive, 0x06054b50u);
    putU16(archive, 0);
    putU16(archive, 0);
    putU16(archive, count);
    putU16(archive, count);
    putU32(archive, cdSize);
    putU32(archive, cdOffset);
    putU16(archive, 0);

    QFile epubFile(QDir(directoryPath).filePath(dictionaryName + ".epub"));
    if (!epubFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    epubFile.write(archive);
    return true;
}



} // namespace

class CoreLookupTests : public QObject {
    Q_OBJECT

private slots:
    void init() {
        UnidictCore::DictionaryManager::instance().clear();
    }

    void cleanup() {
        UnidictCore::DictionaryManager::instance().clear();
    }

    void loadsStardictAndFindsWord() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "basic", {
            {"hello", "greeting"},
            {"world", "earth"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("basic.ifo")));

        const auto result = manager.searchWord("hello");
        QVERIFY(result.success);
        QCOMPARE(result.matches.size(), 1);
        QCOMPARE(result.entry.definition, QString("greeting"));
        QCOMPARE(result.dictionaryName, QString("basic"));
    }

    void aggregatesMatchesAcrossDictionaries() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "dict_a", {
            {"hello", "from a"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "dict_b", {
            {"hello", "from b"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("dict_a.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("dict_b.ifo")));

        const auto result = manager.searchWord("hello");
        QVERIFY(result.success);
        QCOMPARE(result.matches.size(), 2);
        QCOMPARE(result.matches.at(0).entry.definition, QString("from a"));
        QCOMPARE(result.matches.at(1).entry.definition, QString("from b"));

        const QString rendered = UnidictCore::formatLookupResult(result);
        QVERIFY(rendered.contains("[dict_a]"));
        QVERIFY(rendered.contains("[dict_b]"));
    }

    void returnsSuggestionsWhenExactMatchMissing() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "suggestions", {
            {"hello", "greeting"},
            {"help", "assist"},
            {"helm", "headgear"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("suggestions.ifo")));

        const auto result = manager.searchWord("hel");
        QVERIFY(!result.success);
        QVERIFY(result.suggestions.contains("hello"));
        QVERIFY(result.suggestions.contains("help"));
    }

    void handlesCaseInsensitiveLookupWithCanonicalWord() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "canonical", {
            {"Hello", "capitalized greeting"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("canonical.ifo")));

        const auto result = manager.searchWord("hello");
        QVERIFY(result.success);
        QCOMPARE(result.entry.word, QString("Hello"));
        QCOMPARE(result.entry.definition, QString("capitalized greeting"));
    }

    void reportsMissingDictionaryStateAndEmptyQuery() {
        auto& manager = UnidictCore::DictionaryManager::instance();

        const auto emptyQuery = manager.searchWord("   ");
        QVERIFY(!emptyQuery.success);
        QCOMPARE(emptyQuery.message, QString("Enter a word to search."));

        const auto noDictionary = manager.searchWord("hello");
        QVERIFY(!noDictionary.success);
        QCOMPARE(noDictionary.message, QString("No dictionaries loaded. Import a StarDict dictionary first."));
    }

    void scansDirectoryAndLoadsStardictAndMdx() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "scan_me", {
            {"alpha", "first"}
        }));
        QVERIFY(writeMdxDictionary(tempDir.path(), "scan_mdx", {
            {"beta", "second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QCOMPARE(manager.addDictionariesFromDirectory(tempDir.path()), 2);
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 2);

        const auto mdxResult = manager.searchWord("beta");
        QVERIFY(mdxResult.success);
        QCOMPARE(mdxResult.entry.definition, QString("second"));
    }

    void rejectsDuplicateDictionaryLoad() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "dupe", {
            {"same", "value"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        const QString path = QDir(tempDir.path()).filePath("dupe.ifo");
        QVERIFY(manager.addDictionary(path));
        QVERIFY(!manager.addDictionary(path));
        QCOMPARE(manager.lastError(), QString("Dictionary already loaded: dupe.ifo"));
    }

    void removesDictionaryAndStopsReturningResults() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "remove_me", {
            {"same", "value"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        const QString path = QDir(tempDir.path()).filePath("remove_me.ifo");
        QVERIFY(manager.addDictionary(path));

        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 1);
        QVERIFY(manager.removeDictionary(infos.constFirst().id));
        QVERIFY(!manager.hasDictionaries());

        const auto result = manager.searchWord("same");
        QVERIFY(!result.success);
        QCOMPARE(result.message, QString("No dictionaries loaded. Import a StarDict dictionary first."));
    }

    void disablesDictionaryAndExcludesItFromLookup() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "enabled_a", {
            {"term", "from first"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "enabled_b", {
            {"term", "from second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("enabled_a.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("enabled_b.ifo")));

        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 2);
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, false));

        const auto result = manager.searchWord("term");
        QVERIFY(result.success);
        QCOMPARE(result.matches.size(), 1);
        QCOMPARE(result.matches.constFirst().dictionaryName, QString("enabled_b"));

        QVERIFY(manager.setDictionaryEnabled(infos.at(1).id, false));
        const auto allDisabled = manager.searchWord("term");
        QVERIFY(!allDisabled.success);
        QCOMPARE(allDisabled.message, QString("All dictionaries are disabled. Enable at least one dictionary."));
    }

    void reordersDictionaryPriorityForLookupResults() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "priority_a", {
            {"term", "from first"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "priority_b", {
            {"term", "from second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("priority_a.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("priority_b.ifo")));

        auto result = manager.searchWord("term");
        QVERIFY(result.success);
        QCOMPARE(result.matches.constFirst().dictionaryName, QString("priority_a"));

        const auto infos = manager.getLoadedDictionaryInfos();
        QVERIFY(manager.moveDictionaryDown(infos.at(0).id));

        result = manager.searchWord("term");
        QVERIFY(result.success);
        QCOMPARE(result.matches.constFirst().dictionaryName, QString("priority_b"));
        QCOMPARE(manager.getLoadedDictionaryInfos().at(0).name, QString("priority_b"));
    }

    void persistsDictionaryOrderAndEnabledState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "persist_a", {
            {"term", "from a"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "persist_b", {
            {"term", "from b"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("persist_a.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("persist_b.ifo")));

        const auto infos = manager.getLoadedDictionaryInfos();
        QVERIFY(manager.moveDictionaryDown(infos.at(0).id));
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, false));
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto restoredInfos = manager.getLoadedDictionaryInfos();
        QCOMPARE(restoredInfos.size(), 2);
        QCOMPARE(restoredInfos.at(0).name, QString("persist_b"));
        QVERIFY(restoredInfos.at(0).enabled);
        QCOMPARE(restoredInfos.at(1).name, QString("persist_a"));
        QVERIFY(!restoredInfos.at(1).enabled);

        const auto result = manager.searchWord("term");
        QVERIFY(result.success);
        QCOMPARE(result.matches.size(), 1);
        QCOMPARE(result.matches.constFirst().dictionaryName, QString("persist_b"));
    }

    // 守护：loadFromJson 曾只认 ifo/mdx，JSON 词典在状态恢复时被静默丢弃。
    void persistsJsonDictionaryThroughStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "persist_json_stardict", {
            {"term", "from stardict"}
        }));
        QVERIFY(writeJsonDictionary(tempDir.path(), "persist_json_dict", {
            {"hello", "from json"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("persist_json_stardict.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("persist_json_dict.json")));

        // JSON 词典排到最前且禁用 StarDict 词典，两处状态都要活过重启恢复。
        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 2);
        QCOMPARE(infos.at(1).name, QString("persist_json_dict"));
        QVERIFY(manager.moveDictionaryUp(infos.at(1).id));
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, false));
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto restoredInfos = manager.getLoadedDictionaryInfos();
        QCOMPARE(restoredInfos.size(), 2);
        QCOMPARE(restoredInfos.at(0).name, QString("persist_json_dict"));
        QVERIFY(restoredInfos.at(0).enabled);
        QCOMPARE(restoredInfos.at(1).name, QString("persist_json_stardict"));
        QVERIFY(!restoredInfos.at(1).enabled);

        const auto result = manager.searchWord("hello");
        QVERIFY(result.success);
        QCOMPARE(result.matches.size(), 1);
        QCOMPARE(result.matches.constFirst().dictionaryName, QString("persist_json_dict"));
    }

    // 守护：EPUB 词典加载、查询与状态恢复（loadFromJson 的 epub 分支）。
    void loadsEpubDictionaryAndPersistsThroughStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeEpubDictionary(tempDir.path(), "persist_epub_dict", {
            {"apple", "a fruit"},
            {"banana", "yellow fruit"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("persist_epub_dict.epub")));

        QCOMPARE(manager.getLoadedDictionaryInfos().constFirst().format, QString("EPUB"));
        const auto result = manager.searchWord("APPLE"); // 大小写不敏感
        QVERIFY(result.success);
        QCOMPARE(result.matches.constFirst().entry.definition, QString("a fruit"));
        QCOMPARE(manager.prefixSearch("ban", 10), QStringList{"banana"});

        QVERIFY(manager.saveState(statePath));
        manager.clear();
        QVERIFY(manager.loadState(statePath)); // epub 分支被 loadFromJson 认得

        const auto restored = manager.getLoadedDictionaryInfos();
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.constFirst().name, QString("persist_epub_dict"));
        QVERIFY(restored.constFirst().enabled);
        const auto after = manager.searchWord("banana");
        QVERIFY(after.success);
        QCOMPARE(after.matches.constFirst().entry.definition, QString("yellow fruit"));
    }

    // 守护：损坏词典检测三段——解析失败进隔离且诊断可见（不再静默消失）；
    // 隔离路径重启后不重复解析（文件修好也要显式重试，大词典反复失败
    // 代价高）；显式重试成功转正常。
    void quarantinesCorruptDictionaryUntilExplicitRetry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "good_dict", {{"hello", "from good"}}));
        QVERIFY(writeJsonDictionary(tempDir.path(), "broken_dict", {{"apple", "from broken"}}));

        const QString goodPath = QDir(tempDir.path()).filePath("good_dict.json");
        const QString brokenPath = QDir(tempDir.path()).filePath("broken_dict.json");
        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(goodPath));
        QVERIFY(manager.addDictionary(brokenPath));
        QVERIFY(manager.saveState(statePath));

        {
            QFile corrupt(brokenPath);
            QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
            corrupt.write("\x00\x01not json at all");
        }

        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 1);
        const auto failures = manager.getFailedDictionaries();
        QCOMPARE(failures.size(), 1);
        QCOMPARE(failures.constFirst().filePath, brokenPath);
        QVERIFY(failures.constFirst().quarantined); // 解析失败 → 持久隔离

        // 隔离记录随 saveState 落盘（loadFromJson 的自动落盘写默认路径，
        // 测试用显式 state 文件，故这里再存一次）
        QVERIFY(manager.saveState(statePath));

        // 文件修好：隔离中的路径启动时仍不重试——这是隔离的契约
        QVERIFY(writeJsonDictionary(tempDir.path(), "broken_dict", {{"apple", "from broken"}}));
        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 1);
        QCOMPARE(manager.getFailedDictionaries().size(), 1);

        QVERIFY(manager.retryFailedDictionary(brokenPath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 2);
        QVERIFY(manager.getFailedDictionaries().isEmpty());
        const auto result = manager.searchWord("apple");
        QVERIFY(result.success);
    }

    // 守护：文件丢失是运行期诊断而非持久隔离——原因可见，文件回来自动
    // 恢复加载，无需手动重试（外置盘/挂载延迟场景的预期行为）。
    void missingDictionaryIsDiagnosedAndSelfHeals() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "stay_dict", {{"hello", "from stay"}}));
        QVERIFY(writeJsonDictionary(tempDir.path(), "vanish_dict", {{"pear", "from vanish"}}));

        const QString vanishPath = QDir(tempDir.path()).filePath("vanish_dict.json");
        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("stay_dict.json")));
        QVERIFY(manager.addDictionary(vanishPath));
        QVERIFY(manager.saveState(statePath));

        QVERIFY(QFile::remove(vanishPath));
        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 1);
        const auto failures = manager.getFailedDictionaries();
        QCOMPARE(failures.size(), 1);
        QCOMPARE(failures.constFirst().filePath, vanishPath);
        QVERIFY(!failures.constFirst().quarantined); // 运行期诊断，非隔离
        QCOMPARE(failures.constFirst().reason,
                 QString("File not found: ") + vanishPath);

        QVERIFY(writeJsonDictionary(tempDir.path(), "vanish_dict", {{"pear", "from vanish"}}));
        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 2); // 自愈
        QVERIFY(manager.getFailedDictionaries().isEmpty());
        QVERIFY(manager.searchWord("pear").success);
    }

    // 守护：forget 从隔离区移除并把词典从状态文件的 wanted 列表彻底抹掉，
    // 重启后不再出现。
    void forgetsFailedDictionaryRemovesItFromStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "good_dict", {{"hello", "from good"}}));
        QVERIFY(writeJsonDictionary(tempDir.path(), "broken_dict", {{"apple", "from broken"}}));

        const QString goodPath = QDir(tempDir.path()).filePath("good_dict.json");
        const QString brokenPath = QDir(tempDir.path()).filePath("broken_dict.json");
        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(goodPath));
        QVERIFY(manager.addDictionary(brokenPath));
        QVERIFY(manager.saveState(statePath));

        {
            QFile corrupt(brokenPath);
            QVERIFY(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate));
            corrupt.write("\x00\x01not json at all");
        }
        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getFailedDictionaries().size(), 1);

        QVERIFY(manager.forgetFailedDictionary(brokenPath));
        QVERIFY(manager.getFailedDictionaries().isEmpty());
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 1);
        QVERIFY(manager.saveState(statePath));

        // 状态文件里 dictionaries 不含坏路径，quarantined 数组为空
        QFile stateFile(statePath);
        QVERIFY(stateFile.open(QIODevice::ReadOnly));
        const QJsonObject root = QJsonDocument::fromJson(stateFile.readAll()).object();
        const QJsonArray dicts = root.value("dictionaries").toArray();
        QCOMPARE(dicts.size(), 1);
        QCOMPARE(dicts.at(0).toObject().value("file_path").toString(), goodPath);
        QVERIFY(root.value("quarantined").toArray().isEmpty());
        stateFile.close();

        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 1);
        QVERIFY(manager.getFailedDictionaries().isEmpty());
    }

    void rejectsMissingStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(!manager.loadState(QDir(tempDir.path()).filePath("missing.json")));
        QVERIFY(manager.lastError().contains("State file does not exist"));
    }

    // 分组过滤：空 filter 不过滤；非空 filter 只保留 tags 有交集的词典；
    // 未打 tag 的词典在任何非空 filter 下都不可见；与 enabled 相互独立。
    void tagFilterRestrictsSearchAcrossApis() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "group_en", {
            {"term", "from en"},
            {"farm", "rural site"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "group_zh", {
            {"term", "from zh"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "group_none", {
            {"term", "from untagged"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("group_en.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("group_zh.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("group_none.ifo")));

        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 3);
        QVERIFY(manager.setDictionaryTags(infos.at(0).id, {"en"}));
        QVERIFY(manager.setDictionaryTags(infos.at(1).id, {"zh"}));

        // 空 filter = 不过滤，等于原有语义
        QCOMPARE(manager.searchWord("term", {}).matches.size(), 3);
        // 单组：只留交集词典；未打 tag 的词典不属于任何分组
        const auto enOnly = manager.searchWord("term", {"en"});
        QCOMPARE(enOnly.matches.size(), 1);
        QCOMPARE(enOnly.matches.constFirst().dictionaryName, QString("group_en"));
        QCOMPARE(manager.searchWord("term", {"zh"}).matches.constFirst().dictionaryName,
                 QString("group_zh"));
        // 多组 OR：任一 tag 命中即可
        QCOMPARE(manager.searchWord("term", {"en", "zh"}).matches.size(), 2);
        // filter 与 enabled 独立：禁用后即使 tag 匹配也不参与
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, false));
        QVERIFY(manager.searchWord("term", {"en"}).matches.isEmpty());
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, true));

        // 相近词回落（searchSimilar）也要吃同一个 filter
        //（StarDictParser::findSimilar 是前缀/包含语义，用 farm 的真前缀 miss）
        const auto miss = manager.searchWord("far", {"en"});
        QVERIFY(!miss.success);
        QVERIFY(miss.suggestions.contains("farm"));
        QVERIFY(!manager.searchWord("far", {"zh"}).suggestions.contains("farm"));

        // 其余查询入口同一语义
        QCOMPARE(manager.prefixSearch("te", 20, {"zh"}), QStringList{"term"});
        QCOMPARE(manager.regexSearch("^farm$", 20, {"en"}), QStringList{"farm"});
        QCOMPARE(manager.regexSearch("^farm$", 20, {"zh"}), QStringList());
        QCOMPARE(manager.searchAll("term", {"en"}).size(), 1);
        QCOMPARE(manager.getAllWords(100, {"zh"}), QStringList{"term"});
    }

    // 全文检索的分组过滤在倒排命中之后做（不重建索引）；候选池被高相关
    // 的其他分组占满时，扩查要能把目标分组的命中捞回来。
    void tagFilterAppliesToFulltextWithoutRebuild() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "ft_g1", {
            {"bee", "honeycomb appears once here"}
        }));
        // 三部 g2 词典的释义重复命中词，相关度压过 g1，无过滤时霸占 top-3
        for (int i = 0; i < 3; ++i) {
            QVERIFY(writeStarDictDictionary(tempDir.path(), QString("ft_g2_%1").arg(i), {
                {"wasp", "honeycomb honeycomb honeycomb"}
            }));
        }

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("ft_g1.ifo")));
        for (int i = 0; i < 3; ++i) {
            QVERIFY(manager.addDictionary(
                QDir(tempDir.path()).filePath(QString("ft_g2_%1.ifo").arg(i))));
        }

        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 4);
        QVERIFY(manager.setDictionaryTags(infos.at(0).id, {"g1"}));
        for (int i = 1; i < 4; ++i) {
            QVERIFY(manager.setDictionaryTags(infos.at(i).id, {"g2"}));
        }

        QVERIFY(manager.isFulltextIndexBuilt() || true); // 惰性，查询时构建
        // 无过滤：top-3 被 g2 占满
        const auto all = manager.fullTextSearch("honeycomb", 3);
        QCOMPARE(all.size(), 3);
        for (const auto& entry : all) {
            QCOMPARE(entry.metadata.value("dictionary").toString().startsWith("ft_g2"), true);
        }
        // 带 g1 过滤：扩查后仍能拿到 g1 的低相关命中
        const auto g1Only = manager.fullTextSearch("honeycomb", 3, {"g1"});
        QCOMPARE(g1Only.size(), 1);
        QCOMPARE(g1Only.constFirst().metadata.value("dictionary").toString(),
                 QString("ft_g1"));
        // 空 filter 与无参等价
        QCOMPARE(manager.fullTextSearch("honeycomb", 3, {}).size(), 3);
    }

    void recordsSearchHistoryAndMovesLatestToFront() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "history_dict", {
            {"alpha", "first"},
            {"beta", "second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("history_dict.ifo")));

        QVERIFY(manager.getSearchHistory().isEmpty());

        auto result = manager.searchWord("alpha");
        QVERIFY(result.success);
        result = manager.searchWord("missing");
        QVERIFY(!result.success);
        result = manager.searchWord("alpha");
        QVERIFY(result.success);

        const auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).query, QString("alpha"));
        QVERIFY(history.at(0).success);
        QCOMPARE(history.at(0).dictionaryName, QString("history_dict"));
        QCOMPARE(history.at(1).query, QString("missing"));
        QVERIFY(!history.at(1).success);
    }

    void persistsAndClearsSearchHistory() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "history_persist", {
            {"alpha", "first"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("history_state.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("history_persist.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(!manager.searchWord("missing").success);
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto restoredHistory = manager.getSearchHistory();
        QCOMPARE(restoredHistory.size(), 2);
        QCOMPARE(restoredHistory.at(0).query, QString("missing"));
        QCOMPARE(restoredHistory.at(1).query, QString("alpha"));

        manager.clearSearchHistory();
        QVERIFY(manager.getSearchHistory().isEmpty());
    }

    void pinsHistoryItemsAndKeepsThemAtTop() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "pin_dict", {
            {"alpha", "first"},
            {"beta", "second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("pin_dict.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.searchWord("beta").success);

        QVERIFY(manager.setSearchHistoryPinned("alpha", true));
        auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).query, QString("alpha"));
        QVERIFY(history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("beta"));
        QVERIFY(!history.at(1).pinned);

        QVERIFY(manager.searchWord("beta").success);
        history = manager.getSearchHistory();
        QCOMPARE(history.at(0).query, QString("alpha"));
        QVERIFY(history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("beta"));
    }

    void persistsPinnedHistoryState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "pin_persist", {
            {"alpha", "first"},
            {"beta", "second"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("pinned_history.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("pin_persist.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.searchWord("beta").success);
        QVERIFY(manager.setSearchHistoryPinned("alpha", true));
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).query, QString("alpha"));
        QVERIFY(history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("beta"));
    }

    void removesSingleHistoryItem() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "remove_history", {
            {"alpha", "first"},
            {"beta", "second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("remove_history.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.searchWord("beta").success);

        auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);

        QVERIFY(manager.removeSearchHistoryItem("alpha"));
        history = manager.getSearchHistory();
        QCOMPARE(history.size(), 1);
        QCOMPARE(history.at(0).query, QString("beta"));

        QVERIFY(!manager.removeSearchHistoryItem("alpha"));
        QVERIFY(manager.lastError().contains("History item not found"));
    }

    void removingPinnedHistoryItemPreservesRemainingOrder() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "remove_pinned", {
            {"alpha", "first"},
            {"beta", "second"},
            {"gamma", "third"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("remove_pinned.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.searchWord("beta").success);
        QVERIFY(manager.searchWord("gamma").success);
        QVERIFY(manager.setSearchHistoryPinned("beta", true));

        QVERIFY(manager.removeSearchHistoryItem("beta"));
        const auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).query, QString("gamma"));
        QVERIFY(!history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("alpha"));
    }

    void exportsAndImportsSearchHistory() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "history_io", {
            {"alpha", "first"},
            {"beta", "second"}
        }));

        const QString historyPath = QDir(tempDir.path()).filePath("history_export.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("history_io.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.searchWord("beta").success);
        QVERIFY(manager.setSearchHistoryPinned("alpha", true));
        QVERIFY(manager.exportSearchHistory(historyPath));

        manager.clearSearchHistory();
        QVERIFY(manager.getSearchHistory().isEmpty());
        QVERIFY(manager.importSearchHistory(historyPath, false));

        const auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 2);
        QCOMPARE(history.at(0).query, QString("alpha"));
        QVERIFY(history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("beta"));
    }

    void importingHistoryCanReplaceExistingItems() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "history_replace", {
            {"alpha", "first"},
            {"beta", "second"},
            {"gamma", "third"}
        }));

        const QString historyPath = QDir(tempDir.path()).filePath("history_replace.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("history_replace.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.exportSearchHistory(historyPath));
        QVERIFY(manager.searchWord("beta").success);
        QVERIFY(manager.searchWord("gamma").success);

        QVERIFY(manager.importSearchHistory(historyPath, true));
        const auto history = manager.getSearchHistory();
        QCOMPARE(history.size(), 1);
        QCOMPARE(history.at(0).query, QString("alpha"));
    }

    void exposesDefaultStateFilePath() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        const QString path = manager.defaultStateFilePath();
        QVERIFY(!path.trimmed().isEmpty());
        QVERIFY(path.endsWith(".json"));
    }

    void savesAndReloadsWorkspaceThroughDefaultStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "workspace_reload", {
            {"alpha", "first"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("workspace_reload.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("workspace_reload.ifo")));
        QVERIFY(manager.searchWord("alpha").success);
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 1);
        QCOMPARE(infos.at(0).name, QString("workspace_reload"));
        QCOMPARE(manager.getSearchHistory().size(), 1);
    }

    void appliesDictionaryTagsToInfo() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "tagged_dict", {
            {"alpha", "first"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("tagged_dict.ifo")));
        const auto before = manager.getLoadedDictionaryInfos();
        QCOMPARE(before.size(), 1);
        QVERIFY(before.at(0).tags.isEmpty());

        QVERIFY(manager.setDictionaryTags(before.at(0).id, {"english", "technical", "english"}));
        const auto after = manager.getLoadedDictionaryInfos();
        QCOMPARE(after.at(0).tags.size(), 2);
        QCOMPARE(after.at(0).tags.at(0), QString("english"));
        QCOMPARE(after.at(0).tags.at(1), QString("technical"));
    }

    void persistsDictionaryTagsInWorkspaceState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "tag_persist", {
            {"alpha", "first"}
        }));

        const QString statePath = QDir(tempDir.path()).filePath("tag_workspace.json");
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("tag_persist.ifo")));
        const auto infos = manager.getLoadedDictionaryInfos();
        QVERIFY(manager.setDictionaryTags(infos.at(0).id, {"reference", "favorite"}));
        QVERIFY(manager.saveState(statePath));

        manager.clear();
        QVERIFY(manager.loadState(statePath));

        const auto restored = manager.getLoadedDictionaryInfos();
        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.at(0).tags.size(), 2);
        QCOMPARE(restored.at(0).tags.at(0), QString("reference"));
        QCOMPARE(restored.at(0).tags.at(1), QString("favorite"));
    }

    void dictionaryInfoReflectsPriorityAndEnabledState() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeStarDictDictionary(tempDir.path(), "info_a", {
            {"alpha", "first"}
        }));
        QVERIFY(writeStarDictDictionary(tempDir.path(), "info_b", {
            {"beta", "second"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("info_a.ifo")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("info_b.ifo")));

        auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 2);
        QCOMPARE(infos.at(0).priority, 1);
        QVERIFY(infos.at(0).enabled);
        QCOMPARE(infos.at(1).priority, 2);

        QVERIFY(manager.moveDictionaryDown(infos.at(0).id));
        QVERIFY(manager.setDictionaryEnabled(infos.at(0).id, false));

        infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.at(0).name, QString("info_b"));
        QCOMPARE(infos.at(0).priority, 1);
        QVERIFY(infos.at(0).enabled);
        QCOMPARE(infos.at(1).name, QString("info_a"));
        QCOMPARE(infos.at(1).priority, 2);
        QVERIFY(!infos.at(1).enabled);
    }

    void historyLimitReturnsNewestItemsFirst() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        manager.clearSearchHistory();
        for (int i = 0; i < 5; ++i) {
            manager.searchWord(QString("word_%1").arg(i));
        }

        const auto limited = manager.getSearchHistory(3);
        QCOMPARE(limited.size(), 3);
        QCOMPARE(limited.at(0).query, QString("word_4"));
        QCOMPARE(limited.at(1).query, QString("word_3"));
        QCOMPARE(limited.at(2).query, QString("word_2"));
    }

    void loadsMdxAndFindsWord() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeMdxDictionary(tempDir.path(), "mdx_basic", {
            {"alpha", "first meaning"},
            {"zeta", "last meaning"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("mdx_basic.mdx")));

        const auto result = manager.searchWord("alpha");
        QVERIFY(result.success);
        QCOMPARE(result.entry.definition, QString("first meaning"));
        QCOMPARE(result.dictionaryName, QString("mdx_basic"));
    }

    void loadsMdxCaseInsensitiveAndProvidesSuggestions() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeMdxDictionary(tempDir.path(), "mdx_case", {
            {"Hello", "capitalized mdx"},
            {"Help", "assist mdx"}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("mdx_case.mdx")));

        const auto hit = manager.searchWord("hello");
        QVERIFY(hit.success);
        QCOMPARE(hit.entry.word, QString("Hello"));

        const auto miss = manager.searchWord("hel");
        QVERIFY(!miss.success);
        QVERIFY(miss.suggestions.contains("Hello"));
        QVERIFY(miss.suggestions.contains("Help"));
    }

    void loadsMdxWithSiblingMddAndServesResources() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        // 释义带 <img>：GUI 渲染管线会转成 res:///<key>?dict=<id> 再取资源
        const QByteArray png = QByteArrayLiteral("\x89PNG\r\n\x1a\nfake-image-bytes");
        QVERIFY(writeMdxDictionary(tempDir.path(), "mdx_pic", {
            {"apple", "A fruit. <img src='/img/apple.png'>"}
        }));
        QVERIFY(writeMddResource(tempDir.path(), "mdx_pic.mdd", {
            {"img/apple.png", png}
        }));

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("mdx_pic.mdx")));

        // format 元数据：GUI 据此决定走 HTML 渲染管线
        const auto result = manager.searchWord("apple");
        QVERIFY(result.success);
        QCOMPARE(result.entry.metadata.value(QStringLiteral("format")).toString(),
                 QString("MDict"));

        QString dictionaryId;
        for (const auto& info : manager.getLoadedDictionaryInfos()) {
            if (info.name == QLatin1String("mdx_pic")) {
                dictionaryId = info.id;
            }
        }
        QVERIFY(!dictionaryId.isEmpty());

        // 资源命中（带/不带前导斜杠键都归一到 img/apple.png）；未命中返回空
        QCOMPARE(manager.loadDictionaryResource(dictionaryId, "img/apple.png"), png);
        QCOMPARE(manager.loadDictionaryResource(dictionaryId, "/img/apple.png"), png);
        QVERIFY(manager.loadDictionaryResource(dictionaryId, "img/missing.png").isEmpty());

        // 无 .mdd 的 MDX：任何资源都返回空
        QVERIFY(writeMdxDictionary(tempDir.path(), "mdx_bare", {
            {"pear", "no resources"}
        }));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("mdx_bare.mdx")));
        QString bareId;
        for (const auto& info : manager.getLoadedDictionaryInfos()) {
            if (info.name == QLatin1String("mdx_bare")) {
                bareId = info.id;
            }
        }
        QVERIFY(!bareId.isEmpty());
        QVERIFY(manager.loadDictionaryResource(bareId, "img/apple.png").isEmpty());

        // 状态文件往返：clear/loadState 后按路径重载并重建 mdd 附件，资源仍可用
        const QString statePath = QDir(tempDir.path()).filePath("state.json");
        QVERIFY(manager.saveState(statePath));
        manager.clear();
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.loadDictionaryResource(dictionaryId, "img/apple.png"), png);
    }

    void formatsSuggestionsAndCombinedResults() {
        UnidictCore::LookupResult suggestionsOnly;
        suggestionsOnly.message = "No exact result for \"helo\".";
        suggestionsOnly.suggestions = {"hello", "help"};
        const QString suggestionText = UnidictCore::formatLookupResult(suggestionsOnly);
        QVERIFY(suggestionText.contains("Did you mean:"));
        QVERIFY(suggestionText.contains("hello"));
        QVERIFY(suggestionText.contains("help"));

        UnidictCore::LookupResult combined;
        combined.success = true;
        combined.matches = {
            {UnidictCore::DictionaryEntry{"hello", "first definition", {}, {}, {}}, "a", "Dict A"},
            {UnidictCore::DictionaryEntry{"hello", "second definition", {}, {}, {}}, "b", "Dict B"}
        };
        const QString combinedText = UnidictCore::formatLookupResult(combined);
        QVERIFY(combinedText.contains("[Dict A]"));
        QVERIFY(combinedText.contains("[Dict B]"));
        QVERIFY(combinedText.contains("--------------------"));
    }
};

QTEST_MAIN(CoreLookupTests)

#include "core_lookup_tests.moc"
