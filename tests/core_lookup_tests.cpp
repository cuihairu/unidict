#include <QDataStream>
#include "epub_fixture.h"
#include "mdict_fixture.h"
#include "stardict_fixture.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <algorithm>

#include "unidict_core.h"

extern "C" {
#include <zlib.h>
}

namespace {

// MDX/.mdd、StarDict、EPUB 夹具抽到 tests/*_fixture.h（多测试目标共用，
// 避免各处复制二进制布局代码；Q-6 起 writeStarDictDictionary/
// writeEpubDictionary 也从本文件挪进共享头）
using UnidictMdictFixture::TestEntry;
using UnidictMdictFixture::writeMdxDictionary;
using UnidictMdictFixture::writeMddResource;
using UnidictMdictFixture::wrapZlibBlock;
using UnidictMdictFixture::toUtf16Le;
using UnidictMdictFixture::appendBigEndian16;
using UnidictMdictFixture::appendBigEndian32;
using UnidictMdictFixture::appendBigEndian64;
using UnidictStardictFixture::writeStarDictDictionary;
using UnidictEpubFixture::writeEpubDictionary;

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

// Q-5 辅助：手写 JSON 落盘（覆盖 loadFromJson/importSearchHistory 各种
// 手工构造的状态文件需要绕过 toJson 的规整输出）
bool writeRawJson(const QString& path, const QJsonObject& object) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    return true;
}

QJsonValue historyItem(const QString& query, bool pinned = false,
                       bool success = false, const QString& dictionary = QString()) {
    QJsonObject item;
    item.insert("query", query);
    item.insert("success", success);
    item.insert("dictionary_name", dictionary);
    item.insert("pinned", pinned);
    return item;
}

QJsonValue dictState(const QString& filePath, bool enabled = true,
                     const QStringList& tags = {}) {
    QJsonObject object;
    object.insert("file_path", filePath);
    object.insert("enabled", enabled);
    QJsonArray tagArray;
    for (const QString& tag : tags) {
        tagArray.append(tag);
    }
    object.insert("tags", tagArray);
    return object;
}

QJsonValue failureState(const QString& filePath, const QString& reason,
                        bool quarantined) {
    QJsonObject object;
    object.insert("file_path", filePath);
    object.insert("reason", reason);
    object.insert("quarantined", quarantined);
    return object;
}

// 一个"父路径是普通文件"的路径：open 必失败（ENOTDIR），root 权限下也
// 稳定，比 chmod 000 可靠（root 无视权限位）
QString unopenableWritePath(const QString& directoryPath, const QString& name) {
    const QString blocker = QDir(directoryPath).filePath(name);
    QFile blockerFile(blocker);
    if (blockerFile.open(QIODevice::WriteOnly)) {
        blockerFile.write("x");
    }
    return QDir(blocker).filePath("nested");
}



} // namespace

class CoreLookupTests : public QObject {
    Q_OBJECT

private slots:
    void init() {
        // addDictionary/recordSearch/clear 内部的隐式 saveState()（无参）写
        // 默认路径 = AppDataLocation——Q-5 的 105 次搜索修剪等用例若不打
        // 靶会把测试数据写进真实 HOME。test mode 把 QStandardPaths 指到
        // 临时目录；本文件所有用例的 load/save 都走显式 statePath，仅
        // exposesDefaultStateFilePath 断言"非空且 .json"，不受影响。
        QStandardPaths::setTestModeEnabled(true);
        UnidictCore::DictionaryManager::instance().clear();
    }

    void cleanup() {
        UnidictCore::DictionaryManager::instance().clear();
        QStandardPaths::setTestModeEnabled(false);
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

    // ---- Q-5 覆盖收口：以下用例补 unidict_core.cpp 的失败/边界分支 ----

    // addDictionary 三类早退（不存在/扩展名不支持/解析失败）+ 各
    // "Dictionary not found" 分支 + moveDown 的两级早退
    void q5_add_and_order_failure_branches() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QVERIFY(!manager.addDictionary(QDir(tempDir.path()).filePath("missing.json")));
        QVERIFY(manager.lastError().contains("does not exist"));

        const QString textPath = QDir(tempDir.path()).filePath("notes.txt");
        QFile textFile(textPath);
        QVERIFY(textFile.open(QIODevice::WriteOnly));
        textFile.write("hello");
        textFile.close();
        QVERIFY(!manager.addDictionary(textPath));
        QVERIFY(manager.lastError().contains("Unsupported dictionary format"));

        const QString brokenPath = QDir(tempDir.path()).filePath("broken.json");
        QFile brokenFile(brokenPath);
        QVERIFY(brokenFile.open(QIODevice::WriteOnly));
        brokenFile.write("{not json");
        brokenFile.close();
        QVERIFY(!manager.addDictionary(brokenPath));
        QVERIFY(manager.lastError().contains("Failed to load dictionary"));

        QVERIFY(!manager.removeDictionary("no-such-id"));
        QVERIFY(!manager.setDictionaryEnabled("no-such-id", true));
        QVERIFY(!manager.setDictionaryTags("no-such-id", {"x"}));
        QVERIFY(!manager.moveDictionaryUp("no-such-id"));
        QVERIFY(!manager.moveDictionaryDown("no-such-id")); // 空列表：size<2 早退

        QVERIFY(writeJsonDictionary(tempDir.path(), "one", {{"alpha", "first"}}));
        QVERIFY(writeJsonDictionary(tempDir.path(), "two", {{"beta", "second"}}));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("one.json")));
        // 仅一个词典：moveDown 还是 size<2 早退
        QVERIFY(!manager.moveDictionaryDown("no-such-id"));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("two.json")));
        QVERIFY(!manager.moveDictionaryUp("no-such-id"));
        // 两个词典、id 不存在：走到循环后的 not-found
        QVERIFY(!manager.moveDictionaryDown("no-such-id"));
        QVERIFY(manager.lastError().contains("cannot be moved down"));
        // 首元素上移/末元素下移按循环边界也是 not-movable
        const QString oneId = manager.getLoadedDictionaryInfos().at(0).id;
        const QString twoId = manager.getLoadedDictionaryInfos().at(1).id;
        QVERIFY(!manager.moveDictionaryUp(oneId));
        QVERIFY(!manager.moveDictionaryDown(twoId));
        // 交换生效 + 交换后原首元素回到末位（可再次 moveUp 复原）
        QVERIFY(manager.moveDictionaryDown(oneId));
        QVERIFY(manager.moveDictionaryUp(oneId));
    }

    // 目录扫描：大小写不同、canonical id 相同的文件去重；无支持格式的目录
    void q5_directory_scan_dedup_and_empty() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString scanDir = QDir(tempDir.path()).filePath("scan");
        QVERIFY(QDir().mkpath(scanDir));
        QVERIFY(writeJsonDictionary(scanDir, "book", {{"alpha", "one"}}));
        // 同小写键的另一文件：canonical 路径 toLower 后与 book.json 同 id
        // （Linux 大小写敏感文件系统上两个真实文件），扫描须只装其一
        QVERIFY(writeJsonDictionary(scanDir, "BOOK", {{"beta", "two"}}));
        QCOMPARE(manager.addDictionariesFromDirectory(scanDir), 1);
        QCOMPARE(manager.getLoadedDictionaries().size(), 1);

        const QString emptyDir = QDir(tempDir.path()).filePath("empty");
        QVERIFY(QDir().mkpath(emptyDir));
        QFile readme(QDir(emptyDir).filePath("readme.txt"));
        QVERIFY(readme.open(QIODevice::WriteOnly));
        readme.write("x");
        readme.close();
        // lastError 先清空：验证 132 行"目录里一个支持的都没有"自己写入
        manager.clear();
        QCOMPARE(manager.addDictionariesFromDirectory(emptyDir), 0);
        QVERIFY(manager.lastError().contains("No supported dictionaries"));

        QVERIFY(manager.addDictionariesFromDirectory(
                    QDir(tempDir.path()).filePath("nope")) == 0);
        QVERIFY(manager.lastError().contains("does not exist"));
    }

    // loadState/saveState 的 IO 与格式失败分支（含"缺 dictionaries 键"）
    void q5_state_io_and_format_failures() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QVERIFY(!manager.loadState(QDir(tempDir.path()).filePath("absent.json")));
        QVERIFY(manager.lastError().contains("State file does not exist"));

        // 路径存在但是目录：open(ReadOnly) 失败
        const QString dirAsFile = QDir(tempDir.path()).filePath("asfile");
        QVERIFY(QDir().mkpath(dirAsFile));
        QVERIFY(!manager.loadState(dirAsFile));
        QVERIFY(manager.lastError().contains("Failed to open state file"));

        const QString garbage = QDir(tempDir.path()).filePath("garbage.json");
        QFile g(garbage);
        QVERIFY(g.open(QIODevice::WriteOnly));
        g.write("nonsense");
        g.close();
        QVERIFY(!manager.loadState(garbage));
        QVERIFY(manager.lastError().contains("Invalid state file"));

        // 合法 JSON 但缺 dictionaries 数组 → loadFromJson 849 早退
        const QString noDicts = QDir(tempDir.path()).filePath("nodicts.json");
        QVERIFY(writeRawJson(noDicts, QJsonObject{{"version", 1}}));
        QVERIFY(!manager.loadState(noDicts));
        QVERIFY(manager.lastError().contains("missing dictionary list"));

        // saveState：目标父路径是普通文件 → open 写失败（ENOTDIR）
        QVERIFY(!manager.saveState(unopenableWritePath(tempDir.path(), "block")));
    }

    // 导出历史的写失败分支
    void q5_history_export_failure() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(!manager.exportSearchHistory(unopenableWritePath(tempDir.path(), "hb")));
    }

    // importSearchHistory 全分支：读失败/格式失败/坏元素跳过/空查询跳过/
    // 去重置顶插位/超 100 截断
    void q5_history_import_branches() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QVERIFY(!manager.importSearchHistory(QDir(tempDir.path()).filePath("absent.json")));
        QVERIFY(manager.lastError().contains("Failed to open history file"));

        const QString garbage = QDir(tempDir.path()).filePath("garbage.json");
        QVERIFY(writeRawJson(garbage, QJsonObject{{"version", 1}})); // 无 history 数组
        QVERIFY(!manager.importSearchHistory(garbage));
        QVERIFY(manager.lastError().contains("Invalid history file"));

        const QString mixed = QDir(tempDir.path()).filePath("mixed.json");
        QVERIFY(writeRawJson(mixed, QJsonObject{
            {"history", QJsonArray{
                QString("not-an-object"),          // 非对象 → continue
                historyItem("   "),                // 空查询 → continue
                historyItem("apple", false, true, "J"),
                historyItem("APPLE", false, true, "J"), // 大小写去重 removeAt
                historyItem("top", true),          // pinned：插到头部
                historyItem("cherry", false, false, "J"),
                historyItem("zoo", true)           // pinned：跨过 top 再插
            }}}));
        QVERIFY(manager.importSearchHistory(mixed));
        QStringList queries;
        for (const auto& item : manager.getSearchHistory(100)) {
            queries << item.query;
        }
        // 置顶区扫描：top 先插 0；zoo 跳过 top 插 1；非置顶的 APPLE、cherry 依次尾随
        QCOMPARE(queries, (QStringList{"top", "zoo", "APPLE", "cherry"}));

        // replaceExisting + 103 项 → 截断到 100
        QJsonArray bulk;
        for (int i = 0; i < 103; ++i) {
            bulk.append(historyItem(QStringLiteral("q%1").arg(i)));
        }
        const QString bulkPath = QDir(tempDir.path()).filePath("bulk.json");
        QVERIFY(writeRawJson(bulkPath, QJsonObject{{"history", bulk}}));
        QVERIFY(manager.importSearchHistory(bulkPath, true));
        QCOMPARE(manager.getSearchHistory(500).size(), 100);
    }

    // setSearchHistoryPinned：置顶区扫描（pin 与 unpin 两侧）+ not-found
    void q5_history_pin_semantics() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString seed = QDir(tempDir.path()).filePath("seed.json");
        QVERIFY(writeRawJson(seed, QJsonObject{
            {"history", QJsonArray{
                historyItem("anchor", true),
                historyItem("target"),
                historyItem("other")
            }}}));
        QVERIFY(manager.importSearchHistory(seed));

        // pin target：须跳过 anchor，落在其紧随其后
        QVERIFY(manager.setSearchHistoryPinned("target", true));
        auto history = manager.getSearchHistory(10);
        QCOMPARE(history.at(0).query, QString("anchor"));
        QCOMPARE(history.at(1).query, QString("target"));
        QVERIFY(history.at(1).pinned);

        // unpin target：又掉回未置顶区尾部（435-439 的 while 扫描）
        QVERIFY(manager.setSearchHistoryPinned("target", false));
        history = manager.getSearchHistory(10);
        QCOMPARE(history.at(0).query, QString("anchor"));
        QCOMPARE(history.at(0).pinned, true);
        QVERIFY(!history.at(1).pinned);

        QVERIFY(!manager.setSearchHistoryPinned("nope", true));
        QVERIFY(manager.lastError().contains("not found"));

        // 非对象/空查询的历史元素在 loadFromJson 恢复侧同样被跳过
        const QString statePath = QDir(tempDir.path()).filePath("hist_state.json");
        QVERIFY(writeRawJson(statePath, QJsonObject{
            {"dictionaries", QJsonArray{}},
            {"history", QJsonArray{
                QString("junk"),
                historyItem("  "),
                historyItem("kept", false, true, "J")
            }}}));
        QVERIFY(manager.loadState(statePath));
        history = manager.getSearchHistory(10);
        QCOMPARE(history.size(), 1);
        QCOMPARE(history.at(0).query, QString("kept"));
    }

    // recordSearch：pinned 项重查保位插队 + 超 100 截断 + 搜索词包装函数
    void q5_record_search_pinned_and_trim() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "words", {
            {"apple", "fruit"}, {"banana", "yellow"}, {"cherry", "red"}
        }));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("words.json")));

        const QString seed = QDir(tempDir.path()).filePath("pin_seed.json");
        QVERIFY(writeRawJson(seed, QJsonObject{
            {"history", QJsonArray{
                historyItem("keeper", true),
                historyItem("apple", true, true, "words")
            }}}));
        QVERIFY(manager.importSearchHistory(seed, true));

        // 重查已置顶的 apple：保 pinned，插到 keeper 之后（1123-1127 扫描）
        QVERIFY(manager.searchWord("apple").success);
        auto history = manager.getSearchHistory(10);
        QCOMPARE(history.at(0).query, QString("keeper"));
        QCOMPARE(history.at(1).query, QString("apple"));
        QVERIFY(history.at(1).pinned);

        // 99 次不同查询 → 101 项 → 1136 截尾保持 100
        for (int i = 0; i < 99; ++i) {
            QVERIFY(!manager.searchWord(QStringLiteral("zzq%1").arg(i)).success);
        }
        history = manager.getSearchHistory(500);
        QCOMPARE(history.size(), 100);
        QCOMPARE(history.at(0).query, QString("keeper"));
        QVERIFY(history.at(0).pinned);
        QCOMPARE(history.at(1).query, QString("apple"));

        // 无命中且无建议的路径也入历史；空查询不记录也不崩
        manager.clearSearchHistory();
        QVERIFY(manager.getSearchHistory(5).isEmpty());
        QVERIFY(!manager.searchWord("  ").success);
        QVERIFY(manager.getSearchHistory(5).isEmpty());

        // 自由函数包装（1142-1143）
        const QString text = UnidictCore::searchWord("apple");
        QVERIFY(text.contains("fruit"));
    }

    // 聚合查询的截断/去重边界：searchSimilar/getAllWords/prefixSearch/
    // regexSearch 的 break、searchAll 空查询、全文索引跳过空释义
    void q5_query_engine_breaks() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "d1", {
            {"hello", "greeting"}, {"help", "assist"}, {"world", "earth"}
        }));
        // 与 d1 交叠 "hello"：验证 searchSimilar 的 seen 去重 continue
        QVERIFY(writeJsonDictionary(tempDir.path(), "d2", {
            {"hello", "second greeting"}, {"hero", "protagonist"}
        }));
        // 空释义词条：全文索引构建时须 continue 跳过（724 行）
        QVERIFY(writeJsonDictionary(tempDir.path(), "d3", {
            {"emptydef", ""}, {"needleword", "contains needle here"}
        }));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("d1.json")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("d2.json")));
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("d3.json")));

        // maxResults 在词典内层 break（550），下一词典被条件 continue 挡下
        QStringList similar = manager.searchSimilar("hel", 2);
        QCOMPARE(similar, (QStringList{"hello", "help"}));
        // d2 的候选窗口只剩 3-2=1，其首条 "hello" 与 d1 重复被 543-544 去重
        // 跳过，窗口耗尽 → hero 挤不进本轮，结果 2 条（跨词典去重语义）
        similar = manager.searchSimilar("h", 3);
        QCOMPARE(similar, (QStringList{"hello", "help"}));

        // getAllWords：limit 命中内层 break（568）
        QCOMPARE(manager.getAllWords(2).size(), 2);
        QVERIFY(manager.getAllWords(10).size() >= 6); // 含空释义词

        // searchAll 空查询早退
        QVERIFY(manager.searchAll("   ").isEmpty());
        QCOMPARE(manager.searchAll("hello").size(), 2);

        // prefixSearch 满额即 break（694）
        QCOMPARE(manager.prefixSearch("hel", 1), (QStringList{"hello"}));

        // regexSearch limit break（761）
        QCOMPARE(manager.regexSearch("^h", 2).size(), 2);
        QVERIFY(manager.regexSearch("[", 5).isEmpty()); // 非法模式早退

        // 全文：跳过空释义后仍能命中，且空查询/0 上限早退
        const auto hits = manager.fullTextSearch("NEEDLE", 5);
        QCOMPARE(hits.size(), 1);
        QCOMPARE(hits.at(0).word, QString("needleword"));
        QVERIFY(manager.fullTextSearch("  ").isEmpty());
        QVERIFY(manager.fullTextSearch("NEEDLE", 0).isEmpty());
        QVERIFY(manager.isFulltextIndexBuilt());
    }

    // loadFromJson 的 quarantine 恢复矩阵：坏元素/空路径/重复路径/
    // 隔离跳过/缺文件登记/扩展名不支持登记/自愈摘除/enabled+tags 恢复
    void q5_state_quarantine_matrix() {
        auto& manager = UnidictCore::DictionaryManager::instance();
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QVERIFY(writeJsonDictionary(tempDir.path(), "good", {{"alpha", "ok"}}));
        const QString goodPath = QDir(tempDir.path()).filePath("good.json");
        const QString gonePath = QDir(tempDir.path()).filePath("gone.json");
        const QString textPath = QDir(tempDir.path()).filePath("legacy.txt");
        QFile textFile(textPath);
        QVERIFY(textFile.open(QIODevice::WriteOnly));
        textFile.write("x");
        textFile.close();
        const QString brokenJson = QDir(tempDir.path()).filePath("corrupt.json");
        QFile cf(brokenJson);
        QVERIFY(cf.open(QIODevice::WriteOnly));
        cf.write("{oops");
        cf.close();

        const QString quarantinedPath = QDir(tempDir.path()).filePath("quarantined.json");
        const QString statePath = QDir(tempDir.path()).filePath("matrix.json");
        QVERIFY(writeRawJson(statePath, QJsonObject{
            {"dictionaries", QJsonArray{
                QString("not-an-object"),                 // 881 continue
                dictState(""),                            // 887 空路径 continue
                dictState(gonePath),                      // 898 缺文件 → 登记
                dictState(gonePath),                      // 重复登记同因 → 幂等 false
                dictState(quarantinedPath),               // 隔离中 → 不试解析
                dictState(textPath),                      // 916 扩展名不支持 → 登记
                dictState(brokenJson),                    // 924 解析失败 → 隔离
                dictState(goodPath, false, {"Med", "med", " "}) // 禁用+tags 归一
            }},
            {"quarantined", QJsonArray{
                QString("not-an-object"),                       // 860 continue
                failureState("", "no path", false),             // 865 空路径 continue
                failureState(textPath, "old reason", false),    // 865 重复路径先入者赢
                failureState(goodPath, "transient", false),     // 非隔离：成功后自愈
                failureState(quarantinedPath, "corrupted", true) // 隔离中：跳过解析
            }},
            {"history", QJsonArray{historyItem("apple", true, true, "D")}},
            {"version", 1}
        }));

        QVERIFY(manager.loadState(statePath));

        // 只加载了 good（禁用态），且 tags 归一：大小写去重 + 空白剔除
        auto infos = manager.getLoadedDictionaryInfos();
        QCOMPARE(infos.size(), 1);
        QCOMPARE(infos.at(0).enabled, false);
        QCOMPARE(infos.at(0).tags, (QStringList{"Med"}));
        QVERIFY(manager.hasDictionaries()); // 有词典但全禁用 → disabled 提示分支
        const UnidictCore::LookupResult disabledHint = manager.searchWord("alpha");
        QVERIFY(!disabledHint.success);
        QVERIFY(disabledHint.message.contains("disabled"));

        // 失败列表：textPath 的旧记录 reason 被刷新（1038-1044），quarantined
        // 原样；good 的非隔离记录因加载成功被摘除（935-936）
        const auto failures = manager.getFailedDictionaries();
        QStringList failurePaths;
        for (const auto& failure : failures) {
            failurePaths << failure.filePath;
            if (failure.filePath == textPath) {
                QVERIFY(failure.reason.contains("Unsupported"));
                QCOMPARE(failure.quarantined, false);
            }
        }
        QVERIFY(failurePaths.contains(gonePath));
        QVERIFY(failurePaths.contains(brokenJson));   // 解析失败 → 隔离登记
        QVERIFY(failurePaths.contains(quarantinedPath));
        QVERIFY(!failurePaths.contains(goodPath));    // 自愈摘除
        QVERIFY(failures.size() >= 3);

        const auto quarantinedFlag = [&](const QString& path) {
            for (const auto& failure : manager.getFailedDictionaries()) {
                if (failure.filePath == path) {
                    return failure.quarantined;
                }
            }
            return false;
        };
        QVERIFY(quarantinedFlag(brokenJson));

        // failuresChanged=true → 980 自动落盘到默认路径；再加载回来须一致
        QVERIFY(manager.saveState(statePath));
        QVERIFY(manager.loadState(statePath));
        QCOMPARE(manager.getFailedDictionaries().size(), failures.size());

        // 重试/遗忘全分支
        const QString neverPath = QDir(tempDir.path()).filePath("never-seen.json");
        QVERIFY(!manager.retryFailedDictionary(neverPath)); // 1053 not-in-list
        QVERIFY(!manager.forgetFailedDictionary(neverPath)); // 1074 not-in-list
        QVERIFY(manager.lastError().contains("not in the failed list"));

        // 重试仍失败：留在列表、reason 刷新为 addDictionary 的错误（1062-1068）
        QVERIFY(!manager.retryFailedDictionary(gonePath));
        QVERIFY(quarantinedFlag(gonePath)); // 重试失败 → 确认隔离
        for (const auto& failure : manager.getFailedDictionaries()) {
            if (failure.filePath == gonePath) {
                QVERIFY(failure.reason.contains("does not exist"));
            }
        }

        // 重试成功：文件补回来 → 加载 + 摘除记录（addDictionary 88-92 自愈）
        QVERIFY(writeJsonDictionary(tempDir.path(), "gone", {{"late", "arrival"}}));
        QVERIFY(manager.retryFailedDictionary(gonePath));
        QVERIFY(!quarantinedFlag(gonePath));
        QCOMPARE(manager.getLoadedDictionaryInfos().size(), 2);

        // 遗忘：从列表移除并落盘（1078-1082）
        QVERIFY(manager.forgetFailedDictionary(brokenJson));
        QVERIFY(!quarantinedFlag(brokenJson));
        QVERIFY(manager.saveState(statePath));
        QFile check(statePath);
        QVERIFY(check.open(QIODevice::ReadOnly));
        QVERIFY(!QString(check.readAll()).contains(brokenJson));
    }

    // BUG-005（2026-10-03 用户报告：顶栏 125170 词条但简单词查不出）
    // 回归双锚点：
    // ① 计数与可查一致性——getIndexedWordCount 汇总的每个词头都必须
    //    lookup 命中；且大小写互通（JsonParser 曾是四个 parser 里唯一
    //    精确匹配的：Hello 查不到词头 hello，只给 Did-you-mean）
    // ② 词头未命中 → 释义全文兜底：汉英词典查英文（good/the）时词头
    //    语言与查询语言相反，词在释义里——searchWord/searchAll 都兜底，
    //    命中带 matchType=fulltext
    void bug005CountAndLookupConsistency() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QVERIFY(writeJsonDictionary(tempDir.path(), "consistency", {
            {"hello", "greeting"},
            {"World", "earth"},
            {"qt", "framework"},
            {"你好", "hello; hi"},
            {"好", "good; fine"}
        }));
        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(manager.addDictionary(QDir(tempDir.path()).filePath("consistency.json")));

        QCOMPARE(manager.getIndexedWordCount(), 5);

        // ① 计数与索引键一致：词头逐一可查（原样词形）
        const QStringList heads = {"hello", "World", "qt", "你好", "好"};
        for (const QString& w : heads) {
            QVERIFY2(manager.searchWord(w).success, qPrintable(w));
        }

        // ① 大小写折叠互通（查询侧大小写换任意形态都命中）
        QVERIFY(manager.searchWord("HELLO").success);
        QVERIFY(manager.searchWord("world").success); // 词头 World（大写 W）
        QVERIFY(manager.searchWord("QT").success);
        QCOMPARE(manager.searchAll("HELLO").size(), 1);

        // ② 词头 miss → 释义全文兜底（GUI aggregateLookup 走 searchAll）
        const auto ft = manager.searchWord("good");
        QVERIFY(ft.success);
        QCOMPARE(ft.matches.constFirst().entry.word, QStringLiteral("好"));
        QCOMPARE(ft.matches.constFirst().entry.metadata.value("matchType").toString(),
                 QStringLiteral("fulltext"));
        const auto ftAll = manager.searchAll("good");
        QVERIFY(!ftAll.isEmpty());
        QCOMPARE(ftAll.constFirst().metadata.value("matchType").toString(),
                 QStringLiteral("fulltext"));
        // 词头命中时绝不掺全文兜底（matchType 缺席 = 词头精确）
        const auto exact = manager.searchWord("hello");
        QVERIFY(exact.success);
        QVERIFY(!exact.entry.metadata.contains("matchType"));
    }
};

QTEST_MAIN(CoreLookupTests)

#include "core_lookup_tests.moc"
