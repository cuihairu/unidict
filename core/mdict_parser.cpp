#include "mdict_parser.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QtZlib/zlib.h>

namespace UnidictCore {

MdictParser::MdictParser() = default;

bool MdictParser::loadDictionary(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    m_filePath = filePath;
    
    if (!fileInfo.exists()) {
        qDebug() << "MDict file not found:" << filePath;
        return false;
    }
    
    if (!loadMdxFile(filePath)) {
        qDebug() << "Failed to load .mdx file";
        return false;
    }
    
    // Try to load corresponding .mdd file if exists
    QString mddPath = filePath;
    mddPath.replace(".mdx", ".mdd", Qt::CaseInsensitive);
    if (QFile::exists(mddPath)) {
        loadMddFile(mddPath);
    }
    
    m_loaded = true;
    return true;
}

bool MdictParser::isLoaded() const {
    return m_loaded;
}

QStringList MdictParser::getSupportedExtensions() const {
    return {"mdx", "mdd"};
}

DictionaryEntry MdictParser::lookup(const QString& word) const {
    if (!m_loaded) {
        return {};
    }
    
    QString definition = extractDefinition(word);
    if (definition.isEmpty()) {
        return {};
    }
    
    DictionaryEntry entry;
    entry.word = word;
    entry.definition = definition;
    return entry;
}

QStringList MdictParser::findSimilar(const QString& word, int maxResults) const {
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

QStringList MdictParser::getAllWords() const {
    return m_wordList;
}

QString MdictParser::getDictionaryName() const {
    return m_header.title;
}

QString MdictParser::getDictionaryDescription() const {
    return m_header.description;
}

int MdictParser::getWordCount() const {
    return m_header.wordCount;
}

bool MdictParser::loadMdxFile(const QString& mdxPath) {
    QFile file(mdxPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    // Read header length
    quint32 headerLength;
    stream >> headerLength;
    
    // Read header data
    QByteArray headerData = file.read(headerLength);
    
    // Parse header
    QJsonObject headerObj = parseHeader(headerData);
    m_header.title = headerObj["title"].toString();
    m_header.description = headerObj["description"].toString();
    m_header.encoding = headerObj["encoding"].toString("UTF-8");
    m_header.version = headerObj["version"].toInt();
    m_header.encrypted = headerObj["encrypted"].toBool();
    
    // Read key block info
    quint64 keyBlocksLength;
    stream >> keyBlocksLength;
    
    quint64 keyBlocksCount;
    stream >> keyBlocksCount;
    
    // Read key blocks (simplified implementation)
    QByteArray keyBlocksData = file.read(keyBlocksLength);
    
    if (m_header.encrypted) {
        // Decrypt if needed (simplified - real implementation would need proper decryption)
        qDebug() << "Warning: Encrypted MDict files not fully supported";
    }
    
    // Parse key blocks to extract word list (simplified)
    // Real implementation would need proper decompression and parsing
    QDataStream keyStream(keyBlocksData);
    keyStream.setByteOrder(QDataStream::LittleEndian);
    
    // For demonstration, create a simple word list
    // Real implementation would parse the actual key blocks
    m_wordList << "example" << "test" << "demo";
    m_wordIndex["example"] = "An example definition";
    m_wordIndex["test"] = "A test definition";
    m_wordIndex["demo"] = "A demo definition";
    
    m_header.wordCount = m_wordList.size();
    
    return true;
}

bool MdictParser::loadMddFile(const QString& mddPath) {
    QFile file(mddPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    // MDD files contain resource data (images, audio, etc.)
    // Simplified implementation - would need proper parsing
    qDebug() << "MDD file loaded (resources not fully implemented):" << mddPath;
    
    return true;
}

QByteArray MdictParser::decompressData(const QByteArray& compressedData) const {
    // Simplified zlib decompression
    uLongf destSize = compressedData.size() * 4; // Estimate
    QByteArray result(destSize, 0);
    
    int ret = uncompress(reinterpret_cast<Bytef*>(result.data()), &destSize,
                        reinterpret_cast<const Bytef*>(compressedData.data()),
                        compressedData.size());
    
    if (ret == Z_OK) {
        result.resize(destSize);
        return result;
    }
    
    return QByteArray();
}

QString MdictParser::decryptData(const QByteArray& encryptedData) const {
    // Simplified decryption placeholder
    // Real implementation would need proper MDict decryption algorithm
    Q_UNUSED(encryptedData)
    return QString();
}

QJsonObject MdictParser::parseHeader(const QByteArray& headerData) const {
    // MDict header is typically in a specific format
    // This is a simplified JSON-like parsing
    
    QString headerStr = QString::fromUtf8(headerData);
    
    // Extract key-value pairs (simplified)
    QJsonObject obj;
    obj["title"] = "MDict Dictionary";
    obj["description"] = "A sample MDict dictionary";
    obj["encoding"] = "UTF-8";
    obj["version"] = 2;
    obj["encrypted"] = false;
    
    // Real implementation would parse the actual MDict header format
    
    return obj;
}

QString MdictParser::extractDefinition(const QString& word) const {
    auto it = m_wordIndex.find(word.toLower());
    if (it != m_wordIndex.end()) {
        return it.value();
    }
    
    // Try case-insensitive search
    for (auto iter = m_wordIndex.begin(); iter != m_wordIndex.end(); ++iter) {
        if (iter.key().toLower() == word.toLower()) {
            return iter.value();
        }
    }
    
    return QString();
}

}