#include "unidict_core.h"
#include "stardict_parser.h"
#include "mdict_parser.h"
#include "json_parser.h"
#include "plugin_manager.h"
#include "index_engine_qt.h"
#include <QDebug>
#include <QFileInfo>

namespace UnidictCore {

DictionaryManager& DictionaryManager::instance() {
    static DictionaryManager instance;
    return instance;
}

bool DictionaryManager::addDictionary(const QString& filePath) {
    // Use plugin manager to create candidate parsers by extension
    PluginManager::instance().ensureBuiltinsRegistered();
    auto candidates = PluginManager::instance().createCandidatesForFile(filePath);
    if (candidates.empty()) {
        QFileInfo fi(filePath);
        qDebug() << "Unsupported dictionary format:" << fi.suffix().toLower();
        return false;
    }

    std::unique_ptr<DictionaryParser> parser;
    bool loaded = false;
    for (auto& cand : candidates) {
        if (cand->loadDictionary(filePath)) {
            parser = std::move(cand);
            loaded = true;
            break;
        }
    }
    if (!loaded) {
        qDebug() << "Failed to load dictionary:" << filePath;
        return false;
    }
    
    qDebug() << "Successfully loaded dictionary:" << parser->getDictionaryName()
             << "with" << parser->getWordCount() << "words";
    
    // Ensure index exists and add words to it
    if (!m_index) {
        m_index = std::make_unique<::UnidictAdaptersQt::IndexEngineQt>();
    }

    // Add words from this dictionary into the global index
    const QString dictId = parser->getDictionaryName();
    for (const QString& w : parser->getAllWords()) {
        m_index->addWord(w, dictId);
    }
    m_index->buildIndex();

    m_parsers.push_back(std::move(parser));
    return true;
}

bool DictionaryManager::removeDictionary(const QString& dictionaryId) {
    // Remove parser and its words from index
    bool removed = false;
    for (auto it = m_parsers.begin(); it != m_parsers.end(); ) {
        const QString name = (*it)->getDictionaryName();
        if (name == dictionaryId) {
            if (m_index) {
                m_index->clearDictionary(dictionaryId);
            }
            it = m_parsers.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    return removed;
}

void DictionaryManager::clearDictionaries() {
    m_parsers.clear();
    m_index.reset();
}

QStringList DictionaryManager::getLoadedDictionaries() const {
    QStringList names;
    for (const auto& parser : m_parsers) {
        names.append(parser->getDictionaryName());
    }
    return names;
}

QList<DictionaryManager::DictMeta> DictionaryManager::getDictionariesMeta() const {
    QList<DictMeta> out;
    for (const auto& parser : m_parsers) {
        DictMeta m;
        m.name = parser->getDictionaryName();
        m.wordCount = parser->getWordCount();
        m.description = parser->getDictionaryDescription();
        out.push_back(m);
    }
    return out;
}

DictionaryEntry DictionaryManager::searchWord(const QString& word) const {
    for (const auto& parser : m_parsers) {
        if (!parser->isLoaded()) continue;
        
        DictionaryEntry entry = parser->lookup(word);
        if (!entry.word.isEmpty()) {
            return entry;
        }
    }
    
    return DictionaryEntry{};
}

QStringList DictionaryManager::searchSimilar(const QString& word, int maxResults) const {
    QStringList results;
    
    for (const auto& parser : m_parsers) {
        if (!parser->isLoaded()) continue;
        
        QStringList similar = parser->findSimilar(word, maxResults - results.size());
        results.append(similar);
        
        if (results.size() >= maxResults) {
            break;
        }
    }
    
    return results;
}

std::vector<DictionaryEntry> DictionaryManager::searchAll(const QString& word) const {
    std::vector<DictionaryEntry> out;
    for (const auto& parser : m_parsers) {
        if (!parser->isLoaded()) continue;
        DictionaryEntry entry = parser->lookup(word);
        if (!entry.word.isEmpty()) {
            if (!entry.metadata.contains("dictionary")) {
                entry.metadata["dictionary"] = parser->getDictionaryName();
            }
            out.push_back(std::move(entry));
        }
    }
    return out;
}

void DictionaryManager::buildIndex() {
    if (!m_index) {
        m_index = std::make_unique<::UnidictAdaptersQt::IndexEngineQt>();
    }
    m_index->buildIndex();
}

QStringList DictionaryManager::prefixSearch(const QString& prefix, int maxResults) const {
    if (!m_index) return {};
    return m_index->prefixSearch(prefix, maxResults);
}

QStringList DictionaryManager::fuzzySearch(const QString& word, int maxResults) const {
    if (!m_index) return {};
    return m_index->fuzzySearch(word, maxResults);
}

QStringList DictionaryManager::wildcardSearch(const QString& pattern, int maxResults) const {
    if (!m_index) return {};
    return m_index->wildcardSearch(pattern, maxResults);
}

QStringList DictionaryManager::getDictionariesForWord(const QString& word) const {
    if (!m_index) return {};
    return m_index->getDictionariesForWord(word);
}

QStringList DictionaryManager::getAllIndexedWords() const {
    if (!m_index) return {};
    return m_index->getAllWords();
}

int DictionaryManager::getIndexedWordCount() const {
    if (!m_index) return 0;
    return m_index->getWordCount();
}

QStringList DictionaryManager::regexSearch(const QString& pattern, int maxResults) const {
    if (!m_index) return {};
    return m_index->regexSearch(pattern, maxResults);
}

bool DictionaryManager::saveIndex(const QString& filePath) const {
    if (!m_index) return false;
    m_index->saveIndex(filePath);
    return true;
}

bool DictionaryManager::loadIndex(const QString& filePath) {
    if (!m_index) {
        m_index = std::make_unique<UnidictAdaptersQt::IndexEngineQt>();
    }
    return m_index->loadIndex(filePath);
}

QString searchWord(const QString& word) {
    DictionaryEntry entry = DictionaryManager::instance().searchWord(word);
    if (!entry.word.isEmpty()) {
        return entry.definition;
    }
    
    return "Word not found: " + word;
}

}
