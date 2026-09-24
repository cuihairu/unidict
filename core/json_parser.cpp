#include "json_parser.h"

#include <QFile>
#include <QFileInfo>
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
    m_sourcePath = filePath;
    m_dictionaryId = QFileInfo(filePath).canonicalFilePath().toLower();
    const QJsonArray arr = obj.value("entries").toArray();
    for (const auto& v : arr) {
        const QJsonObject e = v.toObject();
        const QString w = e.value("word").toString();
        const QString d = e.value("definition").toString();
        if (!w.isEmpty()) {
            m_entries[w] = d;
            m_words << w;
            m_lowerWords[w.toLower()] = w; // 同小写键后写覆盖，canonical 取最后词形
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

QVector<QPair<QString, QString>> JsonParser::allEntries() const {
    QVector<QPair<QString, QString>> out;
    out.reserve(m_entries.size());
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        out.append(qMakePair(it.key(), it.value()));
    }
    return out;
}

QStringList JsonParser::prefixSearch(const QString& prefix, int maxResults) const {
    QStringList out;
    const QString p = prefix.toLower();
    if (p.isEmpty() || maxResults <= 0) {
        return out;
    }
    // m_lowerWords 小写键有序：lowerBound 落前缀区段起点，越过区段即止
    for (auto it = m_lowerWords.lowerBound(p);
         it != m_lowerWords.constEnd() && out.size() < maxResults; ++it) {
        if (!it.key().startsWith(p)) {
            break;
        }
        out.append(it.value());
    }
    return out;
}

QString JsonParser::getDictionaryName() const { return m_name; }
QString JsonParser::getDictionaryDescription() const { return m_description; }
int JsonParser::getWordCount() const { return m_words.size(); }

QString JsonParser::getSourcePath() const { return m_sourcePath; }
QString JsonParser::getDictionaryId() const { return m_dictionaryId; }
QString JsonParser::getFormatName() const { return "Json"; }

}

