#include "json_parser.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace UnidictCore {

JsonParser::JsonParser() = default;

bool JsonParser::loadDictionary(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return false;
    const QJsonObject obj = doc.object();
    m_name = obj.value("name").toString("JSON Dictionary");
    m_description = obj.value("description").toString();
    const QJsonArray arr = obj.value("entries").toArray();
    for (const auto& v : arr) {
        const QJsonObject e = v.toObject();
        const QString w = e.value("word").toString();
        const QString d = e.value("definition").toString();
        if (!w.isEmpty()) {
            m_entries[w] = d;
            m_words << w;
        }
    }
    m_loaded = true;
    return true;
}

bool JsonParser::isLoaded() const { return m_loaded; }

QStringList JsonParser::getSupportedExtensions() const { return {"json"}; }

DictionaryEntry JsonParser::lookup(const QString& word) const {
    auto it = m_entries.find(word);
    if (it == m_entries.end()) return {};
    DictionaryEntry e; e.word = word; e.definition = it.value();
    return e;
}

QStringList JsonParser::findSimilar(const QString& word, int maxResults) const {
    QStringList out;
    const QString lw = word.toLower();
    for (const QString& w : m_words) {
        if (w.toLower().startsWith(lw)) {
            out << w;
            if (out.size() >= maxResults) break;
        }
    }
    return out;
}

QStringList JsonParser::getAllWords() const { return m_words; }

QString JsonParser::getDictionaryName() const { return m_name; }
QString JsonParser::getDictionaryDescription() const { return m_description; }
int JsonParser::getWordCount() const { return m_words.size(); }

}

