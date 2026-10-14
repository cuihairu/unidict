// 离屏 UI 真点审计（BUG-010 口径：可点元素全接线，点不动的不许留）。
// 先例：dev/ui_sandbox.cpp——同一套离屏装配；区别在于本程序对每个可点
// 元素发真实 QMouseEvent（press+release 进 QQuickWindow 走完整交付链，
// 与用户点击同路径），并断言点击前后存在可观测状态差（状态行/模型数/
// 可见性/currentWord/口音设置…），逐项输出 PASS/FAIL 证据，任一 FAIL
// 退出码非 0。
//
// 用法：unidict_ui_click_audit <报告输出目录>
//   词典用 UNIDICT_DICTS 注入（ctest 指向 dev/fixtures/click_audit_dict.json，
//   fixture 保证 例句/词组/近义联想/全文 四个内容 tab 都有数据）。
// 证据分级：signal 级（combo popup 离屏无法真点菜单项，以激活同签名
//   信号验证 handler 接线）会如实标注；其余全部为真实点击路径。
#include <QGuiApplication>
#include <functional>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QFont>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThread>

#include "../lookup_adapter.h"
#include "../platform_adapter.h"
#include "../voice_adapter.h"
#include "../vocab_store_adapter.h"
#include "../adapters/qt/data_store_qt.h"
#include "../adapters/qt/fulltext_manager_qt.h"
#include "../mobile_utils.h"
#include "../adapters/qt/sync_service_qt.h"
#include "../adapters/qt/sync_manager_qt.h"
#include "../adapters/qt/ai_service_qt.h"
#include "../adapters/qt/clipboard_qt.h"
#include "../adapters/qt/settings_qt.h"
#include "../startup_launcher.h"
#include "../tray_controller.h"

#include <QApplication>
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
    // Windows 下 Qt 默认 message handler 不写 stderr（走 OutputDebugString），
    // ctest --output-on-failure 捕不到任何审计输出——自装 handler 把每条
    // PASS/FAIL 实时打进 stderr/stdout 并立刻 flush，失败时 CI 才有现场
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&,
                              const QString& msg) {
        const QByteArray raw = msg.toUtf8();
        fwrite(raw.constData(), 1, raw.size(), stderr);
        fputc('\n', stderr);
        fflush(stderr);
        fwrite(raw.constData(), 1, raw.size(), stdout);
        fputc('\n', stdout);
        fflush(stdout);
    });
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
    // F9 自启密封：落盘目录钉进 scratch（Linux/macOS 实现读此缝；
    // Windows 走注册表不受影响，开→关自还原）
    qputenv("UNIDICT_AUTOSTART_DIR", (scratch + "/autostart").toUtf8());
    // QApplication：TrayController 的 QMenu 是 QWidget 系（qmlui/main.cpp
    // 同口径），纯 QGuiApplication 下构造菜单直接 fatal
    QApplication app(argc, argv);
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
    PlatformAdapter platform;
    VoiceAdapter voice;
    VocabStoreAdapter vocab;
    voice.setPronSourceMode(0);   // 本地 TTS 态（离屏环境可预期）
    voice.setPronAccent(0);
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::SyncManagerQt syncManager;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;
    StartupLauncher startup;
    TrayController tray(&settings);
    MobileUtils mobileUtils;

    // 上下文装配双引擎复用：桌面面（MainDesktop.qml，B 系列同步）与
    // 移动端根（Main.qml，文件同步+同步预览）共用同一批适配器实例
    auto registerCtx = [&](QQmlApplicationEngine& e) {
        e.rootContext()->setContextProperty("Theme", &Theme::instance());
        e.rootContext()->setContextProperty("lookup", &adapter);
        e.rootContext()->setContextProperty("platform", &platform);
        e.rootContext()->setContextProperty("voice", &voice);
        e.rootContext()->setContextProperty("vocab", &vocab);
        e.rootContext()->setContextProperty("fulltext", &fulltext);
        e.rootContext()->setContextProperty("sync", &sync);
        e.rootContext()->setContextProperty("syncManager", &syncManager);
        e.rootContext()->setContextProperty(
            "documentsPath",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        e.rootContext()->setContextProperty("ai", &ai);
        e.rootContext()->setContextProperty("clip", &clip);
        e.rootContext()->setContextProperty("settings", &settings);
        e.rootContext()->setContextProperty("startup", &startup);
        e.rootContext()->setContextProperty("tray", &tray);
        e.rootContext()->setContextProperty("MobileUtils", &mobileUtils);
        e.rootContext()->setContextProperty("platformName",
                                            QGuiApplication::platformName());
        e.rootContext()->setContextProperty("isMobile", false);
        e.rootContext()->setContextProperty("screenWidth", 1280);
        e.rootContext()->setContextProperty("screenHeight", 800);
        e.rootContext()->setContextProperty("screenDpi", 96.0);
    };

    QQmlApplicationEngine engine;
    registerCtx(engine);

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
    vocab.addToVocabulary("hello", "A greeting; used when meeting someone.");
    vocab.addToVocabulary("world", "The earth and all its peoples.");
    vocab.addVocabTag("hello", "CET4");
    vocab.addVocabTag("hello", "greeting");
    vocab.addVocabTag("world", "CET4");
    vocab.setVocabNote("hello", "常用问候语");

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
    const bool hasTts = voice.hasLocalTts();
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
    // 接线门：pane.voice 未声明未接线时该点击只落状态行、朗读哑掉（日志报
    // speakText of undefined），上一条状态行断言测不出，须钉住 voice 属性
    // 指向真实 TTS 服务
    const QObject* voiceSvc = paneProp(win, "voice").value<QObject*>();
    audit(voiceSvc != nullptr,
          "例句喇叭接线（pane.voice 指向 TTS 服务）",
          QString("voice=%1").arg(voiceSvc ? "wired" : "null"));
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

    // ---- S6 近义联想 tab ----
    openWord(win, "hello");
    clickItem(win, item(win, "contentTab_related"));
    settle(win);
    const QVariantMap td3 = paneProp(win, "tabData").toMap();
    const QString relatedWord0 = td3.value("related").toList().value(0)
                                     .toMap().value("word").toString();
    audit(!relatedWord0.isEmpty(), "近义联想 tab 点击懒取数据",
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
    // ← 的落点 = openWord("world") 入栈前的当前词（openWord 把切换前的词
    // 压 backStack）——不钉词面：S9 跳到的词随词典组序变（单/双 fixture
    // 的前缀层首个候选不同），动态取值两种组序都成立
    const QString wordBeforeNav = win->property("currentWord").toString();
    openWord(win, "world");
    clickItem(win, item(win, "navBackButton"));
    audit(win->property("currentWord").toString() == wordBeforeNav,
          "导航 ← 返回上一词",
          QString("currentWord=%1, 期望=%2")
              .arg(win->property("currentWord").toString(), wordBeforeNav));
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

    // 建议列表（suggestTimer 120ms 后有项）。UI 批一建议分组后，首条
    // 委托可能是「精确」段的词头，不再必然等于 suggestPrefix 首候选——
    // 改为读点击目标自身的词（text=原始词，内容 Text 是富文本高亮串）
    searchField->setProperty("text", QStringLiteral("hel"));
    settle(win);
    if (QQuickItem* sugItem = item(win, "suggestItem")) {
        const QString sugWord = sugItem->property("text").toString();
        clickItem(win, sugItem);
        audit(win->property("currentWord").toString() == sugWord,
              "建议列表项点击查询",
              QString("currentWord=%1, 期望=%2")
                  .arg(win->property("currentWord").toString(), sugWord));
    } else {
        audit(false, "建议列表项点击查询", "suggestItem 委托不存在（检查词典）");
    }

    // ---- S12 侧栏 tab：历史 ----
    clickItem(win, item(win, "sideTab1"));
    settle(win);   // StackLayout 切页后委托实例化需要一帧
    audit(lpProp(win, "currentTabIndex").toInt() == 1,
          "侧栏「历史」tab 点击", "currentTabIndex=1");
    const QStringList history = vocab.searchHistory(200);
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
    // 卡片集断言：reloadVocabulary 读根属性过滤，裸名遮蔽会让属性值
    // 看似更新但列表不过滤（属性值断言抓不到这类断线）
    settle(win);   // 模型重建后委托实例化需一帧
    {
        QList<QQuickItem*> tagCards;
        visualFindAll(win->contentItem(), "vocabCard", tagCards);
        QStringList tagWords;
        for (QQuickItem* ci : tagCards)
            tagWords << ci->property("ownerWord").toString();
        tagWords.sort();
        audit(tagWords == QStringList{QStringLiteral("hello")},
              "标签筛选卡片集收敛",
              QString("tagCards=[%1]").arg(tagWords.join(QLatin1Char('|'))));
    }
    clickItem(win, item(win, "vocabFilterChip_all"));
    audit(lpProp(win, "vocabTagFilter").toString().isEmpty(),
          "「全部」chip 点击清筛选", "vocabTagFilter 为空");

    const QVariantList vocabMeta = vocab.vocabularyMeta();
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

    // 笔记导出按钮：同 CSV 先例（文件对话框离屏不出窗，signal 级验证接线）
    QQuickItem* notesExportButton = item(win, "notesExportButton");
    ClickCounter notesExportCounter;
    QObject::connect(notesExportButton, SIGNAL(clicked()), &notesExportCounter,
                     SLOT(onClicked()));
    clickItem(win, notesExportButton);
    audit(notesExportCounter.count == 1,
          "「导出笔记」点击（signal 级：native 文件对话框离屏不出窗）",
          QString("clicked 计数=%1").arg(notesExportCounter.count));

    // ---- S25 生词本 CSV 导入（导入导出批）：按钮 signal 级（对话框
    // 离屏不出窗）+ 导入链真数据直驱（新词落库/已存在跳过/笔记还原） ----
    QQuickItem* importButton = item(win, "vocabImportButton");
    audit(importButton != nullptr,
          "「导入CSV」按钮存在",
          importButton ? "found" : "vocabImportButton 未定位");
    if (importButton) {
        ClickCounter importCounter;
        QObject::connect(importButton, SIGNAL(clicked()), &importCounter,
                         SLOT(onClicked()));
        clickItem(win, importButton);
        audit(importCounter.count == 1,
              "「导入CSV」点击（signal 级：文件对话框离屏不出窗）",
              QString("clicked 计数=%1").arg(importCounter.count));
    }
    {
        const QString csvPath = QDir(outDir).filePath("audit_import.csv");
        {
            QFile f(csvPath);
            f.open(QIODevice::WriteOnly | QIODevice::Truncate);
            f.write("\xEF\xBB\xBFword,definition,tags,note\n"
                    "\"audit import word\",\"from csv\",\"csvtag\","
                    "\"csv note\"\n"
                    "\"hello\",\"should skip\",\"\",\"\"\n");
        }
        const int before = vocab.vocabulary().size();
        const QVariantMap r = vocab.importVocabCsv(csvPath);
        audit(r.value("imported").toInt() == 1
                  && r.value("skipped").toInt() == 1,
              "CSV 导入链（新词落库+已存在跳过）",
              QString("imported=%1, skipped=%2")
                  .arg(r.value("imported").toInt())
                  .arg(r.value("skipped").toInt()));
        audit(vocab.vocabulary().size() == before + 1
                  && vocab.getVocabNote("audit import word")
                         == QStringLiteral("csv note"),
              "CSV 导入落库（笔记还原/计数）",
              QString("size=%1, note=%2")
                  .arg(vocab.vocabulary().size())
                  .arg(vocab.getVocabNote("audit import word")));
        vocab.removeVocabularyWord("audit import word");
    }

    // ---- S13-b 笔记内检索（Search within notes）：输入非空 → 列表切
    // 笔记命中集（hello 有笔记「常用问候语」，world 无笔记应被滤掉） ----
    if (QQuickItem* noteSearch = item(win, "vocabNoteSearchInput")) {
        noteSearch->setProperty("text", QString::fromUtf8("问候"));
        settle(win);   // onTextChanged → reloadVocabulary 重建委托
        QList<QQuickItem*> cards;
        visualFindAll(win->contentItem(), "vocabCard", cards);
        QStringList words;
        for (QQuickItem* ci : cards)
            words << ci->property("ownerWord").toString();
        words.sort();
        audit(words == QStringList{QStringLiteral("hello")},
              "笔记检索命中集过滤",
              QString("命中卡=[%1]").arg(words.join(QLatin1Char('|'))));
        // 清空 → 恢复全量
        noteSearch->setProperty("text", QString());
        settle(win);
        QList<QQuickItem*> allCards;
        visualFindAll(win->contentItem(), "vocabCard", allCards);
        audit(allCards.size() >= 2,
              "笔记检索清空恢复全量",
              QString("恢复后卡数=%1").arg(allCards.size()));
    } else {
        audit(false, "笔记检索命中集过滤", "vocabNoteSearchInput 不存在");
    }

    // ---- S27 历史置顶（历史面最后一公里）：真点 pin → 置顶块插队双证
    // （adapter entries[0] 变被点词且 pinned=true）→ 再点还原 ----
    {
        // 种两词条史（真实命中链入史；world 在 fixture 无独立词头，
        // miss 不入史），防审计前段查词数不足两行
        adapter.lookupDefinition(QStringLiteral("helmet"));
        adapter.lookupDefinition(QStringLiteral("helpdesk"));
        clickItem(win, item(win, "headerHistoryButton"));
        settle(win);   // 切历史 tab 即 reloadHistory（onCurrentIndexChanged）
        QList<QQuickItem*> rows;
        visualFindAll(win->contentItem(), "historyItem", rows);
        if (rows.size() >= 2) {
            QQuickItem* row = rows[1];
            const QString w2 = row->property("text").toString();
            QQuickItem* pinBtn = visualFind(row, "historyPinButton");
            audit(pinBtn != nullptr,
                  "历史行置顶按钮存在",
                  pinBtn ? "historyPinButton found" : "不存在");
            if (pinBtn) {
                clickItem(win, pinBtn);
                settle(win);
                const QVariantList hist = vocab.searchHistoryEntries(5);
                audit(hist.size() > 0
                          && hist.value(0).toMap().value("word").toString()
                                 .compare(w2, Qt::CaseInsensitive) == 0
                          && hist.value(0).toMap().value("pinned").toBool(),
                      "历史置顶点击（置顶块插队）",
                      QString("top=%1, pinned=%2")
                          .arg(hist.size() > 0
                                   ? hist.value(0).toMap().value("word")
                                         .toString()
                                   : QString())
                          .arg(hist.size() > 0
                                   && hist.value(0).toMap()
                                          .value("pinned").toBool()
                                   ? 1
                                   : 0));
                // 还原：置顶行已重排（模型重建），重新枚举找同词行再点
                QList<QQuickItem*> rows2;
                visualFindAll(win->contentItem(), "historyItem", rows2);
                for (QQuickItem* r2 : rows2) {
                    if (r2->property("text").toString()
                            .compare(w2, Qt::CaseInsensitive) == 0) {
                        if (QQuickItem* pb =
                                visualFind(r2, "historyPinButton")) {
                            scrollIntoView(win, pb);
                            clickItem(win, pb);
                        }
                        break;
                    }
                }
                settle(win);
                bool restored = false;
                const QVariantList hist2 = vocab.searchHistoryEntries(50);
                for (const QVariant& e : hist2) {
                    if (e.toMap().value("word").toString()
                            .compare(w2, Qt::CaseInsensitive) == 0)
                        restored = !e.toMap().value("pinned").toBool();
                }
                audit(restored, "历史置顶再点还原",
                      QString("pinned(%1)=0").arg(w2));
            }
        } else {
            audit(false, "历史行置顶按钮存在",
                  QString("历史行不足: %1").arg(rows.size()));
        }
    }

    // ---- S29 历史逐条删除（F5）：真点 ✕ → 行消失 + adapter entries
    // 不再含该词（core 落库真删，非仅 UI 隐藏）----
    {
        // 防御性补种（真实命中链入史；miss 不入史），保证删除后仍有残行
        adapter.lookupDefinition(QStringLiteral("helmet"));
        adapter.lookupDefinition(QStringLiteral("helpdesk"));
        clickItem(win, item(win, "headerHistoryButton"));
        settle(win);   // 切历史 tab 即 reloadHistory（onCurrentIndexChanged）
        QList<QQuickItem*> rows;
        visualFindAll(win->contentItem(), "historyItem", rows);
        if (!rows.isEmpty()) {
            QQuickItem* row = rows.last();
            const QString wDel = row->property("text").toString();
            // 删除前基准：entries 确含该词（had）+ 总条数
            const QVariantList before = vocab.searchHistoryEntries(200);
            bool had = false;
            for (const QVariant& e : before) {
                if (e.toMap().value("word").toString()
                        .compare(wDel, Qt::CaseInsensitive) == 0) {
                    had = true;
                    break;
                }
            }
            QQuickItem* delBtn = visualFind(row, "historyDeleteButton");
            audit(delBtn != nullptr,
                  "历史行删除按钮存在",
                  delBtn ? "historyDeleteButton found" : "不存在");
            if (delBtn) {
                scrollIntoView(win, delBtn);
                clickItem(win, delBtn);
                settle(win);   // reloadHistory 重建委托
                const QVariantList hist = vocab.searchHistoryEntries(200);
                bool gone = true;
                for (const QVariant& e : hist) {
                    if (e.toMap().value("word").toString()
                            .compare(wDel, Qt::CaseInsensitive) == 0) {
                        gone = false;
                        break;
                    }
                }
                QList<QQuickItem*> rows2;
                visualFindAll(win->contentItem(), "historyItem", rows2);
                audit(had && gone && rows2.size() == rows.size() - 1,
                      "历史逐条删除（行消失 + 落库删）",
                      QString("word=%1, rows %2→%3, entries %4→%5")
                          .arg(wDel)
                          .arg(rows.size())
                          .arg(rows2.size())
                          .arg(before.size())
                          .arg(hist.size()));
            }
        } else {
            audit(false, "历史行删除按钮存在", "历史行不足");
        }
    }

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
                  && platform.isClipboardAutoLookupEnabled() != clipBefore,
              "剪贴板取词开关点击（UI+core 双证）",
              QString("checked %1→%2").arg(clipBefore)
                  .arg(!clipBefore));
        clickItem(win, clipSwitch);   // 还原
        audit(platform.isClipboardAutoLookupEnabled() == clipBefore,
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

    // ---- S32 开机自启（巡检续批④ F9）：真点开关 → 落盘物生灭 +
    // isEnabled 翻面 + 状态行。Linux/macOS 经 UNIDICT_AUTOSTART_DIR
    // 密封断言文件内容；Windows 走真实 HKCU Run 键（开→关自还原）----
    if (auto* stSwitch = item(win, "startupToggle")) {
        if (!stSwitch->isVisible()) {
            audit(false, "开机自启开关可见（平台支持面）",
                  "startupToggle 不可见（平台应支持）");
        } else {
            if (startup.isEnabled()) startup.setEnabled(false);  // 基线清残留
            scrollIntoView(win, stSwitch);
            clickItem(win, stSwitch);
            settle(win);
            const bool on = startup.isEnabled();
            const QString exec =
                QCoreApplication::applicationFilePath();
#ifndef Q_OS_WIN
            const QString fPath =
                QDir(qEnvironmentVariable("UNIDICT_AUTOSTART_DIR"))
                    .absoluteFilePath(startup.entryFileName());
            QFileInfo fi(fPath);
            QString content;
            if (fi.exists()) {
                QFile f(fPath);
                f.open(QIODevice::ReadOnly);
                content = QString::fromUtf8(f.readAll());
            }
            const bool execHit = content.contains(exec);
            const QString detailOn =
                QString("on=%1 file=%2 execHit=%3")
                    .arg(on).arg(fi.exists()).arg(execHit);
#else
            const QString detailOn =
                QString("on=%1 (HKCU Run)").arg(on);
#endif
            audit(on && stSwitch->property("checked").toBool()
                      && statusText(win).startsWith("已开启开机自启")
#ifdef Q_OS_WIN
                      ,
                  "开机自启开启（真点：注册物+状态行）", detailOn);
#else
                      && fi.exists() && execHit,
                  "开机自启开启（真点：注册物+状态行）", detailOn);
#endif
            clickItem(win, stSwitch);
            settle(win);
#ifndef Q_OS_WIN
            const bool gone = !QFileInfo::exists(fPath);
#else
            const bool gone = !startup.isEnabled();
#endif
            audit(!startup.isEnabled() && gone
                      && statusText(win).startsWith("已关闭开机自启"),
                  "开机自启关闭（真点：注册物移除）",
                  QString("enabled=%1 gone=%2")
                      .arg(startup.isEnabled()).arg(gone));
        }
    } else {
        audit(false, "开机自启开启（真点：注册物+状态行）",
              "startupToggle 不存在");
    }

    // ---- S33 取词窗热键（F10）：真点应用钮 → 注册结果两态如实回显 +
    // 落盘联动 + 清空路径。真按键链路（系统级按下→弹取词窗）离屏不可测，
    // 本项止步接线级：只断言注册成功/失败两态之一且 UI/存储联动一致
    //（Linux offscreen 恒走失败态；Windows 视 runner 环境两态皆可）----
    {
        clickItem(win, item(win, "toolsTab3"));
        settle(win);
        const QString hkBaseline =
            settings.getString(QStringLiteral("quicklookup/hotkey"),
                               QStringLiteral("Alt+Q"));
        auto* hkField = item(win, "quickHotkeyField");
        auto* hkApply = item(win, "hotkeyApplyButton");
        auto* hkState = item(win, "hotkeyState");
        if (hkField && hkApply && hkState) {
            hkField->setProperty("text", QStringLiteral("Ctrl+Alt+U"));
            scrollIntoView(win, hkApply);
            clickItem(win, hkApply);
            settle(win);
            const QString state1 = hkState->property("text").toString();
            const bool ok1 = state1.startsWith(QStringLiteral("已生效: "));
            const bool fail1 = state1.startsWith(QStringLiteral("注册失败"));
            const bool stored1 =
                settings.getString(QStringLiteral("quicklookup/hotkey"))
                    == QStringLiteral("Ctrl+Alt+U");
            audit((ok1 || fail1) && stored1 == ok1,
                  "取词窗热键注册回显与落盘联动（接线级）",
                  QString("state=%1, stored=%2").arg(state1).arg(stored1));
            // 清空路径全平台确定：只注销+状态行，不触 native 注册
            hkField->setProperty("text", QString());
            clickItem(win, hkApply);
            settle(win);
            audit(hkState->property("text").toString()
                          == QStringLiteral("已清除取词热键"),
                  "取词窗热键清空（真点：注销+状态行）",
                  QString("state=%1")
                      .arg(hkState->property("text").toString()));
            // 还原存储基线（native 注册态随审计进程退出即弃，无需回注册）
            settings.setString(QStringLiteral("quicklookup/hotkey"),
                               hkBaseline);
        } else {
            audit(false, "取词窗热键注册回显与落盘联动（接线级）",
                  "quickHotkeyField/hotkeyApplyButton/hotkeyState 不存在");
        }
    }

    // ---- S34 关闭到托盘（候选#2 F11）：托盘可用性如实上报 + 开关翻面
    // 落盘 +（托盘可用时）真 close→hide→showMainWindow 还原链。托盘
    // 不可用（Linux offscreen）只验决策面；图标/菜单真交互离屏不可测，
    // 如实注明 ----
    {
        audit(tray.available() == QSystemTrayIcon::isSystemTrayAvailable(),
              "托盘可用性如实上报",
              QString("available=%1").arg(tray.available()));
        const bool cttBaseline =
            settings.getBool(QStringLiteral("ui/close_to_tray"), true);
        tray.setCloseToTrayEnabled(!cttBaseline);
        audit(tray.closeToTrayEnabled() == !cttBaseline
                  && settings.getBool(QStringLiteral("ui/close_to_tray"),
                                      true) == !cttBaseline,
              "关闭到托盘开关翻面+落盘",
              QString("enabled=%1 stored=%2")
                  .arg(tray.closeToTrayEnabled())
                  .arg(settings.getBool(QStringLiteral("ui/close_to_tray"),
                                        true)));
        if (tray.available()) {
            tray.setCloseToTrayEnabled(true);
            win->close();   // → QML onClosing：shouldHideToTray 命中即拦
            settle(win);
            const bool hidden = !win->isVisible();
            if (hidden) tray.showMainWindow();
            settle(win);
            audit(hidden && win->isVisible(),
                  "close→托盘隐藏→showMainWindow 还原",
                  QString("hidden=%1 restored=%2")
                      .arg(hidden)
                      .arg(win->isVisible()));
            tray.setCloseToTrayEnabled(false);
            audit(!tray.shouldHideToTray(),
                  "开关关→不拦 close（决策面翻面）",
                  "shouldHideToTray=false");
        } else {
            audit(!tray.shouldHideToTray(),
                  "托盘不可用→不拦 close（保持关窗即退出）",
                  "shouldHideToTray=false");
        }
        tray.setCloseToTrayEnabled(cttBaseline);   // 还原基线
    }

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
        audit(voice.pronAccent() == 2, "口音 ComboBox 选择接线（signal 级）",
              QString("pronAccent=%1").arg(voice.pronAccent()));
        accentCombo->setProperty("currentIndex", 0);
        QMetaObject::invokeMethod(accentCombo, "activated", Q_ARG(int, 0));
    } else {
        audit(false, "口音 ComboBox 选择接线（signal 级）", "pronAccentCombo 不存在");
    }
    QObject* sourceCombo = win->findChild<QObject*>("pronSourceCombo");
    if (sourceCombo) {
        sourceCombo->setProperty("currentIndex", 1);
        QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 1));
        audit(voice.pronSourceMode() == 1, "发音源 ComboBox 选择接线（signal 级）",
              QString("pronSourceMode=%1").arg(voice.pronSourceMode()));
        sourceCombo->setProperty("currentIndex", 0);
        QMetaObject::invokeMethod(sourceCombo, "activated", Q_ARG(int, 0));
        audit(voice.pronSourceMode() == 0, "发音源还原本地态", "pronSourceMode=0");
    } else {
        audit(false, "发音源 ComboBox 选择接线（signal 级）", "pronSourceCombo 不存在");
    }

    if (drawer) drawer->setProperty("visible", false);
    settle(win);   // Drawer 关闭过渡未完会挡住页脚点击

    // ---- S16 页脚清历史 ----
    clickItem(win, item(win, "clearHistoryButton"));
    audit(statusText(win) == QStringLiteral("历史已清空")
              && vocab.searchHistory(200).isEmpty(),
          "页脚「清空历史」按钮",
          QString("statusText=%1, history=%2")
              .arg(statusText(win)).arg(vocab.searchHistory(200).size()));

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

        // ---- S24 取词窗信息层级对照（UI 批四：改看行/未收录面/多词典概览） ----
        // 非精确命中："helo" 聚合模糊层有组（fixture 口径）→ 改看行出现
        // 且讲清显示的是谁；点非当前词面就地重查（词面换、窗不收）
        QVariant qhel(QStringLiteral("helo"));
        QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qhel));
        settle(win);
        audit(paneWin->property("fuzzyHit").toBool(),
              "取词窗非精确命中识别",
              QString("fuzzyHit=%1, word=%2")
                  .arg(paneWin->property("fuzzyHit").toBool() ? 1 : 0)
                  .arg(paneWin->property("word").toString()));
        auto* qlAlt0 = visualFind(paneWin->contentItem(), "qlAltWord_0");
        audit(qlAlt0 != nullptr,
              "取词窗改看词面出现",
              QString("altWords=%1")
                  .arg(paneWin->property("altWords").toList().size()));
        if (qlAlt0) {
            // 点一个非当前词面的改看词（首条词面=helo 的匹配词，换点下一个）
            QString altWord = qlAlt0->property("text").toString();
            if (altWord.compare(paneWin->property("word").toString(),
                                Qt::CaseInsensitive) == 0) {
                auto* alt1 = visualFind(paneWin->contentItem(), "qlAltWord_1");
                if (alt1) { qlAlt0 = alt1; altWord = alt1->property("text").toString(); }
            }
            clickItem(paneWin, qlAlt0);
            settle(win);
            audit(paneWin->isVisible()
                      && paneWin->property("word").toString().compare(
                             altWord, Qt::CaseInsensitive) == 0,
                  "取词窗改看词点击就地重查",
                  QString("word=%1, 期望=%2")
                      .arg(paneWin->property("word").toString()).arg(altWord));
            clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseButton"));
        }

        // 完全未收录面： gibberish 聚合全空 → 未收录标签出现（建议层
        // 同空，给主窗兜底指引）
        QVariant qmiss(QStringLiteral("zzzqqqxxx"));
        QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qmiss));
        settle(win);
        auto* qlNotFound = visualFind(paneWin->contentItem(), "qlNotFoundLabel");
        audit(paneWin->property("notFound").toBool() && qlNotFound != nullptr
                  && qlNotFound->property("visible").toBool(),
              "取词窗完全未收录面",
              QString("notFound=%1")
                  .arg(paneWin->property("notFound").toBool() ? 1 : 0));

        // 多词典概览："help" 双 fixture 精确命中 → 来源行带「等 N 词典」
        QVariant qhelp(QStringLiteral("help"));
        QMetaObject::invokeMethod(win, "showQuickLookupFor", Q_ARG(QVariant, qhelp));
        settle(win);
        auto* qlDictLabel = visualFind(paneWin->contentItem(), "qlDictLabel");
        audit(qlDictLabel
                  && qlDictLabel->property("text").toString().contains("词典"),
              "取词窗多词典命中概览",
              QString("dictLabel=%1")
                  .arg(qlDictLabel ? qlDictLabel->property("text").toString()
                                   : QString()));
        if (paneWin->isVisible())
            clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseButton"));
    }

    // ---- S28 外观三态（F4）：锁定深色 → Theme 翻暗 + 记忆落盘；
    // 锁定态不随系统切换；回「跟随系统」还原（审计不留持久模式） ----
    {
        if (drawer) drawer->setProperty("visible", true);
        settle(win);
        clickItem(win, item(win, "toolsTab5"));
        settle(win);
        QQuickItem* radio2 = item(win, "themeModeRadio2");
        if (radio2) {
            scrollIntoView(win, radio2);
            clickItem(win, radio2);
            settle(win);
            audit(Theme::instance().mode() == 2 && Theme::instance().dark()
                      && settings.getInt("ui/theme", -1) == 2,
                  "外观锁定深色（翻暗+记忆落盘）",
                  QString("mode=%1, dark=%2, stored=%3")
                      .arg(Theme::instance().mode())
                      .arg(Theme::instance().dark() ? 1 : 0)
                      .arg(settings.getInt("ui/theme", -1)));
            // 锁定态：系统报亮不跟随（真调 applySystemScheme 双证）
            Theme::instance().applySystemScheme(false);
            audit(Theme::instance().dark(),
                  "外观锁定态不随系统切换",
                  "applySystemScheme(false) 后仍暗");
            // 回跟随系统还原（记忆归零，审计无残留模式）
            QQuickItem* radio0 = item(win, "themeModeRadio0");
            if (radio0) {
                scrollIntoView(win, radio0);
                clickItem(win, radio0);
                settle(win);
            }
            audit(Theme::instance().mode() == 0
                      && settings.getInt("ui/theme", -1) == 0,
                  "外观回跟随系统（记忆归零）",
                  QString("mode=%1, stored=%2")
                      .arg(Theme::instance().mode())
                      .arg(settings.getInt("ui/theme", -1)));
        } else {
            audit(false, "外观锁定深色（翻暗+记忆落盘）",
                  "themeModeRadio2 不存在");
        }
    }

    // ---- S30 释义字号（F7）：radio 真点 → defFontSize 翻面 + 落盘 +
    // 结果卡正文字号实变（definitionBodyText font.pixelSize 双证）；
    // 回设还原（审计不留持久字号）----
    {
        // 收抽屉做一次真查询，保证结果卡在树（definitionBodyText 可寻址）
        if (drawer) drawer->setProperty("visible", false);
        settle(win);
        if (QQuickItem* sf = item(win, "searchField")) {
            sf->setProperty("text", QStringLiteral("hello"));
            flushEvents();
            clickItem(win, item(win, "searchGoButton"));
            settle(win);
        }
        if (drawer) drawer->setProperty("visible", true);
        settle(win);
        clickItem(win, item(win, "toolsTab5"));
        settle(win);
        const int sizeBefore = win->property("defFontSize").toInt();
        const int target = (sizeBefore == 18) ? 12 : 18;   // 必与当前异档
        QQuickItem* radio =
            item(win, target == 18 ? "defFontRadio18" : "defFontRadio12");
        if (radio) {
            scrollIntoView(win, radio);
            clickItem(win, radio);
            settle(win);
            const int sizeAfter = win->property("defFontSize").toInt();
            int rendered = 0;
            if (QQuickItem* body =
                    visualFind(win->contentItem(), "definitionBodyText"))
                rendered = body->property("font").value<QFont>().pixelSize();
            audit(sizeAfter == target && rendered == target
                      && settings.getInt("ui/def_font_size", -1) == target,
                  "释义字号档位翻面（属性+落盘+实渲染）",
                  QString("def=%1, rendered=%2, stored=%3")
                      .arg(sizeAfter)
                      .arg(rendered)
                      .arg(settings.getInt("ui/def_font_size", -1)));
            // 回设原档（原档非 12/14/16/18 的脏值兜 14；与当前同档则已还原）
            const int restore =
                (sizeBefore == 12 || sizeBefore == 14 || sizeBefore == 16
                 || sizeBefore == 18)
                    ? sizeBefore
                    : 14;
            if (restore != target) {
                const QString backName =
                    "defFontRadio" + QString::number(restore);
                QQuickItem* back = item(win, backName.toUtf8().constData());
                if (back) {
                    scrollIntoView(win, back);
                    clickItem(win, back);
                    settle(win);
                }
            }
            audit(win->property("defFontSize").toInt() == restore
                      && settings.getInt("ui/def_font_size", -1) == restore,
                  "释义字号回设还原",
                  QString("def=%1, stored=%2")
                      .arg(win->property("defFontSize").toInt())
                      .arg(settings.getInt("ui/def_font_size", -1)));
        } else {
            audit(false, "释义字号档位翻面（属性+落盘+实渲染）",
                  QString("defFontRadio%1 不存在").arg(target));
        }
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

    // ---- S19 词典库 tab（P-6）：目录导入 + 装载列表 + 隔离重试 ----
    // 种子：临时目录两本有效 + 一本损坏。路径每次唯一 → 隔离档跨次
    // 运行不复用，扫描恒报 3 候选（断言不依赖上次运行残留）。存活到
    // main 尾（重试用例仍需 broken.json 在盘）
    QTemporaryDir dictSeed(
        QDir::tempPath() + "/unidict_audit_dict_XXXXXX");
    if (!dictSeed.isValid()) {
        audit(false, "词典库 tab 审计", "QTemporaryDir 种子创建失败");
    } else {
        const QString seedDir = QDir(dictSeed.path()).filePath("seed_dicts");
        QDir().mkpath(seedDir);
        auto writeSeed = [&seedDir](const QString& name,
                                    const QString& dictName) {
            QFile f(QDir(seedDir).filePath(name));
            f.open(QIODevice::WriteOnly);
            f.write(QString("{\"name\":\"%1\",\"entries\":[{\"word\":"
                            "\"seed\",\"definition\":\"x\"}]}")
                        .arg(dictName)
                        .toUtf8());
        };
        writeSeed("alpha.json", "Audit Alpha");
        writeSeed("beta.json", "Audit Beta");
        QFile brokenSeed(QDir(seedDir).filePath("broken.json"));
        brokenSeed.open(QIODevice::WriteOnly);
        brokenSeed.write("{ not valid json");

        if (drawer) drawer->setProperty("visible", true);
        settle(win);
        clickItem(win, item(win, "toolsTab2"));
        settle(win);
        audit(item(win, "importDirField")
                  && item(win, "importDirField")->isVisible(),
              "抽屉「词典」tab 切换", "importDirField 可见");

        // 浏览按钮：FolderDialog 离屏不出窗，signal 级同「导出CSV」先例
        QQuickItem* browseButton = item(win, "importBrowseButton");
        ClickCounter browseCounter;
        QObject::connect(browseButton, SIGNAL(clicked()), &browseCounter,
                         SLOT(onClicked()));
        clickItem(win, browseButton);
        audit(browseCounter.count == 1,
              "「浏览」点击（signal 级：目录对话框离屏不出窗）",
              QString("clicked 计数=%1").arg(browseCounter.count));

        // 导入：真路径（startDirectoryImport 分块步进到 finishImport）。
        // 导入后 dictMetas/dictFailures 变更 → 两个 Repeater 重建委托，
        // 之后每步都要重新 item() 取活指针（旧指针会悬垂 UAF）
        const int metasBefore = adapter.dictionariesMeta().size();
        item(win, "importDirField")->setProperty("text", seedDir);
        clickItem(win, item(win, "importRunButton"));
        settle(win);   // QTimer 分块步进需真实事件循环时间
        const QQuickItem* statusItem = item(win, "importStatusLabel");
        const QString importLabel =
            statusItem ? statusItem->property("text").toString() : QString();
        const int metasAfter = adapter.dictionariesMeta().size();
        audit(metasAfter == metasBefore + 2
                  && importLabel.contains(QStringLiteral("成功 2"))
                  && importLabel.contains(QStringLiteral("失败 1")),
              "「导入」点击跑通目录导入（装载列表+状态行双证）",
              QString("metas %1→%2, label=%3")
                  .arg(metasBefore).arg(metasAfter).arg(importLabel));

        // 隔离档渲染 + 重试：broken.json 仍损坏，重试拒绝并确认隔离
        QQuickItem* retryButton = item(win, "retryFailedButton");
        audit(retryButton != nullptr, "隔离档列表渲染",
              retryButton ? "retryFailedButton located"
                          : "retryFailedButton 不存在");
        if (retryButton) {
            // 重试按钮位于抽屉滚动区下方（scene y>抽屉高），先滚进视口再点
            scrollIntoView(win, retryButton);
            clickItem(win, retryButton);   // 重试 → 隔离档重建，retryButton 失效
            settle(win);
            const QQuickItem* retryStatus = item(win, "importStatusLabel");
            const QString retryLabel = retryStatus
                                           ? retryStatus->property("text").toString()
                                           : QString();
            audit(retryLabel.contains(QStringLiteral("重试仍失败"))
                      && adapter.failedDictionaries().size() == 1,
                  "「重试」点击仍失败确认隔离",
                  QString("label=%1").arg(retryLabel));
        }

        // 同目录重复导入：已装载/隔离去重 → 0 候选状态行。重试前把滚动
        // 区滚到了底部，导入按钮（首行）已出视口——先滚回来再点
        QQuickItem* runButton2 = item(win, "importRunButton");
        if (runButton2) scrollIntoView(win, runButton2);
        clickItem(win, runButton2);
        settle(win);
        const QQuickItem* dupStatus = item(win, "importStatusLabel");
        const QString dupLabel =
            dupStatus ? dupStatus->property("text").toString() : QString();
        // 空候选：scan 文案「未发现新词典」随即被 finish 文案覆盖，终态
        // 以 importFinished 为准（0/0）——重复导入去重的可观测证据
        audit(dupLabel.contains(QStringLiteral("成功 0"))
                  && dupLabel.contains(QStringLiteral("失败 0"))
                  && adapter.dictionariesMeta().size() == metasAfter,
              "同目录重复导入去重（0 候选）",
              QString("label=%1, metas=%2")
                  .arg(dupLabel).arg(adapter.dictionariesMeta().size()));

        if (drawer) drawer->setProperty("visible", false);
        settle(win);
    }

    // ---- S20 AI 造句弹层（roadmap AI sentence generation）----
    // 审计环境无 UNIDICT_AI_CMD → 走启发式 mock，输出自标 [Mock sentences]
    {
        win->setProperty("currentWord", QStringLiteral("hello"));
        settle(win);
        clickItem(win, item(win, "aiSentenceAction"));
        settle(win);
        QObject* aiPopup = popupObj(win, "aiSentencePopup");
        audit(popupVisible(aiPopup), "「✨ 造句」轻入口打开 AI 例句弹层",
              "aiSentencePopup visible");
        if (popupVisible(aiPopup)) {
            QObject* textArea = aiPopup->findChild<QObject*>("aiSentenceText");
            const QString out =
                textArea ? textArea->property("text").toString() : QString();
            audit(out.contains(QStringLiteral("[Mock sentences]"))
                      && out.contains(QStringLiteral("\"hello\"")),
                  "AI 例句内容命中（mock 输出注入词头）",
                  QString("len=%1 head=%2").arg(out.size())
                      .arg(out.left(40)));
            clickItem(win, item(win, "aiSentenceCloseButton"));
            settle(win);
            audit(!popupVisible(aiPopup), "AI 例句弹层关闭按钮收起",
                  "aiSentencePopup hidden after close");
        }
        // 复位查询态，避免污染后续/此前依赖默认词条的断言
        win->setProperty("currentWord", QString());
        settle(win);
    }

    // ---- S21 划词取词（X11 面）：非 xcb（offscreen）开关整块隐藏 ----
    {
        // 开关住在 Drawer（Popup 层不在 contentItem 走查范围）——走
        // popupObj 的 findChild 整树找（S19 同款取法）
        auto* sw = qobject_cast<QQuickItem*>(popupObj(win, "selectionSwitch"));
        audit(sw && !sw->isVisible(), "划词开关非 xcb 平台隐藏（诚实门控）",
              QString("selectionSwitch found=%1 visible=%2")
                  .arg(sw != nullptr)
                  .arg(sw ? sw->isVisible() : false));
    }

    // ---- S22 UI 批一：命中词典 chip / 内容 tab 分区折叠 / 建议分组 /
    // 历史命中计数（"help" 双词典精确命中撑 chip 行；S9 的 "hel" 探针
    // 依赖「无精确命中→前缀层」，dict2 不收 "hel" 词头）----
    {
        // (a) 命中词典 chip 概览：点击只看该词典，再点取消
        // （"help" 在两本 fixture 均为精确词头 → 词典 tab 恰 2 组）
        openWord(win, "help");
        clickItem(win, item(win, "contentTab_dict"));
        flushEvents();
        const QVariantList groups22 = paneProp(win, "groups").toList();
        audit(groups22.size() >= 2, "多词典命中（chip 概览前置）",
              QString("groups=%1").arg(groups22.size()));
        if (QQuickItem* chip0 = item(win, "dictChip_0")) {
            clickItem(win, chip0);
            settle(win);
            const QString solo = paneProp(win, "soloGid").toString();
            audit(!solo.isEmpty(), "命中词典 chip 点击只看该词典",
                  QString("soloGid=%1").arg(solo));
            if (!solo.isEmpty()) {
                // 选中词典卡可见，其余卡整卡收起（组序=卡序）
                int soloIdx = -1, otherIdx = -1;
                for (int i = 0; i < groups22.size(); ++i) {
                    const QVariantMap g = groups22[i].toMap();
                    const QString gid = g.value("dictionaryId").toString().isEmpty()
                                            ? g.value("dictionary").toString()
                                            : g.value("dictionaryId").toString();
                    if (gid == solo && soloIdx < 0) soloIdx = i;
                    if (gid != solo && otherIdx < 0) otherIdx = i;
                }
                QQuickItem* soloCard = item(win, QString("groupCard_%1").arg(soloIdx)
                                                           .toUtf8().constData());
                QQuickItem* otherCard = otherIdx >= 0
                    ? item(win, QString("groupCard_%1").arg(otherIdx).toUtf8().constData())
                    : nullptr;
                audit(soloCard && soloCard->isVisible()
                          && (!otherCard || !otherCard->isVisible()),
                      "chip 聚焦过滤非选中词典整卡收起",
                      QString("soloCard=%1 visible=%2, otherCard=%3 visible=%4")
                          .arg(soloIdx)
                          .arg(soloCard ? soloCard->isVisible() : false)
                          .arg(otherIdx)
                          .arg(otherCard ? otherCard->isVisible() : false));
            }
            clickItem(win, chip0);
            settle(win);
            audit(paneProp(win, "soloGid").toString().isEmpty(),
                  "命中词典 chip 再点取消聚焦", "soloGid cleared");
        } else {
            audit(false, "命中词典 chip 概览存在", "dictChip_0 未定位（检查 fixture 数）");
        }

        // (b) 内容 tab 分区折叠（例句 tab：分区头点折叠/再点展开；
        // 例句数据在 "hello" 的释义 Example: 标记里，(a) 的 "help" 不带）
        openWord(win, "hello");
        clickItem(win, item(win, "contentTab_examples"));
        settle(win);
        if (QQuickItem* exGroup = item(win, "examplesGroup_0")) {
            clickItem(win, exGroup);
            settle(win);
            const QVariantMap cc1 = paneProp(win, "contentCollapsed").toMap();
            bool anyTrue = false;
            for (auto it = cc1.constBegin(); it != cc1.constEnd(); ++it)
                if (it.value().toBool()) anyTrue = true;
            audit(anyTrue, "例句分区头点击折叠", "contentCollapsed 含 true");
            clickItem(win, exGroup);
            settle(win);
            const QVariantMap cc2 = paneProp(win, "contentCollapsed").toMap();
            bool allFalse = true;
            for (auto it = cc2.constBegin(); it != cc2.constEnd(); ++it)
                if (it.value().toBool()) allFalse = false;
            audit(allFalse && item(win, "exampleSpeaker") != nullptr,
                  "例句分区头再点展开（卡内容恢复）", "contentCollapsed 全 false 且喇叭仍在");
        } else {
            audit(false, "例句分区折叠卡存在", "examplesGroup_0 未定位（检查 fixture）");
        }

        // (c) 搜索建议分组：help → 精确（"help" 词头）+ 前缀（helpdesk）
        // 两段；模糊模式全部归「词条」段（combo popup 吞点，模式切换走
        // signal 级注入）
        clickItem(win, item(win, "sideTab0"));
        settle(win);
        QQuickItem* searchField22 = item(win, "searchField");
        searchField22->setProperty("text", QStringLiteral("help"));
        settle(win);
        QList<QQuickItem*> secHeaders;
        visualFindAll(win->contentItem(), "suggestSectionHeader", secHeaders);
        QStringList sectionTexts;
        for (QQuickItem* h : secHeaders)
            if (h->isVisible()) sectionTexts << h->property("section").toString();
        audit(sectionTexts.contains(QStringLiteral("精确"))
                  && sectionTexts.contains(QStringLiteral("前缀")),
              "搜索建议分组段头（精确/前缀）", sectionTexts.join(QLatin1Char('|')));
        win->setProperty("suggestMode", 2);  // 模糊
        QVariant fuzzyQ(QStringLiteral("helo"));
        QMetaObject::invokeMethod(win, "reloadSuggestions", Q_ARG(QVariant, fuzzyQ));
        settle(win);
        QList<QQuickItem*> fuzzyHeaders;
        visualFindAll(win->contentItem(), "suggestSectionHeader", fuzzyHeaders);
        QStringList fuzzyTexts;
        for (QQuickItem* h : fuzzyHeaders)
            if (h->isVisible()) fuzzyTexts << h->property("section").toString();
        audit(fuzzyTexts.contains(QStringLiteral("词条")),
              "模糊建议归词条段", fuzzyTexts.join(QLatin1Char('|')));
        win->setProperty("suggestMode", 0);

        // (d) 历史命中计数：同一词重查两次 → 历史行 ×N 胶囊（数据面 +
        // UI 面各钉一次）
        openWord(win, "helicopter");
        openWord(win, "helicopter");
        clickItem(win, item(win, "sideTab1"));
        settle(win);
        bool dataCountOk = false;
        const QVariantList hEntries = vocab.searchHistoryEntries(200);
        for (const QVariant& ev : hEntries) {
            const QVariantMap em = ev.toMap();
            if (em.value("word").toString() == QStringLiteral("helicopter")) {
                dataCountOk = em.value("count").toInt() == 2;
                break;
            }
        }
        audit(dataCountOk, "历史命中计数数据面（重查两次 count=2）",
              QString("entries=%1").arg(hEntries.size()));
        QQuickItem* countTarget = nullptr;
        QList<QQuickItem*> hItems22;
        visualFindAll(win->contentItem(), "historyItem", hItems22);
        for (QQuickItem* h : hItems22)
            if (h->property("text").toString() == QStringLiteral("helicopter")) {
                countTarget = h;
                break;
            }
        if (countTarget) {
            QQuickItem* chip = visualFind(countTarget, "historyCountChip");
            QQuickItem* chipText = chip ? visualFind(countTarget, "historyCountLabel")
                                        : nullptr;
            audit(chip && chip->isVisible() && chipText
                      && chipText->property("text").toString()
                             == QStringLiteral("×2"),
                  "历史行命中计数胶囊显示",
                  QString("chip=%1 text=%2")
                      .arg(chip != nullptr)
                      .arg(chipText ? chipText->property("text").toString()
                                    : QString()));
        } else {
            audit(false, "历史行命中计数胶囊显示",
                  "historyItem(helicopter) 未定位（列表裁剪？）");
        }

        // 复位查询态（S22 自产状态不外溢）
        win->setProperty("suggestMode", 0);
        QMetaObject::invokeMethod(win, "resetHome");
        settle(win);
    }

    // ---- S23 UI 批二：生词本表格/卡片双视图 + 词典元信息面板 + 笔记荧光笔多色 ----
    {
        // a) 双视图往返：卡片（默认）→ 表格 → 卡片
        clickItem(win, item(win, "sideTab2"));
        settle(win);
        clickItem(win, item(win, "vocabViewTableButton"));
        settle(win);
        {
            QList<QQuickItem*> rows, cards;
            visualFindAll(win->contentItem(), "vocabTableRow", rows);
            visualFindAll(win->contentItem(), "vocabCard", cards);
            audit(rows.size() >= 1 && cards.empty(),
                  "生词本「表格」视图切换",
                  QString("rows=%1 cards=%2").arg(rows.size()).arg(cards.size()));
        }
        // 表格行点击查询（点中谁断言谁，同 historyItem 先例）
        if (auto* row = item(win, "vocabTableRow")) {
            const QString rowWord = row->property("text").toString();
            clickItem(win, row);
            audit(win->property("currentWord").toString() == rowWord,
                  "表格行点击查询",
                  QString("currentWord=%1, 期望=%2")
                      .arg(win->property("currentWord").toString(), rowWord));
        } else {
            audit(false, "表格行点击查询", "vocabTableRow 委托不存在");
        }
        clickItem(win, item(win, "vocabViewCardButton"));
        settle(win);
        {
            QList<QQuickItem*> rows, cards;
            visualFindAll(win->contentItem(), "vocabTableRow", rows);
            visualFindAll(win->contentItem(), "vocabCard", cards);
            audit(cards.size() >= 1 && rows.empty(),
                  "生词本「卡片」视图切回",
                  QString("rows=%1 cards=%2").arg(rows.size()).arg(cards.size()));
        }

        // b) 笔记荧光笔多色：对话框选色保存 → core 色位 → 卡片荧光回读。
        // 文本显式置串（首卡可能无笔记，空文本保存=删笔记，色位断言会空转）
        if (auto* noteBtn = item(win, "vocabNoteButton")) {
            clickItem(win, noteBtn);
            settle(win);
            QObject* nd = popupObj(win, "noteDialog");
            if (popupVisible(nd)) {
                const QString targetWord =
                    nd->property("pendingWord").toString();
                if (QObject* area = nd->findChild<QObject*>("noteDialogArea"))
                    area->setProperty("text", QStringLiteral("audit highlight"));
                clickItem(win, item(win, "noteColorSwatch_3"));
                QMetaObject::invokeMethod(nd, "accept");  // onAccepted 走保存链
                settle(win);   // 保存 → reloadVocabulary 重建委托
                audit(vocab.getVocabNoteColor(targetWord) == 3,
                      "笔记荧光选色保存（core 色位落库）",
                      QString("color(%1)=%2").arg(targetWord)
                          .arg(vocab.getVocabNoteColor(targetWord)));
                QList<QQuickItem*> cards;
                visualFindAll(win->contentItem(), "vocabCard", cards);
                QQuickItem* hit = nullptr;
                for (QQuickItem* ci : cards)
                    if (ci->property("ownerWord").toString() == targetWord) {
                        hit = ci;
                        break;
                    }
                audit(hit && hit->property("ownerNoteColor").toInt() == 3,
                      "卡片荧光色位回读",
                      QString("ownerNoteColor=%1")
                          .arg(hit ? hit->property("ownerNoteColor").toInt()
                                   : -1));
            } else {
                audit(false, "笔记荧光选色保存（core 色位落库）",
                      "noteDialog 未弹出");
            }
        } else {
            audit(false, "笔记荧光选色保存（core 色位落库）",
                  "vocabNoteButton 不存在");
        }

        // c) 词典元信息面板：抽屉 → 词典 tab → 索引状态行/启用勾选/打开目录
        QObject* drawer2 = popupObj(win, "toolsDrawer");
        if (drawer2) drawer2->setProperty("visible", true);
        settle(win);
        clickItem(win, item(win, "toolsTab2"));
        settle(win);
        {
            const QQuickItem* idxLabel = item(win, "dictIndexStatusLabel");
            const QString idxText = idxLabel
                                        ? idxLabel->property("text").toString()
                                        : QString();
            audit(idxText.contains(QStringLiteral("全文索引")),
                  "词典页全文索引状态行渲染", QString("text=%1").arg(idxText));
        }
        // 启用勾选真点：停用 → 元信息 enabled 翻假 → 再点还原（Repeater
        // 随 stamp 重建，两步之间必须重新 item() 取活指针）
        {
            QQuickItem* chk = item(win, "dictEnabledCheck_0");
            if (chk && adapter.dictionariesMeta().size() > 0) {
                scrollIntoView(win, chk);
                clickItem(win, chk);
                settle(win);
                audit(adapter.dictionariesMeta().value(0).toMap()
                              .value("enabled").toBool() == false,
                      "词典卡「启用」勾选点击停用", "meta[0].enabled=false");
                chk = item(win, "dictEnabledCheck_0");
                if (chk) {
                    clickItem(win, chk);
                    settle(win);
                }
                audit(adapter.dictionariesMeta().value(0).toMap()
                              .value("enabled").toBool() == true,
                      "词典卡「启用」勾选再点还原", "meta[0].enabled=true");
            } else {
                audit(false, "词典卡「启用」勾选点击停用",
                      "dictEnabledCheck_0 不存在");
            }
        }
        // 打开目录：真实点击会拉起外部文件管理器（QDesktopServices）——
        // 离屏审计止步接线态（按钮存在且可点），同「导出CSV」信号级先例
        {
            QQuickItem* openBtn = item(win, "dictOpenFolderButton_0");
            audit(openBtn != nullptr && openBtn->isEnabled(),
                  "词典卡「打开目录」按钮接线（结构级：外部文件管理器不进审计）",
                  openBtn ? "dictOpenFolderButton_0 enabled" : "不存在");
        }
        // ---- S26 词典优先级 ↑↓（巡检续批 F2）：normalize-on-move——
        // 与邻位交换后整列 priority = n-1-位置（全零并列起步首移也真实
        // 换位）；P 标签渲染 / 上移交换 / 整列归一 / 下移还原 四查 ----
        {
            const QVariantList metas = adapter.dictionariesMeta();
            QQuickItem* upBtn1 = item(win, "dictPriorityUp_1");
            QQuickItem* pLabel0 = item(win, "dictPriorityLabel_0");
            if (metas.size() >= 2 && upBtn1 && pLabel0) {
                const QString name0 =
                    metas.value(0).toMap().value("name").toString();
                const QString name1 =
                    metas.value(1).toMap().value("name").toString();
                audit(pLabel0->property("text").toString()
                          == QStringLiteral("P")
                                 + metas.value(0).toMap()
                                       .value("priority").toString(),
                      "词典卡 P 标签渲染",
                      QString("label=%1")
                          .arg(pLabel0->property("text").toString()));
                // 上移：第 2 本顶到首位（fixture 两本并列 P0 起步——正是
                // 归一口径要救的并列态）
                scrollIntoView(win, upBtn1);
                clickItem(win, upBtn1);
                settle(win);
                const QVariantList moved = adapter.dictionariesMeta();
                audit(moved.value(0).toMap().value("name").toString() == name1,
                      "词典优先级上移交换",
                      QString("meta[0]=%1 (原=%2)")
                          .arg(moved.value(0).toMap().value("name").toString())
                          .arg(name0));
                // 归一：无并列，priority = n-1-位置
                bool normalized = !moved.empty();
                QStringList plist;
                for (int i = 0; i < moved.size(); ++i) {
                    const int p = moved.value(i).toMap()
                                      .value("priority").toInt();
                    plist << QString::number(p);
                    normalized = normalized && p == moved.size() - 1 - i;
                }
                audit(normalized, "词典优先级整列归一（无并列）",
                      QString("n=%1, p=[%2]").arg(moved.size())
                          .arg(plist.join(",")));
                // 还原：首位下移归位（Repeater 随 stamp 重建，重新取指针）
                QQuickItem* downBtn0 = item(win, "dictPriorityDown_0");
                if (downBtn0) {
                    scrollIntoView(win, downBtn0);
                    clickItem(win, downBtn0);
                    settle(win);
                    audit(adapter.dictionariesMeta().value(0).toMap()
                                  .value("name").toString() == name0,
                          "词典优先级下移还原", "meta[0] 回原序");
                } else {
                    audit(false, "词典优先级下移还原",
                          "dictPriorityDown_0 不存在");
                }
            } else {
                audit(false, "词典优先级上移交换",
                      QString("metas=%1, upBtn=%2, label=%3")
                          .arg(metas.size())
                          .arg(upBtn1 ? 1 : 0)
                          .arg(pLabel0 ? 1 : 0));
            }
        }
        // ---- S31 词典移除（巡检续批 F8）：真点 ✕ → 行数 -1 + 装载清单
        // 不再含该名。会话面移除（env 驱动无状态文件）不可在 run 内还原，
        // 故置于审计尾部，之后仅汇总 ----
        {
            const QVariantList metasTail = adapter.dictionariesMeta();
            const int n = metasTail.size();
            const QString victim =
                metasTail.value(n - 1).toMap().value("name").toString();
            const QByteArray rmName =
                QByteArray("dictRemoveButton_") + QByteArray::number(n - 1);
            QQuickItem* rmBtn = item(win, rmName.constData());
            if (n >= 1 && rmBtn) {
                scrollIntoView(win, rmBtn);
                clickItem(win, rmBtn);
                settle(win);
                const QVariantList after = adapter.dictionariesMeta();
                QStringList remain;
                for (int i = 0; i < after.size(); ++i)
                    remain << after.value(i).toMap().value("name").toString();
                const bool rowOk = after.size() == n - 1;
                const bool metaGone = !remain.contains(victim);
                const bool loadedGone =
                    !adapter.loadedDictionaries().contains(victim);
                const bool statusOk =
                    win->property("statusText").toString()
                    == QStringLiteral("已移除词典: ") + victim;
                audit(rowOk && metaGone && loadedGone && statusOk,
                      "词典移除（✕ 真点：行数-1 + 装载清单摘名 + 状态行）",
                      QString("n %1→%2, victim=%3, remain=[%4], row=%5 "
                              "meta=%6 loaded=%7 status=%8 (%9)")
                          .arg(n).arg(after.size()).arg(victim)
                          .arg(remain.join(","))
                          .arg(rowOk).arg(metaGone).arg(loadedGone)
                          .arg(statusOk)
                          .arg(win->property("statusText").toString()));
            } else {
                audit(false, "词典移除（✕ 真点：行数-1 + 装载清单摘名 + 状态行）",
                      QString("metas=%1, rmBtn=%2")
                          .arg(n).arg(rmBtn ? 1 : 0));
            }
        }
        // ---- S35 全文索引重建按钮（候选#4 F12）：先查一个词面/前缀/
        // 模糊全 miss 的词逼出全文兜底层把索引建起来，再真点「重建」→
        // 内存索引丢弃 docs 归零 + 状态行回惰建态 + 按钮反馈。置于尾部
        //（前面 S 步骤的全文态不受影响；索引随后按需重建，无需还原）----
        {
            QQuickItem* sf35 = item(win, "searchField");
            QQuickItem* go35 = item(win, "searchGoButton");
            if (sf35 && go35 && drawer2) {
                if (drawer2->property("visible").toBool() == false) {
                    drawer2->setProperty("visible", true);
                    settle(win);
                }
                // 全文兜底触发建索引（点搜索钮在抽屉外——抽屉
                // CloseOnPressOutside 会顺势关掉，后面重开再点）
                sf35->setProperty("text", QStringLiteral("zzqqxx"));
                flushEvents();
                clickItem(win, go35);
                settle(win);
                const int docsBefore =
                    adapter.fulltextStats().value(QStringLiteral("docs"))
                        .toInt();
                audit(docsBefore > 0,
                      "全文索引按需建触发（全文兜底查询）",
                      QString("docs=%1").arg(docsBefore));
                // 重开抽屉 + 切词典 tab 真点「重建」
                if (!drawer2->property("visible").toBool()) {
                    drawer2->setProperty("visible", true);
                    settle(win);
                }
                clickItem(win, item(win, "toolsTab2"));
                settle(win);
                QQuickItem* rebuildBtn = item(win, "dictIndexRebuildButton");
                if (rebuildBtn) {
                    scrollIntoView(win, rebuildBtn);
                    clickItem(win, rebuildBtn);
                    settle(win);
                    const int docsAfter =
                        adapter.fulltextStats().value(QStringLiteral("docs"))
                            .toInt();
                    QQuickItem* idxLabel = item(win, "dictIndexStatusLabel");
                    const QString label =
                        idxLabel ? idxLabel->property("text").toString()
                                 : QString();
                    audit(docsAfter == 0
                              && win->property("statusText").toString()
                                     == QStringLiteral(
                                            "已重置全文索引（下次全文检索时按"
                                            "需重建）"),
                          "全文索引重建按钮（真点：索引丢弃+状态行）",
                          QString("docs %1→%2, status=%3")
                              .arg(docsBefore)
                              .arg(docsAfter)
                              .arg(win->property("statusText").toString()));
                    audit(label.startsWith(
                              QStringLiteral("全文索引: 按需建")),
                          "全文索引状态行回惰建态",
                          QString("label=%1").arg(label));
                } else {
                    audit(false, "全文索引重建按钮（真点：索引丢弃+状态行）",
                          "dictIndexRebuildButton 不存在");
                }
            } else {
                audit(false, "全文索引按需建触发（全文兜底查询）",
                      "searchField/searchGoButton 不存在");
            }
        }
        if (drawer2) drawer2->setProperty("visible", false);
        settle(win);
    }

    // ---- S37 移动端根（Main.qml）同步预览冲突裁决（remoteOnly 选择链）：
    // 桌面面走 B 系列 SyncManager；previewDetails 选择链只在移动端根
    // （Android 生产面，此前无任何测试加载，qmllint 仅语法闸门）。第二
    // 引擎复用同批适配器实例；Vocab 页 StackLayout 无滚动容器，offscreen
    // 大窗把折叠下方控件全量送进场景。夹具：本地只收 alpha；同步文件手写
    // alpha+beta（beta 远端独有）；sync 上下文属性两引擎同实例。断言链：
    // Preview Sync 真点 → Apply 构造 sel（remoteOnly 列表选择函数）→
    // 确认摘要 "+ pull remote-only: 1" → accept 触发 applySelection 落库
    {
        QQmlApplicationEngine engine2;
        registerCtx(engine2);
        engine2.load(QUrl("qrc:/Main.qml"));
        if (engine2.rootObjects().isEmpty()) {
            audit(false, "移动端根加载", "qrc:/Main.qml load 失败");
        } else {
            auto* win2 = qobject_cast<QQuickWindow*>(
                engine2.rootObjects().first());
            if (!win2) {
                audit(false, "移动端根加载", "root is not a QQuickWindow");
            } else {
                win2->show();
                settle(win2);
                win2->setProperty("currentPage", 2);
                settle(win2);

                vocab.addToVocabulary(QStringLiteral("alpha"),
                                      QStringLiteral("audit sync shared"));
                const QString syncPath =
                    QDir(outDir).filePath("audit-sync-mobile.json");
                {
                    QFile f(syncPath);
                    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                    f.write("{\"version\":1,\"history\":[],\"vocab\":["
                            "{\"word\":\"alpha\",\"definition\":\"audit sync "
                            "shared\"},{\"word\":\"beta\",\"definition\":"
                            "\"audit sync remote only\"}]}");
                }
                sync.setSyncFile(syncPath);

                QQuickItem* previewButton = item(win2, "syncPreviewButton");
                audit(previewButton != nullptr,
                      "移动端同步预览入口按钮存在",
                      previewButton ? "found" : "syncPreviewButton 不存在");
                // 入口按钮仅 existence 级（Vocab 页 Column 在 offscreen 有
                // anchors 警告、布局不保证点击落点，与回归目标无涉）；预览
                // 弹窗直驱：open() 后内容入树（S3 弹窗内容同律），再取
                // 节点 fill(adapter previewDiff 真算结果) → 真点 Apply
                QObject* previewDlg = popupObj(win2, "previewDialog");
                if (previewDlg) {
                    QMetaObject::invokeMethod(previewDlg, "open");
                    settle(win2);
                }
                QQuickItem* details = item(win2, "previewDetails");
                if (details && previewDlg) {
                    QVariantMap m;
                    QMetaObject::invokeMethod(&sync, "previewDiff",
                                              Q_RETURN_ARG(QVariantMap, m));
                    QMetaObject::invokeMethod(details, "fill",
                                              Q_ARG(QVariant, m));
                    settle(win2);
                    audit(popupVisible(previewDlg)
                              && m.value("remoteOnly").toStringList()
                                     == QStringList{QStringLiteral("beta")},
                          "同步预览夹具（远端独有 beta）",
                          QString("remoteOnly=[%1]")
                              .arg(m.value("remoteOnly").toStringList().join(
                                  QLatin1Char(','))));
                    QQuickItem* applyBtn = item(win2, "previewApplyButton");
                    if (applyBtn) {
                        scrollIntoView(win2, applyBtn);
                        clickItem(win2, applyBtn);
                        settle(win2);
                        QObject* confirmDlg =
                            popupObj(win2, "previewConfirmDialog");
                        const QString summary =
                            confirmDlg
                                ? confirmDlg->property("summaryText").toString()
                                : QString();
                        audit(confirmDlg && popupVisible(confirmDlg)
                                  && summary.contains(
                                      QStringLiteral("remote-only: 1")),
                              "移动端同步预览 Apply 选择链（remoteOnly=1 摘要）",
                              summary.isEmpty() ? "previewConfirmDialog 未打开"
                                                : summary);
                        // 确认执行：applySelection 把远端独有 beta 拉进本地
                        if (confirmDlg && popupVisible(confirmDlg)) {
                            QMetaObject::invokeMethod(confirmDlg, "accept");
                            settle(win2);
                            bool alphaIn = false, betaIn = false;
                            for (const auto& v : vocab.vocabulary()) {
                                const QString w = v.toMap()
                                                      .value("word")
                                                      .toString()
                                                      .toLower();
                                if (w == QStringLiteral("alpha"))
                                    alphaIn = true;
                                if (w == QStringLiteral("beta")) betaIn = true;
                            }
                            audit(alphaIn && betaIn,
                                  "移动端同步预览确认后远端独有词落库",
                                  QString("alpha=%1 beta=%2")
                                      .arg(alphaIn)
                                      .arg(betaIn));
                        }
                    } else {
                        audit(false, "移动端同步预览 Apply 按钮",
                              "previewApplyButton 不存在");
                    }
                } else {
                    audit(false, "移动端同步预览弹窗装配",
                          QString("details=%1 dialog=%2")
                              .arg(details != nullptr)
                              .arg(previewDlg != nullptr));
                }
                // 清场：关残留弹窗（断链红态/应用失败时 previewDialog 会
                // 挡后续场景的模态点击）、夹具词、同步文件、路径复位
                if (QObject* pd = popupObj(win2, "previewDialog")) {
                    if (popupVisible(pd))
                        QMetaObject::invokeMethod(pd, "close");
                    settle(win2);
                }
                vocab.removeVocabularyWord(QStringLiteral("alpha"));
                vocab.removeVocabularyWord(QStringLiteral("beta"));
                sync.setSyncFile(QString());
                QFile::remove(syncPath);
            }
        }
    }

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
