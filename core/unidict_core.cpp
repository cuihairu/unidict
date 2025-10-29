#include "unidict_core.h"
#include "stardict_parser.h"
#include "mdict_parser.h"
#include "index_engine.h"
#include <QDebug>
#include <QFileInfo>

namespace UnidictCore {

DictionaryManager& DictionaryManager::instance() {
    static DictionaryManager instance;
    return instance;
}

bool DictionaryManager::addDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    std::unique_ptr<DictionaryParser> parser;
    
    if (extension == "ifo" || extension == "idx" || extension == "dict") {
        parser = std::make_unique<StarDictParser>();
    } else if (extension == "mdx") {
        parser = std::make_unique<MdictParser>();
    } else {
        qDebug() << "Unsupported dictionary format:" << extension;
        return false;
    }
    
    if (!parser->loadDictionary(filePath)) {
        qDebug() << "Failed to load dictionary:" << filePath;
        return false;
    }
    
    qDebug() << "Successfully loaded dictionary:" << parser->getDictionaryName()
             << "with" << parser->getWordCount() << "words";
    
    m_parsers.push_back(std::move(parser));
    return true;
}

bool DictionaryManager::removeDictionary(const QString& dictionaryId) {
    Q_UNUSED(dictionaryId)
    return false;
}

QStringList DictionaryManager::getLoadedDictionaries() const {
    QStringList names;
    for (const auto& parser : m_parsers) {
        names.append(parser->getDictionaryName());
    }
    return names;
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

QString searchWord(const QString& word) {
    DictionaryEntry entry = DictionaryManager::instance().searchWord(word);
    if (!entry.word.isEmpty()) {
        return entry.definition;
    }
    
    return "Word not found: " + word;
}

}
