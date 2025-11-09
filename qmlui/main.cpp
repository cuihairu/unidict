#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "lookup_adapter.h"
#include "fulltext_manager_qt.h"

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    LookupAdapter adapter;
    adapter.loadDictionariesFromEnv();
    UnidictAdaptersQt::FullTextManagerQt fulltext;
    fulltext.loadDictionariesFromEnv();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("lookup", &adapter);
    engine.rootContext()->setContextProperty("fulltext", &fulltext);
    const QUrl url(QStringLiteral("qrc:/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
