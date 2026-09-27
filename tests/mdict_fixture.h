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
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtEndian>

#include <cstring>

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
inline void appendLittleEndian32(QByteArray& data, quint32 value) {
    const quint32 le = qToLittleEndian(value);
    data.append(reinterpret_cast<const char*>(&le), 4);
}
inline QByteArray wrapNoCompBlock(const QByteArray& raw) {
    QByteArray block;
    block.append("\x00\x00\x00\x00", 4);
    const quint32 checksum = ::adler32(0L, reinterpret_cast<const Bytef*>(raw.constData()), raw.size());
    appendBigEndian32(block, checksum);
    block.append(raw);
    return block;
}

// 可配置的 .mdx 组装规格：正常词典之外，加载失败分支（坏校验和/截断/
// 加密位/低版本引擎/块解码越界等）需要对字节布局做定点手术。字段默认
// 关（-1/false = 按真实布局），打开哪一项就制造哪一种坏文件。
struct MdxSpec {
    QList<TestEntry> entries;
    // 头属性串（Title 由 dictionaryName 注入；Encoding 变化时词条/释义
    // 的编码随之切换：UTF-16LE 走双字节，其余 UTF-8）
    QString headerAttrs =
        QStringLiteral("GeneratedByEngineVersion=\"2.0\" RequiredEngineVersion=\"2.0\" "
                       "Encrypted=\"0\" Encoding=\"UTF-8\" Format=\"Html\"");
    bool noCompressionBlocks = false;     // 块用 \x00 类型（不压缩）而非 zlib
    bool unknownCompressionType = false;  // 块类型改成 5（解析器不认识）
    bool corruptKeyBlockZlib = false;     // keyBlock zlib 载荷翻坏一个字节
    bool corruptHeaderChecksum = false;   // 头 adler32 +1
    bool corruptKeywordChecksum = false;  // 关键词头 adler32 +1
    bool corruptBlockChecksum = false;    // keyInfo 包裹的 adler32 +1（解压成功但校验不过）
    bool truncateAfterHeaderChecksum = false; // 文件在头校验和后截断（关键词头读短）
    int keyInfoDecodedChopBytes = -1;     // keyInfo 解压前内容尾裁 N 字节（字段读到一半）
    qlonglong numEntriesOverride = -1;    // 关键词头的 numEntries（≠词条数 → 加载失败）
    qlonglong recordNumEntriesOverride = -1;   // record 区的 numEntries（→ 解码记录块失败）
    qlonglong recordBlockInfoSizeOverride = -1;// record 区的 infoSize（与块数不符）
    qlonglong recordInfoCompressedSizeOverride = -1;  // record info 压缩尺寸（≠实际）
    qlonglong recordInfoDecompSizeOverride = -1;      // record info 解压尺寸（≠实际）
    qlonglong keyInfoEntryCountOverride = -1;  // keyInfo 的 entryCount（0 → 空块表）
    qlonglong keyInfoCompressedSizeOverride = -1; // keyInfo 声称的块压缩尺寸（超出实际）
    qlonglong keyInfoBlockSizeOverride = -1;   // 关键词头声称的 keyInfo 块尺寸（小 → 读短块）
    qlonglong keyBlocksSizeOverride = -1;      // 关键词头声称的 keyBlock 总尺寸（大 → 越过 EOF）
    qlonglong keyOffsetOverride = -1;     // 词条 record offset 越界（542 防御）
    bool truncateKeyBlockTailByte = false;// keyBlock 数据尾留奇字节（UTF-16 半字）
    qint64 truncateAt = -1;               // 整个文件截断到 N 字节
};

// 记录编码相关常量：词条/释义按 Encoding 落字节；unitWidth 是
// key block 里"一个字符单元"的字节数（UTF-16LE=2，否则 1）
inline QByteArray encodeMdxText(const QString& text, const QString& encoding) {
    return encoding == QLatin1String("UTF-16LE") ? toUtf16Le(text) : text.toUtf8();
}
inline int mdxUnitWidth(const QString& encoding) {
    return encoding == QLatin1String("UTF-16LE") ? 2 : 1;
}
inline QByteArray mdxTerminator(const QString& encoding) {
    return encoding == QLatin1String("UTF-16LE") ? QByteArray("\0\0", 2) : QByteArray(1, '\0');
}

// 按规格组装 .mdx 的完整字节（不落盘；坏变体测试直接改返回值或截断）。
// 布局与 MdictParser::loadMdxFile 的读取顺序一一对应。
inline QByteArray buildMdxBytes(const MdxSpec& spec, const QString& dictionaryName) {
    QList<TestEntry> entries = spec.entries;
    std::sort(entries.begin(), entries.end(), [](const TestEntry& a, const TestEntry& b) {
        return a.word.toLower() < b.word.toLower();
    });
    Q_ASSERT(!entries.isEmpty());

    QString encodingAttr = QStringLiteral("UTF-8");
    const QRegularExpression attrPattern(QStringLiteral("Encoding=\"([^\"]*)\""));
    const auto attrMatch = attrPattern.match(spec.headerAttrs);
    if (attrMatch.hasMatch()) {
        encodingAttr = attrMatch.captured(1);
    }
    const int unitWidth = mdxUnitWidth(encodingAttr);
    const QByteArray terminator = mdxTerminator(encodingAttr);

    QByteArray recordData;
    QVector<quint64> offsets;
    offsets.reserve(entries.size());
    for (const auto& entry : entries) {
        offsets.append(spec.keyOffsetOverride >= 0
                           ? static_cast<quint64>(spec.keyOffsetOverride)
                           : static_cast<quint64>(recordData.size()));
        recordData.append(encodeMdxText(entry.definition, encodingAttr));
        recordData.append(terminator);
    }

    QByteArray keyBlockRaw;
    for (int i = 0; i < entries.size(); ++i) {
        appendBigEndian64(keyBlockRaw, offsets[i]);
        keyBlockRaw.append(encodeMdxText(entries[i].word, encodingAttr));
        keyBlockRaw.append(terminator);
    }
    if (spec.truncateKeyBlockTailByte) {
        keyBlockRaw.chop(1);
    }

    auto wrapBlock = [&](const QByteArray& raw) {
        QByteArray block = spec.noCompressionBlocks ? wrapNoCompBlock(raw) : wrapZlibBlock(raw);
        if (spec.unknownCompressionType) {
            block[0] = '\x05';
        }
        return block;
    };
    QByteArray keyBlock = wrapBlock(keyBlockRaw);
    if (spec.corruptKeyBlockZlib) {
        // 载荷从第 8 字节起是 zlib 流；翻坏中部字节让 inflate 报错
        const int at = qMin(12, keyBlock.size() - 1);
        keyBlock[at] = static_cast<char>(keyBlock.at(at) ^ 0x55);
    }

    QByteArray keyInfoRaw;
    appendBigEndian64(keyInfoRaw, spec.keyInfoEntryCountOverride >= 0
                                     ? static_cast<quint64>(spec.keyInfoEntryCountOverride)
                                     : static_cast<quint64>(entries.size()));
    const QByteArray firstWordBytes = encodeMdxText(entries.constFirst().word, encodingAttr);
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(firstWordBytes.size() / unitWidth));
    keyInfoRaw.append(firstWordBytes);
    keyInfoRaw.append(terminator);
    const QByteArray lastWordBytes = encodeMdxText(entries.constLast().word, encodingAttr);
    appendBigEndian16(keyInfoRaw, static_cast<quint16>(lastWordBytes.size() / unitWidth));
    keyInfoRaw.append(lastWordBytes);
    keyInfoRaw.append(terminator);
    appendBigEndian64(keyInfoRaw, spec.keyInfoCompressedSizeOverride >= 0
                                      ? static_cast<quint64>(spec.keyInfoCompressedSizeOverride)
                                      : static_cast<quint64>(keyBlock.size()));
    appendBigEndian64(keyInfoRaw, static_cast<quint64>(keyBlockRaw.size()));
    if (spec.keyInfoDecodedChopBytes >= 0) {
        keyInfoRaw.chop(spec.keyInfoDecodedChopBytes);
    }
    QByteArray keyInfoBlock = wrapBlock(keyInfoRaw);
    if (spec.corruptBlockChecksum) {
        // 压缩流完好，只翻坏包裹里的 adler32 —— 走"解压成功、校验失败"分支
        quint32 blockChecksum = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(keyInfoBlock.constData() + 4));
        blockChecksum = qToBigEndian(blockChecksum + 1);
        std::memcpy(keyInfoBlock.data() + 4, &blockChecksum, 4);
    }

    QByteArray keywordHeader;
    appendBigEndian64(keywordHeader, 1);
    appendBigEndian64(keywordHeader, spec.numEntriesOverride >= 0
                                         ? static_cast<quint64>(spec.numEntriesOverride)
                                         : static_cast<quint64>(entries.size()));
    appendBigEndian64(keywordHeader, static_cast<quint64>(keyInfoRaw.size()));
    appendBigEndian64(keywordHeader, spec.keyInfoBlockSizeOverride >= 0
                                         ? static_cast<quint64>(spec.keyInfoBlockSizeOverride)
                                         : static_cast<quint64>(keyInfoBlock.size()));
    appendBigEndian64(keywordHeader, spec.keyBlocksSizeOverride >= 0
                                         ? static_cast<quint64>(spec.keyBlocksSizeOverride)
                                         : static_cast<quint64>(keyBlock.size()));
    quint32 keywordChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(keywordHeader.constData()),
                                        keywordHeader.size());
    if (spec.corruptKeywordChecksum) {
        ++keywordChecksum;
    }

    const QByteArray recordBlock = wrapBlock(recordData);
    QByteArray recordSection;
    appendBigEndian64(recordSection, 1);
    appendBigEndian64(recordSection, spec.recordNumEntriesOverride >= 0
                                         ? static_cast<quint64>(spec.recordNumEntriesOverride)
                                         : static_cast<quint64>(entries.size()));
    appendBigEndian64(recordSection, spec.recordBlockInfoSizeOverride >= 0
                                         ? static_cast<quint64>(spec.recordBlockInfoSizeOverride)
                                         : 16);
    appendBigEndian64(recordSection, static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, spec.recordInfoCompressedSizeOverride >= 0
                                         ? static_cast<quint64>(spec.recordInfoCompressedSizeOverride)
                                         : static_cast<quint64>(recordBlock.size()));
    appendBigEndian64(recordSection, spec.recordInfoDecompSizeOverride >= 0
                                         ? static_cast<quint64>(spec.recordInfoDecompSizeOverride)
                                         : static_cast<quint64>(recordData.size()));
    recordSection.append(recordBlock);

    const QString headerText = QStringLiteral("<Dictionary %1 Title=\"%2\" "
                                              "Description=\"MDX test dictionary\" />")
                                   .arg(spec.headerAttrs, dictionaryName);
    QByteArray headerBytes = toUtf16Le(headerText);
    headerBytes.append('\0');
    headerBytes.append('\0');
    quint32 headerChecksum = ::adler32(0L, reinterpret_cast<const Bytef*>(headerBytes.constData()),
                                       headerBytes.size());
    if (spec.corruptHeaderChecksum) {
        ++headerChecksum;
    }

    QByteArray mdx;
    appendBigEndian32(mdx, static_cast<quint32>(headerBytes.size()));
    mdx.append(headerBytes);
    appendLittleEndian32(mdx, headerChecksum);
    mdx.append(keywordHeader);
    appendBigEndian32(mdx, keywordChecksum);
    mdx.append(keyInfoBlock);
    mdx.append(keyBlock);
    mdx.append(recordSection);
    if (spec.truncateAfterHeaderChecksum) {
        // 停在关键词头之前：五个 8 字节数字段 + 4 字节校验全部读短
        mdx.truncate(4 + headerBytes.size() + 4);
    }
    if (spec.truncateAt >= 0) {
        mdx.truncate(static_cast<int>(spec.truncateAt));
    }
    return mdx;
}

// 写一个最小但结构完整的 .mdx（key block / record block 走真实 zlib 包裹，
// 头是 UTF-16LE，Format="Html"）。entries 会按词条降序前先排序。
inline bool writeMdxDictionary(const QString& directoryPath,
                        const QString& dictionaryName,
                        QList<TestEntry> entries) {
    MdxSpec spec;
    spec.entries = entries;
    QFile mdxFile(QDir(directoryPath).filePath(dictionaryName + ".mdx"));
    if (!mdxFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    const QByteArray bytes = buildMdxBytes(spec, dictionaryName);
    mdxFile.write(bytes);
    return true;
}

// 按规格落盘一个 .mdx（坏变体用）
inline bool writeMdxSpecFile(const QString& directoryPath,
                             const QString& dictionaryName,
                             const MdxSpec& spec) {
    QFile mdxFile(QDir(directoryPath).filePath(dictionaryName + ".mdx"));
    if (!mdxFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    mdxFile.write(buildMdxBytes(spec, dictionaryName));
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
