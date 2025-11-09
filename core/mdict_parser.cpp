#include "mdict_parser.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDataStream>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>
#include <zlib.h>

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

    // Header length: MDX uses big-endian 32-bit for header length in most versions
    quint32 headerLengthBE = 0;
    {
        QDataStream s(&file);
        s.setByteOrder(QDataStream::BigEndian);
        s >> headerLengthBE;
    }
    if (headerLengthBE == 0 || headerLengthBE > 10 * 1024 * 1024) {
        qDebug() << "Suspicious MDX header length:" << headerLengthBE;
        return false;
    }

    QByteArray headerData = file.read(headerLengthBE);

    // Try to decode header as UTF-16LE first (common), fallback to UTF-8
    QString headerText = QString::fromUtf16(reinterpret_cast<const char16_t*>(headerData.constData()), headerData.size() / 2);
    if (!headerText.contains('<')) {
        headerText = QString::fromUtf8(headerData);
    }

    // Parse header XML-like attributes
    MdxHeader parsed = parseHeaderText(headerText);
    if (!parsed.title.isEmpty()) m_header.title = parsed.title;
    if (!parsed.description.isEmpty()) m_header.description = parsed.description;
    if (!parsed.encoding.isEmpty()) m_header.encoding = parsed.encoding; else m_header.encoding = "UTF-8";
    m_header.version = parsed.version > 0 ? parsed.version : 2;
    m_header.encrypted = parsed.encrypted;
    m_header.compression = parsed.compression;

    if (m_header.encrypted) {
        qDebug() << "MDX encrypted; not supported yet";
        return false;
    }

    // Skip adler32 (4 bytes) after header when present. Many MDX versions store it.
    // We'll peek next 4 bytes but not strictly depend on it.
    if (file.bytesAvailable() >= 4) {
        file.read(4);
    }

    // If experimental mode is enabled, try to extract words+definitions
    // (zlib + no encryption) for early usability.
    if (qEnvironmentVariableIntValue("UNIDICT_EXPERIMENTAL_MDX") == 1) {
        if (tryExperimentalParse(mdxPath)) {
            m_loaded = true;
            return true;
        }
    }

    // Attempt heuristic extraction of words from zlib streams (non-encrypted only),
    // to at least provide prefix/fuzzy suggestions before full parser lands.
    m_wordList.clear();
    m_wordIndex.clear();

    // Try heuristic build; if fails, fallback to minimal entries.
    bool built = false;
    {
        qint64 pos = file.pos();
        built = false;
        // Read the remainder of file into memory (bounded)
        const qint64 maxRead = 64ll * 1024ll * 1024ll; // 64MB safety cap
        QByteArray tail = file.read(maxRead);
        file.seek(pos); // reset if needed later
        if (!tail.isEmpty()) {
            // scan for zlib headers and try decompress a few blocks
            static const unsigned char zhdrs[][2] = {{0x78,0x01},{0x78,0x9C},{0x78,0xDA}};
            QSet<QString> words;
            int blocks = 0;
            for (int i = 0; i + 2 < tail.size() && blocks < 24; ++i) {
                unsigned char b0 = static_cast<unsigned char>(tail[i]);
                unsigned char b1 = static_cast<unsigned char>(tail[i+1]);
                bool match = false;
                for (auto& h : zhdrs) { if (b0 == h[0] && b1 == h[1]) { match = true; break; } }
                if (!match) continue;
                const char* src = tail.constData() + i;
                uLong srcLen = static_cast<uLong>(tail.size() - i);
                uLongf dstLen = srcLen * 8 + 1024; // rough estimate
                if (dstLen > 32 * 1024 * 1024) dstLen = 32 * 1024 * 1024; // cap 32MB per block
                QByteArray out;
                out.resize(dstLen);
                int ret = uncompress(reinterpret_cast<Bytef*>(out.data()), &dstLen,
                                     reinterpret_cast<const Bytef*>(src), srcLen);
                if (ret == Z_OK && dstLen > 0) {
                    out.resize(dstLen);
                    // decode according to header encoding; fallback to UTF-8
                    QString text = (m_header.encoding.toUpper() == "UTF-16LE")
                                    ? QString::fromUtf16(reinterpret_cast<const char16_t*>(out.constData()), out.size()/2)
                                    : QString::fromUtf8(out);
                    // extract null-terminated tokens and simple words
                    int start = 0;
                    for (int j = 0; j < text.size(); ++j) {
                        if (text.at(j) == QChar('\0') || j == text.size()-1) {
                            int end = (text.at(j) == QChar('\0')) ? j : j+1;
                            const QString tok = text.mid(start, end - start).trimmed();
                            if (!tok.isEmpty()) {
                                // take ascii-ish words to reduce noise
                                if (tok.size() <= 64 && tok.at(0).isLetterOrNumber()) {
                                    words.insert(tok);
                                }
                            }
                            start = j+1;
                        }
                    }
                    blocks++;
                }
            }
            if (!words.isEmpty()) {
                m_wordList = words.values();
                m_wordList.sort(Qt::CaseInsensitive);
                m_header.wordCount = m_wordList.size();
                built = true;
            }
        }
    }

    if (!built) {
        // Minimal fallback entries to indicate status
        m_wordList << "mdict" << "unidict";
        m_wordIndex["mdict"] = "MDict file loaded (header parsed). Real content parsing is in progress.";
        m_wordIndex["unidict"] = QString("Title: %1\nEncoding: %2").arg(m_header.title, m_header.encoding);
        m_header.wordCount = m_wordList.size();
    }

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
    // Legacy stub retained for compatibility in case needed elsewhere
    QJsonObject obj;
    obj["title"] = "MDict Dictionary";
    obj["description"] = "";
    obj["encoding"] = "UTF-8";
    obj["version"] = 2;
    obj["encrypted"] = false;
    return obj;
}

MdxHeader MdictParser::parseHeaderText(const QString& headerText) const {
    MdxHeader h; h.version = 2; h.encrypted = false; h.encoding = "UTF-8"; h.compression = "zlib";
    // Header text looks like XML: <Dictionary ... Attributes ... />
    // We'll extract key="value" pairs with a simple regex
    QRegularExpression rx("(\\w+)\\s*=\\s*\"([^\"]*)\"");
    auto it = rx.globalMatch(headerText);
    while (it.hasNext()) {
        auto m = it.next();
        const QString key = m.captured(1).toLower();
        const QString val = m.captured(2);
        if (key == "title") h.title = val;
        else if (key == "description") h.description = val;
        else if (key == "encoding") h.encoding = val;
        else if (key == "stylesheet") h.stylesheet = val;
        else if (key == "version") h.version = val.toInt();
        else if (key == "encrypted") h.encrypted = (val == "1" || val.toLower() == "yes" || val.toLower() == "true");
        else if (key == "compression") h.compression = val.toLower();
    }
    return h;
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

QList<QByteArray> MdictParser::scanAndDecompressZlibStreams(const QByteArray& data, int maxBlocks, int maxOutPerBlock) {
    QList<QByteArray> outBlocks;
    static const unsigned char zhdrs[][2] = {{0x78,0x01},{0x78,0x9C},{0x78,0xDA}};
    for (int i = 0; i + 2 < data.size() && (int)outBlocks.size() < maxBlocks; ++i) {
        unsigned char b0 = static_cast<unsigned char>(data[i]);
        unsigned char b1 = static_cast<unsigned char>(data[i+1]);
        bool match = false;
        for (auto& h : zhdrs) { if (b0 == h[0] && b1 == h[1]) { match = true; break; } }
        if (!match) continue;
        const char* src = data.constData() + i;
        uLong srcLen = static_cast<uLong>(data.size() - i);
        uLongf dstLen = static_cast<uLongf>(std::min(maxOutPerBlock, (int)srcLen * 8 + 1024));
        QByteArray out; out.resize(dstLen);
        int ret = uncompress(reinterpret_cast<Bytef*>(out.data()), &dstLen,
                             reinterpret_cast<const Bytef*>(src), srcLen);
        if (ret == Z_OK && dstLen > 0) {
            out.resize(dstLen);
            outBlocks.append(out);
        }
    }
    return outBlocks;
}

bool MdictParser::tryExperimentalParse(const QString& mdxPath) {
    if (m_header.encrypted) return false;
    if (m_header.compression.toLower() != "zlib" && !m_header.compression.isEmpty()) return false;

    QFile f(mdxPath);
    if (!f.open(QIODevice::ReadOnly)) return false;

    // Skip header
    quint32 headerLengthBE = 0;
    {
        QDataStream s(&f); s.setByteOrder(QDataStream::BigEndian); s >> headerLengthBE;
    }
    if (headerLengthBE == 0 || headerLengthBE > 50 * 1024 * 1024) return false;
    f.seek(f.pos() + headerLengthBE);
    if (f.bytesAvailable() >= 4) f.read(4); // skip adler32

    // Read a large tail segment into memory (cap 128MB)
    const qint64 maxRead = 128ll * 1024ll * 1024ll;
    QByteArray tail = f.read(maxRead);
    if (tail.isEmpty()) return false;

    // Decompress up to 48 zlib blocks (each up to 4MB out)
    const QList<QByteArray> blocks = scanAndDecompressZlibStreams(tail, 48, 4 * 1024 * 1024);
    if (blocks.isEmpty()) return false;

    QSet<QString> uniqueWords;
    QMap<QString, QString> defs;
    auto decode = [&](const QByteArray& b) -> QString {
        if (m_header.encoding.toUpper() == "UTF-16LE") {
            return QString::fromUtf16(reinterpret_cast<const char16_t*>(b.constData()), b.size()/2);
        }
        return QString::fromUtf8(b);
    };

    for (const QByteArray& blk : blocks) {
        // Split by NUL and attempt word/definition pairing
        int i = 0;
        while (i < blk.size()) {
            int j = blk.indexOf('\0', i);
            if (j == -1) j = blk.size();
            QByteArray token = blk.mid(i, j - i);
            i = (j == blk.size()) ? j : j + 1;
            if (token.isEmpty()) continue;
            // ASCII-ish candidate word
            bool ascii = true; for (unsigned char c : token) { if (c < 0x20 && c != '\t') { ascii = false; break; } }
            if (!ascii) continue;
            QString w = decode(token).trimmed();
            if (w.isEmpty()) continue;
            if (w.size() > 64) continue;
            if (!w.at(0).isLetterOrNumber()) continue;
            // Peek next as rough definition snippet
            if (i < blk.size()) {
                int k = blk.indexOf('\0', i);
                if (k == -1) k = std::min(i + 1024, blk.size()); // cap 1KB snippet
                QByteArray defBytes = blk.mid(i, k - i);
                QString def = decode(defBytes).trimmed();
                if (!def.isEmpty()) {
                    uniqueWords.insert(w);
                    if (defs.value(w).isEmpty()) defs[w] = def.left(2000); // limit size
                }
            }
        }
    }

    if (uniqueWords.isEmpty()) return false;
    m_wordList = uniqueWords.values();
    m_wordList.sort(Qt::CaseInsensitive);
    m_header.wordCount = m_wordList.size();

    // Install a small subset of definitions to make lookup non-empty
    int count = 0;
    for (const QString& w : m_wordList) {
        auto dit = defs.find(w);
        if (dit != defs.end()) {
            m_wordIndex[w.toLower()] = dit.value();
            if (++count > 20000) break; // cap
        }
    }
    return true;
}

}
