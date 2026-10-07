#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#include <QAndroidJniObject>
#endif

#ifdef Q_OS_IOS
#include <QtCore/qglobal.h>
#endif

#include "lookup_adapter.h"
#include "fulltext_manager_qt.h"
#include "mobile_utils.h"
#include "learning_manager.h"
#include "sync_service_qt.h"
#include "sync_manager_qt.h"
#include "ai_service_qt.h"
#include "clipboard_qt.h"
#include "settings_qt.h"
#include "theme.h"

#include <QStyleHints>

// 平台检测和初始化
void initializePlatform() {
#ifdef Q_OS_ANDROID
    // Android权限请求
    QStringList permissions = {
        "android.permission.READ_EXTERNAL_STORAGE",
        "android.permission.WRITE_EXTERNAL_STORAGE",
        "android.permission.RECORD_AUDIO"
    };

    for (const QString &permission : permissions) {
        if (QtAndroid::checkPermission(permission) == QtAndroid::PermissionResult::Denied) {
            QtAndroid::requestPermissions(QStringList() << permission);
        }
    }

    // 设置Android应用目录
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir().mkpath(documentsPath + "/Unidict");

#elif defined(Q_OS_IOS)
    // iOS初始化
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir().mkpath(documentsPath + "/Unidict");
#endif
}

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // 设置应用属性
    app.setApplicationName("Unidict");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("YourCompany");
    app.setOrganizationDomain("yourcompany.com");

    // 平台初始化
    initializePlatform();

    // 创建适配器和工具类
    // 分发包兜底（BUGS.md BUG-002/004）：UNIDICT_DICTS 未设时加载全部
    // 随包词典——内置 CC-CEDICT 汉英词典（12.5 万简体词头，开箱即可
    // 查真实词条）+ 演示样本；exe 同目录（Windows/Linux 包布局）或
    // .app 的 Contents/Resources（macOS）。env 显式设置时以 env 为准，
    // 源码构建用户不受影响
    if (qEnvironmentVariableIsEmpty("UNIDICT_DICTS")) {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList searchDirs = {
            appDir,
            appDir + QStringLiteral("/../Resources"),
        };
        const QStringList bundledNames = {
            QStringLiteral("ccedict-zh-en.json"),
            QStringLiteral("dict.json"),
        };
        QStringList found;
        for (const QString& dir : searchDirs) {
            for (const QString& name : bundledNames) {
                const QString candidate = dir + QStringLiteral("/") + name;
                if (!found.contains(candidate) && QFileInfo::exists(candidate)) {
                    found << candidate;
                }
            }
        }
        if (!found.isEmpty()) {
            qInfo("UNIDICT_DICTS 未设，已加载随包词典: %s",
                  qPrintable(found.join(QDir::listSeparator())));
            qputenv("UNIDICT_DICTS",
                    found.join(QDir::listSeparator()).toUtf8());
        }
    }
    LookupAdapter adapter;
    adapter.loadDictionariesFromEnv();
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();
    UnidictAdaptersQt::SyncServiceQt sync;
    UnidictAdaptersQt::SyncManagerQt syncManager;
    UnidictAdaptersQt::AiServiceQt ai;
    UnidictAdaptersQt::ClipboardQt clip;
    UnidictAdaptersQt::SettingsQt settings;

    // 移动端工具类和学习管理器
    MobileUtils mobileUtils;
    LearningManager learningManager;

    // QML引擎设置
    QQmlApplicationEngine engine;

    // 主题单例：跟随系统亮/暗（Qt6.5+ colorScheme），未探测到时亮；
    // 运行中系统切换经 colorSchemeChanged 实时跟随（M5 深色主题，
    // 与 Android 端 day/night 资源限定符同口径）。
    auto* styleHints = QGuiApplication::styleHints();
    auto applyColorScheme = [styleHints] {
        UnidictQml::Theme::instance().setDark(
            styleHints->colorScheme() == Qt::ColorScheme::Dark);
    };
    applyColorScheme();
    QObject::connect(styleHints, &QStyleHints::colorSchemeChanged, &app, applyColorScheme);
    engine.rootContext()->setContextProperty("Theme", &UnidictQml::Theme::instance());

    // 注册上下文属性
    engine.rootContext()->setContextProperty(
        "documentsPath",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
    engine.rootContext()->setContextProperty("lookup", &adapter);
    engine.rootContext()->setContextProperty("fulltext", &fulltext);
    engine.rootContext()->setContextProperty("sync", &sync);
    engine.rootContext()->setContextProperty("syncManager", &syncManager);
    engine.rootContext()->setContextProperty("ai", &ai);
    engine.rootContext()->setContextProperty("clip", &clip);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("MobileUtils", &mobileUtils);
    engine.rootContext()->setContextProperty("learningManager", &learningManager);

    // 平台信息
    engine.rootContext()->setContextProperty("platformName", QGuiApplication::platformName());
    engine.rootContext()->setContextProperty("isMobile",
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        true
#else
        false
#endif
    );

    // 屏幕信息（移动端需要）
    if (app.primaryScreen()) {
        engine.rootContext()->setContextProperty("screenWidth", app.primaryScreen()->size().width());
        engine.rootContext()->setContextProperty("screenHeight", app.primaryScreen()->size().height());
        engine.rootContext()->setContextProperty("screenDpi", app.primaryScreen()->logicalDotsPerInch());
    }

    // 加载QML文件
    const QUrl url(
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
        QStringLiteral("qrc:/Main.qml")
#else
        QStringLiteral("qrc:/MainDesktop.qml")
#endif
    );
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        }, Qt::QueuedConnection);

    engine.load(url);

    return app.exec();
}
