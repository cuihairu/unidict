// Q-7 覆盖收口：adapters/qt 里此前 0% 的 std↔Qt 薄桥接——parser 桥 ×3
// （json/mdict/stardict，断言共用同一模板：加载失败清空标识、成功记录
// canonical 路径、getter 全透传）、AI 服务（外部命令 + 无命令回落启发式）、
// 剪贴板、设置项（QSettings 走 test mode 落盘临时目录）。
// ai_service/clipboard/settings 没有库目标（只有 qmlui 直接编译），照
// test_sync_service_qt 的做法把源码挂进本测试 target。

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory> // 多态删除走基类指针（DictionaryManager 的持有方式）
#include <zlib.h> // writeStdMdx 直写 std 解析器认得的原始 zlib 块

#include "ai_service_qt.h"
#include "clipboard_qt.h"
#include "json_parser_qt.h"
#include "mdict_parser_qt.h"
#include "settings_qt.h"
#include "stardict_parser_qt.h"

#include "mdict_fixture.h"
#include "stardict_fixture.h"

using UnidictMdictFixture::TestEntry;
using UnidictStardictFixture::writeStarDictDictionary;

namespace {

QList<TestEntry> demoEntries() {
    return {{"apple", "fruit"}, {"apricot", "stone fruit"}, {"banana", "yellow berry"}};
}

bool writeJsonDictionary(const QString& directoryPath,
                         const QString& dictionaryName,
                         const QList<TestEntry>& entries) {
    QJsonArray entryArray;
    for (const auto& entry : entries) {
        QJsonObject entryObject;
        entryObject.insert("word", entry.word);
        entryObject.insert("definition", entry.definition);
        entryArray.append(entryObject);
    }
    QJsonObject root;
    root.insert("name", dictionaryName);
    root.insert("description", QStringLiteral("json bridge"));
    root.insert("entries", entryArray);

    QFile jsonFile(QDir(directoryPath).filePath(dictionaryName + ".json"));
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    jsonFile.write(QJsonDocument(root).toJson());
    return true;
}

// MdictParserStd 不认识 writeMdxSpecFile 的真实 MDX 分块布局——它的
// best-effort 解析只扫"文本头 + 原始 zlib 块"，块内容支持
// word:<w>\ndefinition:<d>\n\n 简单对（同 mdict_zlib_std_test 的做法）。
// 用这种布局让 std 侧真正解析出词条，桥接层才有非空的透传内容可断言
bool writeStdMdx(const QString& directoryPath,
                 const QString& dictionaryName,
                 const QList<TestEntry>& entries) {
    QByteArray text;
    for (const auto& entry : entries) {
        text.append(QStringLiteral("word:%1\ndefinition:%2\n\n")
                        .arg(entry.word, entry.definition)
                        .toUtf8());
    }
    uLongf bound = compressBound(static_cast<uLong>(text.size()));
    QByteArray compressed(static_cast<int>(bound), Qt::Uninitialized);
    if (compress2(reinterpret_cast<Bytef*>(compressed.data()), &bound,
                  reinterpret_cast<const Bytef*>(text.constData()),
                  static_cast<uLong>(text.size()), Z_BEST_COMPRESSION) != Z_OK) {
        return false;
    }
    compressed.resize(static_cast<int>(bound));

    QFile mdx(QDir(directoryPath).filePath(dictionaryName + ".mdx"));
    if (!mdx.open(QIODevice::WriteOnly)) {
        return false;
    }
    mdx.write(QStringLiteral("<Dictionary title=\"%1\" description=\"MDX test dictionary\"/>\n")
                  .arg(dictionaryName)
                  .toUtf8());
    mdx.write(compressed);
    return true;
}

} // namespace

class QtAdaptersTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    // parser 桥 ×3：加载失败/成功 + 全 getter 透传 + 查询面
    void q7_json_parser_bridge();
    void q7_mdict_parser_bridge();
    void q7_stardict_parser_bridge();
    // AI 服务：env 命令入口、外部命令三种结局（stdout/stderr/启动失败/崩溃）、
    // 无命令时的回落启发式
    void q7_ai_service();
    // 剪贴板读写往返（offscreen 平台插件提供内存剪贴板）
    void q7_clipboard();
    // 设置项：bool/string/int 往返 + 缺省值 + 落盘后跨实例可读
    void q7_settings();
};

void QtAdaptersTest::initTestCase() {
    // QSettings(UserScope) 与剪贴板都不碰真实用户目录
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("unidict-test"));
    QCoreApplication::setApplicationName(QStringLiteral("qt-adapters-test"));
}

void QtAdaptersTest::cleanupTestCase() {
    QStandardPaths::setTestModeEnabled(false);
}

void QtAdaptersTest::q7_json_parser_bridge() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // 加载失败：清空标识并返回 false
    UnidictAdaptersQt::JsonParserQt missing;
    QVERIFY(!missing.loadDictionary(QDir(dir.path()).filePath("nope.json")));
    QVERIFY(!missing.isLoaded());
    QVERIFY(missing.getSourcePath().isEmpty());
    QVERIFY(missing.getDictionaryId().isEmpty());

    QVERIFY(writeJsonDictionary(dir.path(), "jd", demoEntries()));
    UnidictAdaptersQt::JsonParserQt parser;
    const QString path = QDir(dir.path()).filePath("jd.json");
    QVERIFY(parser.loadDictionary(path));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getSupportedExtensions(), (QStringList{"json"}));
    QCOMPARE(parser.getDictionaryName(), QString("jd"));
    QCOMPARE(parser.getDictionaryDescription(), QString("json bridge"));
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getSourcePath(), QFileInfo(path).canonicalFilePath());
    QCOMPARE(parser.getDictionaryId(), QFileInfo(path).canonicalFilePath().toLower());
    QCOMPARE(parser.getFormatName(), QString("JSON"));

    const UnidictCore::DictionaryEntry hit = parser.lookup("apple");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "apricot", "banana"}));
    QCOMPARE(parser.findSimilar("ap", 1), (QStringList{"apple"}));
    QVERIFY(!parser.findSimilar("ap", 5).isEmpty());

    // 多态删除路径：DictionaryManager 以基类指针持有 parser，重置时走
    // 派生类的 D0 deleting 析构（栈对象的 D2 已在上面覆盖过）
    {
        std::unique_ptr<UnidictCore::DictionaryParser> polymorphic =
            std::make_unique<UnidictAdaptersQt::JsonParserQt>();
        QVERIFY(!polymorphic->getSupportedExtensions().isEmpty());
    }
}

void QtAdaptersTest::q7_mdict_parser_bridge() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    UnidictAdaptersQt::MdictParserQt missing;
    QVERIFY(!missing.loadDictionary(QDir(dir.path()).filePath("nope.mdx")));
    QVERIFY(!missing.isLoaded());
    QVERIFY(missing.getSourcePath().isEmpty());
    QVERIFY(missing.getDictionaryId().isEmpty());

    // std 侧语义注意：与 core 的 MdictParser 不同，std 的 lookup 大小写
    // 敏感、find_similar 只做前缀匹配——桥接层原样透传这些行为
    QVERIFY(writeStdMdx(dir.path(), "mdxbridge", demoEntries()));
    UnidictAdaptersQt::MdictParserQt parser;
    const QString path = QDir(dir.path()).filePath("mdxbridge.mdx");
    QVERIFY(parser.loadDictionary(path));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getSupportedExtensions(), (QStringList{"mdx", "mdd"}));
    QCOMPARE(parser.getDictionaryName(), QString("mdxbridge"));
    QCOMPARE(parser.getDictionaryDescription(), QString("MDX test dictionary"));
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getSourcePath(), QFileInfo(path).canonicalFilePath());
    QCOMPARE(parser.getDictionaryId(), QFileInfo(path).canonicalFilePath().toLower());
    QCOMPARE(parser.getFormatName(), QString("MDict"));

    const UnidictCore::DictionaryEntry hit = parser.lookup("apple");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "apricot", "banana"}));
    QCOMPARE(parser.findSimilar("ap", 1), (QStringList{"apple"}));
    QVERIFY(!parser.findSimilar("ap", 5).isEmpty());

    // 同 json 桥：补 D0 deleting 析构计数
    {
        std::unique_ptr<UnidictCore::DictionaryParser> polymorphic =
            std::make_unique<UnidictAdaptersQt::MdictParserQt>();
        QVERIFY(!polymorphic->getSupportedExtensions().isEmpty());
    }
}

void QtAdaptersTest::q7_stardict_parser_bridge() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    UnidictAdaptersQt::StarDictParserQt missing;
    QVERIFY(!missing.loadDictionary(QDir(dir.path()).filePath("nope.ifo")));
    QVERIFY(!missing.isLoaded());
    QVERIFY(missing.getSourcePath().isEmpty());
    QVERIFY(missing.getDictionaryId().isEmpty());

    QVERIFY(writeStarDictDictionary(dir.path(), "sdbridge", demoEntries()));
    UnidictAdaptersQt::StarDictParserQt parser;
    const QString path = QDir(dir.path()).filePath("sdbridge.ifo");
    QVERIFY(parser.loadDictionary(path));
    QVERIFY(parser.isLoaded());
    QCOMPARE(parser.getSupportedExtensions(), (QStringList{"ifo", "idx", "dict", "dz"}));
    QCOMPARE(parser.getDictionaryName(), QString("sdbridge"));
    QCOMPARE(parser.getDictionaryDescription(), QString("Test dictionary"));
    QCOMPARE(parser.getWordCount(), 3);
    QCOMPARE(parser.getSourcePath(), QFileInfo(path).canonicalFilePath());
    QCOMPARE(parser.getDictionaryId(), QFileInfo(path).canonicalFilePath().toLower());
    QCOMPARE(parser.getFormatName(), QString("StarDict"));

    const UnidictCore::DictionaryEntry hit = parser.lookup("apple");
    QCOMPARE(hit.word, QString("apple"));
    QCOMPARE(hit.definition, QString("fruit"));
    QVERIFY(parser.lookup("missing").definition.isEmpty());

    QCOMPARE(parser.getAllWords(), (QStringList{"apple", "apricot", "banana"}));
    QCOMPARE(parser.findSimilar("b", 1), (QStringList{"banana"}));
    QVERIFY(!parser.findSimilar("b", 5).isEmpty());

    // 同 json 桥：补 D0 deleting 析构计数
    {
        std::unique_ptr<UnidictCore::DictionaryParser> polymorphic =
            std::make_unique<UnidictAdaptersQt::StarDictParserQt>();
        QVERIFY(!polymorphic->getSupportedExtensions().isEmpty());
    }
}

void QtAdaptersTest::q7_ai_service() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // runExternal 是私有的：用临时目录里的脚本当"外部命令"，从
    // translate/grammarCheck 公共入口驱动它的全部出口。
    // （lambda 里不能用 QVERIFY——它展开出裸 return;——失败时返回空串，
    // 由调用侧 QVERIFY(!path.isEmpty()) 接住）
    auto writeScript = [&dir](const QString& name,
                              const QByteArray& body) -> QString {
        const QString path = QDir(dir.path()).filePath(name);
        QFile script(path);
        if (!script.open(QIODevice::WriteOnly)) {
            return {};
        }
        script.write(body);
        script.close();
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                        QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                                        QFileDevice::ExeGroup | QFileDevice::ReadOther |
                                        QFileDevice::ExeOther);
        return path;
    };

    // 外部命令不能用 /bin/cat：本机 coreutils 是 uutils 实现，对 translate
    // 固定附加的 --to 参数直接报错退出——改用忽略全部参数的 cat 脚本
    const QString catSh = writeScript(QStringLiteral("cat.sh"),
                                      QByteArray("#!/bin/sh\ncat\n"));
    QVERIFY(!catSh.isEmpty());

    // 构造期 env 入口：有配置 → 直接作为外部命令
    qputenv("UNIDICT_AI_CMD", catSh.toUtf8());
    UnidictAdaptersQt::AiServiceQt viaEnv;
    QCOMPARE(viaEnv.command(), catSh);
    // P-9 provider 可替换面：有命令 = command + heuristic 兜底链
    QCOMPARE(viaEnv.providerNames(), (QStringList{"command", "heuristic"}));
    qunsetenv("UNIDICT_AI_CMD");

    // 无配置 → 空命令
    UnidictAdaptersQt::AiServiceQt plain;
    QVERIFY(plain.command().isEmpty());
    // 无命令 = 仅 heuristic 一家
    QCOMPARE(plain.providerNames(), (QStringList{"heuristic"}));

    // setCommand/command 读写
    plain.setCommand(catSh);
    QCOMPARE(plain.command(), catSh);
    QCOMPARE(plain.providerNames(), (QStringList{"command", "heuristic"}));

    // 清空命令 → 摘除 command provider（可替换双向成立）
    plain.setCommand(QString());
    QCOMPARE(plain.providerNames(), (QStringList{"heuristic"}));
    QCOMPARE(plain.command(), QString());
    plain.setCommand(catSh);

    // 外部命令三结局（stdout 命中 / stderr 兜底 / 信号击杀）都靠
    // shebang 脚本——QProcess 在 Windows 上经 CreateProcess 拉不起
    // .sh（无关联执行器 → FailedToStart），这三条只由 POSIX 作业覆盖；
    // Windows 侧断言该环境下实际发生的形态：命令起不来 → 回落启发式
#ifndef Q_OS_WIN
    // 外部命令命中：cat 把 stdin 原样回显 → translate/grammarCheck 直接采用
    QCOMPARE(plain.translate("hello", "zh"), QString("hello"));
    QCOMPARE(plain.grammarCheck("some text"), QString("some text"));

    // 只有 stderr 输出 → 读 stderr 兜底
    const QString stderrSh = writeScript(QStringLiteral("stderr.sh"),
                                         QByteArray("#!/bin/sh\necho boom >&2\n"));
    QVERIFY(!stderrSh.isEmpty());
    plain.setCommand(stderrSh);
    QCOMPARE(plain.translate("hello", "zh"), QString("boom"));

    // 进程被信号杀死 → 非 NormalExit → 外部输出作废，走回落启发式
    const QString crashSh = writeScript(QStringLiteral("crash.sh"),
                                        QByteArray("#!/bin/sh\nkill -9 $$\n"));
    QVERIFY(!crashSh.isEmpty());
    plain.setCommand(crashSh);
    QCOMPARE(plain.translate("hello", "zh"), QString("[Mock Translation to Chinese]\nhello"));
#else
    QCOMPARE(plain.translate("hello", "zh"),
             QString("[Mock Translation to Chinese]\nhello"));
#endif

    // 启动失败 → 外部输出为空 → 回落启发式
    plain.setCommand(QStringLiteral("unidict_no_such_cmd_xyz"));
    QCOMPARE(plain.translate("hi", "zh"), QString("[Mock Translation to Chinese]\nhi"));
    // 失败无感：坏命令仍在链上占位（providerNames 反映配置态），
    // 但对上层零报错——静默让位 heuristic
    QCOMPARE(plain.providerNames(), (QStringList{"command", "heuristic"}));

    // 无命令：runExternal 空返回，translate/grammarCheck 走回落启发式
    UnidictAdaptersQt::AiServiceQt none;
    QCOMPARE(none.translate("hi", "zh"), QString("[Mock Translation to Chinese]\nhi"));
    QCOMPARE(none.translate("hi", "en"), QString("[Mock Translation to English]\nhi"));
    QCOMPARE(none.translate("hi", "fr"), QString("[Mock Translation to fr]\nhi"));
    QCOMPARE(none.grammarCheck(QStringLiteral("   ")), QString("Input is empty."));
    QCOMPARE(none.grammarCheck(QStringLiteral("hello world")),
             QString("Suggestions:\n- Consider ending the sentence with punctuation.\n"
                     "- Sentence may start with a capital letter."));
    QCOMPARE(none.grammarCheck(QStringLiteral("Hello world")),
             QString("Suggestions:\n- Consider ending the sentence with punctuation."));
    QCOMPARE(none.grammarCheck(QStringLiteral("hello world.")),
             QString("Suggestions:\n- Sentence may start with a capital letter."));
    QCOMPARE(none.grammarCheck(QStringLiteral("Hello world.")),
             QString("No obvious issues (mock)."));

    // AI 语境造句（roadmap AI sentence generation）：无命令 → 启发式
    // mock 逐字断言；空白词 → 空串（链上无输出）；有命令 → sentences
    // 子命令与 translate/grammar 同管道（cat 回显即命中）
    QCOMPARE(none.generateSentences(QStringLiteral("hello")),
             QString("[Mock sentences]\n"
                     "1. She said \"hello\" when she opened the door.\n"
                     "2. He greeted everyone with a friendly \"hello\".\n"
                     "3. Try using \"hello\" in your next conversation."));
    QCOMPARE(none.generateSentences(QStringLiteral("  ")), QString());
    plain.setCommand(catSh);
#ifndef Q_OS_WIN
    QCOMPARE(plain.generateSentences(QStringLiteral("hello")), QString("hello"));
#else
    QCOMPARE(plain.generateSentences(QStringLiteral("hello")),
             QString("[Mock sentences]\n"
                     "1. She said \"hello\" when she opened the door.\n"
                     "2. He greeted everyone with a friendly \"hello\".\n"
                     "3. Try using \"hello\" in your next conversation."));
#endif
    plain.setCommand(QString());
}

void QtAdaptersTest::q7_clipboard() {
    UnidictAdaptersQt::ClipboardQt cb;
    cb.setText(QStringLiteral("unidict-q7"));
    QCOMPARE(cb.text(), QString("unidict-q7"));
    cb.setText(QString());
    QCOMPARE(cb.text(), QString());
}

void QtAdaptersTest::q7_settings() {
    UnidictAdaptersQt::SettingsQt settings;
    settings.setBool("q7/flag", true);
    QCOMPARE(settings.getBool("q7/flag"), true);
    QCOMPARE(settings.getBool("q7/absent", true), true);
    QCOMPARE(settings.getBool("q7/absent"), false);

    settings.setString("q7/word", QStringLiteral("hello"));
    QCOMPARE(settings.getString("q7/word"), QString("hello"));
    QCOMPARE(settings.getString("q7/absent", QStringLiteral("dflt")), QString("dflt"));
    QVERIFY(settings.getString("q7/absent").isEmpty());

    settings.setInt("q7/num", 42);
    QCOMPARE(settings.getInt("q7/num"), 42);
    QCOMPARE(settings.getInt("q7/absent", 7), 7);
    QCOMPARE(settings.getInt("q7/absent"), 0);

    // set* 都已 sync 落盘：新实例（重新打开 ini）仍能读到
    UnidictAdaptersQt::SettingsQt reopened;
    QCOMPARE(reopened.getInt("q7/num"), 42);
    QCOMPARE(reopened.getBool("q7/flag"), true);
}

QTEST_MAIN(QtAdaptersTest)
#include "qt_adapters_test.moc"
