#include "stardict_parser.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDataStream>

namespace UnidictCore {

StarDictParser::StarDictParser() = default;

bool StarDictParser::loadDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.completeBaseName();
    QString dirPath = fileInfo.absolutePath();
    
    QString ifoPath = QDir(dirPath).filePath(baseName + ".ifo");
    QString idxPath = QDir(dirPath).filePath(baseName + ".idx");
    QString dictPath = QDir(dirPath).filePath(baseName + ".dict");
    
    if (!QFile::exists(ifoPath) || !QFile::exists(idxPath) || !QFile::exists(dictPath)) {
        qDebug() << "StarDict files not found:" << baseName;
        return false;
    }
    
    if (!loadIfoFile(ifoPath)) {
        qDebug() << "Failed to load .ifo file";
        return false;
    }
    
    if (!loadIdxFile(idxPath)) {
        qDebug() << "Failed to load .idx file";
        return false;
    }
    
    if (!loadDictFile(dictPath)) {
        qDebug() << "Failed to load .dict file";
        return false;
    }
    
    m_loaded = true;
    return true;
}

bool StarDictParser::isLoaded() const {
    return m_loaded;
}

QStringList StarDictParser::getSupportedExtensions() const {
    return {"ifo", "idx", "dict"};
}

DictionaryEntry StarDictParser::lookup(const QString& word) const {
    if (!m_loaded) {
        return {};
    }
    
    auto it = m_wordIndex.find(word.toLower());
    if (it == m_wordIndex.end()) {
        return {};
    }
    
    QString definition = extractDefinition(it->first, it->second);
    
    DictionaryEntry entry;
    entry.word = word;
    entry.definition = definition;
    return entry;
}

QStringList StarDictParser::findSimilar(const QString& word, int maxResults) const {
    QStringList results;
    QString lowerWord = word.toLower();
    
    for (const QString& dictWord : m_wordList) {
        if (dictWord.toLower().startsWith(lowerWord)) {
            results.append(dictWord);
            if (results.size() >= maxResults) {
                break;
            }
        }
    }
    
    return results;
}

QStringList StarDictParser::getAllWords() const {
    return m_wordList;
}

QString StarDictParser::getDictionaryName() const {
    return m_header.bookName;
}

QString StarDictParser::getDictionaryDescription() const {
    return m_header.description;
}

int StarDictParser::getWordCount() const {
    return m_header.wordCount;
}

bool StarDictParser::loadIfoFile(const QString& ifoPath) {
    QFile file(ifoPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    QString line;
    
    // Read magic string
    line = stream.readLine();
    if (line != "StarDict's dict ifo file") {
        return false;
    }
    
    // Parse header fields
    while (!stream.atEnd()) {
        line = stream.readLine();
        if (line.isEmpty()) continue;
        
        int equalPos = line.indexOf('=');
        if (equalPos == -1) continue;
        
        QString key = line.left(equalPos);
        QString value = line.mid(equalPos + 1);
        
        if (key == "version") {
            m_header.version = value;
        } else if (key == "bookname") {
            m_header.bookName = value;
        } else if (key == "wordcount") {
            m_header.wordCount = value.toInt();
        } else if (key == "idxfilesize") {
            m_header.indexFileSize = value.toInt();
        } else if (key == "author") {
            m_header.author = value;
        } else if (key == "email") {
            m_header.email = value;
        } else if (key == "website") {
            m_header.website = value;
        } else if (key == "description") {
            m_header.description = value;
        } else if (key == "date") {
            m_header.date = value;
        }
    }
    
    return true;
}

bool StarDictParser::loadIdxFile(const QString& idxPath) {
    QFile file(idxPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::BigEndian);
    
    while (!stream.atEnd()) {
        // Read word (null-terminated string)
        QByteArray wordBytes;
        char ch;
        while (stream.readRawData(&ch, 1) == 1 && ch != '\0') {
            wordBytes.append(ch);
        }
        
        if (wordBytes.isEmpty()) break;
        
        QString word = QString::fromUtf8(wordBytes);
        
        // Read offset and size
        quint32 offset, size;
        stream >> offset >> size;
        
        m_wordIndex[word.toLower()] = qMakePair(static_cast<qint64>(offset), static_cast<qint32>(size));
        m_wordList.append(word);
    }
    
    return true;
}

bool StarDictParser::loadDictFile(const QString& dictPath) {
    m_dictFile.setFileName(dictPath);
    return m_dictFile.open(QIODevice::ReadOnly);
}

QString StarDictParser::extractDefinition(qint64 offset, qint32 size) const {
    if (!m_dictFile.isOpen()) {
        return QString();
    }
    
    QFile& dictFile = const_cast<QFile&>(m_dictFile);
    if (!dictFile.seek(offset)) {
        return QString();
    }
    
    QByteArray data = dictFile.read(size);
    return QString::fromUtf8(data);
}

}