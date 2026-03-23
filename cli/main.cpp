#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include "unidict_core.h"

namespace {

void loadDefaultDictionaryLocations() {
    auto& manager = UnidictCore::DictionaryManager::instance();
    manager.loadState();
    const QString envDir = qEnvironmentVariable("UNIDICT_DICT_DIR");
    if (!envDir.isEmpty()) {
        manager.addDictionariesFromDirectory(envDir);
    }

    const QString localDir = QDir(QCoreApplication::applicationDirPath()).filePath("dictionaries");
    if (QFileInfo::exists(localDir)) {
        manager.addDictionariesFromDirectory(localDir);
    }
}

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("unidict_cli");
    QCoreApplication::setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Unidict command line dictionary");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption dictOption({"d", "dict"}, "Load a dictionary file (.ifo or .mdx).", "file");
    QCommandLineOption dictDirOption({"D", "dict-dir"}, "Load all supported dictionaries from a directory.", "dir");
    QCommandLineOption listOption({"l", "list"}, "List currently loaded dictionaries.");
    QCommandLineOption historyOption({"H", "history"}, "Show recent search history.");
    QCommandLineOption saveStateOption({"save-state"}, "Save workspace state to a file.", "file");
    QCommandLineOption loadStateOption({"load-state"}, "Load workspace state from a file.", "file");
    QCommandLineOption exportHistoryOption({"export-history"}, "Export search history to a file.", "file");
    QCommandLineOption importHistoryOption({"import-history"}, "Import search history from a file.", "file");
    QCommandLineOption replaceHistoryOption({"replace-history"}, "Replace existing history when importing.");
    QCommandLineOption clearHistoryOption({"clear-history"}, "Clear recent search history.");

    parser.addOption(dictOption);
    parser.addOption(dictDirOption);
    parser.addOption(listOption);
    parser.addOption(historyOption);
    parser.addOption(saveStateOption);
    parser.addOption(loadStateOption);
    parser.addOption(exportHistoryOption);
    parser.addOption(importHistoryOption);
    parser.addOption(replaceHistoryOption);
    parser.addOption(clearHistoryOption);
    parser.addPositionalArgument("word", "Word to search.");
    parser.process(app);

    auto& manager = UnidictCore::DictionaryManager::instance();
    loadDefaultDictionaryLocations();

    for (const QString& filePath : parser.values(dictOption)) {
        manager.addDictionary(filePath);
    }

    for (const QString& dirPath : parser.values(dictDirOption)) {
        manager.addDictionariesFromDirectory(dirPath);
    }

    QTextStream out(stdout);

    if (parser.isSet(loadStateOption)) {
        if (!manager.loadState(parser.value(loadStateOption))) {
            out << manager.lastError() << "\n";
            return 1;
        }
        out << "Workspace loaded.\n";
        return 0;
    }

    if (parser.isSet(saveStateOption)) {
        if (!manager.saveState(parser.value(saveStateOption))) {
            out << "Failed to save workspace.\n";
            return 1;
        }
        out << "Workspace saved.\n";
        return 0;
    }

    if (parser.isSet(listOption)) {
        const auto infos = manager.getLoadedDictionaryInfos();
        if (infos.isEmpty()) {
            out << "No dictionaries loaded.\n";
            if (!manager.lastError().isEmpty()) {
                out << manager.lastError() << "\n";
            }
            return 1;
        }

        for (const auto& info : infos) {
            out << info.name << " [" << info.format << "] " << info.filePath << "\n";
        }
        return 0;
    }

    if (parser.isSet(clearHistoryOption)) {
        manager.clearSearchHistory();
        out << "History cleared.\n";
        return 0;
    }

    if (parser.isSet(exportHistoryOption)) {
        if (!manager.exportSearchHistory(parser.value(exportHistoryOption))) {
            out << "Failed to export history.\n";
            return 1;
        }
        out << "History exported.\n";
        return 0;
    }

    if (parser.isSet(importHistoryOption)) {
        if (!manager.importSearchHistory(parser.value(importHistoryOption), parser.isSet(replaceHistoryOption))) {
            out << manager.lastError() << "\n";
            return 1;
        }
        out << "History imported.\n";
        return 0;
    }

    if (parser.isSet(historyOption)) {
        const auto items = manager.getSearchHistory(20);
        if (items.isEmpty()) {
            out << "No recent searches.\n";
            return 0;
        }

        for (const auto& item : items) {
            if (item.pinned) {
                out << "[Pinned] ";
            }
            out << item.query;
            if (item.success && !item.dictionaryName.isEmpty()) {
                out << " [" << item.dictionaryName << "]";
            } else if (!item.success) {
                out << " [not found]";
            }
            out << "\n";
        }
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        out << "Usage: unidict_cli --dict <path-to-ifo> <word>\n";
        out << "       unidict_cli --dict-dir <directory> <word>\n";
        out << "       unidict_cli --list\n";
        out << "       unidict_cli --history\n";
        out << "       unidict_cli --save-state <file>\n";
        out << "       unidict_cli --load-state <file>\n";
        out << "       unidict_cli --export-history <file>\n";
        out << "       unidict_cli --import-history <file> [--replace-history]\n";
        out << "       unidict_cli --clear-history\n";
        return 1;
    }

    const auto result = UnidictCore::lookupWord(positional.constFirst());
    out << UnidictCore::formatLookupResult(result) << "\n";
    return result.success ? 0 : 2;
}
