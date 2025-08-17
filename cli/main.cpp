#include <QCoreApplication>
#include <QDebug>
#include <QStringList>

#include "unidict_core.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QStringList args = app.arguments();

    if (args.size() < 2) {
        qInfo() << "Usage: unidict_cli <word>";
        qInfo() << "Example: unidict_cli hello";
        return 1;
    }

    QString word = args.at(1);
    QString definition = UnidictCore::searchWord(word);

    qInfo().noquote() << definition;

    return 0;
}
