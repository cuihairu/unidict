#include "stardict_parser.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDataStream>
#include <zlib.h>
#include <QCryptographicHash>
#include "path_utils.h"
#include <cstdlib>

namespace UnidictCore {

StarDictParser::StarDictParser() = default;

StarDictParser::~StarDictParser() {
    if (m_mappedPtr && m_mappedSize > 0) {
        m_dictFile.unmap(m_mappedPtr);
        m_mappedPtr = nullptr;
        m_mappedSize = 0;
    }
}

bool StarDictParser::loadDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();
    QString baseName = fileInfo.completeBaseName();
    // Handle names like foo.dict.dz to derive baseName = foo
    if (fileName.endsWith(".dict.dz", Qt::CaseInsensitive)) {
        baseName = fileName.left(fileName.size() - QString(".dict.dz").size());
    } else if (fileName.endsWith(".dict", Qt::CaseInsensitive)) {
        baseName = fileName.left(fileName.size() - QString(".dict").size());
    }
    QString dirPath = fileInfo.absolutePath();
    
    QString ifoPath = QDir(dirPath).filePath(baseName + ".ifo");
    QString idxPath = QDir(dirPath).filePath(baseName + ".idx");
    QString dictPath = QDir(dirPath).filePath(baseName + ".dict");
    QString dictDzPath = QDir(dirPath).filePath(baseName + ".dict.dz");
    
    if (!QFile::exists(ifoPath) || !QFile::exists(idxPath)) {
        qDebug() << "StarDict files not found (ifo/idx):" << baseName;
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
    
    // prefer uncompressed .dict; fallback to .dict.dz
    QString chosenDict = dictPath;
    if (!QFile::exists(chosenDict) && QFile::exists(dictDzPath)) {
        chosenDict = dictDzPath;
    }

    if (!loadDictFile(chosenDict)) {
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
        } else if (key == "idxoffsetbits") {
            m_header.idxOffsetBits = value.toInt();
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
        quint32 size;
        qint64 offset = 0;
        if (m_header.idxOffsetBits == 64) {
            quint64 off64 = 0;
            stream >> off64 >> size;
            offset = static_cast<qint64>(off64);
        } else {
            quint32 off32 = 0;
            stream >> off32 >> size;
            offset = static_cast<qint64>(off32);
        }
        
        m_wordIndex[word.toLower()] = qMakePair(offset, static_cast<qint32>(size));
        m_wordList.append(word);
    }
    
    return true;
}

bool StarDictParser::loadDictFile(const QString& dictPath) {
    if (dictPath.endsWith(".dz", Qt::CaseInsensitive)) {
        // Prefer centralized disk cache to reduce memory usage for large dicts
        QFileInfo fi(dictPath);
        const QByteArray key = QCryptographicHash::hash(fi.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex();
        const QString cacheDirPath = PathUtils::cacheDir();
        PathUtils::ensureDir(cacheDirPath);
        m_cachePath = QDir(cacheDirPath).filePath(QString::fromLatin1(key) + "_" + fi.completeBaseName() + ".dict");
        if (QFile::exists(m_cachePath) || decompressGzipToFile(dictPath, m_cachePath)) {
            m_dictFile.setFileName(m_cachePath);
            return m_dictFile.open(QIODevice::ReadOnly);
        }
        // fallback: in-memory
        m_dictBuffer = decompressGzipFile(dictPath);
        if (m_dictBuffer.isEmpty()) {
            qDebug() << "Failed to decompress dict.dz:" << dictPath;
            return false;
        }
        return true;
    }
    m_dictFile.setFileName(dictPath);
    if (!m_dictFile.open(QIODevice::ReadOnly)) return false;
    // Try memory-map for faster random access
    m_mappedPtr = m_dictFile.map(0, m_dictFile.size());
    if (m_mappedPtr) {
        m_mappedSize = m_dictFile.size();
    }
    return true;
}

QString StarDictParser::extractDefinition(qint64 offset, qint32 size) const {
    if (!m_dictBuffer.isEmpty()) {
        if (offset < 0 || size <= 0 || offset + size > m_dictBuffer.size()) {
            return QString();
        }
        return QString::fromUtf8(m_dictBuffer.mid(static_cast<int>(offset), size));
    }

    if (!m_dictBuffer.isEmpty()) {
        if (offset < 0 || size <= 0 || offset + size > m_dictBuffer.size()) {
            return QString();
        }
        return QString::fromUtf8(m_dictBuffer.mid(static_cast<int>(offset), size));
    }

    if (m_mappedPtr && m_mappedSize > 0) {
        if (offset < 0 || size <= 0 || offset + size > m_mappedSize) return QString();
        const char* p = reinterpret_cast<const char*>(m_mappedPtr + offset);
        return QString::fromUtf8(p, size);
    }

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

QByteArray StarDictParser::decompressGzipFile(const QString& gzPath) const {
    QByteArray out;
    gzFile gzf = gzopen(QFile::encodeName(gzPath).constData(), "rb");
    if (!gzf) return out;
    const int chunk = 1 << 15; // 32KB
    char* buf = static_cast<char*>(malloc(chunk));
    if (!buf) { gzclose(gzf); return out; }
    int n = 0;
    while ((n = gzread(gzf, buf, chunk)) > 0) {
        out.append(buf, n);
    }
    free(buf);
    gzclose(gzf);
    return out;
}

bool StarDictParser::decompressGzipToFile(const QString& gzPath, const QString& outPath) const {
    gzFile gzf = gzopen(QFile::encodeName(gzPath).constData(), "rb");
    if (!gzf) return false;
    QFile out(outPath);
    if (!out.open(QIODevice::WriteOnly)) { gzclose(gzf); return false; }
    const int chunk = 1 << 15; // 32KB
    char* buf = static_cast<char*>(malloc(chunk));
    if (!buf) { gzclose(gzf); return false; }
    int n = 0;
    while ((n = gzread(gzf, buf, chunk)) > 0) {
        out.write(buf, n);
    }
    free(buf);
    gzclose(gzf);
    out.close();
    return true;
}

}
