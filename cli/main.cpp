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

    // CLI 定位收敛为 man 式纯查词：只管加载词典与查询，生词本/历史/笔记等
    // 学习管理一律走桌面 GUI（Qt Widgets）。词典与索引的诊断入口保留 --list。
    QCommandLineParser parser;
    parser.setApplicationDescription(
        "Unidict command line lookup (management lives in the desktop GUI)");
    parser.addHelpOption();
    parser.addVersionOption();

    // 注意：单元素花括号列表 {"x"} 在 QCommandLineOption(QString) 与
    // (QStringList) 重载间有歧义（CI 上 clang/gcc/msvc 均报错），显式用 QString
    QCommandLineOption dictOption({"d", "dict"}, "Load a dictionary file (.ifo, .mdx, .json or .epub).", "file");
    QCommandLineOption dictDirOption({"D", "dict-dir"}, "Load all supported dictionaries from a directory.", "dir");
    QCommandLineOption listOption({"l", "list"}, "List currently loaded dictionaries.");

    parser.addOption(dictOption);
    parser.addOption(dictDirOption);
    parser.addOption(listOption);
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

    if (parser.isSet(listOption)) {
        const auto infos = manager.getLoadedDictionaryInfos();
        const auto failures = manager.getFailedDictionaries();
        if (infos.isEmpty() && failures.isEmpty()) {
            out << "No dictionaries loaded.\n";
            if (!manager.lastError().isEmpty()) {
                out << manager.lastError() << "\n";
            }
            return 1;
        }

        for (const auto& info : infos) {
            out << info.name << " [" << info.format << "] " << info.filePath << "\n";
        }
        for (const auto& failure : failures) {
            out << "[FAILED] " << failure.filePath << " (" << failure.reason << ")\n";
        }
        return 0;
    }

    const QStringList positional = parser.positionalArguments();
    if (positional.isEmpty()) {
        out << "Usage: unidict_cli --dict <path-to-ifo> <word>\n";
        out << "       unidict_cli --dict-dir <directory> <word>\n";
        out << "       unidict_cli --list\n";
        return 1;
    }

    const auto result = UnidictCore::lookupWord(positional.constFirst());
    out << UnidictCore::formatLookupResult(result) << "\n";
    return result.success ? 0 : 2;
}
