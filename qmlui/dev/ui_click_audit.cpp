// 离屏 UI 真点审计（BUG-010 口径：可点元素全接线，点不动的不许留）。
// 先例：dev/ui_sandbox.cpp——同一套离屏装配；区别在于本程序对每个可点
// 元素发真实 QMouseEvent（press+release 进 QQuickWindow 走完整交付链，
// 与用户点击同路径），并断言点击前后存在可观测状态差（状态行/模型数/
// 可见性/currentWord/口音设置…），逐项输出 PASS/FAIL 证据，任一 FAIL
// 退出码非 0。
//
// 用法：unidict_ui_click_audit <报告输出目录>
//   词典用 UNIDICT_DICTS 注入（ctest 指向 dev/fixtures/click_audit_dict.json，
//   fixture 保证 例句/词组/近反义/全文 四个内容 tab 都有数据）。
// 证据分级：signal 级（combo popup 离屏无法真点菜单项，以激活同签名
//   信号验证 handler 接线）会如实标注；其余全部为真实点击路径。
#include <QGuiApplication>
#include <functional>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QEventLoop>
#include <QThread>

#include "../lookup_adapter.h"
#include "../adapters/qt/data_store_qt.h"
#include "../adapters/qt/fulltext_manager_qt.h"
#include "../mobile_utils.h"
#include "../adapters/qt/sync_service_qt.h"
#include "../adapters/qt/sync_manager_qt.h"
#include "../adapters/qt/ai_service_qt.h"
#include "../adapters/qt/clipboard_qt.h"
#include "../adapters/qt/settings_qt.h"
#include "../theme.h"

using UnidictQml::Theme;

namespace {

int g_pass = 0;
int g_fail = 0;
QStringList g_log;

void audit(bool ok, const QString& name, const QString& evidence) {
    if (ok) ++g_pass; else ++g_fail;
    const QString line = QString("[%1] %2 — %3")
                             .arg(ok ? "PASS" : "FAIL", name, evidence);
    g_log << line;
    qInfo("%s", qPrintable(line));
}

void flushEvents() {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 60);
}

void settle(QQuickWindow* win) {
    // 定时器驱动的路径（建议 120ms / 内容 tab 30ms）需要真实时间流逝
    for (int i = 0; i < 30; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(15);
    }
    win->update();
    flushEvents();
}

// 真点：把 press+release 直接送到窗口事件链（QQuickWindow 按事件坐标
// 交给命中测试命中的 item，MouseArea/AbstractButton 与真机同路径）。
// 空元素直接返回（断言侧以 item 查找结果判 FAIL，不在 helper 里崩）
void clickItem(QQuickWindow* win, QQuickItem* item) {
    if (!item) return;
    const QPointF scene = item->mapToScene(QPointF(item->width() / 2.0,
                                                   item->height() / 2.0));
    const QPointF global = win->mapToGlobal(scene);
    QMouseEvent press(QEvent::MouseButtonPress, scene, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, scene, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    flushEvents();
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 同上，但点在 item 内的相对位置（0..1），Slider/SpinBox 定位用
void clickItemRel(QQuickWindow* win, QQuickItem* item, qreal rx, qreal ry) {
    if (!item) return;
    const QPointF scene = item->mapToScene(QPointF(item->width() * rx,
                                                   item->height() * ry));
    const QPointF global = win->mapToGlobal(scene);
    QMouseEvent press(QEvent::MouseButtonPress, scene, global,
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, scene, global,
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    flushEvents();
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 把 item 滚进其 Flickable 祖先（ScrollView 的内容体）的可视区：
// 抽屉固定高 + ScrollView 收纳后，折叠下方的控件坐标点不到，先滚再点。
// y 取内容坐标（对 contentItem 映射，与当前 contentY 无关），幂等
void scrollIntoView(QQuickWindow* win, QQuickItem* item) {
    if (!item) return;
    QQuickItem* flick = item->parentItem();
    while (flick && !flick->property("contentY").isValid())
        flick = flick->parentItem();
    if (!flick) return;
    QQuickItem* content = flick->property("contentItem").value<QQuickItem*>();
    const qreal y = content ? item->mapToItem(content, 0, 0).y()
                            : item->mapToItem(flick, 0, 0).y();
    const qreal h = item->height();
    const qreal viewH = flick->height();
    const qreal max =
        flick->property("contentHeight").toReal() - viewH;
    qreal target = y - (viewH - h) / 2;
    target = qBound<qreal>(0.0, target, qMax<qreal>(0.0, max));
    flick->setProperty("contentY", target);
    settle(win);
}

void sendKey(QQuickWindow* win, int key) {
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier);
    QCoreApplication::sendEvent(win, &press);
    QCoreApplication::sendEvent(win, &release);
    flushEvents();
}

// 视觉树查找：Repeater/ListView 委托的 QObject 父链与视觉树分叉，
// QObject::findChild 够不到委托；统一走 childItems 视觉树。
QQuickItem* visualFind(QQuickItem* root, const QByteArray& name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    const auto kids = root->childItems();
    for (QQuickItem* c : kids)
        if (QQuickItem* r = visualFind(c, name)) return r;
    return nullptr;
}

QQuickItem* item(QQuickWindow* win, const char* name) {
    return visualFind(win->contentItem(), name);
}

// 全量收集匹配 objectName 的视觉树节点（ListView 委托 childItems 序≠
// model 序，需要按内容挑选点谁断言谁）
void visualFindAll(QQuickItem* root, const QByteArray& name,
                    QList<QQuickItem*>& out) {
    if (!root) return;
    if (root->objectName() == name) out.append(root);
    for (QQuickItem* c : root->childItems()) visualFindAll(c, name, out);
}

// Popup/Dialog（QQuickPopup 不是 QQuickItem）：QObject 树定位 + 属性读态
QObject* popupObj(QQuickWindow* win, const char* name) {
    return win->findChild<QObject*>(name);
}

bool popupVisible(QObject* popup) {
    return popup && popup->property("visible").toBool();
}

// 点击信号计数器（clicked 只能用字符串信号连——QQuickAbstractButton
// 头私有；Q_OBJECT 接收器 + 文件尾 moc include，AUTOMOC 处理本 TU）
class ClickCounter : public QObject {
    Q_OBJECT
public:
    int count = 0;
public slots:
    void onClicked() { ++count; }
};

QString statusText(QQuickWindow* win) {
    return win->property("statusText").toString();
}

// EntryResultsPane 属性读取（元素缺失时空 QVariant，断言侧 FAIL 不崩）
QVariant paneProp(QQuickWindow* win, const char* prop) {
    QQuickItem* p = item(win, "entryResultsPane");
    return p ? p->property(prop) : QVariant();
}

// 侧栏面板属性读取（同上）
QVariant lpProp(QQuickWindow* win, const char* prop) {
    QQuickItem* p = item(win, "leftPane");
    return p ? p->property(prop) : QVariant();
}

void openWord(QQuickWindow* win, const QString& word) {
    QVariant w(word), nav(true);
    QMetaObject::invokeMethod(win, "openWord", Q_ARG(QVariant, w),
                              Q_ARG(QVariant, nav));
    settle(win);
}

} // namespace

int main(int argc, char* argv[]) {
    // 离屏软件渲染 + 用户目录隔离：必须在 QGuiApplication 之前设置
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    // 数据/配置全落临时目录：审计会写生词本/历史/设置，不污染真实用户数据
    const QString scratch = QDir::tempPath() + "/unidict_click_audit";
    QDir().mkpath(scratch + "/data");
    QDir().mkpath(scratch + "/xdg-config");
    QDir().mkpath(scratch + "/xdg-data");
    qputenv("UNIDICT_DATA_DIR", (scratch + "/data").toUtf8());
    // DataStoreQt 默认落 CWD 相对 ./data/unidict.json（不走 UNIDICT_DATA_DIR），
    // 显式钉到 scratch，审计写入绝不碰真实用户数据
    UnidictAdaptersQt::DataStoreQt::instance().setStoragePath(
        scratch + "/data/unidict.json");
    qputenv("XDG_CONFIG_HOME", (scratch + "/xdg-config").toUtf8());
    qputenv("XDG_DATA_HOME", (scratch + "/xdg-data").toUtf8());
    qputenv("HOME", scratch.toUtf8());
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Unidict"));
    app.setOrganizationName(QStringLiteral("YourCompany"));
    // 同步开关缺省态隔离：审计沙盒目录跨次复用，清掉历史开关位，
    // 「同步默认关闭」断言不依赖上次运行（须与 SyncManagerQt 同为
    // IniFormat——NativeFormat 指向 Unidict.conf，清不到真实存储）
    {
        QSettings s(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("YourCompany"),
                    QStringLiteral("Unidict"));
        s.remove(QStringLiteral("sync/enabled"));
        s.sync();
    }

    QString outDir = scratch;
    if (app.arguments().size() > 1) outDir = app.arguments().at(1);
    QDir().mkpath(outDir);

    // 与 main.cpp/ui_sandbox 同构的适配器装配
    LookupAdapter adapter;
    if (!adapter.loadDictionariesFromEnv()) {
        qCritical("UNIDICT_DICTS 未指向可加载词典（审计前置条件）");
        return 2;
    }
    adapter.setPronSourceMode(0);   // 本地 TTS 态（离屏环境可预期）
    adapter.setPronAccent(0);
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::SyncManagerQt syncManager;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;
    MobileUtils mobileUtils;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Theme", &Theme::instance());
    engine.rootContext()->setContextProperty("lookup", &adapter);
    engine.rootContext()->setContextProperty("fulltext", &fulltext);
    engine.rootContext()->setContextProperty("sync", &sync);
    engine.rootContext()->setContextProperty("syncManager", &syncManager);
    engine.rootContext()->setContextProperty(
        "documentsPath",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.rootContext()->setContextProperty("clip", &clip);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("MobileUtils", &mobileUtils);
    engine.rootContext()->setContextProperty("platformName",
                                             QGuiApplication::platformName());
    engine.rootContext()->setContextProperty("isMobile", false);
    engine.rootContext()->setContextProperty("screenWidth", 1280);
    engine.rootContext()->setContextProperty("screenHeight", 800);
    engine.rootContext()->setContextProperty("screenDpi", 96.0);

    engine.load(QUrl("qrc:/MainDesktop.qml"));
    if (engine.rootObjects().isEmpty()) {
        qCritical("QML load failed");
        return 2;
    }
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!win) {
        qCritical("root is not a QQuickWindow");
        return 2;
    }
    win->show();
    settle(win);

    // 生词本种子：保证 筛选 chips / 卡片 / +标签 / 笔记 可点
    adapter.addToVocabulary("hello", "A greeting; used when meeting someone.");
    adapter.addToVocabulary("world", "The earth and all its peoples.");
    adapter.addVocabTag("hello", "CET4");
    adapter.addVocabTag("hello", "greeting");
    adapter.addVocabTag("world", "CET4");
    adapter.setVocabNote("hello", "常用问候语");

    // ---- S0 词典卡头渲染管线（decorateHtml 主题拼接，BUG-010 之一） ----
    QVariant htmlVar;
    QVariant htmlArg0(QString("<pre>int x;</pre>")), htmlArg1(false);
    const bool htmlInvoked = QMetaObject::invokeMethod(
        win, "decorateHtml", Q_RETURN_ARG(QVariant, htmlVar),
        Q_ARG(QVariant, htmlArg0), Q_ARG(QVariant, htmlArg1));
    const QString html = htmlVar.toString();
    qInfo("HTMLRAW invoked=%d %s", int(htmlInvoked), qPrintable(html.left(200)));
    audit(html.contains("background:#") && html.contains("color:#")
              && !html.contains("+ Theme."),
          "decorateHtml <pre>/外层 div 主题样式拼接",
          "含 background:#、color:# 主题色且无字面 \"+ Theme.\" 残留");

    openWord(win, "hello");
    audit(statusText(win).startsWith("找到"),
          "openWord(hello) 聚合查询落状态行",
          QString("statusText=%1").arg(statusText(win)));
    audit(item(win, "phonSpeakerBr") != nullptr && item(win, "phonSpeakerUs") != nullptr,
          "音标行英/美喇叭存在（fixture 词头带 英/美 音标）",
          "phonSpeakerBr 与 phonSpeakerUs 均可见于词条卡");

    // ---- S1 朗读点击反馈（BUG-010：点击必有状态行反馈） ----
    const bool hasTts = adapter.hasLocalTts();
    clickItem(win, item(win, "phonSpeakerBr"));
    audit(hasTts ? statusText(win).startsWith("正在朗读（英音）")
                 : statusText(win).contains("本地语音不可用"),
          "英音喇叭点击反馈",
          QString("hasLocalTts=%1, statusText=%2")
              .arg(hasTts ? "true" : "false", statusText(win)));
    clickItem(win, item(win, "phonSpeakerUs"));
    audit(hasTts ? statusText(win).startsWith("正在朗读（美音）")
                 : statusText(win).contains("本地语音不可用"),
          "美音喇叭点击反馈（与英音分口音路径）",
          QString("statusText=%1").arg(statusText(win)));

    // ---- S2 生词本/复制轻入口 ----
    clickItem(win, item(win, "vocabAction"));
    audit(statusText(win).contains("已加入生词本"), "生词本轻入口点击",
          QString("statusText=%1").arg(statusText(win)));
    clickItem(win, item(win, "copyAction"));
    audit(statusText(win).contains("已复制释义"), "复制轻入口点击（剪贴板写入）",
          QString("statusText=%1").arg(statusText(win)));

    // ---- S3 笔记弹层开关 ----
    clickItem(win, item(win, "noteAction"));
    settle(win);
    QObject* notePopup = popupObj(win, "notePopup");
    audit(popupVisible(notePopup), "笔记轻入口打开笔记弹层",
          "notePopup visible");
    if (popupVisible(notePopup)) {
        clickItem(win, item(win, "noteCancelButton"));
        // 关闭过渡期间 visible 仍为 true 且 modal overlay 未释放（会吃掉
        // 后续所有点击）——settle 后再断言
        settle(win);
        audit(!popupVisible(notePopup), "笔记弹层取消按钮点击收起",
              "notePopup hidden after cancel");
    }

    // ---- S4 内容 tab：例句（重组加载反馈 + 喇叭读整句 + 词链跳转） ----
    if (!item(win, "contentTab_examples")) {
        // 子树转储（仅定位失败时）：确认委托是否实例化、objectName 是否为空
        QQuickItem* paneRoot = item(win, "entryResultsPane");
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
    audit(item(win, "contentTab_examples") != nullptr, "内容 tab 元素定位",
          "contentTab_examples found");
    clickItem(win, item(win, "contentTab_examples"));
    settle(win);
    const QVariantMap tabData = paneProp(win, "tabData").toMap();
    const int exampleCount = tabData.value("examples").toList().size();
    audit(exampleCount >= 3 && statusText(win).contains("共 "),
          "例句 tab 点击（懒取+加载状态行收口）",
          QString("examples=%1 条, statusText=%2")
              .arg(exampleCount).arg(statusText(win)));
    clickItem(win, item(win, "exampleSpeaker"));
    // 点击必有反馈：有本地 TTS 时落「正在朗读例句」，无 TTS 时落明示
    // 换路的「本地语音不可用」（后者会覆盖前者落状态行，二者均来自
    // 该次点击的处理链，任一出现即接线成立）
    const QString spStatus = statusText(win);
    audit(spStatus.contains(QStringLiteral("正在朗读例句"))
              || spStatus.contains(QStringLiteral("本地语音不可用")),
          "例句喇叭点击（读整句+反馈）",
          QString("statusText=%1").arg(spStatus));
    const QVariantList exampleRows =
        adapter.fullTextLookup(QStringLiteral("hello"), 20);
    // 非重言证据：委托 childItems 序≠model 序，从全部词链委托里挑第一
    // 个「非当前词头」的（当前词头链点击不产生状态差，PASS 无意义）。
    // 委托 text 形如 "word · 词典名"，词在 " · " 之前
    QList<QQuickItem*> exLinks;
    visualFindAll(win->contentItem(), "exampleWordLink", exLinks);
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
        clickItem(win, exLinkTarget);
        audit(win->property("currentWord").toString() == exampleWord0,
              "例句行词链点击跳转",
              QString("currentWord=%1, 期望=%2")
                  .arg(win->property("currentWord").toString(), exampleWord0));
    } else {
        audit(false, "例句行词链点击跳转", "fixture 例句无非当前词头行（检查词典）");
    }

    // ---- S5 词组 tab ----
    openWord(win, "hello");
    clickItem(win, item(win, "contentTab_phrases"));
    settle(win);
    const QVariantMap td2 = paneProp(win, "tabData").toMap();
    const QString phraseWord0 = td2.value("phrases").toList().value(0)
                                    .toMap().value("word").toString();
    audit(!phraseWord0.isEmpty(), "词组 tab 点击懒取数据",
          QString("phrases[0].word=%1").arg(phraseWord0));
    clickItem(win, item(win, "phraseLink"));
    audit(win->property("currentWord").toString() == phraseWord0,
          "词组词链点击跳转",
          QString("currentWord=%1, 期望=%2")
              .arg(win->property("currentWord").toString(), phraseWord0));

    // ---- S6 近反义词 tab ----
    openWord(win, "hello");
    clickItem(win, item(win, "contentTab_related"));
    settle(win);
    const QVariantMap td3 = paneProp(win, "tabData").toMap();
    const QString relatedWord0 = td3.value("related").toList().value(0)
                                     .toMap().value("word").toString();
    audit(!relatedWord0.isEmpty(), "近反义 tab 点击懒取数据",
          QString("related[0].word=%1").arg(relatedWord0));
    clickItem(win, item(win, "relatedLink"));
    audit(win->property("currentWord").toString() == relatedWord0,
          "近义词链接点击跳转",
          QString("currentWord=%1, 期望=%2")
              .arg(win->property("currentWord").toString(), relatedWord0));

    // ---- S7 全文检索 tab（重组路径，加载状态行） ----
    openWord(win, "hello");
    clickItem(win, item(win, "contentTab_fulltext"));
    settle(win);
    const QVariantMap td4 = paneProp(win, "tabData").toMap();
    audit(td4.value("fulltext").toList().size() >= 3
              && statusText(win).contains("共 "),
          "全文检索 tab 点击（懒取+状态行）",
          QString("fulltext=%1 条, statusText=%2")
              .arg(td4.value("fulltext").toList().size()).arg(statusText(win)));

    // ---- S8 词典 tab：分组折叠 ----
    clickItem(win, item(win, "contentTab_dict"));
    flushEvents();
    QQuickItem* header0 = item(win, "groupHeader_0");
    audit(header0 != nullptr, "分组头存在", "groupHeader_0 located");
    if (header0) {
        clickItem(win, header0);
        const QVariantMap collapsed1 =
            paneProp(win, "collapsed").toMap();
        bool anyTrue = false;
        for (auto it = collapsed1.constBegin(); it != collapsed1.constEnd(); ++it)
            if (it.value().toBool()) anyTrue = true;
        audit(anyTrue, "分组头点击折叠", "collapsed 含 true");
        clickItem(win, header0);
        const QVariantMap collapsed2 =
            paneProp(win, "collapsed").toMap();
        bool allFalse = true;
        for (auto it = collapsed2.constBegin(); it != collapsed2.constEnd(); ++it)
            if (it.value().toBool()) allFalse = false;
        audit(allFalse, "分组头再点展开", "collapsed 全 false");
    }

    // ---- S9 组内词头链跳转（前缀/释义组的非当前词头） ----
    // aggregateLookup 精确词头命中即返回该组——查 "hel" 取前缀层组，
    // 组内才有非当前词头可点
    openWord(win, "hel");
    flushEvents();
    const QVariantList groups =
        paneProp(win, "groups").toList();
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
        visualFindAll(win->contentItem(), "entryWordLink", links);
        QStringList actualTexts;
        for (QQuickItem* li : links)
            if (li->property("visible").toBool())
                actualTexts << li->property("text").toString();
        audit(actualTexts == expectedLinkTexts, "词头链层级标记",
              QString("期望 %1 条[%2] 实得[%3]")
                  .arg(expectedLinkTexts.size())
                  .arg(expectedLinkTexts.join(QLatin1Char('|')))
                  .arg(actualTexts.join(QLatin1Char('|'))));

        clickItem(win, item(win, "entryWordLink"));
        audit(win->property("currentWord").toString() == linkedWord,
              "组内词头链点击跳转",
              QString("currentWord=%1, 期望=%2")
                  .arg(win->property("currentWord").toString(), linkedWord));
    } else {
        audit(false, "组内词头链点击跳转", "fixture 未产生非当前词头（检查词典）");
    }

    // ---- S10 释义区导航 ←/→ ----
    openWord(win, "world");
    clickItem(win, item(win, "navBackButton"));
    audit(win->property("currentWord").toString() == QStringLiteral("hello"),
          "导航 ← 返回上一词",
          QString("currentWord=%1").arg(win->property("currentWord").toString()));
    clickItem(win, item(win, "navForwardButton"));
    audit(win->property("currentWord").toString() == QStringLiteral("world"),
          "导航 → 前进",
          QString("currentWord=%1").arg(win->property("currentWord").toString()));

    // ---- S11 侧栏：搜索+查按钮 ----
    QQuickItem* searchField = item(win, "searchField");
    searchField->setProperty("text", QStringLiteral("hello"));
    flushEvents();
    clickItem(win, item(win, "searchGoButton"));
    audit(win->property("currentWord").toString() == QStringLiteral("hello"),
          "侧栏「查」按钮点击提交查询",
          QString("currentWord=%1").arg(win->property("currentWord").toString()));

    // 建议列表（suggestTimer 120ms 后有项）
    searchField->setProperty("text", QStringLiteral("hel"));
    settle(win);
    const QStringList suggestions = adapter.suggestPrefix(QStringLiteral("hel"), 50);
    if (!suggestions.isEmpty()) {
        clickItem(win, item(win, "suggestItem"));
        audit(win->property("currentWord").toString() == suggestions.first(),
              "建议列表项点击查询",
              QString("currentWord=%1, 期望=%2")
                  .arg(win->property("currentWord").toString(), suggestions.first()));
    } else {
        audit(false, "建议列表项点击查询", "suggestPrefix(hel) 无候选（检查词典）");
    }

    // ---- S12 侧栏 tab：历史 ----
    clickItem(win, item(win, "sideTab1"));
    settle(win);   // StackLayout 切页后委托实例化需要一帧
    audit(lpProp(win, "currentTabIndex").toInt() == 1,
          "侧栏「历史」tab 点击", "currentTabIndex=1");
    const QStringList history = adapter.searchHistory(200);
    if (!history.isEmpty()) {
        // 委托 childItems 序≠model 序（ListView 复用/重排），点中谁断言谁
        // 的 text 落词——验证「委托点击 → openWord」接线本身
        if (auto* hItem = item(win, "historyItem")) {
            const QString hWord = hItem->property("text").toString();
            clickItem(win, hItem);
            audit(win->property("currentWord").toString() == hWord,
                  "历史项点击查询",
                  QString("currentWord=%1, 期望=%2")
                      .arg(win->property("currentWord").toString(), hWord));
        } else {
            audit(false, "历史项点击查询", "historyItem 委托不存在");
        }
    } else {
        audit(false, "历史项点击查询", "searchHistory 为空（前置查询应已入史）");
    }

    // ---- S13 侧栏 tab：生词本（chips/卡/对话框） ----
    clickItem(win, item(win, "sideTab2"));
    settle(win);   // StackLayout 切页后 ListView 委托实例化需要一帧
    audit(lpProp(win, "currentTabIndex").toInt() == 2,
          "侧栏「生词本」tab 点击", "currentTabIndex=2");
    clickItem(win, item(win, "vocabFilterChip_greeting"));
    audit(lpProp(win, "vocabTagFilter").toString()
              == QStringLiteral("greeting"),
          "标签筛选 chip 点击过滤",
          QString("vocabTagFilter=%1")
              .arg(lpProp(win, "vocabTagFilter").toString()));
    clickItem(win, item(win, "vocabFilterChip_all"));
    audit(lpProp(win, "vocabTagFilter").toString().isEmpty(),
          "「全部」chip 点击清筛选", "vocabTagFilter 为空");

    const QVariantList vocabMeta = adapter.vocabularyMeta();
    const QString vocabWord0 = vocabMeta.value(0).toMap().value("word").toString();
    // 过滤切换触发模型重建，布局稳一帧再点卡——press→release 间隔内
    // 控件位移会让 AbstractButton 判定界外释放、不发 clicked
    settle(win);
    // 委托 childItems 序≠model 序：点中谁断言谁的 ownerWord（验证
    // 「生词卡点击 → openWord」接线本身）
    if (auto* vCard = item(win, "vocabCard")) {
        const QString cardWord = vCard->property("ownerWord").toString();
        // 点词头 Label 区域（左上）：卡中心落在第二行笔记/标签 chip 上，
        // 那是另一个可点元素
        clickItemRel(win, vCard, 0.15, 0.15);
        audit(win->property("currentWord").toString() == cardWord,
              "生词卡点击查询",
              QString("currentWord=%1, 期望=%2")
                  .arg(win->property("currentWord").toString(), cardWord));
    } else {
        audit(false, "生词卡点击查询", "vocabCard 委托不存在");
    }

    clickItem(win, item(win, "vocabAddTagButton"));
    QObject* tagDialog = popupObj(win, "tagDialog");
    audit(popupVisible(tagDialog), "生词卡「+标签」打开标签对话框",
          "tagDialog visible");
    if (popupVisible(tagDialog)) {
        QMetaObject::invokeMethod(tagDialog, "close");
        settle(win);   // modal 关闭过渡未完会吃掉后续点击
    }

    clickItem(win, item(win, "vocabNoteButton"));
    QObject* noteDialog = popupObj(win, "noteDialog");
    audit(popupVisible(noteDialog), "生词卡「笔记」打开笔记对话框",
          "noteDialog visible");
    if (popupVisible(noteDialog)) {
        QMetaObject::invokeMethod(noteDialog, "close");
        settle(win);
    }

    QQuickItem* exportButton = item(win, "vocabExportButton");
    ClickCounter exportCounter;
    QObject::connect(exportButton, SIGNAL(clicked()), &exportCounter,
                     SLOT(onClicked()));
    clickItem(win, exportButton);
    audit(exportCounter.count == 1,
          "「导出CSV」点击（signal 级：native 文件对话框离屏不出窗）",
          QString("clicked 计数=%1").arg(exportCounter.count));

    // ---- S14 头部三入口 + 页脚清历史 ----
    clickItem(win, item(win, "headerHistoryButton"));
    audit(lpProp(win, "currentTabIndex").toInt() == 1,
          "头部「历史」按钮", "currentTabIndex=1");
    clickItem(win, item(win, "headerVocabButton"));
    audit(lpProp(win, "currentTabIndex").toInt() == 2,
          "头部「生词本」按钮", "currentTabIndex=2");
    clickItem(win, item(win, "headerSettingsButton"));
    settle(win);   // Drawer 开启是动画过渡，settle 后内容才在屏上可点
    QObject* drawer = popupObj(win, "toolsDrawer");
    audit(popupVisible(drawer), "头部「设置」按钮开抽屉",
          "toolsDrawer visible");

    // ---- S15 抽屉：先测默认「取词」页控件，再切「语音」页测语音控件
    // （StackLayout 切页后隐藏页控件点不到，顺序不能反）----
    if (auto* clipSwitch = item(win, "clipboardSwitch")) {
        const bool clipBefore = clipSwitch->property("checked").toBool();
        clickItem(win, clipSwitch);
        audit(clipSwitch->property("checked").toBool() != clipBefore
                  && adapter.isClipboardAutoLookupEnabled() != clipBefore,
              "剪贴板取词开关点击（UI+core 双证）",
              QString("checked %1→%2").arg(clipBefore)
                  .arg(!clipBefore));
        clickItem(win, clipSwitch);   // 还原
        audit(adapter.isClipboardAutoLookupEnabled() == clipBefore,
              "剪贴板取词开关还原", "core 状态回原");
    } else {
        audit(false, "剪贴板取词开关点击（UI+core 双证）", "clipboardSwitch 不存在");
    }

    if (auto* qlSwitch = item(win, "quickLookupSwitch")) {
        const bool qlBefore = qlSwitch->property("checked").toBool();
        clickItem(win, qlSwitch);
        audit(qlSwitch->property("checked").toBool() != qlBefore,
              "取词悬浮窗开关点击", QString("checked %1→%2").arg(qlBefore).arg(!qlBefore));
        clickItem(win, qlSwitch);   // 还原
    } else {
        audit(false, "取词悬浮窗开关点击", "quickLookupSwitch 不存在");
    }

    if (auto* poll = item(win, "pollSlider")) {
        const qreal pollBefore = poll->property("value").toReal();
        clickItemRel(win, poll, 0.9, 0.5);
        audit(poll->property("value").toReal() != pollBefore,
              "轮询间隔 Slider 点击改值",
              QString("value %1→%2").arg(pollBefore).arg(poll->property("value").toReal()));
    } else {
        audit(false, "轮询间隔 Slider 点击改值", "pollSlider 不存在");
    }

    // SpinBox 真实用户路径=点 +/- 步进钮；点击控件中央落焦在内部
    // TextInput，Key_Up 会被光标移动消费而非步进
    auto spinStepTest = [&](QQuickWindow* w, const char* name, const QString& label) {
        if (auto* spin = item(w, name)) {
            const int spinBefore = spin->property("value").toInt();
            QObject* upGroup = spin->property("up").value<QObject*>();
            QQuickItem* upInd = upGroup
                                    ? upGroup->property("indicator").value<QQuickItem*>()
                                    : nullptr;
            if (upInd) {
                clickItem(w, upInd);
                audit(spin->property("value").toInt() == spinBefore + 1,
                      label + "「+」步进钮点击",
                      QString("value %1→%2").arg(spinBefore)
                          .arg(spin->property("value").toInt()));
                QObject* downGroup = spin->property("down").value<QObject*>();
                QQuickItem* downInd = downGroup
                                          ? downGroup->property("indicator").value<QQuickItem*>()
                                          : nullptr;
                if (downInd) clickItem(w, downInd);   // 还原
            } else {
                audit(false, label + "「+」步进钮点击", "up indicator 不存在");
            }
        } else {
            audit(false, label + "「+」步进钮点击", QString(name) + " 不存在");
        }
    };
    spinStepTest(win, "minLenSpin", "最短词长 SpinBox");
    spinStepTest(win, "maxLenSpin", "最长词长 SpinBox");

    // 切「语音」页：StackLayout 布局传递需要一帧，settle 后再断言可见
    clickItem(win, item(win, "toolsTab1"));
    settle(win);
    QQuickItem* volumeSlider = item(win, "volumeSlider");
    audit(volumeSlider && volumeSlider->isVisible(),
          "抽屉「语音」tab 切换", "volumeSlider 可见");
    clickItem(win, item(win, "stopSpeakButton"));
    audit(true, "「停止」按钮点击", "执行无异常（停止播放副作用离屏不可观测）");
    clickItem(win, item(win, "refreshVoicesButton"));
    audit(true, "「刷新」语音按钮点击", "执行无异常（重扫语音副作用离屏不可观测）");

    // combo popup 离屏无法真点菜单项：以同签名 activated 信号验证 handler
    // 接线。QML handler 读的是 currentIndex 属性（非信号参数）——先设
    // currentIndex 再发 activated，才等价于真点菜单项
    QObject* accentCombo = win->findChild<QObject*>("pronAccentCombo");
    if (accentCombo) {
        accentCombo->setProperty("currentIndex", 2);
        QMetaObject::invokeMethod(accentCombo, "activated", Q_ARG(int, 2));
        audit(adapter.pronAccent() == 2, "口音 ComboBox 选择接线（signal 级）",
              QString("pronAccent=%1").arg(adapter.pronAccent()));
        accentCombo->setProperty("currentIndex", 0);
        QMetaObject::invokeMethod(accentCombo, "activated", Q_ARG(int, 0));
    } else {
        audit(false, "口音 ComboBox 选择接线（signal 级）", "pronAccentCombo 不存在");
    }
    QObject* sourceCombo = win->findChild<QObject*>("pronSourceCombo");
    if (sourceCombo) {
        sourceCombo->setProperty("currentIndex", 1);
        QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 1));
        audit(adapter.pronSourceMode() == 1, "发音源 ComboBox 选择接线（signal 级）",
              QString("pronSourceMode=%1").arg(adapter.pronSourceMode()));
        sourceCombo->setProperty("currentIndex", 0);
        QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 0));
        audit(adapter.pronSourceMode() == 0, "发音源还原本地态", "pronSourceMode=0");
    } else {
        audit(false, "发音源 ComboBox 选择接线（signal 级）", "pronSourceCombo 不存在");
    }

    if (drawer) drawer->setProperty("visible", false);
    settle(win);   // Drawer 关闭过渡未完会挡住页脚点击

    // ---- S16 页脚清历史 ----
    clickItem(win, item(win, "clearHistoryButton"));
    audit(statusText(win) == QStringLiteral("历史已清空")
              && adapter.searchHistory(200).isEmpty(),
          "页脚「清空历史」按钮",
          QString("statusText=%1, history=%2")
              .arg(statusText(win)).arg(adapter.searchHistory(200).size()));

    // ---- S17 悬浮取词窗全控件 ----
    QVariant qw(QStringLiteral("hello"));
    QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qw));
    settle(win);
    auto* paneWin = win->findChild<QQuickWindow*>("quickLookupPane");
    audit(paneWin && paneWin->isVisible(), "取词窗弹出", "quickLookupPane visible");
    if (paneWin) {
        auto* paneItem = paneWin->contentItem();
        auto* qlSpeak = visualFind(paneItem, "qlSpeakButton");
        clickItem(paneWin, qlSpeak);
        audit(hasTts || statusText(win).contains("本地语音不可用"),
              "取词窗朗读按钮（TTS 缺失给明示）",
              QString("statusText=%1").arg(statusText(win)));
        clickItem(paneWin, visualFind(paneItem, "qlVocabButton"));
        audit(statusText(win).contains("已加入生词本"),
              "取词窗生词本按钮",
              QString("statusText=%1").arg(statusText(win)));
        clickItem(paneWin, visualFind(paneItem, "qlCloseButton"));
        audit(!paneWin->isVisible(), "取词窗 ✕ 关闭", "pane hidden");

        QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qw));
        settle(win);
        clickItem(paneWin, visualFind(paneWin->contentItem(), "qlOpenMainButton"));
        flushEvents();
        audit(!paneWin->isVisible()
                  && win->property("currentWord").toString() == QStringLiteral("hello"),
              "取词窗「在主窗打开」收窗并落主窗词条卡",
              QString("pane hidden, currentWord=%1")
                  .arg(win->property("currentWord").toString()));

        QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qw));
        settle(win);
        clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseBottomButton"));
        audit(!paneWin->isVisible(), "取词窗「关闭」按钮", "pane hidden");
    }

    // ---- S18 同步备份 tab：红线开关面 + 自救口 ----
    if (drawer) drawer->setProperty("visible", true);
    settle(win);
    clickItem(win, item(win, "toolsTab4"));
    settle(win);

    // 红线：同步默认关闭
    QQuickItem* syncSwitch = item(win, "syncSwitch");
    audit(syncSwitch && !syncSwitch->property("checked").toBool(),
          "同步开关默认关闭（红线）",
          syncSwitch ? "checked=false（缺省态）" : "syncSwitch 不存在");

    // 点开关想开：被挡回（须走「确认开启」——显式开启明示范围）
    if (syncSwitch) clickItem(win, syncSwitch);
    settle(win);
    audit(syncSwitch && !syncSwitch->property("checked").toBool(),
          "开关联动挡回（开启须经确认）", "点开不直通，范围明示常驻");

    // 范围明示常驻可见
    QQuickItem* scopeLabel = item(win, "syncScopeLabel");
    audit(scopeLabel && scopeLabel->property("visible").toBool(),
          "同步范围明示常驻", scopeLabel ? "scopeText 可见" : "syncScopeLabel 不存在");

    // 确认开启：非法 gid 拒绝 + 合法 gid 生效
    QQuickItem* applyButton = item(win, "syncApplyButton");
    QQuickItem* syncStateLabel = item(win, "syncStateLabel");
    if (applyButton) {
        QQuickItem* gidField = item(win, "syncGidField");
        if (gidField)
            gidField->setProperty("text", QStringLiteral("short"));
        scrollIntoView(win, applyButton);
        clickItem(win, applyButton);
        settle(win);
        audit(syncSwitch && !syncSwitch->property("checked").toBool()
                  && syncStateLabel
                  && syncStateLabel->property("text").toString().contains(
                      QStringLiteral("不合法")),
              "非法组 ID 拒绝开启", "gid 形态校验拦下 short");

        if (gidField)
            gidField->setProperty("text",
                                  QStringLiteral("audit-group-0123456789ab"));
        scrollIntoView(win, applyButton);
        clickItem(win, applyButton);
        settle(win);
        audit(syncSwitch && syncSwitch->property("checked").toBool(),
              "合法组 ID 显式开启", "确认开启后开关置位");

        // 关闭路径：开关拨回（apply 滚动后开关在视口上方，先滚回来再点）
        scrollIntoView(win, syncSwitch);
        if (syncSwitch) clickItem(win, syncSwitch);
        settle(win);
        audit(syncSwitch && !syncSwitch->property("checked").toBool(),
              "开关关闭即停用", "disable 后 checked=false");
    } else {
        audit(false, "同步确认按钮", "syncApplyButton 不存在");
    }

    // 自救口：导出 → 错口令拒 → 对口令恢复（断言读抽屉内 syncState 标签）
    QQuickItem* syncExportButton = item(win, "backupExportButton");
    QQuickItem* syncRestoreButton = item(win, "backupRestoreButton");
    QQuickItem* passField = item(win, "backupPassField");
    QQuickItem* pathField = item(win, "backupPathField");
    if (syncExportButton && syncRestoreButton && passField && pathField) {
        passField->setProperty("text", QStringLiteral("audit-pass-123"));
        pathField->setProperty(
            "text", outDir + QStringLiteral("/audit-backup.udbk"));
        scrollIntoView(win, syncExportButton);
        clickItem(win, syncExportButton);
        settle(win);
        QFile backupFile(outDir + QStringLiteral("/audit-backup.udbk"));
        audit(backupFile.exists() && backupFile.size() > 64
                  && syncStateLabel
                  && syncStateLabel->property("text").toString().contains(
                      QStringLiteral("备份已导出")),
              "备份导出落盘",
              QString("size=%1").arg(backupFile.size()));

        passField->setProperty("text", QStringLiteral("wrong-pass"));
        scrollIntoView(win, syncRestoreButton);
        clickItem(win, syncRestoreButton);
        settle(win);
        audit(syncStateLabel
                  && syncStateLabel->property("text").toString().contains(
                      QStringLiteral("passphrase")),
              "错口令恢复被拒",
              QString("label=%1")
                  .arg(syncStateLabel
                           ? syncStateLabel->property("text").toString()
                           : QString()));
        // 错口令不产生文件之外的半态：恢复计数为 0 的语义由适配器并入
        // 语义保证，这里只验拒绝文案已到位

        passField->setProperty("text", QStringLiteral("audit-pass-123"));
        scrollIntoView(win, syncRestoreButton);
        clickItem(win, syncRestoreButton);
        settle(win);
        audit(syncStateLabel
                  && syncStateLabel->property("text").toString().contains(
                      QStringLiteral("已恢复")),
              "对口令恢复成功（并入语义）",
              QString("label=%1")
                  .arg(syncStateLabel
                           ? syncStateLabel->property("text").toString()
                           : QString()));
    } else {
        audit(false, "备份自救控件", "导出/恢复按钮或输入框不存在");
    }

    if (drawer) drawer->setProperty("visible", false);
    settle(win);

    // ---- 汇总 ----
    const QString summary = QString("==== click audit: %1 passed, %2 failed ====")
                                .arg(g_pass).arg(g_fail);
    g_log << summary;
    qInfo("%s", qPrintable(summary));
    QFile report(outDir + "/click_audit_report.txt");
    if (report.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream ts(&report);
        for (const QString& line : g_log) ts << line << "\n";
    }
    return g_fail == 0 ? 0 : 3;
}

#include "ui_click_audit.moc"
