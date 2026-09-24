#include "epub_parser.h"

#include <QFileInfo>

namespace UnidictCore {

bool EpubParser::loadDictionary(const QString& filePath) {
    m_loaded = false;
    m_words.clear();
    m_definitions.clear();
    m_lowerWords.clear();
    m_name.clear();

    m_sourcePath = filePath;
    m_dictionaryId = QFileInfo(filePath).canonicalFilePath().toLower();

    if (!parser_.load_dictionary(filePath.toStdString())) {
        return false;
    }

    m_name = QString::fromStdString(parser_.dictionary_name());
    for (const std::string& word : parser_.all_words()) {
        const QString qword = QString::fromStdString(word);
        const QString definition = QString::fromStdString(parser_.lookup(word));
        m_words << qword;
        const QString lower = qword.toLower();
        m_definitions[lower] = definition;
        m_lowerWords[lower] = qword; // 同小写键后写覆盖，canonical 取最后词形
    }
    m_loaded = true;
    return true;
}

bool EpubParser::isLoaded() const { return m_loaded; }

QStringList EpubParser::getSupportedExtensions() const { return {"epub"}; }

DictionaryEntry EpubParser::lookup(const QString& word) const {
    const auto it = m_definitions.find(word.toLower());
    if (it == m_definitions.end()) return {};
    DictionaryEntry e;
    e.word = m_lowerWords.value(word.toLower(), word);
    e.definition = it.value();
    return e;
}

QStringList EpubParser::findSimilar(const QString& word, int maxResults) const {
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

QStringList EpubParser::getAllWords() const { return m_words; }

QVector<QPair<QString, QString>> EpubParser::allEntries() const {
    QVector<QPair<QString, QString>> out;
    out.reserve(m_words.size());
    for (const QString& w : m_words) {
        out.append(qMakePair(w, m_definitions.value(w.toLower())));
    }
    return out;
}

QStringList EpubParser::prefixSearch(const QString& prefix, int maxResults) const {
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

QString EpubParser::getDictionaryName() const { return m_name; }

QString EpubParser::getDictionaryDescription() const {
    return QString::fromStdString(parser_.dictionary_description());
}

int EpubParser::getWordCount() const { return m_words.size(); }

QString EpubParser::getSourcePath() const { return m_sourcePath; }

QString EpubParser::getDictionaryId() const { return m_dictionaryId; }

QString EpubParser::getFormatName() const { return QStringLiteral("EPUB"); }

}
