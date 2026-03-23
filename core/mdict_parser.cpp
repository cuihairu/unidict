#include "mdict_parser.h"

#include <QBuffer>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>
#include <QtEndian>

#include <algorithm>
#include <zlib.h>

namespace UnidictCore {

namespace {

constexpr char kNoCompression[] = "\x00\x00\x00\x00";
constexpr char kZlibCompression[] = "\x02\x00\x00\x00";

QString normalizeEncoding(QString encoding) {
    encoding = encoding.trimmed().toUpper();
    if (encoding == "GBK" || encoding == "GB2312") {
        return "GB18030";
    }
    if (encoding.isEmpty()) {
        return "UTF-8";
    }
    return encoding;
}

QByteArray readExact(QIODevice& device, qint64 size) {
    const QByteArray data = device.read(size);
    return data.size() == size ? data : QByteArray();
}

} // namespace

MdictParser::MdictParser() = default;

bool MdictParser::loadDictionary(const QString& filePath) {
    m_filePath = filePath;
    m_dictionaryId = QFileInfo(filePath).canonicalFilePath().toLower();
    m_loaded = false;
    m_wordIndex.clear();
    m_canonicalWords.clear();
    m_wordList.clear();
    m_resources.clear();
    m_keyEntries.clear();
    m_header = {};
    m_numberWidth = 8;
    m_encoding = "UTF-8";

    if (!QFileInfo::exists(filePath)) {
        return false;
    }

    if (!loadMdxFile(filePath)) {
        return false;
    }

    const QString mddPath = QFileInfo(filePath).absolutePath() + QDir::separator() +
                            QFileInfo(filePath).completeBaseName() + ".mdd";
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

    const QString definition = extractDefinition(word);
    if (definition.isEmpty()) {
        return {};
    }

    DictionaryEntry entry;
    entry.word = m_canonicalWords.value(word.toLower(), word);
    entry.definition = definition;
    entry.metadata.insert("source", m_filePath);
    return entry;
}

QStringList MdictParser::findSimilar(const QString& word, int maxResults) const {
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

QStringList MdictParser::getAllWords() const {
    return m_wordList;
}

QString MdictParser::getDictionaryName() const {
    return m_header.title.isEmpty() ? QFileInfo(m_filePath).completeBaseName() : m_header.title;
}

QString MdictParser::getDictionaryDescription() const {
    return m_header.description;
}

int MdictParser::getWordCount() const {
    return m_wordList.size();
}

QString MdictParser::getSourcePath() const {
    return m_filePath;
}

QString MdictParser::getDictionaryId() const {
    return m_dictionaryId;
}

QString MdictParser::getFormatName() const {
    return "MDict";
}

bool MdictParser::loadMdxFile(const QString& mdxPath) {
    QFile file(mdxPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray headerLengthBytes = readExact(file, 4);
    if (headerLengthBytes.size() != 4) {
        return false;
    }

    const quint32 headerLength = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(headerLengthBytes.constData()));
    if (headerLength == 0) {
        return false;
    }

    const QByteArray headerData = readExact(file, headerLength);
    const QByteArray checksumBytes = readExact(file, 4);
    if (headerData.isEmpty() || checksumBytes.size() != 4) {
        return false;
    }

    const quint32 expectedChecksum = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar*>(checksumBytes.constData()));
    if (static_cast<quint32>(::adler32(0L, reinterpret_cast<const Bytef*>(headerData.constData()), headerData.size())) !=
        expectedChecksum) {
        return false;
    }

    const QJsonObject headerObject = parseHeader(headerData);
    m_header.title = headerObject.value("Title").toString();
    m_header.description = headerObject.value("Description").toString();
    m_header.encoding = normalizeEncoding(headerObject.value("Encoding").toString("UTF-8"));
    m_encoding = m_header.encoding;
    m_header.version = static_cast<int>(headerObject.value("GeneratedByEngineVersion").toString().toDouble());
    m_header.encrypted = headerObject.value("Encrypted").toString() != "0" &&
                         headerObject.value("Encrypted").toString().compare("No", Qt::CaseInsensitive) != 0;

    if (m_header.encrypted) {
        return false;
    }

    const double engineVersion = headerObject.value("GeneratedByEngineVersion").toString().toDouble();
    m_numberWidth = engineVersion >= 2.0 ? 8 : 4;
    if (m_numberWidth != 8) {
        return false;
    }

    const quint64 numKeyBlocks = readBigEndianNumber(file, m_numberWidth);
    const quint64 numEntries = readBigEndianNumber(file, m_numberWidth);
    const quint64 keyBlockInfoDecompSize = readBigEndianNumber(file, m_numberWidth);
    const quint64 keyBlockInfoSize = readBigEndianNumber(file, m_numberWidth);
    const quint64 keyBlocksSize = readBigEndianNumber(file, m_numberWidth);
    Q_UNUSED(keyBlockInfoDecompSize)

    const QByteArray keywordChecksumBytes = readExact(file, 4);
    if (keywordChecksumBytes.size() != 4) {
        return false;
    }
    const quint32 keywordHeaderChecksum = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(keywordChecksumBytes.constData()));
    QByteArray keywordHeaderBytes;
    QDataStream keywordHeaderStream(&keywordHeaderBytes, QIODevice::WriteOnly);
    keywordHeaderStream.setByteOrder(QDataStream::BigEndian);
    keywordHeaderStream << numKeyBlocks << numEntries << keyBlockInfoDecompSize << keyBlockInfoSize << keyBlocksSize;
    if (static_cast<quint32>(::adler32(0L, reinterpret_cast<const Bytef*>(keywordHeaderBytes.constData()),
                                      keywordHeaderBytes.size())) != keywordHeaderChecksum) {
        return false;
    }

    if (!decodeKeyBlocks(file, keyBlockInfoSize, keyBlocksSize)) {
        return false;
    }

    if (numEntries != static_cast<quint64>(m_wordList.size())) {
        return false;
    }

    return decodeRecordBlocks(file);
}

bool MdictParser::loadMddFile(const QString& mddPath) {
    Q_UNUSED(mddPath)
    return false;
}

QByteArray MdictParser::decompressData(const QByteArray& compressedData) const {
    if (compressedData.size() < 8) {
        return {};
    }

    const QByteArray compType = compressedData.left(4);
    const quint32 expectedChecksum = qFromBigEndian<quint32>(
        reinterpret_cast<const uchar*>(compressedData.constData() + 4));
    QByteArray output;

    if (compType == QByteArray::fromRawData(kNoCompression, 4)) {
        output = compressedData.mid(8);
    } else if (compType == QByteArray::fromRawData(kZlibCompression, 4)) {
        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.constData() + 8));
        stream.avail_in = static_cast<uInt>(compressedData.size() - 8);

        if (inflateInit(&stream) != Z_OK) {
            return {};
        }

        QByteArray buffer;
        char temp[4096];
        int ret = Z_OK;
        do {
            stream.next_out = reinterpret_cast<Bytef*>(temp);
            stream.avail_out = sizeof(temp);
            ret = inflate(&stream, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&stream);
                return {};
            }
            buffer.append(temp, sizeof(temp) - stream.avail_out);
        } while (ret != Z_STREAM_END);

        inflateEnd(&stream);
        output = buffer;
    } else {
        return {};
    }

    if (static_cast<quint32>(::adler32(0L, reinterpret_cast<const Bytef*>(output.constData()), output.size())) !=
        expectedChecksum) {
        return {};
    }

    return output;
}

QJsonObject MdictParser::parseHeader(const QByteArray& headerData) const {
    QJsonObject result;
    const QString headerText = QString::fromUtf16(reinterpret_cast<const ushort*>(headerData.constData()),
                                                  headerData.size() / 2)
                                   .trimmed();

    QRegularExpression attributePattern(R"(([A-Za-z0-9_]+)="([^"]*)")");
    auto it = attributePattern.globalMatch(headerText);
    while (it.hasNext()) {
        const auto match = it.next();
        result.insert(match.captured(1), match.captured(2));
    }

    return result;
}

QString MdictParser::extractDefinition(const QString& word) const {
    const auto it = m_wordIndex.constFind(word.toLower());
    if (it != m_wordIndex.cend()) {
        return it.value();
    }
    return {};
}

QString MdictParser::decodeText(const QByteArray& data, bool stripTrailingNull) const {
    QByteArray working = data;
    if (stripTrailingNull) {
        if (m_encoding == "UTF-16" || m_encoding == "UTF-16LE") {
            while (working.size() >= 2 && working.endsWith(QByteArray("\0\0", 2))) {
                working.chop(2);
            }
        } else {
            while (!working.isEmpty() && working.endsWith('\0')) {
                working.chop(1);
            }
        }
    }

    QString text;
    if (m_encoding == "UTF-16" || m_encoding == "UTF-16LE") {
        text = QString::fromUtf16(reinterpret_cast<const ushort*>(working.constData()), working.size() / 2);
    } else if (m_encoding == "UTF-8") {
        text = QString::fromUtf8(working);
    } else if (m_encoding == "GB18030") {
        text = QString::fromLocal8Bit(working);
    } else {
        text = QString::fromUtf8(working);
    }

    return stripTrailingNull ? text.trimmed() : text;
}

quint64 MdictParser::readBigEndianNumber(QIODevice& device, int width) const {
    const QByteArray bytes = readExact(device, width);
    if (bytes.size() != width) {
        return 0;
    }

    if (width == 8) {
        return qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(bytes.constData()));
    }
    return qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData()));
}

quint16 MdictParser::readBigEndianWord(QIODevice& device) const {
    const QByteArray bytes = readExact(device, 2);
    if (bytes.size() != 2) {
        return 0;
    }
    return qFromBigEndian<quint16>(reinterpret_cast<const uchar*>(bytes.constData()));
}

bool MdictParser::decodeKeyBlocks(QIODevice& device, quint64 keyBlockInfoSize, quint64 keyBlocksSize) {
    const QByteArray keyBlockInfoRaw = readExact(device, static_cast<qint64>(keyBlockInfoSize));
    const QByteArray keyBlocksRaw = readExact(device, static_cast<qint64>(keyBlocksSize));
    if (keyBlockInfoRaw.isEmpty() || keyBlocksRaw.isEmpty()) {
        return false;
    }

    const QVector<KeyBlockInfo> blockInfos = decodeKeyBlockInfo(decompressData(keyBlockInfoRaw));
    if (blockInfos.isEmpty()) {
        return false;
    }

    qint64 offset = 0;
    for (const auto& blockInfo : blockInfos) {
        const QByteArray blockSlice = keyBlocksRaw.mid(offset, static_cast<qint64>(blockInfo.compressedSize));
        if (blockSlice.size() != static_cast<qint64>(blockInfo.compressedSize)) {
            return false;
        }

        const QVector<KeyEntry> entries = decodeKeyBlockData(decompressData(blockSlice), blockInfo.entryCount);
        if (entries.size() != static_cast<qsizetype>(blockInfo.entryCount)) {
            return false;
        }

        for (const auto& entry : entries) {
            if (!m_wordIndex.contains(entry.word.toLower())) {
                m_wordIndex.insert(entry.word.toLower(), QString());
                m_canonicalWords.insert(entry.word.toLower(), entry.word);
                m_wordList.append(entry.word);
                m_keyEntries.append(entry);
            }
        }

        offset += static_cast<qint64>(blockInfo.compressedSize);
    }

    return true;
}

QVector<MdictParser::KeyBlockInfo> MdictParser::decodeKeyBlockInfo(const QByteArray& data) const {
    QVector<KeyBlockInfo> infos;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);

    while (!buffer.atEnd()) {
        KeyBlockInfo info;
        info.entryCount = readBigEndianNumber(buffer, m_numberWidth);
        if (info.entryCount == 0 && buffer.atEnd()) {
            break;
        }

        const quint16 firstSizeUnits = readBigEndianWord(buffer);
        const int unitWidth = (m_encoding == "UTF-16" || m_encoding == "UTF-16LE") ? 2 : 1;
        const QByteArray firstWordBytes = readExact(buffer, firstSizeUnits * unitWidth);
        readExact(buffer, unitWidth);
        const quint16 lastSizeUnits = readBigEndianWord(buffer);
        const QByteArray lastWordBytes = readExact(buffer, lastSizeUnits * unitWidth);
        readExact(buffer, unitWidth);

        info.firstWord = decodeText(firstWordBytes, false);
        info.lastWord = decodeText(lastWordBytes, false);
        info.compressedSize = readBigEndianNumber(buffer, m_numberWidth);
        info.decompressedSize = readBigEndianNumber(buffer, m_numberWidth);
        infos.append(info);
    }

    return infos;
}

QVector<MdictParser::KeyEntry> MdictParser::decodeKeyBlockData(const QByteArray& data, quint64 expectedEntryCount) const {
    QVector<KeyEntry> entries;
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    const int unitWidth = (m_encoding == "UTF-16" || m_encoding == "UTF-16LE") ? 2 : 1;

    while (!buffer.atEnd() && entries.size() < static_cast<qsizetype>(expectedEntryCount)) {
        KeyEntry entry;
        entry.offset = readBigEndianNumber(buffer, m_numberWidth);

        QByteArray wordBytes;
        while (!buffer.atEnd()) {
            const QByteArray chunk = readExact(buffer, unitWidth);
            if (chunk.size() != unitWidth) {
                return {};
            }

            bool isTerminator = true;
            for (char byte : chunk) {
                if (byte != '\0') {
                    isTerminator = false;
                    break;
                }
            }
            if (isTerminator) {
                break;
            }

            wordBytes.append(chunk);
        }

        entry.word = decodeText(wordBytes, false).trimmed();
        entries.append(entry);
    }

    return entries;
}

bool MdictParser::decodeRecordBlocks(QIODevice& device) {
    const quint64 numRecordBlocks = readBigEndianNumber(device, m_numberWidth);
    const quint64 numEntries = readBigEndianNumber(device, m_numberWidth);
    const quint64 recordBlockInfoSize = readBigEndianNumber(device, m_numberWidth);
    const quint64 recordBlocksSize = readBigEndianNumber(device, m_numberWidth);
    Q_UNUSED(recordBlocksSize)

    if (numEntries != static_cast<quint64>(m_wordList.size())) {
        return false;
    }

    QVector<QPair<quint64, quint64>> recordInfos;
    quint64 bytesRead = 0;
    while (bytesRead < recordBlockInfoSize) {
        const quint64 compressedSize = readBigEndianNumber(device, m_numberWidth);
        const quint64 decompressedSize = readBigEndianNumber(device, m_numberWidth);
        recordInfos.append(qMakePair(compressedSize, decompressedSize));
        bytesRead += static_cast<quint64>(m_numberWidth * 2);
    }

    if (recordInfos.size() != static_cast<qsizetype>(numRecordBlocks)) {
        return false;
    }

    QByteArray records;
    for (const auto& info : recordInfos) {
        const QByteArray compressedBlock = readExact(device, static_cast<qint64>(info.first));
        const QByteArray block = decompressData(compressedBlock);
        if (block.size() != static_cast<qint64>(info.second)) {
            return false;
        }
        records.append(block);
    }

    QVector<KeyEntry> entries = m_keyEntries;
    std::sort(entries.begin(), entries.end(), [](const KeyEntry& a, const KeyEntry& b) {
        return a.offset < b.offset;
    });

    for (int i = 0; i < entries.size(); ++i) {
        const quint64 start = entries[i].offset;
        const quint64 end = i + 1 < entries.size() ? entries[i + 1].offset : static_cast<quint64>(records.size());
        if (end < start || end > static_cast<quint64>(records.size())) {
            return false;
        }

        const QByteArray recordBytes = records.mid(static_cast<qint64>(start), static_cast<qint64>(end - start));
        m_wordIndex[entries[i].word.toLower()] = decodeText(recordBytes, true);
    }

    return true;
}

} // namespace UnidictCore
