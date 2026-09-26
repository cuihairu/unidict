// 词条呈现与 .mdd 资源闭环（docs/pro_dictionary_gap.md P0 ①）的行为测试。
//
// 覆盖 LookupAdapter 这一层此前是"实现了但没接线"的部分：
//   - sanitizeHtml 过去是手搓正则的白名单外实现，绕过 core/std 的逐 token
//     白名单；这里钉住"该挡的挡住 + 协议白名单生效 + 词条正文不丢"。
//   - rewriteResourceUrls 过去是空壳（Q_UNUSED 直接原样返回），.mdd 图片/
//     音频在 QML 端永远加载不出来；这里用真 .mdx + 真 .mdd 走通
//     "推导 .mdd → 解析 → 落缓存 → 改写成 file://" 全链路。
//   - presentEntry 是一站式管线，钉住三步的**顺序**（清洗会剔 src，所以
//     资源重写必须最后做）以及资源清单里 found 的语义。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include "core/unidict_core.h"
#include "mdict_fixture.h"
#include "qmlui/lookup_adapter.h"

using namespace UnidictCore;
using namespace UnidictMdictFixture;

class EntryPresentationTest : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // 清洗：白名单与协议
    void sanitize_keepsDefinitionBody();
    void sanitize_dropsScriptAndHandlers();
    void sanitize_dropsJavascriptAndDataHtmlUrls();
    void sanitize_dropsNonWhitelistedTags();
    void sanitize_isIdempotent();

    // 纯文本抽取
    void extractText_stripsTagsAndEntities();

    // 交叉引用链接
    void rewriteLinks_mapsEntryAndBwordSchemes();
    void rewriteLinks_inlinesAtAtLinkMarker();

    // .mdd 资源：真实文件
    void rewriteResourceUrls_resolvesImageFromSiblingMdd();
    void rewriteResourceUrls_resolvesAudioAndNormalizesDotSlash();
    void rewriteResourceUrls_keepsDataAndRemoteUrls();
    void rewriteResourceUrls_leavesMissUnchanged();
    void rewriteResourceUrls_unknownDictionaryOrNoMddIsNoop();
    void rewriteResourceUrls_isIdempotent();

    // 资源直取三件套
    void resourceAccessors_reportExistenceDataAndUrl();
    void resourceAccessors_emptyArgsAndMisses();

    // 一站式管线
    void presentEntry_returnsHtmlTextAndManifest();
    void presentEntry_manifestFlagsMissingResource();
    void presentEntry_runsLinkRewriteBeforeResourceRewrite();
    void presentEntry_withoutP0FallsBackToInput();

    // 多词典 FIFO
    void ensureMdd_keepsSeveralDictionariesMounted();

private:
    // 写一份 .mdx + 同名 .mdd，返回词典 id（DictionaryManager 生成的）
    QString makeDictWithMdd(const QString& dir, const QString& name,
                            const QList<QPair<QString, QByteArray>>& resources);
    QString loadedIdFor(const QString& filePath) const;

    QTemporaryDir m_dir;
};

void EntryPresentationTest::init() {
    DictionaryManager::instance().clearDictionaries();
    qunsetenv("UNIDICT_DICTS");
    // 资源缓存目录落在 QStandardPaths::CacheLocation 下，测试之间共用；
    // 换一份随机子目录，免得读到上一轮遗留的文件而"假通过"。
    QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .removeRecursively();
}

void EntryPresentationTest::cleanup() {
    DictionaryManager::instance().clearDictionaries();
    QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .removeRecursively();
}

QString EntryPresentationTest::loadedIdFor(const QString& filePath) const {
    for (const auto& info :
         DictionaryManager::instance().getLoadedDictionaryInfos()) {
        if (info.filePath == filePath) {
            return info.id;
        }
    }
    return {};
}

QString EntryPresentationTest::makeDictWithMdd(
    const QString& dir, const QString& name,
    const QList<QPair<QString, QByteArray>>& resources) {
    if (!writeMdxDictionary(dir, name, {{QStringLiteral("hello"),
                                         QStringLiteral("<p>hi</p>")}})) {
        return {};
    }
    if (!resources.isEmpty() &&
        !writeMddResource(dir, name, resources)) {
        return {};
    }
    const QString path = QDir(dir).filePath(name + QStringLiteral(".mdx"));
    if (!DictionaryManager::instance().addDictionary(path)) {
        return {};
    }
    return loadedIdFor(path);
}

// ===========================================================================
// 清洗
// ===========================================================================

void EntryPresentationTest::sanitize_keepsDefinitionBody() {
    LookupAdapter a;
    const QString out = a.sanitizeHtml(
        QStringLiteral("<p>hello <b>world</b></p>"));
    QVERIFY2(out.contains(QStringLiteral("<b>world</b>")),
             qPrintable(QStringLiteral("标签被误删: %1").arg(out)));
    QVERIFY(out.contains(QStringLiteral("hello")));
}

void EntryPresentationTest::sanitize_dropsScriptAndHandlers() {
    LookupAdapter a;
    // 这些过去用手搓正则时会漏掉一部分：<script/src=x> 没有闭合标签，
    // 正则的 <script[^>]*>.*?</script> 匹配不上；onerror 不带引号也漏。
    const QString out = a.sanitizeHtml(
        QStringLiteral("<p onclick=\"steal()\">x</p>"
                       "<script/src=\"http://evil/x.js\"></script>"
                       "<iframe src=\"http://evil\"></iframe>"
                       "<div onmouseover=steal()>y</div>"));
    QVERIFY2(!out.contains(QStringLiteral("script")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("iframe")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("onclick")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("onmouseover")), qPrintable(out));
    // 正文还在
    QVERIFY(out.contains(QStringLiteral("x")));
    QVERIFY(out.contains(QStringLiteral("y")));
}

void EntryPresentationTest::sanitize_dropsJavascriptAndDataHtmlUrls() {
    LookupAdapter a;
    // 协议白名单：javascript: 与 data:text/html 必须被剔掉。
    // 旧的正则实现没有任何协议校验，这两种是原样进富文本的。
    const QString out = a.sanitizeHtml(
        QStringLiteral("<a href=\"javascript:alert(1)\">a</a>"
                       "<a href=\"JaVaScRiPt:alert(2)\">b</a>"
                       "<a href=\"data:text/html;base64,PHNjcmlwdD4=\">c</a>"
                       "<a href=\"https://example.com/\">d</a>"));
    QVERIFY2(!out.contains(QStringLiteral("javascript:"), Qt::CaseInsensitive),
             qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("data:text/html")), qPrintable(out));
    // 正常外链保留
    QVERIFY2(out.contains(QStringLiteral("https://example.com/")), qPrintable(out));
}

void EntryPresentationTest::sanitize_dropsNonWhitelistedTags() {
    LookupAdapter a;
    // <style>/<form>/<base> 过去没有标签白名单，能整个进 QML 富文本。
    const QString out = a.sanitizeHtml(
        QStringLiteral("<style>body{display:none}</style>"
                       "<form action=\"http://evil\"><input></form>"
                       "<base href=\"http://evil/\">"
                       "<p>keep</p>"));
    QVERIFY2(!out.contains(QStringLiteral("<style")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("<form")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("<base")), qPrintable(out));
    QVERIFY(out.contains(QStringLiteral("keep")));
}

void EntryPresentationTest::sanitize_isIdempotent() {
    LookupAdapter a;
    const QString once =
        a.sanitizeHtml(QStringLiteral("<p>a <b>b</b> <img src=\"x.png\"></p>"));
    // 清洗结果再过一次白名单应当不变——否则 QML 每次刷新词条都会再削一层
    const QString twice = a.sanitizeHtml(once);
    QCOMPARE(twice, once);
}

// ===========================================================================
// 纯文本
// ===========================================================================

void EntryPresentationTest::extractText_stripsTagsAndEntities() {
    LookupAdapter a;
    const QString out = a.extractTextFromHtml(
        QStringLiteral("<p>Tom &amp; <b>Jerry</b>&nbsp;&lt;3</p>"));
    // 标签剥了，但实体的**内容**要留下：&lt; 解成 <（"I <3" 里的 <），
    // 不能因为解出来的字符长得像标签开头就当标签再剥一次
    QVERIFY2(!out.contains(QStringLiteral("<b>")), qPrintable(out));
    QVERIFY2(!out.contains(QStringLiteral("</b>")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("<3")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("Tom")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("Jerry")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("&")), qPrintable(out));
}

// ===========================================================================
// 交叉引用
// ===========================================================================

void EntryPresentationTest::rewriteLinks_mapsEntryAndBwordSchemes() {
    LookupAdapter a;
    const QString out = a.rewriteCrossReferenceLinks(
        QStringLiteral("<a href=\"entry://apple\">a</a> "
                       "<a href=\"bword://orange\">b</a>"),
        QStringLiteral("dictX"));
    QVERIFY2(out.contains(QStringLiteral("unidict://lookup?word=apple")),
             qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("unidict://lookup?word=orange")),
             qPrintable(out));
}

void EntryPresentationTest::rewriteLinks_inlinesAtAtLinkMarker() {
    LookupAdapter a;
    // @@@LINK= 是 MDX 的纯文本交叉引用标记，就地换成目标词
    const QString out = a.rewriteCrossReferenceLinks(
        QStringLiteral("see @@@LINK=apple for more"), QStringLiteral("dictX"));
    QVERIFY2(!out.contains(QStringLiteral("@@@LINK")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("apple")), qPrintable(out));
}

// ===========================================================================
// .mdd 资源
// ===========================================================================

void EntryPresentationTest::rewriteResourceUrls_resolvesImageFromSiblingMdd() {
    const QByteArray png = fakePng();
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("picbook"),
        {{QStringLiteral("pic/apple.png"), png}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QString out = a.rewriteResourceUrls(
        QStringLiteral("<img src=\"pic/apple.png\">"), id);

    // 过去这里是 Q_UNUSED 直接原样返回，.mdd 里的图永远出不来
    QVERIFY2(!out.contains(QStringLiteral("src=\"pic/apple.png\"")),
             qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("src=\"file://")), qPrintable(out));

    // 指向的缓存文件必须真实存在，且字节与 .mdd 里的一致
    const QRegularExpression re(QStringLiteral("src=\"([^\"]+)\""));
    const QString url = re.match(out).captured(1);
    const QString local = QUrl(url).toLocalFile();
    QVERIFY2(QFile::exists(local), qPrintable(QStringLiteral("缓存文件不存在: %1").arg(local)));
    QFile f(local);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), png);
}

void EntryPresentationTest::rewriteResourceUrls_resolvesAudioAndNormalizesDotSlash() {
    const QByteArray mp3("\x49\x44\x33\x04\x00\x00\x00\x00\x00\x00\x54\x59", 12);
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("soundbook"),
        {{QStringLiteral("snd/hello.mp3"), mp3}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    // "./" 前缀 .mdd 里没有（键是 snd/hello.mp3），归一后应命中
    const QString out = a.rewriteResourceUrls(
        QStringLiteral("<audio src=\"./snd/hello.mp3\"></audio>"), id);
    QVERIFY2(out.contains(QStringLiteral("file://")), qPrintable(out));

    // 音频与图片走同一套键解析，source 子标签也算
    const QString out2 = a.rewriteResourceUrls(
        QStringLiteral("<video><source src=\"/snd/hello.mp3\"></video>"), id);
    QVERIFY2(out2.contains(QStringLiteral("file://")), qPrintable(out2));
}

void EntryPresentationTest::rewriteResourceUrls_keepsDataAndRemoteUrls() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("mixbook"),
        {{QStringLiteral("pic/a.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QString html =
        QStringLiteral("<img src=\"data:image/png;base64,iVBORw0KGgo=\">"
                       "<img src=\"https://example.com/remote.png\">"
                       "<img src=\"pic/a.png\">");
    const QString out = a.rewriteResourceUrls(html, id);
    QVERIFY2(out.contains(QStringLiteral("data:image/png")), qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("https://example.com/remote.png")),
             qPrintable(out));
    QVERIFY2(out.contains(QStringLiteral("file://")), qPrintable(out));
}

void EntryPresentationTest::rewriteResourceUrls_leavesMissUnchanged() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("partialbook"),
        {{QStringLiteral("pic/have.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    // .mdd 里没有的键：原样保留（改成空 src 等于把图彻底抹掉，比留个
    // 破图更难排查），由 presentEntry 清单里的 found=false 告诉 UI 兜底。
    const QString out = a.rewriteResourceUrls(
        QStringLiteral("<img src=\"pic/missing.png\">"), id);
    QCOMPARE(out, QStringLiteral("<img src=\"pic/missing.png\">"));
}

void EntryPresentationTest::rewriteResourceUrls_unknownDictionaryOrNoMddIsNoop() {
    const QString html = QStringLiteral("<img src=\"pic/a.png\">");

    // 词典 id 瞎编
    LookupAdapter a;
    QCOMPARE(a.rewriteResourceUrls(html, QStringLiteral("nope")), html);
    // 空 id
    QCOMPARE(a.rewriteResourceUrls(html, QString()), html);
    // 空 HTML
    QVERIFY(a.rewriteResourceUrls(QString(), QStringLiteral("nope")).isEmpty());

    // 词典在，但同目录没有 .mdd
    const QString id = makeDictWithMdd(m_dir.path(), QStringLiteral("nomdd"), {});
    QVERIFY2(!id.isEmpty(), "词典没装上");
    QCOMPARE(a.rewriteResourceUrls(html, id), html);
}

void EntryPresentationTest::rewriteResourceUrls_isIdempotent() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("twicebook"),
        {{QStringLiteral("pic/a.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QString once = a.rewriteResourceUrls(
        QStringLiteral("<img src=\"pic/a.png\">"), id);
    // 第二次不能再改：file:// 已在排除列表里，否则会把缓存路径当资源键
    // 去 .mdd 里查（查不到，侥幸不坏；查到别的图就串图了）
    const QString twice = a.rewriteResourceUrls(once, id);
    QCOMPARE(twice, once);
}

// ===========================================================================
// 资源直取
// ===========================================================================

void EntryPresentationTest::resourceAccessors_reportExistenceDataAndUrl() {
    const QByteArray png = fakePng(64);
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("accbook"),
        {{QStringLiteral("pic/a.png"), png}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    QVERIFY(a.hasDictionaryResource(id, QStringLiteral("pic/a.png")));
    QCOMPARE(a.loadDictionaryResourceData(id, QStringLiteral("pic/a.png")), png);

    const QString url = a.dictionaryResourceUrl(id, QStringLiteral("pic/a.png"));
    QVERIFY2(url.startsWith(QStringLiteral("file://")), qPrintable(url));
    QFile f(QUrl(url).toLocalFile());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QCOMPARE(f.readAll(), png);

    // 未命中：三条都给出"没有"，且 URL 是空串而不是 file:// （空路径）
    QVERIFY(!a.hasDictionaryResource(id, QStringLiteral("pic/none.png")));
    QVERIFY(a.loadDictionaryResourceData(id, QStringLiteral("pic/none.png")).isEmpty());
    QVERIFY(a.dictionaryResourceUrl(id, QStringLiteral("pic/none.png")).isEmpty());
}

void EntryPresentationTest::resourceAccessors_emptyArgsAndMisses() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("emptyargs"),
        {{QStringLiteral("pic/a.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    QVERIFY(!a.hasDictionaryResource(QString(), QStringLiteral("pic/a.png")));
    QVERIFY(!a.hasDictionaryResource(id, QString()));
    QVERIFY(a.loadDictionaryResourceData(QString(), QString()).isEmpty());
    QVERIFY(a.loadDictionaryResourceData(id, QString()).isEmpty());
    QVERIFY(a.dictionaryResourceUrl(QString(), QStringLiteral("x")).isEmpty());
    // 未知词典
    QVERIFY(!a.hasDictionaryResource(QStringLiteral("nope"),
                                     QStringLiteral("pic/a.png")));
}

// ===========================================================================
// 一站式管线
// ===========================================================================

void EntryPresentationTest::presentEntry_returnsHtmlTextAndManifest() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("pipebook"),
        {{QStringLiteral("pic/a.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QVariantMap out = a.presentEntry(
        QStringLiteral("<p>hello <b>world</b></p>"
                       "<img src=\"pic/a.png\">"
                       "<a href=\"entry://apple\">see</a>"),
        id);

    const QString html = out.value(QStringLiteral("html")).toString();
    const QString text = out.value(QStringLiteral("text")).toString();
    const QVariantList refs = out.value(QStringLiteral("resources")).toList();

    // 清洗后的富文本：正文标签在，资源已解析成交给 QML 的 file://，
    // 交叉引用已转成站内 scheme
    QVERIFY2(html.contains(QStringLiteral("<b>world</b>")), qPrintable(html));
    QVERIFY2(html.contains(QStringLiteral("file://")), qPrintable(html));
    QVERIFY2(html.contains(QStringLiteral("unidict://lookup?word=apple")),
             qPrintable(html));

    // 纯文本回退
    QVERIFY2(text.contains(QStringLiteral("world")), qPrintable(text));
    QVERIFY2(!text.contains(QStringLiteral("<b>")), qPrintable(text));

    // 资源清单
    QCOMPARE(refs.size(), 1);
    const QVariantMap ref = refs.first().toMap();
    QCOMPARE(ref.value(QStringLiteral("key")).toString(),
             QStringLiteral("pic/a.png"));
    QCOMPARE(ref.value(QStringLiteral("found")).toBool(), true);
    QVERIFY(ref.value(QStringLiteral("url")).toString().startsWith(
        QStringLiteral("file://")));
}

void EntryPresentationTest::presentEntry_manifestFlagsMissingResource() {
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("missbook"),
        {{QStringLiteral("pic/have.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QVariantMap out = a.presentEntry(
        QStringLiteral("<img src=\"pic/have.png\"><img src=\"pic/gone.png\">"),
        id);
    const QVariantList refs = out.value(QStringLiteral("resources")).toList();
    QCOMPARE(refs.size(), 2);
    QCOMPARE(refs.at(0).toMap().value(QStringLiteral("found")).toBool(), true);
    // 缺失的这条：found=false、url 空、但 html 里原样保留
    const QVariantMap miss = refs.at(1).toMap();
    QCOMPARE(miss.value(QStringLiteral("key")).toString(),
             QStringLiteral("pic/gone.png"));
    QCOMPARE(miss.value(QStringLiteral("found")).toBool(), false);
    QVERIFY(miss.value(QStringLiteral("url")).toString().isEmpty());
    QVERIFY2(out.value(QStringLiteral("html")).toString()
                 .contains(QStringLiteral("pic/gone.png")),
             "缺失资源不应被抹掉");
}

void EntryPresentationTest::presentEntry_runsLinkRewriteBeforeResourceRewrite() {
    // 顺序是契约：清洗按协议白名单剔 src，所以资源重写必须最后一步。
    // 若顺序反了（先重写资源再清洗），填进去的 file:// 会被清洗器当未知
    // 协议剥掉，图又没了。
    const QString id = makeDictWithMdd(
        m_dir.path(), QStringLiteral("orderbook"),
        {{QStringLiteral("pic/a.png"), fakePng()}});
    QVERIFY2(!id.isEmpty(), "词典没装上");

    LookupAdapter a;
    const QString html = a.presentEntry(
        QStringLiteral("<img src=\"pic/a.png\">"), id)
                             .value(QStringLiteral("html")).toString();
    QVERIFY2(html.contains(QStringLiteral("file://")),
             qPrintable(QStringLiteral("file:// 资源被清洗掉了: %1").arg(html)));
}

void EntryPresentationTest::presentEntry_withoutP0FallsBackToInput() {
    // m_p0 为空时（构造失败/未来重构）不能崩，至少原样返回
    LookupAdapter a;
    const QVariantMap out = a.presentEntry(QStringLiteral("<p>x</p>"),
                                            QStringLiteral("nope"));
    QVERIFY(out.contains(QStringLiteral("html")));
    QVERIFY(out.contains(QStringLiteral("text")));
    QVERIFY(out.value(QStringLiteral("resources")).toList().isEmpty());
}

// ===========================================================================
// 多词典挂载
// ===========================================================================

void EntryPresentationTest::ensureMdd_keepsSeveralDictionariesMounted() {
    // 多词典对照是查词软件的主场景：A/B 两个词典的同名图 key（pic/a.png）
    // 内容不同，来回切必须各自命中自己的，不能串。
    QList<QString> ids;
    for (int i = 0; i < 6; ++i) {  // 超过 FIFO 上限(4)，逼出淘汰路径
        const QString name = QStringLiteral("multi%1").arg(i);
        const QString id = makeDictWithMdd(
            m_dir.path(), name,
            {{QStringLiteral("pic/a.png"),
              QByteArray("IMG") + QByteArray::number(i)}});
        QVERIFY2(!id.isEmpty(), qPrintable(QStringLiteral("词典 %1 没装上").arg(name)));
        ids.append(id);
    }

    LookupAdapter a;
    for (int i = 0; i < ids.size(); ++i) {
        const QByteArray want = QByteArray("IMG") + QByteArray::number(i);
        QCOMPARE(a.loadDictionaryResourceData(ids.at(i),
                                              QStringLiteral("pic/a.png")),
                 want);
    }
    // 最早的已被淘汰，资源依然可取（会重新挂载）
    QCOMPARE(a.loadDictionaryResourceData(ids.first(),
                                          QStringLiteral("pic/a.png")),
             QByteArray("IMG0"));
}

QTEST_MAIN(EntryPresentationTest)
#include "entry_presentation_test.moc"
