// StarDict 词典的**测试夹具**，供多个测试目标共用。
//
// 从 core_lookup_tests.cpp 抽出来的（与 mdict_fixture.h 同理：多个测试
// 目标都要造 .ifo/.idx/.dict 三件套，布局代码只留一份）。在原实现之上
// 参数化了坏变体——首行 magic、无 '=' 的杂行、缺组件文件、idx 内容覆盖，
// 覆盖 StarDictParser 加载失败分支时不用再复制一份 writer。

#ifndef UNIDICT_TESTS_STARDICT_FIXTURE_H
#define UNIDICT_TESTS_STARDICT_FIXTURE_H

#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QStringList>

#include "mdict_fixture.h" // TestEntry

namespace UnidictStardictFixture {

struct StarDictSpec {
    QList<UnidictMdictFixture::TestEntry> entries;
    QStringList extraIfoLines;   // 追加 key=value 字段（author/email/website/date/…）
    QStringList junkIfoLines;    // 无 '=' 的杂行（解析器按 continue 跳过）
    QString ifoMagic = QStringLiteral("StarDict's dict ifo file"); // 首行
    QByteArray idxOverride;      // 非空则整体替代生成的 idx（坏变体）
    bool writeIdx = true;        // false → 缺 .idx
    bool writeDict = true;       // false → 缺 .dict
};

inline bool writeStarDictSpecFile(const QString& directoryPath,
                                  const QString& dictionaryName,
                                  const StarDictSpec& spec) {
    const QString basePath = QDir(directoryPath).filePath(dictionaryName);
    QFile dictFile(basePath + ".dict");
    QFile idxFile(basePath + ".idx");
    QFile ifoFile(basePath + ".ifo");

    // 不带 QIODevice::Text：Text 模式在 Windows 把 \n 译成 \r\n，.ifo 的
    // 字符串字段（bookname 等）会被真实世界的 CRLF 变体污染（解析器已
    // 剥 \r，这里再保证夹具字节跨平台确定）
    if (!ifoFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QByteArray idxData;
    if (spec.writeIdx && !idxFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    if (spec.writeDict && !dictFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    QDataStream idxStream(&idxFile);
    idxStream.setByteOrder(QDataStream::BigEndian);

    // idxOverride 非空时整体替代生成的 idx（坏变体只动索引不动词典）
    const bool useIdxOverride = !spec.idxOverride.isEmpty();
    QByteArray dictData;
    quint32 offset = 0;
    if (!useIdxOverride) {
        for (const auto& entry : spec.entries) {
            const QByteArray wordBytes = entry.word.toUtf8();
            const QByteArray definitionBytes = entry.definition.toUtf8();

            idxFile.write(wordBytes);
            idxFile.putChar('\0');
            idxStream << offset << static_cast<quint32>(definitionBytes.size());
            dictData.append(definitionBytes);
            offset += static_cast<quint32>(definitionBytes.size());
        }
    }
    if (spec.writeDict) {
        dictFile.write(dictData);
    }

    const QByteArray ifoData =
        spec.ifoMagic.toUtf8() + "\n"
        "version=2.4.2\n"
        "bookname=" + dictionaryName.toUtf8() + "\n"
        "wordcount=" + QByteArray::number(spec.entries.size()) + "\n"
        "idxfilesize=" + QByteArray::number(idxFile.size()) + "\n"
        "description=Test dictionary\n";
    QByteArray extraLines;
    for (const QString& line : spec.extraIfoLines) {
        extraLines += line.toUtf8() + "\n";
    }
    for (const QString& line : spec.junkIfoLines) {
        extraLines += line.toUtf8() + "\n";
    }
    ifoFile.write(ifoData + extraLines);

    if (spec.writeIdx) {
        idxFile.write(spec.idxOverride.isEmpty() ? idxData : spec.idxOverride);
    }
    return true;
}

inline bool writeStarDictDictionary(const QString& directoryPath,
                                    const QString& dictionaryName,
                                    const QList<UnidictMdictFixture::TestEntry>& entries) {
    StarDictSpec spec;
    spec.entries = entries;
    return writeStarDictSpecFile(directoryPath, dictionaryName, spec);
}

} // namespace UnidictStardictFixture

#endif // UNIDICT_TESTS_STARDICT_FIXTURE_H
