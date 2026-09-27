// 最小 EPUB 词典的**测试夹具**，供多个测试目标共用。
//
// 从 core_lookup_tests.cpp 抽出来的（与 mdict_fixture.h 同理）：stored zip
// （免压缩依赖，手摆 local file header + central directory）+ container +
// OPF + 一章 XHTML。词头用 <h2>/<h3>，走 EpubParserStd 的 heading 提取约定。

#ifndef UNIDICT_TESTS_EPUB_FIXTURE_H
#define UNIDICT_TESTS_EPUB_FIXTURE_H

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QList>
#include <QString>
#include <QtEndian>

extern "C" {
#include <zlib.h>
}

#include "mdict_fixture.h" // TestEntry

namespace UnidictEpubFixture {

inline bool writeEpubDictionary(const QString& directoryPath,
                                const QString& dictionaryName,
                                const QList<UnidictMdictFixture::TestEntry>& entries) {
    auto putU16 = [](QByteArray& out, quint16 v) {
        out.append(static_cast<char>(v & 0xff));
        out.append(static_cast<char>(v >> 8));
    };
    auto putU32 = [](QByteArray& out, quint32 v) {
        for (int i = 0; i < 4; ++i) {
            out.append(static_cast<char>((v >> (8 * i)) & 0xff));
        }
    };

    QString chapter = QStringLiteral("<html><head><title>c</title></head><body>");
    for (const auto& entry : entries) {
        chapter += QStringLiteral("<h2>%1</h2><p>%2</p>")
                       .arg(entry.word.toHtmlEscaped(), entry.definition.toHtmlEscaped());
    }
    chapter += QStringLiteral("</body></html>");

    const QByteArray container =
        "<rootfiles><rootfile full-path=\"content.opf\" "
        "media-type=\"application/oebps-package+xml\"/></rootfiles>";
    const QByteArray opf =
        "<package><metadata><dc:title>" + dictionaryName.toUtf8() +
        "</dc:title></metadata>"
        "<manifest><item id=\"c\" href=\"c.xhtml\" "
        "media-type=\"application/xhtml+xml\"/></manifest></package>";
    const QByteArray chapterBytes = chapter.toUtf8();

    struct LocalEntry {
        QByteArray name;
        QByteArray data;
    };
    const LocalEntry localEntries[] = {
        {"mimetype", "application/epub+zip"},
        {"META-INF/container.xml", container},
        {"content.opf", opf},
        {"c.xhtml", chapterBytes},
    };

    QByteArray archive;
    QByteArray central;
    quint16 count = 0;
    for (const auto& item : localEntries) {
        const quint32 crc = ::crc32(0, reinterpret_cast<const Bytef*>(item.data.constData()),
                                    static_cast<uInt>(item.data.size()));
        const quint32 size = static_cast<quint32>(item.data.size());
        const quint32 offset = static_cast<quint32>(archive.size());
        putU32(archive, 0x04034b50u);
        putU16(archive, 20);
        putU16(archive, 0);
        putU16(archive, 0); // stored
        putU16(archive, 0);
        putU16(archive, 0);
        putU32(archive, crc);
        putU32(archive, size);
        putU32(archive, size);
        putU16(archive, static_cast<quint16>(item.name.size()));
        putU16(archive, 0);
        archive.append(item.name);
        archive.append(item.data);

        putU32(central, 0x02014b50u);
        putU16(central, 20);
        putU16(central, 20);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU32(central, crc);
        putU32(central, size);
        putU32(central, size);
        putU16(central, static_cast<quint16>(item.name.size()));
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU16(central, 0);
        putU32(central, 0);
        putU32(central, offset);
        central.append(item.name);
        ++count;
    }
    const quint32 cdOffset = static_cast<quint32>(archive.size());
    const quint32 cdSize = static_cast<quint32>(central.size());
    archive.append(central);
    putU32(archive, 0x06054b50u);
    putU16(archive, 0);
    putU16(archive, 0);
    putU16(archive, count);
    putU16(archive, count);
    putU32(archive, cdSize);
    putU32(archive, cdOffset);
    putU16(archive, 0);

    QFile epubFile(QDir(directoryPath).filePath(dictionaryName + ".epub"));
    if (!epubFile.open(QIODevice::WriteOnly)) {
        return false;
    }
    epubFile.write(archive);
    return true;
}

} // namespace UnidictEpubFixture

#endif // UNIDICT_TESTS_EPUB_FIXTURE_H
