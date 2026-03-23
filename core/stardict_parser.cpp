#include "stardict_parser.h"

#include <QDataStream>
#include <QDir>
#include <QFileInfo>

namespace UnidictCore {

StarDictParser::StarDictParser() = default;

bool StarDictParser::loadDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    const QString baseName = fileInfo.completeBaseName();
    const QString dirPath = fileInfo.absolutePath();

    const QString ifoPath = QDir(dirPath).filePath(baseName + ".ifo");
    const QString idxPath = QDir(dirPath).filePath(baseName + ".idx");
    const QString dictPath = QDir(dirPath).filePath(baseName + ".dict");

    if (!QFile::exists(ifoPath) || !QFile::exists(idxPath) || !QFile::exists(dictPath)) {
        return false;
    }

    m_header = {};
    m_wordIndex.clear();
    m_canonicalWords.clear();
    m_wordList.clear();
    m_dictFile.close();
    m_loaded = false;

    if (!loadIfoFile(ifoPath) || !loadIdxFile(idxPath) || !loadDictFile(dictPath)) {
        return false;
    }

    m_sourcePath = ifoPath;
    m_dictionaryId = QFileInfo(ifoPath).canonicalFilePath().toLower();
    m_loaded = true;
    return true;
}

bool StarDictParser::isLoaded() const {
    return m_loaded;
}

QStringList StarDictParser::getSupportedExtensions() const {
    return {"ifo"};
}

DictionaryEntry StarDictParser::lookup(const QString& word) const {
    if (!m_loaded) {
        return {};
    }

    const auto it = m_wordIndex.constFind(word.toLower());
    if (it == m_wordIndex.cend()) {
        return {};
    }

    DictionaryEntry entry;
    entry.word = m_canonicalWords.value(word.toLower(), word);
    entry.definition = extractDefinition(it.value().first, it.value().second);
    entry.metadata.insert("source", m_sourcePath);
    return entry;
}

QStringList StarDictParser::findSimilar(const QString& word, int maxResults) const {
    QStringList results;
    const QString lowerWord = word.toLower();

    for (const QString& dictWord : m_wordList) {
        if (dictWord.toLower().startsWith(lowerWord)) {
            results.append(dictWord);
            if (results.size() >= maxResults) {
                return results;
            }
        }
    }

    for (const QString& dictWord : m_wordList) {
        if (dictWord.toLower().contains(lowerWord) && !results.contains(dictWord)) {
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
    return m_header.bookName.isEmpty() ? QFileInfo(m_sourcePath).completeBaseName() : m_header.bookName;
}

QString StarDictParser::getDictionaryDescription() const {
    return m_header.description;
}

int StarDictParser::getWordCount() const {
    return m_wordList.size();
}

QString StarDictParser::getSourcePath() const {
    return m_sourcePath;
}

QString StarDictParser::getDictionaryId() const {
    return m_dictionaryId;
}

QString StarDictParser::getFormatName() const {
    return "StarDict";
}

bool StarDictParser::loadIfoFile(const QString& ifoPath) {
    QFile file(ifoPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    if (stream.readLine() != "StarDict's dict ifo file") {
        return false;
    }

    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int equalPos = line.indexOf('=');
        if (equalPos <= 0) {
            continue;
        }

        const QString key = line.left(equalPos);
        const QString value = line.mid(equalPos + 1);

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
        QByteArray wordBytes;
        char ch = 0;
        while (stream.readRawData(&ch, 1) == 1 && ch != '\0') {
            wordBytes.append(ch);
        }

        if (wordBytes.isEmpty()) {
            break;
        }

        quint32 offset = 0;
        quint32 size = 0;
        stream >> offset >> size;

        const QString word = QString::fromUtf8(wordBytes);
        m_wordIndex.insert(word.toLower(), qMakePair(static_cast<qint64>(offset), static_cast<qint32>(size)));
        m_canonicalWords.insert(word.toLower(), word);
        m_wordList.append(word);
    }

    return !m_wordList.isEmpty();
}

bool StarDictParser::loadDictFile(const QString& dictPath) {
    m_dictFile.setFileName(dictPath);
    return m_dictFile.open(QIODevice::ReadOnly);
}

QString StarDictParser::extractDefinition(qint64 offset, qint32 size) const {
    if (!m_dictFile.isOpen()) {
        return {};
    }

    QFile& dictFile = const_cast<QFile&>(m_dictFile);
    if (!dictFile.seek(offset)) {
        return {};
    }

    const QByteArray data = dictFile.read(size);
    return QString::fromUtf8(data).trimmed();
}

} // namespace UnidictCore
