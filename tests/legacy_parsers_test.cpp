// Q-6 覆盖收口：core/ 其余 legacy 解析器与 DictionaryParser 接口默认实现。
//
// mdict_parser 的失败分支靠 MdxSpec 的字节手术（mdict_fixture.h）；stardict
// 的失败分支靠 StarDictSpec 的坏变体 + 目录替身；DictionaryParser 的默认
// allEntries/prefixSearch 用一个只实现纯虚接口的 BareParser 触发。
// plugin_manager / path_utils 是 0% 的薄门面，各调一遍即可，行为断言
// 落在适配器的既有语义上（env 变量驱动目录、工厂表小写归一）。

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>

#include "epub_fixture.h"
#include "mdict_fixture.h"
#include "stardict_fixture.h"

#include "epub_parser.h"
#include "json_parser.h"
#include "mdict_parser.h"
#include "path_utils.h"
#include "plugin_manager.h"
#include "stardict_parser.h"
#include "unidict_core.h"

using namespace UnidictCore;
using UnidictEpubFixture::writeEpubDictionary;
using UnidictMdictFixture::MdxSpec;
using UnidictMdictFixture::TestEntry;
using UnidictMdictFixture::writeMdxSpecFile;
using UnidictStardictFixture::StarDictSpec;
using UnidictStardictFixture::writeStarDictSpecFile;

namespace {

// 只实现 12 个纯虚接口的最小 parser：allEntries/prefixSearch 走基类默认实现
class BareParser final : public DictionaryParser {
public:
    bool loadDictionary(const QString&) override { return false; }
    bool isLoaded() const override { return false; }
    QStringList getSupportedExtensions() const override { return {"bare"}; }
    DictionaryEntry lookup(const QString&) const override { return {}; }
    QStringList findSimilar(const QString&, int) const override { return {}; }
    QStringList getAllWords() const override { return {"apple", "apricot", "banana"}; }
    QString getDictionaryName() const override { return "bare"; }
    QString getDictionaryDescription() const override { return {}; }
    int getWordCount() const override { return 3; }
    QString getSourcePath() const override { return {}; }
    QString getDictionaryId() const override { return "bare"; }
    QString getFormatName() const override { return "Bare"; }
};

QList<TestEntry> demoEntries() {
    return {{"apple", "fruit"}, {"apricot", "stone fruit"}, {"banana", "yellow berry"}};
}

} // namespace

class LegacyParsersTest : public QObject {
    Q_OBJECT

private slots:
    // unidict_core.h：DictionaryParser 默认 allEntries/prefixSearch + 虚析构
    void q6_parser_interface_defaults();
    // mdict_parser：加载成功后的查询面（findSimilar 前缀环/包含环、前缀搜索）
    void q6_mdict_interface_surface();
    // mdict_parser：文件形态类加载失败（缺失/目录/截断/头校验/加密/低版本引擎）
    void q6_mdict_load_failures();
    // mdict_parser：块解码类失败（zlib/校验/字段越界/计数不符）+ 不压缩块的成功路径
    void q6_mdict_block_failures();
    // mdict_parser：Encoding 全分支（UTF-16LE/GBK→GB18030/空→UTF-8/未知编码兜底）
    void q6_mdict_encodings();
    // stardict_parser：查询面 + ifo/idx 缺失与畸形
    void q6_stardict_surface_and_failures();
    // epub_parser：加载失败 + 查询面
    void q6_epub_surface();
    // json_parser / plugin_manager / path_utils 三个薄面
    void q6_json_plugins_paths();
};

void LegacyParsersTest::q6_parser_interface_defaults() {
    {
        // 基类指针析构：虚析构（= default）经 delete 走一遍
        std::unique_ptr<DictionaryParser> parser = std::make_unique<BareParser>();
        QVERIFY(parser->allEntries().isEmpty());
        QCOMPARE(parser->prefixSearch("AP", 1), (QStringList{"apple"}));
        QCOMPARE(parser->prefixSearch("a", 5), (QStringList{"apple", "apricot"}));
        QVERIFY(parser->prefixSearch("", 2).isEmpty());
        QVERIFY(parser->prefixSearch("zz", 5).isEmpty());
    }
}

void LegacyParsersTest::q6_mdict_interface_surface() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QVERIFY(writeMdxSpecFile(dir.path(), "surface", MdxSpec{demoEntries()}));

    // 未加载状态：查询全部空返回
    MdictParser fresh;
    QCOMPARE(fresh.getSupportedExtensions(), (QStringList{"mdx", "mdd"}));
    QVERIFY(!fresh.isLoaded());
    QVERIFY(fresh.lookup("apple").word.isEmpty());
    QVERIFY(fresh.findSimilar("apple", 5).isEmpty());
    QVERIFY(fresh.getAllWords().isEmpty());
    QVERIFY(fresh.allEntries().isEmpty());
    QVERIFY(fresh.prefixSearch("a", 5).isEmpty());
    QVERIFY(fresh.prefixSearch("a", 0).isEmpty());
    QVERIFY(fresh.getDictionaryName().isEmpty());

    MdictParser parser;
    const QString path = QDir(dir.path()).filePath("surface.mdx");
    QVERIFY(parser.loadDictionary(path));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getFormatName(), QString("MDict"));
    QCOMPARE(parser.getSourcePath(), path);
    QCOMPARE(parser.getDictionaryId(), QFileInfo(path).canonicalFilePath().toLower());
    QCOMPARE(parser.getDictionaryName(), QString("surface"));
    QCOMPARE(parser.getDictionaryDescription(), QString("MDX test dictionary"));
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "apricot", "banana"}));

    // lookup：命中 + 大小写归一回原词形 + 未命中
    const DictionaryEntry hit = parser.lookup("APPLE");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QCOMPARE(hit.metadata.value("source"), path);
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    // findSimilar：前缀环提前 return / 包含环去重 + maxResults break
    QCOMPARE(parser.findSimilar("ap", 1), (QStringList{"apple"}));
    QCOMPARE(parser.findSimilar("nan", 5), (QStringList{"banana"}));
    const QStringList all = parser.findSimilar("a", 3);
    QCOMPARE(all.size(), 3);
    QVERIFY(all.contains("apple") && all.contains("apricot") && all.contains("banana"));

    QCOMPARE(parser.allEntries().size(), 3);

    QCOMPARE(parser.prefixSearch("ap", 5), (QStringList{"apple", "apricot"}));
    QCOMPARE(parser.prefixSearch("AP", 5), (QStringList{"apple", "apricot"}));
    QCOMPARE(parser.prefixSearch("ap", 1), (QStringList{"apple"}));
    QVERIFY(parser.prefixSearch("ap", 0).isEmpty());
    QVERIFY(parser.prefixSearch("", 5).isEmpty());
}

void LegacyParsersTest::q6_mdict_load_failures() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto expectReject = [&dir](const QString& name) {
        MdictParser parser;
        QVERIFY2(!parser.loadDictionary(QDir(dir.path()).filePath(name + ".mdx")),
                 qPrintable(QStringLiteral("variant %1 should not load").arg(name)));
        QVERIFY(!parser.isLoaded());
    };
    auto writeSpec = [&dir](const QString& name, const MdxSpec& spec) {
        QVERIFY2(writeMdxSpecFile(dir.path(), name, spec), qPrintable(name));
    };

    // 文件不存在
    MdictParser missing;
    QVERIFY(!missing.loadDictionary(QDir(dir.path()).filePath("nope.mdx")));

    // 路径是目录：QFile 打不开
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("adir.mdx")));
    expectReject(QStringLiteral("adir"));

    // 不足 4 字节
    {
        QFile tiny(QDir(dir.path()).filePath("tiny.mdx"));
        QVERIFY(tiny.open(QIODevice::WriteOnly));
        tiny.write("MD");
    }
    expectReject(QStringLiteral("tiny"));

    // headerLength=0
    {
        QFile zeroLen(QDir(dir.path()).filePath("zerolen.mdx"));
        QVERIFY(zeroLen.open(QIODevice::WriteOnly));
        QByteArray bytes;
        UnidictMdictFixture::appendBigEndian32(bytes, 0);
        zeroLen.write(bytes);
    }
    expectReject(QStringLiteral("zerolen"));

    // 头长度声明远超文件 → 头读短
    {
        QFile hugeHeader(QDir(dir.path()).filePath("hugeheader.mdx"));
        QVERIFY(hugeHeader.open(QIODevice::WriteOnly));
        QByteArray bytes;
        UnidictMdictFixture::appendBigEndian32(bytes, 100000);
        hugeHeader.write(bytes);
    }
    expectReject(QStringLiteral("hugeheader"));

    // 头校验和坏
    MdxSpec badHeader;
    badHeader.entries = demoEntries();
    badHeader.corruptHeaderChecksum = true;
    writeSpec(QStringLiteral("badheader"), badHeader);
    expectReject(QStringLiteral("badheader"));

    // 加密位
    MdxSpec encrypted;
    encrypted.entries = demoEntries();
    encrypted.headerAttrs =
        QStringLiteral("GeneratedByEngineVersion=\"2.0\" RequiredEngineVersion=\"2.0\" "
                       "Encrypted=\"1\" Encoding=\"UTF-8\"");
    writeSpec(QStringLiteral("encrypted"), encrypted);
    expectReject(QStringLiteral("encrypted"));

    // 引擎版本 < 2.0 → 数字宽度回落 4 → 拒绝
    MdxSpec oldEngine;
    oldEngine.entries = demoEntries();
    oldEngine.headerAttrs =
        QStringLiteral("GeneratedByEngineVersion=\"1.0\" RequiredEngineVersion=\"1.0\" "
                       "Encrypted=\"0\" Encoding=\"UTF-8\"");
    writeSpec(QStringLiteral("oldengine"), oldEngine);
    expectReject(QStringLiteral("oldengine"));

    // 关键词头段整体截断（数字字段读短 + 校验和缺失）
    MdxSpec truncated;
    truncated.entries = demoEntries();
    truncated.truncateAfterHeaderChecksum = true;
    writeSpec(QStringLiteral("truncated"), truncated);
    expectReject(QStringLiteral("truncated"));

    // 关键词头校验和坏
    MdxSpec badKeyword;
    badKeyword.entries = demoEntries();
    badKeyword.corruptKeywordChecksum = true;
    writeSpec(QStringLiteral("badkeyword"), badKeyword);
    expectReject(QStringLiteral("badkeyword"));

    // keyInfo 块尺寸声明过小 → 解压出空数据 → 块表为空
    MdxSpec shortKeyInfo;
    shortKeyInfo.entries = demoEntries();
    shortKeyInfo.keyInfoBlockSizeOverride = 4;
    writeSpec(QStringLiteral("shortkeyinfo"), shortKeyInfo);
    expectReject(QStringLiteral("shortkeyinfo"));

    // keyBlock 总尺寸声明过大 → 读短为空
    MdxSpec hugeKeyBlocks;
    hugeKeyBlocks.entries = demoEntries();
    hugeKeyBlocks.keyBlocksSizeOverride = 100000;
    writeSpec(QStringLiteral("hugekeyblocks"), hugeKeyBlocks);
    expectReject(QStringLiteral("hugekeyblocks"));

    // numEntries 与实际词条数不符
    MdxSpec badCount;
    badCount.entries = demoEntries();
    badCount.numEntriesOverride = 99;
    writeSpec(QStringLiteral("badcount"), badCount);
    expectReject(QStringLiteral("badcount"));
}

void LegacyParsersTest::q6_mdict_block_failures() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto expectReject = [&dir](const QString& name) {
        MdictParser parser;
        QVERIFY2(!parser.loadDictionary(QDir(dir.path()).filePath(name + ".mdx")),
                 qPrintable(QStringLiteral("variant %1 should not load").arg(name)));
        QVERIFY(!parser.isLoaded());
    };
    auto writeSpec = [&dir](const QString& name, const MdxSpec& spec) {
        QVERIFY2(writeMdxSpecFile(dir.path(), name, spec), qPrintable(name));
    };

    // keyBlock zlib 载荷坏字节 → inflate 报错
    MdxSpec badZlib;
    badZlib.entries = demoEntries();
    badZlib.corruptKeyBlockZlib = true;
    writeSpec(QStringLiteral("badzlib"), badZlib);
    expectReject(QStringLiteral("badzlib"));

    // 包裹校验和坏（解压成功但 adler 不过）
    MdxSpec badChecksum;
    badChecksum.entries = demoEntries();
    badChecksum.corruptBlockChecksum = true;
    writeSpec(QStringLiteral("badchecksum"), badChecksum);
    expectReject(QStringLiteral("badchecksum"));

    // 未知压缩类型
    MdxSpec unknownType;
    unknownType.entries = demoEntries();
    unknownType.unknownCompressionType = true;
    writeSpec(QStringLiteral("unknowntype"), unknownType);
    expectReject(QStringLiteral("unknowntype"));

    // keyInfo 声称的块压缩尺寸超出实际 → 切片读短
    MdxSpec badCompressed;
    badCompressed.entries = demoEntries();
    badCompressed.keyInfoCompressedSizeOverride = 100000;
    writeSpec(QStringLiteral("badcompressed"), badCompressed);
    expectReject(QStringLiteral("badcompressed"));

    // keyInfo entryCount 虚高 → 解出的词条数与块头不符
    MdxSpec inflatedCount;
    inflatedCount.entries = demoEntries();
    inflatedCount.keyInfoEntryCountOverride = 9;
    writeSpec(QStringLiteral("inflatedcount"), inflatedCount);
    expectReject(QStringLiteral("inflatedcount"));

    // keyInfo 内容裁到只剩 entryCount=0 → 空块表提前 break
    MdxSpec emptyInfos;
    emptyInfos.entries = demoEntries();
    emptyInfos.keyInfoEntryCountOverride = 0;
    emptyInfos.keyInfoDecodedChopBytes = 33; // 41 字节 keyInfoRaw 只留前 8 字节
    writeSpec(QStringLiteral("emptyinfos"), emptyInfos);
    expectReject(QStringLiteral("emptyinfos"));

    // keyInfo 内容裁到词长字段中间 → 定长字读短
    MdxSpec halfField;
    halfField.entries = demoEntries();
    halfField.keyInfoDecodedChopBytes = 24; // 41 字节留 17：lastWord 长度读不满
    writeSpec(QStringLiteral("halffield"), halfField);
    expectReject(QStringLiteral("halffield"));

    // record 区：numEntries 不符
    MdxSpec badRecordCount;
    badRecordCount.entries = demoEntries();
    badRecordCount.recordNumEntriesOverride = 99;
    writeSpec(QStringLiteral("badrecordcount"), badRecordCount);
    expectReject(QStringLiteral("badrecordcount"));

    // record 区 infoSize 与块数不符
    MdxSpec badInfoSize;
    badInfoSize.entries = demoEntries();
    badInfoSize.recordBlockInfoSizeOverride = 32; // 16 字节只该有 1 对
    writeSpec(QStringLiteral("badinfosize"), badInfoSize);
    expectReject(QStringLiteral("badinfosize"));

    // record 区解压尺寸与实际不符
    MdxSpec badDecompSize;
    badDecompSize.entries = demoEntries();
    badDecompSize.recordInfoDecompSizeOverride = 500;
    writeSpec(QStringLiteral("baddecompsize"), badDecompSize);
    expectReject(QStringLiteral("baddecompsize"));

    // 词条 record offset 越界 → 装配防御拒绝
    MdxSpec badOffset;
    badOffset.entries = demoEntries();
    badOffset.keyOffsetOverride = 999999999;
    writeSpec(QStringLiteral("badoffset"), badOffset);
    expectReject(QStringLiteral("badoffset"));

    // UTF-16LE 词条流的尾巴砍掉半个终止符 → 字符块读短
    MdxSpec oddTail;
    oddTail.entries = demoEntries();
    oddTail.headerAttrs =
        QStringLiteral("GeneratedByEngineVersion=\"2.0\" RequiredEngineVersion=\"2.0\" "
                       "Encrypted=\"0\" Encoding=\"UTF-16LE\" Format=\"Html\"");
    oddTail.truncateKeyBlockTailByte = true;
    writeSpec(QStringLiteral("oddtail"), oddTail);
    expectReject(QStringLiteral("oddtail"));

    // 不压缩块（\x00 类型）是合法变体：应加载成功
    MdxSpec rawBlocks;
    rawBlocks.entries = demoEntries();
    rawBlocks.noCompressionBlocks = true;
    writeSpec(QStringLiteral("rawblocks"), rawBlocks);
    MdictParser parser;
    QVERIFY(parser.loadDictionary(QDir(dir.path()).filePath("rawblocks.mdx")));
    QCOMPARE(parser.lookup("banana").definition, QString("yellow berry"));
}

void LegacyParsersTest::q6_mdict_encodings() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QStringList encodings = {"UTF-16LE", "GBK", "", "ISO-8859-1"};
    for (const QString& encoding : encodings) {
        MdxSpec spec;
        spec.entries = demoEntries();
        spec.headerAttrs = QStringLiteral("GeneratedByEngineVersion=\"2.0\" "
                                          "RequiredEngineVersion=\"2.0\" Encrypted=\"0\" "
                                          "Encoding=\"%1\" Format=\"Html\"")
                               .arg(encoding);
        const QString name =
            QStringLiteral("enc_%1").arg(encoding.isEmpty() ? QString("empty") : encoding);
        QVERIFY2(writeMdxSpecFile(dir.path(), name, spec), qPrintable(name));
        MdictParser parser;
        QVERIFY2(parser.loadDictionary(QDir(dir.path()).filePath(name + ".mdx")),
                 qPrintable(name));
        QVERIFY2(parser.isLoaded(), qPrintable(name));
        QVERIFY2(parser.getAllWords() == (QStringList{"apple", "apricot", "banana"}),
                 qPrintable(name));
        QVERIFY2(parser.lookup("apricot").definition == QLatin1String("stone fruit"),
                 qPrintable(name));
    }

    // GBK 归一为 GB18030 后名字解析仍走 Title
    MdictParser parser;
    QVERIFY(parser.loadDictionary(QDir(dir.path()).filePath("enc_GBK.mdx")));
    QCOMPARE(parser.getDictionaryName(), QString("enc_GBK"));
}

void LegacyParsersTest::q6_stardict_surface_and_failures() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 未加载：查询面全空
    StarDictParser fresh;
    QCOMPARE(fresh.getSupportedExtensions(), (QStringList{"ifo"}));
    QVERIFY(!fresh.isLoaded());
    QVERIFY(fresh.lookup("apple").word.isEmpty());
    QVERIFY(fresh.findSimilar("apple", 5).isEmpty());
    QVERIFY(fresh.getAllWords().isEmpty());
    QVERIFY(fresh.allEntries().isEmpty());
    QVERIFY(fresh.prefixSearch("a", 5).isEmpty());
    QVERIFY(fresh.prefixSearch("a", 0).isEmpty());

    const QString basePath = QDir(dir.path()).filePath("sd");
    StarDictSpec spec;
    spec.entries = {{"apple", "fruit"}, {"banana", "yellow berry"}, {"cherry", "red fruit"}};
    spec.extraIfoLines = {"author=Someone", "email=a@b.c", "website=https://example.com",
                          "date=2026-09-27"};
    spec.junkIfoLines = {"no-equals-sign-here"};
    QVERIFY(writeStarDictSpecFile(dir.path(), "sd", spec));

    StarDictParser parser;
    QVERIFY(parser.loadDictionary(basePath + ".ifo"));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getFormatName(), QString("StarDict"));
    QCOMPARE(parser.getSourcePath(), basePath + ".ifo");
    QCOMPARE(parser.getDictionaryName(), QString("sd"));
    QCOMPARE(parser.getDictionaryDescription(), QString("Test dictionary"));
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "banana", "cherry"}));

    const DictionaryEntry hit = parser.lookup("APPLE");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    QCOMPARE(parser.findSimilar("a", 1), (QStringList{"apple"}));
    QCOMPARE(parser.findSimilar("an", 1), (QStringList{"banana"}));
    QCOMPARE(parser.findSimilar("an", 5), (QStringList{"banana"}));
    QCOMPARE(parser.allEntries().size(), 3);
    QCOMPARE(parser.prefixSearch("b", 5), (QStringList{"banana"}));
    QVERIFY(parser.prefixSearch("b", 0).isEmpty());
    QVERIFY(parser.prefixSearch("", 5).isEmpty());

    // 缺 .dict 组件 → 前置存在性检查拒绝
    StarDictSpec plain = spec;
    plain.junkIfoLines.clear();
    StarDictSpec noDict = plain;
    noDict.writeDict = false;
    QVERIFY(writeStarDictSpecFile(dir.path(), "nodict", noDict));
    StarDictParser noDictParser;
    QVERIFY(!noDictParser.loadDictionary(QDir(dir.path()).filePath("nodict.ifo")));

    // ifo 首行 magic 不对
    StarDictSpec badMagic = plain;
    badMagic.ifoMagic = "not the real magic";
    QVERIFY(writeStarDictSpecFile(dir.path(), "badmagic", badMagic));
    StarDictParser badMagicParser;
    QVERIFY(!badMagicParser.loadDictionary(QDir(dir.path()).filePath("badmagic.ifo")));

    // idx 首词条为空（\0 打头）→ 词表为空 → idx 解析失败
    StarDictSpec emptyWord = plain;
    emptyWord.idxOverride = QByteArray(1, '\0') + QByteArray(8, '\x01');
    QVERIFY(writeStarDictSpecFile(dir.path(), "emptyword", emptyWord));
    StarDictParser emptyWordParser;
    QVERIFY(!emptyWordParser.loadDictionary(QDir(dir.path()).filePath("emptyword.ifo")));

    // .ifo 换成目录：存在性检查过，QFile 打不开
    QVERIFY(writeStarDictSpecFile(dir.path(), "dirifo", plain));
    QVERIFY(QFile::remove(QDir(dir.path()).filePath("dirifo.ifo")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("dirifo.ifo")));
    StarDictParser dirIfoParser;
    QVERIFY(!dirIfoParser.loadDictionary(QDir(dir.path()).filePath("dirifo.ifo")));

    // .idx 换成目录：ifo 正常，idx 打不开
    QVERIFY(writeStarDictSpecFile(dir.path(), "diridx", plain));
    QVERIFY(QFile::remove(QDir(dir.path()).filePath("diridx.idx")));
    QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("diridx.idx")));
    StarDictParser dirIdxParser;
    QVERIFY(!dirIdxParser.loadDictionary(QDir(dir.path()).filePath("diridx.ifo")));
}

void LegacyParsersTest::q6_epub_surface() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 不存在的路径 → std 解析器拒绝
    EpubParser missing;
    QCOMPARE(missing.getSupportedExtensions(), (QStringList{"epub"}));
    QVERIFY(!missing.loadDictionary(QDir(dir.path()).filePath("none.epub")));
    QVERIFY(!missing.isLoaded());

    QVERIFY(writeEpubDictionary(dir.path(), "ebook", demoEntries()));
    EpubParser parser;
    const QString path = QDir(dir.path()).filePath("ebook.epub");
    QVERIFY(parser.loadDictionary(path));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getDictionaryName(), QString("ebook"));
    QCOMPARE(parser.getSourcePath(), path);
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "apricot", "banana"}));

    const DictionaryEntry hit = parser.lookup("APPLE");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    QCOMPARE(parser.findSimilar("a", 1), (QStringList{"apple"}));
    QCOMPARE(parser.findSimilar("b", 5), (QStringList{"banana"}));
    QCOMPARE(parser.allEntries().size(), 3);

    QVERIFY(parser.prefixSearch("ap", 0).isEmpty());
    QVERIFY(parser.prefixSearch("", 5).isEmpty());
    QCOMPARE(parser.prefixSearch("ap", 5), (QStringList{"apple", "apricot"}));
    QCOMPARE(parser.prefixSearch("b", 5), (QStringList{"banana"}));
}

void LegacyParsersTest::q6_json_plugins_paths() {
    // json_parser：扩展名表
    JsonParser json;
    QCOMPARE(json.getSupportedExtensions(), (QStringList{"json"}));

    // plugin_manager 门面：内置注册（幂等）/ 查询（大小写归一）/ 实例化 / 统计
    auto& pm = PluginManager::instance();
    pm.ensureBuiltinsRegistered();
    pm.ensureBuiltinsRegistered();
    QVERIFY(!pm.factoriesForExtension("json").empty());
    QVERIFY(!pm.factoriesForExtension("MDX").empty());
    QVERIFY(pm.factoriesForExtension("nosuchext").empty());
    pm.registerFactory({"XYZ"}, [] { return std::make_unique<JsonParser>(); });
    QCOMPARE(static_cast<int>(pm.factoriesForExtension("xyz").size()), 1);
    QCOMPARE(static_cast<int>(pm.factoriesForExtension("XYZ").size()), 1);
    const auto candidates = pm.createCandidatesForFile("f.xyz");
    QCOMPARE(static_cast<int>(candidates.size()), 1);
    QVERIFY(candidates.front() != nullptr);
    const QMap<QString, int> stats = pm.extensionStats();
    QVERIFY(stats.contains(QStringLiteral("json")));
    QVERIFY(stats.contains(QStringLiteral("mdx")));
    QCOMPARE(stats.value(QStringLiteral("xyz")), 1);

    // path_utils 门面：env 驱动的目录 + 缓存清理/裁剪
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dataDir = QDir(dir.path()).filePath("data");
    const QString cacheDir = QDir(dir.path()).filePath("cache");
    const QByteArray oldData = qgetenv("UNIDICT_DATA_DIR");
    const QByteArray oldCache = qgetenv("UNIDICT_CACHE_DIR");
    qputenv("UNIDICT_DATA_DIR", dataDir.toUtf8());
    qputenv("UNIDICT_CACHE_DIR", cacheDir.toUtf8());

    QCOMPARE(PathUtils::dataDir(), dataDir);
    QCOMPARE(PathUtils::cacheDir(), cacheDir);
    QVERIFY(PathUtils::ensureDir(cacheDir));
    QVERIFY(PathUtils::ensureDir(dataDir + "/nested/deep"));
    {
        QFile blob(cacheDir + "/blob.bin");
        QVERIFY(blob.open(QIODevice::WriteOnly));
        blob.write(QByteArray(10, 'x'));
    }
    QCOMPARE(PathUtils::cacheSizeBytes(), quint64(10));
    QVERIFY(PathUtils::pruneCacheBytes(5));
    QCOMPARE(PathUtils::cacheSizeBytes(), quint64(0));
    QVERIFY(PathUtils::pruneCacheOlderThanDays(1));
    QVERIFY(PathUtils::pruneCacheOlderThanDays(0));
    QVERIFY(PathUtils::clearCache());

    if (oldData.isNull()) {
        qunsetenv("UNIDICT_DATA_DIR");
    } else {
        qputenv("UNIDICT_DATA_DIR", oldData);
    }
    if (oldCache.isNull()) {
        qunsetenv("UNIDICT_CACHE_DIR");
    } else {
        qputenv("UNIDICT_CACHE_DIR", oldCache);
    }
}

QTEST_MAIN(LegacyParsersTest)
#include "legacy_parsers_test.moc"
