#include "mdict_parser_qt.h"

namespace UnidictAdaptersQt {

static inline std::string cs(const QString& s) { return std::string(s.toUtf8().constData()); }
static inline QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }

bool MdictParserQt::loadDictionary(const QString& filePath) { return impl_.load_dictionary(cs(filePath)); }
bool MdictParserQt::isLoaded() const { return impl_.is_loaded(); }
QStringList MdictParserQt::getSupportedExtensions() const { return {"mdx", "mdd"}; }

UnidictCore::DictionaryEntry MdictParserQt::lookup(const QString& word) const {
    UnidictCore::DictionaryEntry e; auto d = impl_.lookup(cs(word)); if (!d.empty()) { e.word = word; e.definition = qs(d); }
    return e;
}

QStringList MdictParserQt::findSimilar(const QString& word, int maxResults) const {
    QStringList out; auto v = impl_.find_similar(cs(word), maxResults); out.reserve((int)v.size()); for (auto& s : v) out.append(qs(s)); return out;
}

QStringList MdictParserQt::getAllWords() const { QStringList out; auto v = impl_.all_words(); out.reserve((int)v.size()); for (auto& s : v) out.append(qs(s)); return out; }

QString MdictParserQt::getDictionaryName() const { return qs(impl_.dictionary_name()); }
QString MdictParserQt::getDictionaryDescription() const { return qs(impl_.dictionary_description()); }
int MdictParserQt::getWordCount() const { return impl_.word_count(); }

} // namespace UnidictAdaptersQt

