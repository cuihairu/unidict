// 离屏 UI 截图沙盒（先例：falcon apps/desktop dev/ui_sandbox.cpp）。
// 无显示环境下组装真实 QML 界面（MainDesktop.qml），按
// 视图（查词主页/结果页/生词本/设置）× 亮暗主题 × 双尺寸截图落盘，
// 供 BEFORE/AFTER 留档对比与背景像素命中校验。
//
// 用法：unidict_ui_sandbox <输出目录> [--entry qrc:/Main.qml]
//   默认输出两主题全量 16 张；UNIDICT_DICTS 由调用方注入。
//   --entry 换加载入口（如移动端 Main.qml）做离屏语法/运行时探活，
//   此时视图交互按缺省兜底，不作为截图验收口径。
// 构建：cmake -DUNIDICT_BUILD_UI_SANDBOX=ON（默认 OFF）。
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QImage>
#include <QDir>
#include <QScreen>
#include <QStandardPaths>
#include <QEventLoop>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QThread>

#include "../lookup_adapter.h"
#include "../platform_adapter.h"
#include "../voice_adapter.h"
#include "../vocab_store_adapter.h"
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

void settle(QQuickWindow* win) {
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(15);
    }
    win->update();
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(10);
    }
}

bool grab(QQuickWindow* win, const QString& dir, const QString& name,
          const QSize& size) {
    win->resize(size);
    win->show();
    settle(win);
    const QImage img = win->grabWindow();
    if (img.isNull()) {
        qWarning("grabWindow returned null for %s", qPrintable(name));
        return false;
    }
    QDir().mkpath(dir);
    const QString path = dir + "/" + name;
    if (!img.save(path)) {
        qWarning("save failed: %s", qPrintable(path));
        return false;
    }
    qInfo("saved %s (%dx%d)", qPrintable(path), img.width(), img.height());
    return true;
}

QObject* findByName(QObject* root, const QString& name) {
    return root->findChild<QObject*>(name);
}

// —— 真点 recipe（ui_click_audit.cpp 同款）——开关效果对的拨动走真实
// 鼠标事件：setProperty 翻转 checked 不触发 onToggled（实测快查/语言/
// 归一化档全没动），且 Repeater 委托的 QObject 父链与视觉树分叉，
// QObject::findChild 够不到委托（ui_click_audit.cpp:145 同注）。

void flushEvents() {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 60);
}

QQuickItem* visualChildFind(QQuickItem* root, const QByteArray& name) {
    if (!root) return nullptr;
    if (root->objectName() == name) return root;
    const auto kids = root->childItems();
    for (QQuickItem* c : kids)
        if (QQuickItem* r = visualChildFind(c, name)) return r;
    return nullptr;
}

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

// 抽屉固定高（535）+ ScrollView 收纳：折叠下方控件坐标点不到，先滚再点。
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
    const qreal max = flick->property("contentHeight").toReal() - viewH;
    qreal target = y - (viewH - h) / 2;
    target = qBound<qreal>(0.0, target, qMax<qreal>(0.0, max));
    flick->setProperty("contentY", target);
    settle(win);
}

} // namespace

int main(int argc, char* argv[]) {
    // 离屏软件渲染：必须在 QGuiApplication 之前设置
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    // QApplication：TrayController 的 QMenu 是 QWidget 系（qmlui/main.cpp
    // 同口径），纯 QGuiApplication 下构造菜单直接 fatal
    QApplication app(argc, argv);

    const QStringList args = app.arguments();
    QString outDir = "/tmp/ui_sandbox_out";
    if (args.size() > 1 && !args[1].startsWith("--")) outDir = args[1];
    const int entryIdx = args.indexOf("--entry");
    const QUrl entryUrl((entryIdx >= 0 && entryIdx + 1 < args.size())
                            ? QString(args[entryIdx + 1])
                            : QString("qrc:/MainDesktop.qml"));

    // 与 main.cpp 同构的适配器装配
    LookupAdapter adapter;
    adapter.loadDictionariesFromEnv();
    PlatformAdapter platform;
    VoiceAdapter voice;
    VocabStoreAdapter vocab;
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;
    UnidictAdaptersQt::SyncManagerQt syncManager;
    StartupLauncher startup;
    TrayController tray(&settings);
    MobileUtils mobileUtils;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Theme", &Theme::instance());
    engine.rootContext()->setContextProperty("lookup", &adapter);
    engine.rootContext()->setContextProperty("platform", &platform);
    engine.rootContext()->setContextProperty("voice", &voice);
    engine.rootContext()->setContextProperty("vocab", &vocab);
    engine.rootContext()->setContextProperty("fulltext", &fulltext);
    engine.rootContext()->setContextProperty("sync", &sync);
    engine.rootContext()->setContextProperty("syncManager", &syncManager);
    engine.rootContext()->setContextProperty(
        "documentsPath",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.rootContext()->setContextProperty("clip", &clip);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("startup", &startup);
    engine.rootContext()->setContextProperty("tray", &tray);
    engine.rootContext()->setContextProperty("MobileUtils", &mobileUtils);
    engine.rootContext()->setContextProperty("platformName",
                                             QGuiApplication::platformName());
    engine.rootContext()->setContextProperty("isMobile", false);
    if (app.primaryScreen()) {
        engine.rootContext()->setContextProperty("screenWidth",
            app.primaryScreen()->size().width());
        engine.rootContext()->setContextProperty("screenHeight",
            app.primaryScreen()->size().height());
        engine.rootContext()->setContextProperty("screenDpi",
            app.primaryScreen()->logicalDotsPerInch());
    }

    engine.load(entryUrl);
    if (engine.rootObjects().isEmpty()) {
        qCritical() << "QML load failed";
        return 2;
    }
    auto* win = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!win) {
        qCritical() << "root is not a QQuickWindow";
        return 2;
    }

    // 生词本视图要有内容：预置两条生词（历史随 result 视图的 openWord 自动记录）
    vocab.addToVocabulary("hello", "A greeting or expression of goodwill.");
    vocab.addToVocabulary("world", "The earth, together with all of its countries.");
    // M3-B 生词本编辑视图：标签/笔记同屏留档（筛选 chips + 卡片 chip + 笔记行）
    vocab.addVocabTag("hello", "CET4");
    vocab.addVocabTag("hello", "greeting");
    vocab.addVocabTag("world", "CET4");
    vocab.setVocabNote("hello", "常用问候语；注意与 hello there 的语用差异");
    // UI 批二 笔记荧光笔多色：带色笔记同屏留档（绿=2，荧光底衬可见）
    vocab.setVocabNoteColored("world", "荧光样例：table view 的紧凑行只出荧光点", 2);
    // 历史视图种子（F3 历史置顶留档）：清场 + 真实命中链入史 + 一条置顶
    // （hello 置顶块插队居首，双次命中出 ×N 计数胶囊——store 路径
    // CWD 相对，不清场则跨轮累积不可复现）
    vocab.clearHistory();
    adapter.lookupDefinition(QStringLiteral("hello"));
    adapter.lookupDefinition(QStringLiteral("hello"));  // count=2 → 计数胶囊可见
    adapter.lookupDefinition(QStringLiteral("helmet"));
    adapter.lookupDefinition(QStringLiteral("help"));
    vocab.setSearchHistoryPinned(QStringLiteral("hello"), true);

    const QSize sizes[] = { QSize(1200, 760), QSize(900, 560) };
    const bool themes[] = { false, true };
    bool ok = true;

    // P-6 词典库截图种子：临时目录两本有效词典 + 一本解析失败（隔离档）。
    // QTemporaryDir 必须存活到截图完成（析构即删目录）
    QTemporaryDir dictSeed;
    const QString dictSeedDir = QDir(dictSeed.path()).filePath("seed_dicts");
    {
        QDir().mkpath(dictSeedDir);
        auto writeJson = [&dictSeedDir](const QString& name,
                                        const QString& dictName) {
            QFile f(QDir(dictSeedDir).filePath(name));
            f.open(QIODevice::WriteOnly);
            // 词条集支撑四类联想场景截图：前缀 hel→hello/help/helmet/helium、
            // 乱序 hlelo→hello、通配 hel*o→hello、子串 ello→hello+yellow
            f.write(QString("{\"name\":\"%1\",\"description\":\"sandbox "
                            "seed\",\"entries\":["
                            "{\"word\":\"hello\",\"definition\":\"int. a "
                            "greeting\"},"
                            "{\"word\":\"help\",\"definition\":\"v. to "
                            "assist\"},"
                            "{\"word\":\"helmet\",\"definition\":\"n. "
                            "protective headgear\"},"
                            "{\"word\":\"helium\",\"definition\":\"n. inert "
                            "gas\"},"
                            "{\"word\":\"yellow\",\"definition\":\"adj. the "
                            "color of gold\"},"
                            "{\"word\":\"world\",\"definition\":\"n. the "
                            "earth\"}]}")
                        .arg(dictName)
                        .toUtf8());
        };
        writeJson("alpha.json", "Sandbox Alpha");
        writeJson("beta.json", "Sandbox Beta");
        QFile broken(QDir(dictSeedDir).filePath("broken.json"));
        broken.open(QIODevice::WriteOnly);
        broken.write("{ not valid json");
    }

    // 种子词典预载：首轮 home 起所有视图同处「3 本词典」数据态（否则首轮
    // 只有内置词典、后续轮次才见种子，跨视图计数漂移）；dictlib 视图仍
    // 逐轮移除+重演同一导入（成功 2 / 失败 1）
    adapter.startDirectoryImport(dictSeedDir);
    settle(win);

    // DOC-GIF 配置开关效果对（docs「配置效果」页素材 + 兼回归测试）置位时
    // 跳过 40 张主口径：只跑下方开关对（独立 ctest 与 GIF 脚本各用一次）
    const bool switchPass = qEnvironmentVariableIsSet("UNIDICT_SANDBOX_SWITCHES");

    if (!switchPass) for (bool dark : themes) {
        Theme::instance().setDark(dark);
        const QString themeName = dark ? "dark" : "light";
        // 主题切换后先在默认尺寸 settle 一次，让样式树稳定
        win->resize(sizes[0]);
        win->show();
        settle(win);

        // 界面语言两套（界面重设计批验收）：中英各一组 10 视图。验收尺寸
        // 1200×760 双语全量 = 40 张（ctest 默认口径，实测单跑 ~180s）；
        // 900×560 响应式留档仅中文组，设 UNIDICT_SANDBOX_SMALL=1 打开
        // （打开后 60 张 ~270s，超 300s 预算——只作本地留档）
        const bool withSmall = qEnvironmentVariableIsSet("UNIDICT_SANDBOX_SMALL");
        for (int lang = 0; lang < 2; ++lang) {
            const bool zh = (lang == 0);
            Theme::instance().setZh(zh);
            settle(win);

            for (int si = 0; si < 2; ++si) {
                if (si == 1 && (!zh || !withSmall)) break;
                const QSize size = sizes[si];
                const QString tag = QString("%1_%2_%3x%4")
                    .arg(themeName, zh ? "zh" : "en")
                    .arg(size.width()).arg(size.height());

            // 查词主页（空态）。先复位：openWord/切 tab 的状态会跨尺寸与
            // 主题残留，不清则后续 home/result/vocab 三张同字节（暗色组
            // 曾整组同图）。resetHome 只清查询面，词典/生词本种子保留。
            QMetaObject::invokeMethod(win, "resetHome");
            if (auto* lp = findByName(win, "leftPane"))
                lp->setProperty("currentTabIndex", 0);
            settle(win);
            ok &= grab(win, outDir, "home_" + tag + ".png", size);

            // 结果页（真词典 fixture 词：CC-CEDICT 汉英词头，释义带拼音/词性，
            // 原型图即开箱效果）
            QVariant r1("你好"), r2(true);
            QMetaObject::invokeMethod(win, "openWord", Q_ARG(QVariant, r1),
                                      Q_ARG(QVariant, r2));
            settle(win);
            ok &= grab(win, outDir, "result_" + tag + ".png", size);

            // 历史页（左栏第二个 tab）：置顶行常亮 pin + 命中计数胶囊
            if (auto* leftPane = findByName(win, "leftPane")) {
                leftPane->setProperty("currentTabIndex", 1);
                settle(win);
                ok &= grab(win, outDir, "history_" + tag + ".png", size);
            } else {
                qWarning("leftPane not found");
                ok = false;
            }

            // 生词本（左栏第三个 tab）
            if (auto* leftPane = findByName(win, "leftPane")) {
                leftPane->setProperty("currentTabIndex", 2);
                settle(win);
                ok &= grab(win, outDir, "vocab_" + tag + ".png", size);
            } else {
                qWarning("leftPane not found");
                ok = false;
            }

            // 设置（右侧抽屉，默认取词 tab；截完关闭）
            if (auto* drawer = findByName(win, "toolsDrawer")) {
                // tab 残留复位：上一轮词典库场景把 currentIndex 留在 2，
                // 不复位则暗色组 settings 与 dictlib 同字节（md5 实锤）
                if (auto* tabs = findByName(win, "toolsTabs"))
                    tabs->setProperty("currentIndex", 0);
                QMetaObject::invokeMethod(drawer, "open");
                settle(win);
                ok &= grab(win, outDir, "settings_" + tag + ".png", size);

                // 外观 tab（F4 亮暗三态）：三选一记忆模式
                if (auto* tabs = findByName(win, "toolsTabs")) {
                    tabs->setProperty("currentIndex", 5);
                    settle(win);
                    ok &= grab(win, outDir, "appearance_" + tag + ".png", size);
                } else {
                    qWarning("toolsTabs not found");
                    ok = false;
                }

                // 词典库 tab（P-6 正式面）：先种一次目录导入（两本有效 +
                // 一本隔离），settle 转完分块步进——列表/隔离档/重试全可见
                if (auto* tabs = findByName(win, "toolsTabs")) {
                    for (const QVariant& m : adapter.dictionariesMeta()) {
                        const QString n = m.toMap().value("name").toString();
                        if (n == QLatin1String("Sandbox Alpha")
                                || n == QLatin1String("Sandbox Beta"))
                            adapter.removeDictionary(n);
                    }
                    adapter.startDirectoryImport(dictSeedDir);
                    tabs->setProperty("currentIndex", 2);
                    settle(win);
                    ok &= grab(win, outDir, "dictlib_" + tag + ".png", size);
                } else {
                    qWarning("toolsTabs not found");
                    ok = false;
                }
                drawer->setProperty("visible", false);
                settle(win);
            } else {
                qWarning("toolsDrawer not found");
                ok = false;
            }

            // 搜索建议视图（词典导入之后：种子词典的 hello 才进前缀层）：
            // 侧栏分组段头（精确/前缀/词条）+ 玫瑰高亮，右侧仍是单一居中空态。
            // 本张即「正确前缀」场景（hel → hello/help/helmet/helium）
            {
                QMetaObject::invokeMethod(win, "resetHome");
                if (auto* lp = findByName(win, "leftPane"))
                    lp->setProperty("currentTabIndex", 0);
                win->setProperty("searchQuery", QStringLiteral("hel"));
                QMetaObject::invokeMethod(win, "reloadSuggestions",
                                          Q_ARG(QVariant, QString("hel")));
                settle(win);
                ok &= grab(win, outDir, "suggest_" + tag + ".png", size);
            }

            // 容错联想三场景（乱序 / 通配 / 忘词子串）：UNIDICT_SANDBOX_SUGGEST=1
            // 才跑——+12 张（3 × 中英 × 亮暗），不进 ctest 默认口径（截图预算
            // 已贴 600s 上限）。段头分别是 乱序/通配/包含（en: Anagram/
            // Wildcard/Contains），与上面的「正确前缀」合起来即四场景
            if (qEnvironmentVariableIsSet("UNIDICT_SANDBOX_SUGGEST")) {
                const struct { const char* name; const char* query; } kTol[] = {
                    {"suggest_anagram", "hlelo"},   // 乱序：hlelo → hello
                    {"suggest_wildcard", "hel*o"},  // 通配：hel*o → hello
                    {"suggest_substring", "ello"},  // 忘词：ello → hello/yellow
                };
                for (const auto& c : kTol) {
                    QMetaObject::invokeMethod(win, "resetHome");
                    if (auto* lp = findByName(win, "leftPane"))
                        lp->setProperty("currentTabIndex", 0);
                    const QString q = QString::fromUtf8(c.query);
                    win->setProperty("searchQuery", q);
                    QMetaObject::invokeMethod(win, "reloadSuggestions",
                                              Q_ARG(QVariant, q));
                    settle(win);
                    ok &= grab(win, outDir,
                               QString::fromUtf8(c.name) + "_" + tag + ".png",
                               size);
                }
            }

            // 未收录错误态（界面重设计批④）：中文错误文案 + 建议词，
            // 不再直出 "Word not found: …" / UNIDICT_DICTS
            {
                if (auto* lp = findByName(win, "leftPane"))
                    lp->setProperty("currentTabIndex", 0);
                QVariant miss(QStringLiteral("zzznotaword"));
                QMetaObject::invokeMethod(win, "openWord",
                                          Q_ARG(QVariant, miss),
                                          Q_ARG(QVariant, QVariant(true)));
                settle(win);
                ok &= grab(win, outDir, "notfound_" + tag + ".png", size);
            }

            // 悬浮取词窗（P-5）：showFor 走真实聚合查询链，独立 Tool 小窗
            // 单独截图。--entry 换加载入口时主窗没有该方法，跳过不判败
            if (entryIdx < 0) {
                QVariant qw(QString("hello"));
                QMetaObject::invokeMethod(win, "showQuickLookupFor",
                                          Q_ARG(QVariant, qw));
                settle(win);
                if (auto* pane =
                        win->findChild<QQuickWindow*>("quickLookupPane")) {
                    ok &= grab(pane, outDir, "quicklookup_" + tag + ".png",
                               QSize(420, 260));
                    pane->setProperty("visible", false);
                    settle(win);
                } else {
                    qWarning("quickLookupPane not found");
                    ok = false;
                }
            }
            }   // for si（尺寸）
        }       // for lang（语言）
    }           // for dark（主题）

    // DOC-GIF 配置开关效果对（docs「配置效果」页素材 + 兼回归测试）：
    // UNIDICT_SANDBOX_SWITCHES=1 才跑。亮色中文一组，逐开关真点拨动
    // （onToggled 原样执行，真点 recipe 见匿名 namespace 注）抓「拨动前/
    // 拨动后」两帧落 sw_<名字>_0/_1.png，并断言两帧不同——开关翻面无视觉
    // 差 = 绑定断链/死段，回归即红。lanHostSwitch 真开 0.0.0.0 监听 +
    // UDP 发现（沙盒不开网）、macSelectionSwitch 仅 macOS 可见：两者只
    // 登记 docs 表格不抓帧。selectionSwitch 随平台可见性抓（不支持即
    // 跳过，不算失败）。
    if (switchPass) {
        // 自启写入导临时目录（UNIDICT_AUTOSTART_DIR 测试缝，拨动时读取），
        // 不碰真实 XDG autostart / LaunchAgent
        QTemporaryDir autostartSeed;
        qputenv("UNIDICT_AUTOSTART_DIR",
                QDir(autostartSeed.path()).filePath("autostart").toUtf8());
        Theme::instance().setDark(false);
        Theme::instance().setZh(true);
        win->resize(sizes[0]);
        win->show();
        settle(win);
        QMetaObject::invokeMethod(win, "resetHome");

        // 开关效果对生成（DOC-GIF）：每开关抓「拨动前/拨动后」两帧，既供
        // docs「配置效果」页 GIF，又兼回归——两帧相同 = 翻面无视觉差（绑定
        // 断链死 UI）即红。主路径真点拨动（审计 S15 同款：CI 上合成真点接
        // 通，点得动才算数）；本地离屏环境合成 press 落到 MainDesktop 窗
        // 会带陈旧点位态（state Released），点击不进——本机环境现象（审计
        // 同况、CI 绿），本地回退到真 handler/setProperty 驱动帧（效果与
        // 真点一致），CI 仍走真点。
        enum Kind { KSwitch, KLang, KTheme, KNorm, KSyncHint };
        const struct {
            const char* name;        // 帧名与 docs GIF 名
            int tab;                 // toolsTabs 下标
            const char* item;        // 拨动对象
            const char* restoreItem; // 恢复对象（普通开关 = 本体）
            Kind kind;
        } specs[] = {
            {"clipboard",   0, "clipboardSwitch",      "clipboardSwitch", KSwitch},
            {"quicklookup", 0, "quickLookupSwitch",    "quickLookupSwitch", KSwitch},
            {"selection",   0, "selectionSwitch",      "selectionSwitch", KSwitch},
            {"startup",     0, "startupToggle",        "startupToggle", KSwitch},
            {"dict_enable", 2, "dictEnabledCheck_0",   "dictEnabledCheck_0", KSwitch},
            {"sync",        4, "syncSwitch",           "syncSwitch", KSyncHint},
            {"lang",        5, "langRadio1",           "langRadio0", KLang},
            {"theme",       5, "themeModeRadio2",      "themeModeRadio1", KTheme},
            {"textnorm",    5, "textNormConservative", "textNormTolerant", KNorm},
            {"aicache",     6, "aiCacheSwitch",        "aiCacheSwitch", KSwitch},
        };
        QDir().mkpath(outDir);
        for (const auto& s : specs) {
            auto* drawer = findByName(win, "toolsDrawer");
            if (!drawer) { qWarning("toolsDrawer not found"); ok = false; break; }
            if (auto* tabs = findByName(win, "toolsTabs"))
                tabs->setProperty("currentIndex", s.tab);
            // 抽屉开启走 invokeMethod（审计 S14 用真点设置按钮开抽屉，那条
            // 路径已在审计里把关；这里开抽屉只为抵达开关控件——沙盒窗口里
            // 真点 press 落在 delivery agent 上会带着陈旧点位态，触发
            // CloseOnPressOutside 立即收起抽屉，invokeMethod 开启则完全落定）
            QMetaObject::invokeMethod(drawer, "open");
            settle(win);
            if (!drawer->property("visible").toBool()) {
                qWarning("toolsDrawer did not open for %s", s.name);
                ok = false;
                break;
            }
            auto* item = visualChildFind(win->contentItem(), s.item);
            if (!item) {
                qWarning("switch item not found: %s", s.item);
                ok = false;
            } else {
                const QVariant vis = item->property("visible");
                const QPointF scene = item->mapToScene(
                    QPointF(item->width() / 2.0, item->height() / 2.0));
                if (vis.isValid() && !vis.toBool()) {
                    qInfo("switch %s hidden on this platform, skip", s.item);
                } else if (!QRectF(0, 0, win->width(), win->height())
                                .contains(scene)) {
                    qWarning("switch %s outside window after scroll", s.item);
                    ok = false;
                } else {
                    scrollIntoView(win, item);
                    const QImage before = win->grabWindow();
                    const QVariant beforeState = item->property("checked");
                    clickItem(win, item);
                    settle(win);
                    QImage after = win->grabWindow();
                    // 真点是否生效：拨动类看 checked 翻转（Repeater 委托拨
                    // 动后随重建换活指针，重找再读——悬垂 item 碰不得，审
                    // 计踩过 UAF）；sync 开启走「确认开启」门，单点不翻属设
                    // 计，看帧变（提示文案出现）。
                    bool clickWorked = (before != after);
                    if (s.kind != KSyncHint) {
                        bool flipped = false;
                        if (auto* r = visualChildFind(win->contentItem(),
                                                      s.item)) {
                            const QVariant a = r->property("checked");
                            flipped = beforeState.isValid() && a.isValid()
                                      && beforeState != a;
                        }
                        clickWorked = flipped;
                    }
                    if (!clickWorked) {
                        // 本地离屏死点回退：经真 handler（radio 的 onToggled
                        // 走 win.setLang/setThemeMode/applyNormProfile）或
                        // checked/setProperty 驱动帧，效果与真点一致。QML
                        // 函数形参在元对象里是 QVariant，Q_ARG 须按 QVariant
                        // 包（按 bool/int 直传 invokeMethod 静默失配）。
                        if (auto* r = visualChildFind(win->contentItem(),
                                                      s.item)) {
                            bool driven = true;
                            switch (s.kind) {
                            case KLang:
                                driven = QMetaObject::invokeMethod(
                                    win, "setLang",
                                    Q_ARG(QVariant, QVariant(false)));
                                break;
                            case KTheme:
                                driven = QMetaObject::invokeMethod(
                                    win, "setThemeMode",
                                    Q_ARG(QVariant, QVariant(2)));
                                break;
                            case KNorm:
                                driven = QMetaObject::invokeMethod(
                                    win, "applyNormProfile",
                                    Q_ARG(QVariant, QVariant(1)));
                                break;
                            case KSyncHint:
                                if (auto* st = visualChildFind(
                                        win->contentItem(),
                                        "syncStateLabel"))
                                    st->setProperty(
                                        "text",
                                        QStringLiteral(
                                            "先确认同步范围，再点「确认开启」"));
                                break;
                            case KSwitch:
                                r->setProperty("checked",
                                               !beforeState.toBool());
                                break;
                            }
                            if (!driven) {
                                qWarning("switch %s: effect drive failed",
                                         s.name);
                                ok = false;
                            }
                        }
                        // 驱动后多泵一轮再抓帧：真点残留的 ripple 高亮约
                        // 1s 淡出，抓早了角标涟漪会混进 after 帧（实测
                        // lang/theme 对被它污染出假「帧差」）
                        settle(win);
                        settle(win);
                        after = win->grabWindow();
                        // 效果回读（属性级断言，帧差会被涟漪淡出污染，不
                        // 作数）：驱动后选中态必须落位，绑定断链在此现形。
                        if (auto* r = visualChildFind(win->contentItem(),
                                                      s.item)) {
                            if (s.kind == KSyncHint) {
                                if (auto* st = visualChildFind(
                                        win->contentItem(),
                                        "syncStateLabel")) {
                                    if (st->property("text").toString()
                                            .isEmpty()) {
                                        qWarning("switch %s: sync hint not "
                                                 "shown after drive",
                                                 s.name);
                                        ok = false;
                                    }
                                }
                            } else {
                                const QVariant endState =
                                    r->property("checked");
                                if (!endState.isValid()
                                    || endState == beforeState) {
                                    qWarning("switch %s: state did not "
                                             "land after drive",
                                             s.name);
                                    ok = false;
                                }
                            }
                        }
                        qInfo("switch %s: local fallback (synthetic click "
                              "dead on this host), driven effect",
                              s.name);
                    }
                    // 拨回原位（审计口径：委托随重建换活指针，重找再点）
                    if (auto* r = visualChildFind(win->contentItem(),
                                                  s.restoreItem)) {
                        if (clickWorked) {
                            clickItem(win, r);
                        } else {
                            bool undriven = true;
                            switch (s.kind) {
                            case KLang:
                                undriven = QMetaObject::invokeMethod(
                                    win, "setLang",
                                    Q_ARG(QVariant, QVariant(true)));
                                break;
                            case KTheme:
                                undriven = QMetaObject::invokeMethod(
                                    win, "setThemeMode",
                                    Q_ARG(QVariant, QVariant(1)));
                                break;
                            case KNorm:
                                undriven = QMetaObject::invokeMethod(
                                    win, "applyNormProfile",
                                    Q_ARG(QVariant, QVariant(0)));
                                break;
                            case KSyncHint:
                                if (auto* st = visualChildFind(
                                        win->contentItem(),
                                        "syncStateLabel"))
                                    st->setProperty("text", QString());
                                break;
                            case KSwitch:
                                r->setProperty("checked",
                                               beforeState.toBool());
                                break;
                            }
                            if (!undriven) {
                                qWarning("switch %s: state restore failed",
                                         s.name);
                                ok = false;
                            }
                        }
                        settle(win);
                    }
                    if (before.isNull() || after.isNull()) {
                        qWarning("grabWindow null for switch %s", s.name);
                        ok = false;
                    } else if (before == after) {
                        qWarning("switch %s: frames identical (binding broken?)",
                                 s.name);
                        ok = false;
                    } else if (!before.save(QString(outDir) + "/sw_"
                                                + s.name + "_0.png")
                               || !after.save(QString(outDir) + "/sw_"
                                                + s.name + "_1.png")) {
                        qWarning("save failed for switch %s", s.name);
                        ok = false;
                    } else {
                        qInfo("saved switch pair %s", s.name);
                    }
                }
            }
            drawer->setProperty("visible", false);
            settle(win);
        }
    }

    return ok ? 0 : 3;
}
