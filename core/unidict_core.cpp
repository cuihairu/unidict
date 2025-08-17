#include "unidict_core.h"
#include <QMap>
#include <QString>

// Create a simple in-memory dictionary.
// In a real application, this would be loaded from a file.
const QMap<QString, QString> dictionary = {
    {"hello", "A common greeting."}, 
    {"world", "The planet Earth, its inhabitants, and its environment."},
    {"qt", "A cross-platform application development framework."},
    {"cmake", "A cross-platform build system generator."}
};

namespace UnidictCore {

QString searchWord(const QString& word) {
    if (dictionary.contains(word.toLower())) {
        return dictionary.value(word.toLower());
    } else {
        return QString("Word '%1' not found.").arg(word);
    }
}

}
