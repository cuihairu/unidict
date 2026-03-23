#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <algorithm>

#include "unidict_core.h"

extern "C" {
#include <zlib.h>
}

namespace {

struct TestEntry {
    QString word;
    QString definition;
};

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

QByteArray toUtf16Le(const QString& text) {
    QByteArray bytes;
    bytes.reserve(text.size() * 2);
    for (QChar ch : text) {
        const char16_t value = ch.unicode();
        bytes.append(static_cast<char>(value & 0xff));
        bytes.append(static_cast<char>((value >> 8) & 0xff));
    }
    return bytes;
}

QByteArray wrapZlibBlock(const QByteArray& raw) {
    uLongf bound = compressBound(raw.size());
    QByteArray compressed(static_cast<int>(bound), '\0');
    if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &bound,
                  reinterpret_cast<const Bytef*>(raw.constData()), raw.size(), Z_BEST_COMPRESSION) != Z_OK) {
        return {};
    }
    compressed.resize(static_cast<int>(bound));

    QByteArray block;
    block.append("\x02\x00\x00\x00", 4);
    const quint32 checksum = ::adler32(0L, reinterpret_cast<const Bytef*>(raw.constData()), raw.size());
    quint32 checksumBe = qToBigEndian(checksum);
    block.append(reinterpret_cast<const char*>(&checksumBe), 4);
    block.append(compressed);
    return block;
}

void appendBigEndian16(QByteArray& data, quint16 value) {
    const quint16 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 2);
}

void appendBigEndian32(QByteArray& data, quint32 value) {
    const quint32 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 4);
}

void appendBigEndian64(QByteArray& data, quint64 value) {
    const quint64 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 8);
}

bool writeMdxDictionary(const QString& directoryPath,
                        const QString& dictionaryName,
                        QList<TestEntry> entries) {
    std::sort(entries.begin(), entries.end(), [](const TestEntry& a, const TestEntry& b) {
        return a.word.toLower() < b.word.toLower();
    });

    QByteArray recordData;
    QVector<quint64> offsets;
    offsets.reserve(entries.size());
    for (const auto& entry : entries) {
        offsets.append(static_cast<quint64>(recordData.size()));
        recordData.append(entry.definition.toUtf8());
        recordData.append('\0');
    }

    QByteArray keyBlockRaw;
    for (int i = 0; i < entries.size(); ++i) {
        appendBigEndian64(keyBlockRaw, offsets[i]);
        keyBlockRaw.append(entries[i].word.toUtf8());
        keyBlockRaw.append('\0');
    }
    const QByteArray keyBlock = wrapZlibBlock(keyBlockRaw);

    QByteArray keyInfoRaw;
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(entries.size()));
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(entries.constFirst().word.toUtf8().size()));
    keyInfoRaw.append(entries.constFirst().word.toUtf8());
    keyInfoRaw.append('\0');
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(entries.constLast().word.toUtf8().size()));
    keyInfoRaw.append(entries.constLast().word.toUtf8());
    keyInfoRaw.append('\0');
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(keyBlock.size()));
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(keyBlockRaw.size()));
    const QByteArray keyInfoBlock = wrapZlibBlock(keyInfoRaw);

    QByteArray keywordHeader;
    appendBigEndian64(keywordHeader, 1);
    appendBigEndian64(keywordHeader, static_cast<quint64>(entries.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyInfoRaw.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyInfoBlock.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyBlock.size()));
    const quint32 keywordChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(keywordHeader.constData()),
                                              keywordHeader.size());

    const QByteArray recordBlock = wrapZlibBlock(recordData);
    QByteArray recordSection;
    appendBigEndian64(recordSection, 1);
    appendBigEndian64(recordSection, static_cast<quint64>(entries.size()));
    appendBigEndian64(recordSection, 16);
    appendBigEndian64(recordSection, static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, static_cast<quint64>(recordData.size()));
    recordSection.append(recordBlock);

    const QString headerText =
        QString("<Dictionary GeneratedByEngineVersion=\"2.0\" RequiredEngineVersion=\"2.0\" "
                "Encrypted=\"0\" Encoding=\"UTF-8\" Format=\"Html\" Title=\"%1\" Description=\"MDX test dictionary\" />")
            .arg(dictionaryName);
    QByteArray headerBytes = toUtf16Le(headerText);
    headerBytes.append('\0');
    headerBytes.append('\0');
    const quint32 headerChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(headerBytes.constData()),
                                             headerBytes.size());

    QFile mdxFile(QDir(directoryPath).filePath(dictionaryName + ".mdx"));
    if (!mdxFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QByteArray prefix;
    appendBigEndian32(prefix, static_cast<quint32>(headerBytes.size()));
    mdxFile.write(prefix);
    mdxFile.write(headerBytes);
    quint32 headerChecksumLe = qToLittleEndian(headerChecksum);
    mdxFile.write(reinterpret_cast<const char*>(&headerChecksumLe), 4);
    mdxFile.write(keywordHeader);
    quint32 keywordChecksumBe = qToBigEndian(keywordChecksum);
    mdxFile.write(reinterpret_cast<const char*>(&keywordChecksumBe), 4);
    mdxFile.write(keyInfoBlock);
    mdxFile.write(keyBlock);
    mdxFile.write(recordSection);

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
        QVERIFY(manager.setDictionaryEnabled(infos.at(1).id, false));
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

    void rejectsMissingStateFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        auto& manager = UnidictCore::DictionaryManager::instance();
        QVERIFY(!manager.loadState(QDir(tempDir.path()).filePath("missing.json")));
        QVERIFY(manager.lastError().contains("State file does not exist"));
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
        QVERIFY(manager.setDictionaryEnabled(infos.at(1).id, false));

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
