// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 1/6 批）：
// 场景清单与调用顺序见 click_audit_sections.h，共享面见 click_audit_helpers.h。

#include "click_audit_sections.h"
#include "click_audit_helpers.h"
#include "../theme.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>

using namespace ui_audit;
using UnidictQml::Theme;

using UnidictQml::Theme;

void registerAuditContext(QQmlApplicationEngine& e, AuditCtx& c) {
        e.rootContext()->setContextProperty("Theme", &Theme::instance());
        e.rootContext()->setContextProperty("lookup", &c.adapter);
        e.rootContext()->setContextProperty("platform", &c.platform);
        e.rootContext()->setContextProperty("voice", &c.voice);
        e.rootContext()->setContextProperty("vocab", &c.vocab);
        e.rootContext()->setContextProperty("fulltext", &c.fulltext);
        e.rootContext()->setContextProperty("sync", &c.sync);
        e.rootContext()->setContextProperty("syncManager", &c.syncManager);
        e.rootContext()->setContextProperty(
            "documentsPath",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        e.rootContext()->setContextProperty("ai", &c.ai);
        e.rootContext()->setContextProperty("clip", &c.clip);
        e.rootContext()->setContextProperty("settings", &c.settings);
        e.rootContext()->setContextProperty("startup", &c.startup);
        e.rootContext()->setContextProperty("tray", &c.tray);
        e.rootContext()->setContextProperty("MobileUtils", &c.mobileUtils);
        e.rootContext()->setContextProperty("platformName",
                                            QGuiApplication::platformName());
        e.rootContext()->setContextProperty("isMobile", false);
        e.rootContext()->setContextProperty("screenWidth", 1280);
        e.rootContext()->setContextProperty("screenHeight", 800);
        e.rootContext()->setContextProperty("screenDpi", 96.0);
}

void audit_s0(AuditCtx& c) {
// ---- S0 词典卡头渲染管线（decorateHtml 主题拼接，BUG-010 之一） ----
QVariant htmlVar;
QVariant htmlArg0(QString("<pre>int x;</pre>")), htmlArg1(false);
const bool htmlInvoked = QMetaObject::invokeMethod(
    c.win, "decorateHtml", Q_RETURN_ARG(QVariant, htmlVar),
    Q_ARG(QVariant, htmlArg0), Q_ARG(QVariant, htmlArg1));
const QString html = htmlVar.toString();
qInfo("HTMLRAW invoked=%d %s", int(htmlInvoked), qPrintable(html.left(200)));
audit(html.contains("background:#") && html.contains("color:#")
          && !html.contains("+ Theme."),
      "decorateHtml <pre>/外层 div 主题样式拼接",
      "含 background:#、color:# 主题色且无字面 \"+ Theme.\" 残留");

openWord(c.win, "hello");
audit(statusText(c.win).startsWith("找到"),
      "openWord(hello) 聚合查询落状态行",
      QString("statusText=%1").arg(statusText(c.win)));
audit(item(c.win, "phonSpeakerBr") != nullptr && item(c.win, "phonSpeakerUs") != nullptr,
      "音标行英/美喇叭存在（fixture 词头带 英/美 音标）",
      "phonSpeakerBr 与 phonSpeakerUs 均可见于词条卡");

}

void audit_s1(AuditCtx& c) {
// ---- S1 朗读点击反馈（BUG-010：点击必有状态行反馈） ----
c.hasTts = c.voice.hasLocalTts();
clickItem(c.win, item(c.win, "phonSpeakerBr"));
audit(c.hasTts ? statusText(c.win).startsWith("正在朗读（英音）")
             : statusText(c.win).contains("本地语音不可用"),
      "英音喇叭点击反馈",
      QString("hasLocalTts=%1, statusText=%2")
          .arg(c.hasTts ? "true" : "false", statusText(c.win)));
clickItem(c.win, item(c.win, "phonSpeakerUs"));
audit(c.hasTts ? statusText(c.win).startsWith("正在朗读（美音）")
             : statusText(c.win).contains("本地语音不可用"),
      "美音喇叭点击反馈（与英音分口音路径）",
      QString("statusText=%1").arg(statusText(c.win)));

}

void audit_s2(AuditCtx& c) {
// ---- S2 生词本/复制轻入口 ----
clickItem(c.win, item(c.win, "vocabAction"));
audit(statusText(c.win).contains("已加入生词本"), "生词本轻入口点击",
      QString("statusText=%1").arg(statusText(c.win)));
clickItem(c.win, item(c.win, "copyAction"));
audit(statusText(c.win).contains("已复制释义"), "复制轻入口点击（剪贴板写入）",
      QString("statusText=%1").arg(statusText(c.win)));

}

void audit_s3(AuditCtx& c) {
// ---- S3 笔记弹层开关 ----
clickItem(c.win, item(c.win, "noteAction"));
settle(c.win);
QObject* notePopup = popupObj(c.win, "notePopup");
audit(popupVisible(notePopup), "笔记轻入口打开笔记弹层",
      "notePopup visible");
if (popupVisible(notePopup)) {
    clickItem(c.win, item(c.win, "noteCancelButton"));
    // 关闭过渡期间 visible 仍为 true 且 modal overlay 未释放（会吃掉
    // 后续所有点击）——settle 后再断言
    settle(c.win);
    audit(!popupVisible(notePopup), "笔记弹层取消按钮点击收起",
          "notePopup hidden after cancel");
}

}

void audit_s4(AuditCtx& c) {
// ---- S4 内容 tab：例句（重组加载反馈 + 喇叭读整句 + 词链跳转） ----
if (!item(c.win, "contentTab_examples")) {
    // 子树转储（仅定位失败时）：确认委托是否实例化、objectName 是否为空
    QQuickItem* paneRoot = item(c.win, "entryResultsPane");
    int dumped = 0;
    std::function<void(QQuickItem*, int)> walk = [&](QQuickItem* it, int depth) {
        if (!it || depth > 4 || dumped > 80) return;
        ++dumped;
        qInfo("DUMP %*s'%s' %s", depth * 2, "",
              qPrintable(it->objectName()), it->metaObject()->className());
        for (auto* c : it->childItems()) walk(c, depth + 1);
    };
    walk(paneRoot, 0);
}
audit(item(c.win, "contentTab_examples") != nullptr, "内容 tab 元素定位",
      "contentTab_examples found");
clickItem(c.win, item(c.win, "contentTab_examples"));
settle(c.win);
const QVariantMap tabData = paneProp(c.win, "tabData").toMap();
const int exampleCount = tabData.value("examples").toList().size();
audit(exampleCount >= 3 && statusText(c.win).contains("共 "),
      "例句 tab 点击（懒取+加载状态行收口）",
      QString("examples=%1 条, statusText=%2")
          .arg(exampleCount).arg(statusText(c.win)));
clickItem(c.win, item(c.win, "exampleSpeaker"));
// 点击必有反馈：有本地 TTS 时落「正在朗读例句」，无 TTS 时落明示
// 换路的「本地语音不可用」（后者会覆盖前者落状态行，二者均来自
// 该次点击的处理链，任一出现即接线成立）
const QString spStatus = statusText(c.win);
audit(spStatus.contains(QStringLiteral("正在朗读例句"))
          || spStatus.contains(QStringLiteral("本地语音不可用")),
      "例句喇叭点击（读整句+反馈）",
      QString("statusText=%1").arg(spStatus));
// 接线门：pane.voice 未声明未接线时该点击只落状态行、朗读哑掉（日志报
// speakText of undefined），上一条状态行断言测不出，须钉住 voice 属性
// 指向真实 TTS 服务
const QObject* voiceSvc = paneProp(c.win, "voice").value<QObject*>();
audit(voiceSvc != nullptr,
      "例句喇叭接线（pane.voice 指向 TTS 服务）",
      QString("voice=%1").arg(voiceSvc ? "wired" : "null"));
const QVariantList exampleRows =
    c.adapter.fullTextLookup(QStringLiteral("hello"), 20);
// 非重言证据：委托 childItems 序≠model 序，从全部词链委托里挑第一
// 个「非当前词头」的（当前词头链点击不产生状态差，PASS 无意义）。
// 委托 text 形如 "word · 词典名"，词在 " · " 之前
QList<QQuickItem*> exLinks;
visualFindAll(c.win->contentItem(), "exampleWordLink", exLinks);
QString exampleWord0;
QQuickItem* exLinkTarget = nullptr;
for (auto* link : exLinks) {
    const QString text = link->property("text").toString();
    const QString w = text.section(QStringLiteral(" · "), 0, 0);
    if (w != QStringLiteral("hello") && !w.isEmpty()) {
        exampleWord0 = w;
        exLinkTarget = link;
        break;
    }
}
if (!exampleWord0.isEmpty() && exLinkTarget) {
    clickItem(c.win, exLinkTarget);
    audit(c.win->property("currentWord").toString() == exampleWord0,
          "例句行词链点击跳转",
          QString("currentWord=%1, 期望=%2")
              .arg(c.win->property("currentWord").toString(), exampleWord0));
} else {
    audit(false, "例句行词链点击跳转", "fixture 例句无非当前词头行（检查词典）");
}

}

void audit_s5(AuditCtx& c) {
// ---- S5 词组 tab ----
openWord(c.win, "hello");
clickItem(c.win, item(c.win, "contentTab_phrases"));
settle(c.win);
const QVariantMap td2 = paneProp(c.win, "tabData").toMap();
const QString phraseWord0 = td2.value("phrases").toList().value(0)
                                .toMap().value("word").toString();
audit(!phraseWord0.isEmpty(), "词组 tab 点击懒取数据",
      QString("phrases[0].word=%1").arg(phraseWord0));
clickItem(c.win, item(c.win, "phraseLink"));
audit(c.win->property("currentWord").toString() == phraseWord0,
      "词组词链点击跳转",
      QString("currentWord=%1, 期望=%2")
          .arg(c.win->property("currentWord").toString(), phraseWord0));

}

void audit_s6(AuditCtx& c) {
// ---- S6 近义联想 tab ----
openWord(c.win, "hello");
clickItem(c.win, item(c.win, "contentTab_related"));
settle(c.win);
const QVariantMap td3 = paneProp(c.win, "tabData").toMap();
const QString relatedWord0 = td3.value("related").toList().value(0)
                                 .toMap().value("word").toString();
audit(!relatedWord0.isEmpty(), "近义联想 tab 点击懒取数据",
      QString("related[0].word=%1").arg(relatedWord0));
clickItem(c.win, item(c.win, "relatedLink"));
audit(c.win->property("currentWord").toString() == relatedWord0,
      "近义词链接点击跳转",
      QString("currentWord=%1, 期望=%2")
          .arg(c.win->property("currentWord").toString(), relatedWord0));

}

void audit_s7(AuditCtx& c) {
// ---- S7 全文检索 tab（重组路径，加载状态行） ----
openWord(c.win, "hello");
clickItem(c.win, item(c.win, "contentTab_fulltext"));
settle(c.win);
const QVariantMap td4 = paneProp(c.win, "tabData").toMap();
audit(td4.value("fulltext").toList().size() >= 3
          && statusText(c.win).contains("共 "),
      "全文检索 tab 点击（懒取+状态行）",
      QString("fulltext=%1 条, statusText=%2")
          .arg(td4.value("fulltext").toList().size()).arg(statusText(c.win)));

}

void audit_s8(AuditCtx& c) {
// ---- S8 词典 tab：分组折叠 ----
clickItem(c.win, item(c.win, "contentTab_dict"));
flushEvents();
QQuickItem* header0 = item(c.win, "groupHeader_0");
audit(header0 != nullptr, "分组头存在", "groupHeader_0 located");
if (header0) {
    clickItem(c.win, header0);
    const QVariantMap collapsed1 =
        paneProp(c.win, "collapsed").toMap();
    bool anyTrue = false;
    for (auto it = collapsed1.constBegin(); it != collapsed1.constEnd(); ++it)
        if (it.value().toBool()) anyTrue = true;
    audit(anyTrue, "分组头点击折叠", "collapsed 含 true");
    clickItem(c.win, header0);
    const QVariantMap collapsed2 =
        paneProp(c.win, "collapsed").toMap();
    bool allFalse = true;
    for (auto it = collapsed2.constBegin(); it != collapsed2.constEnd(); ++it)
        if (it.value().toBool()) allFalse = false;
    audit(allFalse, "分组头再点展开", "collapsed 全 false");
}

}

void audit_s9(AuditCtx& c) {
// ---- S9 组内词头链跳转（前缀/释义组的非当前词头） ----
// aggregateLookup 精确词头命中即返回该组——查 "hel" 取前缀层组，
// 组内才有非当前词头可点
openWord(c.win, "hel");
flushEvents();
const QVariantList groups =
    paneProp(c.win, "groups").toList();
QString linkedWord;
QStringList expectedLinkTexts;
for (const QVariant& gv : groups) {
    const QVariantList entries = gv.toMap().value("entries").toList();
    for (const QVariant& ev : entries) {
        const QString w = ev.toMap().value("word").toString();
        if (w == QStringLiteral("hel")) continue;
        if (linkedWord.isEmpty()) linkedWord = w;
        // 层级标记口径与 EntryResultsPane 同式（rel 3 词条 · / 2 前缀 ·
        // / 1 原形 · / 4 模糊 · / 0 无前缀）——非当前词头即可见链，
        // 逐条配期望
        const int rel = ev.toMap().value("relevance").toInt();
        expectedLinkTexts << ((rel == 3 ? QStringLiteral("词条 · ")
                                        : rel == 2 ? QStringLiteral("前缀 · ")
                                        : rel == 1 ? QStringLiteral("原形 · ")
                                        : rel == 4 ? QStringLiteral("模糊 · ")
                                                   : QString()) + w);
    }
}
if (!linkedWord.isEmpty()) {
    // 层级标记逐条断言（P-4 聚合卡片细化）：模型序 ↔ 委托序配对，
    // 可见链文本必须 = 层级前缀 + 词头——前缀命中与精确层不再混观。
    // 趁未跳转先断言（点链会切 currentWord 重渲面板）
    // QML 委托项的 QObject 父链不经窗口（findChildren 走 QObject
    // 树找不到），须走 childItems 视觉树——与 visualFind 同径
    QList<QQuickItem*> links;
    visualFindAll(c.win->contentItem(), "entryWordLink", links);
    QStringList actualTexts;
    for (QQuickItem* li : links)
        if (li->property("visible").toBool())
            actualTexts << li->property("text").toString();
    audit(actualTexts == expectedLinkTexts, "词头链层级标记",
          QString("期望 %1 条[%2] 实得[%3]")
              .arg(expectedLinkTexts.size())
              .arg(expectedLinkTexts.join(QLatin1Char('|')))
              .arg(actualTexts.join(QLatin1Char('|'))));

    clickItem(c.win, item(c.win, "entryWordLink"));
    audit(c.win->property("currentWord").toString() == linkedWord,
          "组内词头链点击跳转",
          QString("currentWord=%1, 期望=%2")
              .arg(c.win->property("currentWord").toString(), linkedWord));
} else {
    audit(false, "组内词头链点击跳转", "fixture 未产生非当前词头（检查词典）");
}

}

void audit_s10(AuditCtx& c) {
// ---- S10 释义区导航 ←/→ ----
// ← 的落点 = openWord("world") 入栈前的当前词（openWord 把切换前的词
// 压 backStack）——不钉词面：S9 跳到的词随词典组序变（单/双 fixture
// 的前缀层首个候选不同），动态取值两种组序都成立
const QString wordBeforeNav = c.win->property("currentWord").toString();
openWord(c.win, "world");
clickItem(c.win, item(c.win, "navBackButton"));
audit(c.win->property("currentWord").toString() == wordBeforeNav,
      "导航 ← 返回上一词",
      QString("currentWord=%1, 期望=%2")
          .arg(c.win->property("currentWord").toString(), wordBeforeNav));
clickItem(c.win, item(c.win, "navForwardButton"));
audit(c.win->property("currentWord").toString() == QStringLiteral("world"),
      "导航 → 前进",
      QString("currentWord=%1").arg(c.win->property("currentWord").toString()));

}
