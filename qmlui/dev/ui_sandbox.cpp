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
#include <QEventLoop>
#include <QThread>

#include "../lookup_adapter.h"
#include "../adapters/qt/fulltext_manager_qt.h"
#include "../mobile_utils.h"
#include "../learning_manager.h"
#include "../adapters/qt/sync_service_qt.h"
#include "../adapters/qt/ai_service_qt.h"
#include "../adapters/qt/clipboard_qt.h"
#include "../adapters/qt/settings_qt.h"
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

} // namespace

int main(int argc, char* argv[]) {
    // 离屏软件渲染：必须在 QGuiApplication 之前设置
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
    QGuiApplication app(argc, argv);

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
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;
    MobileUtils mobileUtils;
    LearningManager learningManager;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Theme", &Theme::instance());
    engine.rootContext()->setContextProperty("lookup", &adapter);
    engine.rootContext()->setContextProperty("fulltext", &fulltext);
    engine.rootContext()->setContextProperty("sync", &sync);
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.rootContext()->setContextProperty("clip", &clip);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("MobileUtils", &mobileUtils);
    engine.rootContext()->setContextProperty("learningManager", &learningManager);
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
    adapter.addToVocabulary("hello", "A greeting or expression of goodwill.");
    adapter.addToVocabulary("world", "The earth, together with all of its countries.");
    // M3-B 生词本编辑视图：标签/笔记同屏留档（筛选 chips + 卡片 chip + 笔记行）
    adapter.addVocabTag("hello", "CET4");
    adapter.addVocabTag("hello", "greeting");
    adapter.addVocabTag("world", "CET4");
    adapter.setVocabNote("hello", "常用问候语；注意与 hello there 的语用差异");

    const QSize sizes[] = { QSize(1200, 760), QSize(900, 560) };
    const bool themes[] = { false, true };
    bool ok = true;

    for (bool dark : themes) {
        Theme::instance().setDark(dark);
        const QString themeName = dark ? "dark" : "light";
        // 主题切换后先在默认尺寸 settle 一次，让样式树稳定
        win->resize(sizes[0]);
        win->show();
        settle(win);

        for (const QSize& size : sizes) {
            const QString tag = QString("%1_%2x%3")
                .arg(themeName).arg(size.width()).arg(size.height());

            // 查词主页（空态）。先复位：openWord/切 tab 的状态会跨尺寸与
            // 主题残留，不清则后续 home/result/vocab 三张同字节（暗色组
            // 曾整组同图）。resetHome 只清查询面，词典/生词本种子保留。
            QMetaObject::invokeMethod(win, "resetHome");
            if (auto* lp = findByName(win, "leftPane"))
                lp->setProperty("currentTabIndex", 0);
            settle(win);
            ok &= grab(win, outDir, "home_" + tag + ".png", size);

            // 结果页
            QVariant r1("hello"), r2(true);
            QMetaObject::invokeMethod(win, "openWord", Q_ARG(QVariant, r1),
                                      Q_ARG(QVariant, r2));
            settle(win);
            ok &= grab(win, outDir, "result_" + tag + ".png", size);

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
                QMetaObject::invokeMethod(drawer, "open");
                settle(win);
                ok &= grab(win, outDir, "settings_" + tag + ".png", size);
                drawer->setProperty("visible", false);
                settle(win);
            } else {
                qWarning("toolsDrawer not found");
                ok = false;
            }
        }
    }

    return ok ? 0 : 3;
}
