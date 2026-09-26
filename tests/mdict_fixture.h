// MDX 词典 + 配套 .mdd 资源包的**测试夹具**，供多个测试目标共用。
//
// 从 core_lookup_tests.cpp 抽出来的：写 .mdx 需要 zlib 包裹的 key block /
// record block / UTF-16LE 头，写 .mdd 需要按 .mdd 容器格式摆 header + 索引表，
// 约 190 行。放在头里而不是复制两份，是为了让"词典能加载"和"资源能解析"
// 这两件事在各测试目标里的造法保持一致——夹具本身写错的话，至少只错一处。
//
// 约定：.mdd 与 .mdx 同名同目录（MDict 的资源包约定）。core/std 的
// MddResourceParser 与 qmlui/LookupAdapter::deriveMddPath 都依赖这一点。

#ifndef UNIDICT_TESTS_MDICT_FIXTURE_H
#define UNIDICT_TESTS_MDICT_FIXTURE_H

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtEndian>

#include <zlib.h>

namespace UnidictMdictFixture {

struct TestEntry {
    QString word;
    QString definition;
};

inline QByteArray toUtf16Le(const QString& text) {
    QByteArray bytes;
    bytes.reserve(text.size() * 2);
    for (QChar ch : text) {
        const char16_t value = ch.unicode();
        bytes.append(static_cast<char>(value & 0xff));
        bytes.append(static_cast<char>((value >> 8) & 0xff));
    }
    return bytes;
}
inline QByteArray wrapZlibBlock(const QByteArray& raw) {
    uLongf bound = compressBound(raw.size());
    QByteArray compressed(static_cast<int>(bound), '\0');
    if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &bound,
                  reinterpret_cast<const Bytef*>(raw.constData()), raw.size(), Z_BEST_COMPRESSION) != Z_OK) {
        return {};
    }
    compressed.resize(static_cast<int>(bound));

    QByteArray block;
    block.append("\x02\x00\x00\x00", 4);
    const quint32 checksum = ::adler32(0L, reinterpret_cast<const Bytef*>(raw.constData()), raw.size());
    quint32 checksumBe = qToBigEndian(checksum);
    block.append(reinterpret_cast<const char*>(&checksumBe), 4);
    block.append(compressed);
    return block;
}
inline void appendBigEndian16(QByteArray& data, quint16 value) {
    const quint16 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 2);
}
inline void appendBigEndian32(QByteArray& data, quint32 value) {
    const quint32 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 4);
}
inline void appendBigEndian64(QByteArray& data, quint64 value) {
    const quint64 be = qToBigEndian(value);
    data.append(reinterpret_cast<const char*>(&be), 8);
}
// 写一个最小但结构完整的 .mdx（key block / record block 走真实 zlib 包裹，
// 头是 UTF-16LE，Format="Html"）。entries 会按词条降序前先排序。
inline bool writeMdxDictionary(const QString& directoryPath,
                        const QString& dictionaryName,
                        QList<TestEntry> entries) {
    std::sort(entries.begin(), entries.end(), [](const TestEntry& a, const TestEntry& b) {
        return a.word.toLower() < b.word.toLower();
    });

    QByteArray recordData;
    QVector<quint64> offsets;
    offsets.reserve(entries.size());
    for (const auto& entry : entries) {
        offsets.append(static_cast<quint64>(recordData.size()));
        recordData.append(entry.definition.toUtf8());
        recordData.append('\0');
    }

    QByteArray keyBlockRaw;
    for (int i = 0; i < entries.size(); ++i) {
        appendBigEndian64(keyBlockRaw, offsets[i]);
        keyBlockRaw.append(entries[i].word.toUtf8());
        keyBlockRaw.append('\0');
    }
    const QByteArray keyBlock = wrapZlibBlock(keyBlockRaw);

    QByteArray keyInfoRaw;
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(entries.size()));
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(entries.constFirst().word.toUtf8().size()));
    keyInfoRaw.append(entries.constFirst().word.toUtf8());
    keyInfoRaw.append('\0');
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(entries.constLast().word.toUtf8().size()));
    keyInfoRaw.append(entries.constLast().word.toUtf8());
    keyInfoRaw.append('\0');
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(keyBlock.size()));
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(keyBlockRaw.size()));
    const QByteArray keyInfoBlock = wrapZlibBlock(keyInfoRaw);

    QByteArray keywordHeader;
    appendBigEndian64(keywordHeader, 1);
    appendBigEndian64(keywordHeader, static_cast<quint64>(entries.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyInfoRaw.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyInfoBlock.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyBlock.size()));
    const quint32 keywordChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(keywordHeader.constData()),
                                              keywordHeader.size());

    const QByteArray recordBlock = wrapZlibBlock(recordData);
    QByteArray recordSection;
    appendBigEndian64(recordSection, 1);
    appendBigEndian64(recordSection, static_cast<quint64>(entries.size()));
    appendBigEndian64(recordSection, 16);
    appendBigEndian64(recordSection, static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, static_cast<quint64>(recordData.size()));
    recordSection.append(recordBlock);

    const QString headerText =
        QString("<Dictionary GeneratedByEngineVersion=\"2.0\" RequiredEngineVersion=\"2.0\" "
                "Encrypted=\"0\" Encoding=\"UTF-8\" Format=\"Html\" Title=\"%1\" Description=\"MDX test dictionary\" />")
            .arg(dictionaryName);
    QByteArray headerBytes = toUtf16Le(headerText);
    headerBytes.append('\0');
    headerBytes.append('\0');
    const quint32 headerChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(headerBytes.constData()),
                                             headerBytes.size());

    QFile mdxFile(QDir(directoryPath).filePath(dictionaryName + ".mdx"));
    if (!mdxFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QByteArray prefix;
    appendBigEndian32(prefix, static_cast<quint32>(headerBytes.size()));
    mdxFile.write(prefix);
    mdxFile.write(headerBytes);
    quint32 headerChecksumLe = qToLittleEndian(headerChecksum);
    mdxFile.write(reinterpret_cast<const char*>(&headerChecksumLe), 4);
    mdxFile.write(keywordHeader);
    quint32 keywordChecksumBe = qToBigEndian(keywordChecksum);
    mdxFile.write(reinterpret_cast<const char*>(&keywordChecksumBe), 4);
    mdxFile.write(keyInfoBlock);
    mdxFile.write(keyBlock);
    mdxFile.write(recordSection);

    return true;
}

// 写配套 .mdd：V2 头（magic(3)+header_len(2)+version(2)）+ single-block 索引表
// + 表后的资源值。表长决定资源值起点，所以先量一遍表长。
// resourceName 传不带扩展名的词条名（自动补 .mdd）；已带 .mdd 的原样用。
//
// 注意头部必须是 8 字节：解析器一次读 8 字节再跳 header_len-8，字段从
// buf+3 起取（buf[0..2] 是 magic）。core/std/mdd_resource_std.cpp。
inline bool writeMddResource(const QString& directoryPath,
                             const QString& resourceName,
                             QList<QPair<QString, QByteArray>> resources) {
    constexpr int kHeaderLen = 16;
    QByteArray body;
    body.append(QByteArray("\x1b#\x01", 3));
    appendBigEndian16(body, static_cast<quint16>(kHeaderLen));
    appendBigEndian16(body, 0x2d);
    while (body.size() < kHeaderLen) {
        body.append('\0');
    }

    int tableSize = 0;
    for (const auto& r : std::as_const(resources)) {
        tableSize += 2 + r.first.toUtf8().size() + 8 + 8;
    }

    quint64 dataOffset = static_cast<quint64>(kHeaderLen + tableSize);
    QByteArray table;
    QByteArray blob;
    for (const auto& r : std::as_const(resources)) {
        const QByteArray key = r.first.toUtf8();
        appendBigEndian16(table, static_cast<quint16>(key.size()));
        table.append(key);
        appendBigEndian64(table, dataOffset);
        appendBigEndian64(table, static_cast<quint64>(r.second.size()));
        blob.append(r.second);
        dataOffset += static_cast<quint64>(r.second.size());
    }
    body.append(table);
    body.append(blob);

    const QString fileName =
        resourceName.endsWith(QStringLiteral(".mdd"), Qt::CaseInsensitive)
            ? resourceName
            : resourceName + QStringLiteral(".mdd");
    QFile mddFile(QDir(directoryPath).filePath(fileName));
    if (!mddFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    mddFile.write(body);
    mddFile.close();
    return true;
}

// PNG 最小合法字节流（IHDR 头足够让 QPixmap/QImageLoader 认出格式）
inline QByteArray fakePng(int payloadBytes = 16) {
    QByteArray png("\x89PNG\r\n\x1a\n", 8);
    png.append("IHDR");
    png.append(QByteArray::fromHex("0000000D49484452"));
    for (int i = 0; i < payloadBytes; ++i) {
        png.append(static_cast<char>('A' + (i % 26)));
    }
    return png;
}

}  // namespace UnidictMdictFixture

#endif  // UNIDICT_TESTS_MDICT_FIXTURE_H
