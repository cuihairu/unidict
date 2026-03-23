#include "unidict_core.h"
#include "mdict_parser.h"
#include "stardict_parser.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSet>
#include <algorithm>

namespace UnidictCore {

namespace {

QString normalizeDictionaryId(const QString& filePath) {
    return QFileInfo(filePath).canonicalFilePath().toLower();
}

bool isSupportedDictionaryFile(const QFileInfo& fileInfo) {
    const QString extension = fileInfo.suffix().toLower();
    return extension == "ifo" || extension == "mdx";
}

QString defaultStateFilePathValue() {
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QDir::homePath() + QDir::separator() + ".unidict";
    }
    return QDir(baseDir).filePath("dictionary_state.json");
}

} // namespace

DictionaryManager& DictionaryManager::instance() {
    static DictionaryManager instance;
    return instance;
}

bool DictionaryManager::addDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        m_lastError = QString("Dictionary file does not exist: %1").arg(filePath);
        return false;
    }

    const QString dictionaryId = normalizeDictionaryId(filePath);
    for (const auto& record : m_parsers) {
        if (record.parser->getDictionaryId() == dictionaryId) {
            m_lastError = QString("Dictionary already loaded: %1").arg(fileInfo.fileName());
            return false;
        }
    }

    const QString extension = fileInfo.suffix().toLower();
    std::unique_ptr<DictionaryParser> parser;

    if (extension == "ifo") {
        parser = std::make_unique<StarDictParser>();
    } else if (extension == "mdx") {
        parser = std::make_unique<MdictParser>();
    } else {
        m_lastError = QString("Unsupported dictionary format: %1").arg(extension);
        return false;
    }

    if (!parser->loadDictionary(filePath)) {
        m_lastError = QString("Failed to load dictionary: %1").arg(filePath);
        return false;
    }

    m_lastError.clear();
    m_parsers.push_back(DictionaryRecord{std::move(parser), true, {}});
    saveState();
    return true;
}

int DictionaryManager::addDictionariesFromDirectory(const QString& directoryPath) {
    QDir dir(directoryPath);
    if (!dir.exists()) {
        m_lastError = QString("Dictionary directory does not exist: %1").arg(directoryPath);
        return 0;
    }

    int loaded = 0;
    QSet<QString> seenDictionaryIds;

    QDirIterator it(directoryPath, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo fileInfo(it.next());
        if (!isSupportedDictionaryFile(fileInfo)) {
            continue;
        }

        const QString dictionaryId = normalizeDictionaryId(fileInfo.absoluteFilePath());
        if (seenDictionaryIds.contains(dictionaryId)) {
            continue;
        }

        if (addDictionary(fileInfo.absoluteFilePath())) {
            seenDictionaryIds.insert(dictionaryId);
            ++loaded;
        }
    }

    if (loaded == 0 && m_lastError.isEmpty()) {
        m_lastError = QString("No supported dictionaries found in: %1").arg(directoryPath);
    }

    return loaded;
}

bool DictionaryManager::removeDictionary(const QString& dictionaryId) {
    const auto it = std::remove_if(
        m_parsers.begin(),
        m_parsers.end(),
        [&](const DictionaryRecord& record) {
            return record.parser->getDictionaryId() == dictionaryId;
        });

    if (it == m_parsers.end()) {
        m_lastError = QString("Dictionary not found: %1").arg(dictionaryId);
        return false;
    }

    m_parsers.erase(it, m_parsers.end());
    m_lastError.clear();
    saveState();
    return true;
}

bool DictionaryManager::setDictionaryEnabled(const QString& dictionaryId, bool enabled) {
    for (auto& record : m_parsers) {
        if (record.parser->getDictionaryId() == dictionaryId) {
            record.enabled = enabled;
            m_lastError.clear();
            saveState();
            return true;
        }
    }

    m_lastError = QString("Dictionary not found: %1").arg(dictionaryId);
    return false;
}

bool DictionaryManager::setDictionaryTags(const QString& dictionaryId, const QStringList& tags) {
    QStringList normalizedTags;
    for (const QString& tag : tags) {
        const QString trimmed = tag.trimmed();
        if (!trimmed.isEmpty() && !normalizedTags.contains(trimmed, Qt::CaseInsensitive)) {
            normalizedTags.append(trimmed);
        }
    }

    for (auto& record : m_parsers) {
        if (record.parser->getDictionaryId() == dictionaryId) {
            record.tags = normalizedTags;
            m_lastError.clear();
            saveState();
            return true;
        }
    }

    m_lastError = QString("Dictionary not found: %1").arg(dictionaryId);
    return false;
}

bool DictionaryManager::moveDictionaryUp(const QString& dictionaryId) {
    for (std::size_t i = 1; i < m_parsers.size(); ++i) {
        if (m_parsers[i].parser->getDictionaryId() == dictionaryId) {
            std::swap(m_parsers[i - 1], m_parsers[i]);
            m_lastError.clear();
            saveState();
            return true;
        }
    }

    m_lastError = QString("Dictionary cannot be moved up: %1").arg(dictionaryId);
    return false;
}

bool DictionaryManager::moveDictionaryDown(const QString& dictionaryId) {
    if (m_parsers.size() < 2) {
        m_lastError = QString("Dictionary cannot be moved down: %1").arg(dictionaryId);
        return false;
    }

    for (std::size_t i = 0; i + 1 < m_parsers.size(); ++i) {
        if (m_parsers[i].parser->getDictionaryId() == dictionaryId) {
            std::swap(m_parsers[i], m_parsers[i + 1]);
            m_lastError.clear();
            saveState();
            return true;
        }
    }

    m_lastError = QString("Dictionary cannot be moved down: %1").arg(dictionaryId);
    return false;
}

bool DictionaryManager::loadState(const QString& stateFilePath) {
    const QString resolvedPath = resolveStateFilePath(stateFilePath);
    QFile file(resolvedPath);
    if (!file.exists()) {
        m_lastError = QString("State file does not exist: %1").arg(resolvedPath);
        return false;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Failed to open state file: %1").arg(resolvedPath);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        m_lastError = QString("Invalid state file: %1").arg(resolvedPath);
        return false;
    }

    return loadFromJson(document.object());
}

bool DictionaryManager::saveState(const QString& stateFilePath) const {
    const QString resolvedPath = resolveStateFilePath(stateFilePath);
    QFileInfo fileInfo(resolvedPath);
    QDir().mkpath(fileInfo.absolutePath());

    QFile file(resolvedPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    return true;
}

void DictionaryManager::clear() {
    m_parsers.clear();
    m_history.clear();
    m_lastError.clear();
    saveState();
}

bool DictionaryManager::hasDictionaries() const {
    return !m_parsers.empty();
}

QStringList DictionaryManager::getLoadedDictionaries() const {
    QStringList names;
    for (const auto& record : m_parsers) {
        names.append(record.parser->getDictionaryName());
    }
    return names;
}

QVector<DictionaryInfo> DictionaryManager::getLoadedDictionaryInfos() const {
    QVector<DictionaryInfo> infos;
    infos.reserve(static_cast<qsizetype>(m_parsers.size()));

    for (int i = 0; i < static_cast<int>(m_parsers.size()); ++i) {
        const auto& record = m_parsers[static_cast<std::size_t>(i)];
        infos.append(DictionaryInfo{
            record.parser->getDictionaryId(),
            record.parser->getDictionaryName(),
            record.parser->getDictionaryDescription(),
            record.parser->getSourcePath(),
            record.parser->getFormatName(),
            record.tags,
            record.parser->getWordCount(),
            record.enabled,
            i + 1
        });
    }

    return infos;
}

QVector<SearchHistoryItem> DictionaryManager::getSearchHistory(int maxItems) const {
    if (maxItems <= 0 || m_history.size() <= maxItems) {
        return m_history;
    }
    return m_history.mid(0, maxItems);
}

void DictionaryManager::clearSearchHistory() {
    m_history.clear();
    saveState();
}

bool DictionaryManager::exportSearchHistory(const QString& filePath) const {
    QFile file(filePath);
    QFileInfo fileInfo(filePath);
    QDir().mkpath(fileInfo.absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonArray history;
    for (const auto& item : m_history) {
        QJsonObject historyItem;
        historyItem.insert("query", item.query);
        historyItem.insert("success", item.success);
        historyItem.insert("dictionary_name", item.dictionaryName);
        historyItem.insert("pinned", item.pinned);
        history.append(historyItem);
    }

    QJsonObject root;
    root.insert("version", 1);
    root.insert("history", history);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool DictionaryManager::importSearchHistory(const QString& filePath, bool replaceExisting) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QString("Failed to open history file: %1").arg(filePath);
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject() || !document.object().value("history").isArray()) {
        m_lastError = QString("Invalid history file: %1").arg(filePath);
        return false;
    }

    if (replaceExisting) {
        m_history.clear();
    }

    const QJsonArray history = document.object().value("history").toArray();
    for (const auto& value : history) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject historyObject = value.toObject();
        const QString query = historyObject.value("query").toString().trimmed();
        if (query.isEmpty()) {
            continue;
        }

        SearchHistoryItem item{
            query,
            historyObject.value("success").toBool(false),
            historyObject.value("dictionary_name").toString(),
            historyObject.value("pinned").toBool(false)
        };

        for (int i = 0; i < m_history.size(); ++i) {
            if (m_history[i].query.compare(query, Qt::CaseInsensitive) == 0) {
                m_history.removeAt(i);
                break;
            }
        }

        if (item.pinned) {
            int insertAt = 0;
            while (insertAt < m_history.size() && m_history[insertAt].pinned) {
                ++insertAt;
            }
            m_history.insert(insertAt, item);
        } else {
            m_history.append(item);
        }
    }

    while (m_history.size() > 100) {
        m_history.removeLast();
    }

    saveState();
    m_lastError.clear();
    return true;
}

bool DictionaryManager::removeSearchHistoryItem(const QString& query) {
    const QString normalizedQuery = query.trimmed();
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].query.compare(normalizedQuery, Qt::CaseInsensitive) == 0) {
            m_history.removeAt(i);
            saveState();
            m_lastError.clear();
            return true;
        }
    }

    m_lastError = QString("History item not found: %1").arg(normalizedQuery);
    return false;
}

bool DictionaryManager::setSearchHistoryPinned(const QString& query, bool pinned) {
    const QString normalizedQuery = query.trimmed();
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].query.compare(normalizedQuery, Qt::CaseInsensitive) == 0) {
            m_history[i].pinned = pinned;
            const SearchHistoryItem item = m_history.takeAt(i);
            if (pinned) {
                int insertAt = 0;
                while (insertAt < m_history.size() && m_history[insertAt].pinned) {
                    ++insertAt;
                }
                m_history.insert(insertAt, item);
            } else {
                int insertAt = 0;
                while (insertAt < m_history.size() && m_history[insertAt].pinned) {
                    ++insertAt;
                }
                m_history.insert(insertAt, item);
            }
            saveState();
            m_lastError.clear();
            return true;
        }
    }

    m_lastError = QString("History item not found: %1").arg(normalizedQuery);
    return false;
}

LookupResult DictionaryManager::searchWord(const QString& word) const {
    LookupResult result;
    result.query = word.trimmed();

    if (result.query.isEmpty()) {
        result.message = "Enter a word to search.";
        return result;
    }

    const bool hasEnabledDictionaries = std::any_of(
        m_parsers.begin(),
        m_parsers.end(),
        [](const DictionaryRecord& record) { return record.enabled; });

    if (!hasDictionaries()) {
        result.message = "No dictionaries loaded. Import a StarDict dictionary first.";
        const_cast<DictionaryManager*>(this)->recordSearch(result);
        return result;
    }

    if (!hasEnabledDictionaries) {
        result.message = "All dictionaries are disabled. Enable at least one dictionary.";
        const_cast<DictionaryManager*>(this)->recordSearch(result);
        return result;
    }

    for (const auto& record : m_parsers) {
        if (!record.enabled || !record.parser->isLoaded()) {
            continue;
        }

        DictionaryEntry entry = record.parser->lookup(result.query);
        if (!entry.word.isEmpty()) {
            result.matches.append(DictionaryMatch{
                entry,
                record.parser->getDictionaryId(),
                record.parser->getDictionaryName()
            });
        }
    }

    if (!result.matches.isEmpty()) {
        result.success = true;
        result.entry = result.matches.constFirst().entry;
        result.dictionaryId = result.matches.constFirst().dictionaryId;
        result.dictionaryName = result.matches.constFirst().dictionaryName;
        result.message = result.matches.size() == 1
                             ? QString("Found in %1").arg(result.dictionaryName)
                             : QString("Found in %1 dictionaries").arg(result.matches.size());
        const_cast<DictionaryManager*>(this)->recordSearch(result);
        return result;
    }

    result.suggestions = searchSimilar(result.query, 12);
    if (result.suggestions.isEmpty()) {
        result.message = QString("No result for \"%1\".").arg(result.query);
    } else {
        result.message = QString("No exact result for \"%1\".").arg(result.query);
    }
    const_cast<DictionaryManager*>(this)->recordSearch(result);
    return result;
}

QStringList DictionaryManager::searchSimilar(const QString& word, int maxResults) const {
    QStringList results;
    QSet<QString> seen;

    for (const auto& record : m_parsers) {
        if (!record.enabled || !record.parser->isLoaded() || results.size() >= maxResults) {
            continue;
        }

        const QStringList similar = record.parser->findSimilar(word, maxResults - results.size());
        for (const QString& item : similar) {
            if (seen.contains(item.toLower())) {
                continue;
            }

            results.append(item);
            seen.insert(item.toLower());
            if (results.size() >= maxResults) {
                break;
            }
        }
    }

    return results;
}

QString DictionaryManager::lastError() const {
    return m_lastError;
}

QString DictionaryManager::resolveStateFilePath(const QString& stateFilePath) const {
    return stateFilePath.isEmpty() ? defaultStateFilePathValue() : stateFilePath;
}

QString DictionaryManager::defaultStateFilePath() const {
    return defaultStateFilePathValue();
}

QJsonObject DictionaryManager::toJson() const {
    QJsonArray dictionaries;
    for (const auto& record : m_parsers) {
        QJsonObject dictionary;
        dictionary.insert("file_path", record.parser->getSourcePath());
        dictionary.insert("enabled", record.enabled);
        QJsonArray tags;
        for (const QString& tag : record.tags) {
            tags.append(tag);
        }
        dictionary.insert("tags", tags);
        dictionaries.append(dictionary);
    }

    QJsonArray history;
    for (const auto& item : m_history) {
        QJsonObject historyItem;
        historyItem.insert("query", item.query);
        historyItem.insert("success", item.success);
        historyItem.insert("dictionary_name", item.dictionaryName);
        historyItem.insert("pinned", item.pinned);
        history.append(historyItem);
    }

    QJsonObject root;
    root.insert("version", 1);
    root.insert("dictionaries", dictionaries);
    root.insert("history", history);
    return root;
}

bool DictionaryManager::loadFromJson(const QJsonObject& object) {
    if (!object.value("dictionaries").isArray()) {
        m_lastError = "State file is missing dictionary list.";
        return false;
    }

    const QJsonArray dictionaries = object.value("dictionaries").toArray();
    std::vector<DictionaryRecord> loaded;
    loaded.reserve(static_cast<std::size_t>(dictionaries.size()));

    for (const auto& value : dictionaries) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject dictionary = value.toObject();
        const QString filePath = dictionary.value("file_path").toString();
        if (filePath.isEmpty()) {
            continue;
        }

        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            continue;
        }

        std::unique_ptr<DictionaryParser> parser;
        const QString extension = fileInfo.suffix().toLower();
        if (extension == "ifo") {
            parser = std::make_unique<StarDictParser>();
        } else if (extension == "mdx") {
            parser = std::make_unique<MdictParser>();
        } else {
            continue;
        }

        if (!parser->loadDictionary(filePath)) {
            continue;
        }

        QStringList tags;
        const QJsonArray tagsArray = dictionary.value("tags").toArray();
        for (const auto& tagValue : tagsArray) {
            const QString tag = tagValue.toString().trimmed();
            if (!tag.isEmpty() && !tags.contains(tag, Qt::CaseInsensitive)) {
                tags.append(tag);
            }
        }

        loaded.push_back(DictionaryRecord{
            std::move(parser),
            dictionary.value("enabled").toBool(true),
            tags
        });
    }

    m_parsers = std::move(loaded);
    m_history.clear();
    const QJsonArray history = object.value("history").toArray();
    for (const auto& value : history) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject historyObject = value.toObject();
        const QString query = historyObject.value("query").toString().trimmed();
        if (query.isEmpty()) {
            continue;
        }
        m_history.append(SearchHistoryItem{
            query,
            historyObject.value("success").toBool(false),
            historyObject.value("dictionary_name").toString(),
            historyObject.value("pinned").toBool(false)
        });
    }
    m_lastError.clear();
    return true;
}

void DictionaryManager::recordSearch(const LookupResult& result) {
    const QString query = result.query.trimmed();
    if (query.isEmpty()) {
        return;
    }

    SearchHistoryItem item{
        query,
        result.success,
        result.dictionaryName,
        false
    };

    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].query.compare(query, Qt::CaseInsensitive) == 0) {
            item.pinned = m_history[i].pinned;
            m_history.removeAt(i);
            break;
        }
    }

    if (item.pinned) {
        int insertAt = 0;
        while (insertAt < m_history.size() && m_history[insertAt].pinned) {
            ++insertAt;
        }
        m_history.insert(insertAt, item);
    } else {
        int insertAt = 0;
        while (insertAt < m_history.size() && m_history[insertAt].pinned) {
            ++insertAt;
        }
        m_history.insert(insertAt, item);
    }
    while (m_history.size() > 100) {
        m_history.removeLast();
    }

    saveState();
}

QString searchWord(const QString& word) {
    return formatLookupResult(DictionaryManager::instance().searchWord(word));
}

LookupResult lookupWord(const QString& word) {
    return DictionaryManager::instance().searchWord(word);
}

QString formatLookupResult(const LookupResult& result) {
    if (result.success) {
        QStringList sections;
        for (const auto& match : result.matches) {
            QString section = match.entry.definition.trimmed();
            if (!match.dictionaryName.isEmpty()) {
                section.prepend(QString("[%1]\n\n").arg(match.dictionaryName));
            }
            sections.append(section);
        }
        return sections.join("\n\n--------------------\n\n");
    }

    QString text = result.message;
    if (!result.suggestions.isEmpty()) {
        text += "\n\nDid you mean:\n- " + result.suggestions.join("\n- ");
    }
    return text;
}

} // namespace UnidictCore
