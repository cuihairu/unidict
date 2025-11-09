#include <QCoreApplication>
#include <QDebug>
#include <QStringList>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QRegularExpression>
#include <QDirIterator>
#include <QDir>

#include "unidict_core.h"
#include "data_store.h"
#include "plugin_manager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Unidict CLI - offline dictionary lookups");
    parser.addHelpOption();

    QCommandLineOption dictOpt({"d", "dict"}, "Dictionary file path (repeatable)", "path");
    parser.addOption(dictOpt);

    QCommandLineOption modeOpt({"m", "mode"}, "Search mode: exact|prefix|fuzzy|wildcard|regex", "mode", "exact");
    parser.addOption(modeOpt);

    QCommandLineOption patternOpt({"p", "pattern"}, "Pattern for wildcard mode (if different from word)", "pattern");
    parser.addOption(patternOpt);

    QCommandLineOption listDictsOpt("list-dicts", "List loaded dictionaries and exit");
    parser.addOption(listDictsOpt);

    QCommandLineOption listDictsVerboseOpt("list-dicts-verbose", "List loaded dictionaries with word count and description, then exit");
    parser.addOption(listDictsVerboseOpt);

    QCommandLineOption historyOpt("history", "Show recent search history (default 20)", "N");
    parser.addOption(historyOpt);

    QCommandLineOption saveOpt("save", "When using exact mode, save the result to vocabulary");
    parser.addOption(saveOpt);

    QCommandLineOption showVocabOpt("show-vocab", "Show saved vocabulary items and exit");
    parser.addOption(showVocabOpt);

    QCommandLineOption allOpt("all", "Show exact-match results from all dictionaries");
    parser.addOption(allOpt);

    QCommandLineOption whereOpt("where", "List which dictionaries contain the word and exit", "word");
    parser.addOption(whereOpt);

    QCommandLineOption scanDirOpt("scan-dir", "Scan a directory recursively for supported dictionary files", "path");
    parser.addOption(scanDirOpt);

    QCommandLineOption indexSaveOpt("index-save", "Save in-memory index to a file and exit", "path");
    parser.addOption(indexSaveOpt);

    QCommandLineOption indexLoadOpt("index-load", "Load index from a file before searching", "path");
    parser.addOption(indexLoadOpt);

    QCommandLineOption clearCacheOpt("clear-cache", "Clear the dictionary cache directory and exit");
    parser.addOption(clearCacheOpt);

    QCommandLineOption cachePruneMbOpt("cache-prune-mb", "Prune cache to at most N megabytes and exit", "N");
    parser.addOption(cachePruneMbOpt);

    QCommandLineOption cachePruneDaysOpt("cache-prune-days", "Remove cache files older than N days and exit", "N");
    parser.addOption(cachePruneDaysOpt);

    QCommandLineOption cacheSizeOpt("cache-size", "Print cache size (bytes) and exit");
    parser.addOption(cacheSizeOpt);

    QCommandLineOption cacheDirOpt("cache-dir", "Print cache directory path and exit");
    parser.addOption(cacheDirOpt);

    QCommandLineOption dataDirOpt("data-dir", "Print data directory path and exit");
    parser.addOption(dataDirOpt);

    QCommandLineOption exportVocabOpt("export-vocab", "Export vocabulary to CSV file and exit", "file");
    parser.addOption(exportVocabOpt);

    QCommandLineOption dumpWordsOpt("dump-words", "Dump first N words from index and exit", "N");
    parser.addOption(dumpWordsOpt);

    QCommandLineOption indexCountOpt("index-count", "Print indexed word count and exit");
    parser.addOption(indexCountOpt);

    QCommandLineOption dropDictOpt("drop-dict", "Remove a loaded dictionary by its name and exit", "name");
    parser.addOption(dropDictOpt);

    QCommandLineOption listPluginsOpt("list-plugins", "List registered parser extensions and exit");
    parser.addOption(listPluginsOpt);

    parser.addPositionalArgument("word", "Word to search or prefix/pattern depending on mode");

    parser.process(app);

    const QStringList dicts = parser.values(dictOpt);
    const QString mode = parser.value(modeOpt).toLower();
    const QStringList positional = parser.positionalArguments();

    if (positional.isEmpty() &&
        !parser.isSet(listDictsOpt) && !parser.isSet(historyOpt) &&
        !parser.isSet(showVocabOpt) && !parser.isSet(scanDirOpt) &&
        !parser.isSet(indexSaveOpt) && !parser.isSet(dumpWordsOpt) && !parser.isSet(dropDictOpt) && !parser.isSet(listPluginsOpt) && !parser.isSet(indexCountOpt)) {
        qInfo() << "Usage: unidict_cli [-d <dict> ...] [-m exact|prefix|fuzzy|wildcard] <word>";
        qInfo() << "Examples:";
        qInfo() << "  unidict_cli -d mydict.mdx hello";
        qInfo() << "  UNIDICT_DICTS=path/to/a.mdx:path/to/b.ifo unidict_cli --mode prefix inter";
        return 1;
    }

    // Load dictionaries from options or env var
    QStringList dictPaths = dicts;
    if (dictPaths.isEmpty()) {
        const QString env = qEnvironmentVariable("UNIDICT_DICTS");
        if (!env.isEmpty()) {
            // support both ':' and ';' as separators
            for (const QString& p : env.split(QRegularExpression("[:;]"), Qt::SkipEmptyParts)) {
                dictPaths << p.trimmed();
            }
        }
    }

    for (const QString& path : dictPaths) {
        UnidictCore::DictionaryManager::instance().addDictionary(path);
    }

    if (parser.isSet(scanDirOpt)) {
        const QString dir = parser.value(scanDirOpt);
        QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
        int added = 0;
        while (it.hasNext()) {
            const QString p = it.next();
            if (UnidictCore::DictionaryManager::instance().addDictionary(p)) added++;
        }
        qInfo() << "Scanned" << dir << ", added:" << added;
        // if no other action, exit
        if (positional.isEmpty() && !parser.isSet(listDictsOpt) && !parser.isSet(historyOpt) && !parser.isSet(showVocabOpt) && !parser.isSet(indexSaveOpt))
            return 0;
    }

    if (parser.isSet(indexLoadOpt)) {
        const QString p = parser.value(indexLoadOpt);
        if (!UnidictCore::DictionaryManager::instance().loadIndex(p)) {
            qWarning() << "Failed to load index from" << p;
        }
    }

    if (parser.isSet(clearCacheOpt)) {
        const bool ok = UnidictCore::PathUtils::clearCache();
        qInfo() << (ok ? "Cache cleared" : "Cache clear failed");
        if (positional.isEmpty()) return ok ? 0 : 4;
    }

    if (parser.isSet(cachePruneMbOpt)) {
        bool okInt = false;
        const quint64 mb = parser.value(cachePruneMbOpt).toULongLong(&okInt);
        const quint64 bytes = mb * 1024ull * 1024ull;
        const bool ok = okInt && UnidictCore::PathUtils::pruneCacheBytes(bytes);
        qInfo() << (ok ? "Cache pruned" : "Cache prune failed") << "(max MB=" << mb << ")";
        if (positional.isEmpty()) return ok ? 0 : 4;
    }

    if (parser.isSet(cachePruneDaysOpt)) {
        bool okInt = false; int days = parser.value(cachePruneDaysOpt).toInt(&okInt);
        const bool ok = okInt && UnidictCore::PathUtils::pruneCacheOlderThanDays(days);
        qInfo() << (ok ? "Cache pruned by age" : "Cache age prune failed") << "(days=" << days << ")";
        if (positional.isEmpty()) return ok ? 0 : 4;
    }

    if (parser.isSet(cacheSizeOpt)) {
        const quint64 sz = UnidictCore::PathUtils::cacheSizeBytes();
        qInfo() << sz;
        if (positional.isEmpty()) return 0;
    }

    if (parser.isSet(cacheDirOpt)) {
        qInfo().noquote() << UnidictCore::PathUtils::cacheDir();
        if (positional.isEmpty()) return 0;
    }

    if (parser.isSet(dataDirOpt)) {
        qInfo().noquote() << UnidictCore::PathUtils::dataDir();
        if (positional.isEmpty()) return 0;
    }

    if (parser.isSet(listDictsOpt) || parser.isSet(listDictsVerboseOpt)) {
        const auto& mgr = UnidictCore::DictionaryManager::instance();
        const QStringList names = mgr.getLoadedDictionaries();
        qInfo() << "Loaded dictionaries (" << names.size() << ")";
        if (parser.isSet(listDictsVerboseOpt)) {
            const auto metas = mgr.getDictionariesMeta();
            for (const auto& m : metas) {
                qInfo().noquote() << "- " << m.name << " (words=" << m.wordCount << ") " << m.description;
            }
        } else {
            for (const QString& n : names) qInfo().noquote() << "- " << n;
        }
        return 0;
    }

    if (parser.isSet(dropDictOpt)) {
        const QString name = parser.value(dropDictOpt);
        const bool ok = UnidictCore::DictionaryManager::instance().removeDictionary(name);
        qInfo() << (ok ? "Removed" : "Not found") << name;
        return ok ? 0 : 6;
    }

    if (parser.isSet(listPluginsOpt)) {
        UnidictCore::PluginManager::instance().ensureBuiltinsRegistered();
        const auto stats = UnidictCore::PluginManager::instance().extensionStats();
        qInfo() << "Registered parser extensions:";
        for (auto it = stats.begin(); it != stats.end(); ++it) {
            qInfo().noquote() << it.key() << ":" << it.value();
        }
        return 0;
    }

    if (parser.isSet(whereOpt)) {
        const QString w = parser.value(whereOpt);
        const QStringList ds = UnidictCore::DictionaryManager::instance().getDictionariesForWord(w);
        qInfo().noquote() << ds.join("\n");
        return 0;
    }

    if (parser.isSet(historyOpt)) {
        bool ok = false;
        int N = parser.value(historyOpt).toInt(&ok);
        if (!ok || N <= 0) N = 20;
        const QStringList h = UnidictCore::DataStore::instance().getSearchHistory(N);
        qInfo().noquote() << h.join("\n");
        return 0;
    }

    if (parser.isSet(showVocabOpt)) {
        const auto items = UnidictCore::DataStore::instance().getVocabulary();
        for (const auto& e : items) {
            qInfo().noquote() << e.word << ": " << e.definition;
        }
        return 0;
    }

    if (parser.isSet(indexSaveOpt)) {
        const QString p = parser.value(indexSaveOpt);
        if (UnidictCore::DictionaryManager::instance().saveIndex(p)) {
            qInfo() << "Index saved to" << p;
            return 0;
        } else {
            qWarning() << "Failed to save index to" << p;
            return 3;
        }
    }

    if (parser.isSet(exportVocabOpt)) {
        const QString p = parser.value(exportVocabOpt);
        const bool ok = UnidictCore::DataStore::instance().exportVocabularyCSV(p);
        qInfo() << (ok ? "Vocabulary exported to" : "Failed to export vocabulary to") << p;
        return ok ? 0 : 5;
    }

    if (parser.isSet(dumpWordsOpt)) {
        bool ok = false; int N = parser.value(dumpWordsOpt).toInt(&ok); if (!ok || N <= 0) N = 100;
        const QStringList words = UnidictCore::DictionaryManager::instance().getAllIndexedWords();
        const int count = std::min(N, words.size());
        for (int i = 0; i < count; ++i) qInfo().noquote() << words.at(i);
        return 0;
    }

    if (parser.isSet(indexCountOpt)) {
        qInfo() << UnidictCore::DictionaryManager::instance().getIndexedWordCount();
        return 0;
    }

    const QString word = positional.first();

    if (mode == "exact") {
        if (parser.isSet(allOpt)) {
            const auto entries = UnidictCore::DictionaryManager::instance().searchAll(word);
            if (entries.empty()) {
                qInfo().noquote() << "Word not found: " + word;
                return 0;
            }
            for (const auto& e : entries) {
                const QString dict = e.metadata.value("dictionary").toString();
                qInfo().noquote() << (dict.isEmpty() ? e.word : ("[" + dict + "] " + e.word));
                qInfo().noquote() << e.definition << "\n";
            }
            UnidictCore::DataStore::instance().addSearchHistory(word);
            return 0;
        } else {
            const QString definition = UnidictCore::searchWord(word);
            if (!definition.startsWith("Word not found")) {
                UnidictCore::DataStore::instance().addSearchHistory(word);
                if (parser.isSet(saveOpt)) {
                    UnidictCore::DictionaryEntry e; e.word = word; e.definition = definition;
                    UnidictCore::DataStore::instance().addVocabularyItem(e);
                }
            }
            qInfo().noquote() << definition;
            return 0;
        }
    } else if (mode == "prefix") {
        const QStringList results = UnidictCore::DictionaryManager::instance().prefixSearch(word, 20);
        qInfo().noquote() << results.join("\n");
        return 0;
    } else if (mode == "fuzzy") {
        const QStringList results = UnidictCore::DictionaryManager::instance().fuzzySearch(word, 20);
        qInfo().noquote() << results.join("\n");
        return 0;
    } else if (mode == "wildcard") {
        const QString pattern = parser.isSet(patternOpt) ? parser.value(patternOpt) : word;
        const QStringList results = UnidictCore::DictionaryManager::instance().wildcardSearch(pattern, 20);
        qInfo().noquote() << results.join("\n");
        return 0;
    } else if (mode == "regex") {
        const QString pattern = parser.isSet(patternOpt) ? parser.value(patternOpt) : word;
        const QStringList results = UnidictCore::DictionaryManager::instance().regexSearch(pattern, 20);
        qInfo().noquote() << results.join("\n");
        return 0;
    } else {
        qCritical() << "Unknown mode:" << mode;
        return 2;
    }
}
