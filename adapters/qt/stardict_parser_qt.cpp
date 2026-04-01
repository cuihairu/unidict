#include "stardict_parser_qt.h"

#include <QFileInfo>

namespace UnidictAdaptersQt {

static inline std::string cs(const QString& s) { return std::string(s.toUtf8().constData()); }
static inline QString qs(const std::string& s) { return QString::fromUtf8(s.c_str()); }

bool StarDictParserQt::loadDictionary(const QString& filePath) {
    if (!impl_.load_dictionary(cs(filePath))) {
        sourcePath_.clear();
        dictionaryId_.clear();
        return false;
    }

    QFileInfo info(filePath);
    sourcePath_ = info.canonicalFilePath();
    if (sourcePath_.isEmpty()) {
        sourcePath_ = info.absoluteFilePath();
    }
    dictionaryId_ = sourcePath_.toLower();
    return true;
}
bool StarDictParserQt::isLoaded() const { return impl_.is_loaded(); }
QStringList StarDictParserQt::getSupportedExtensions() const { return {"ifo", "idx", "dict", "dz"}; }

UnidictCore::DictionaryEntry StarDictParserQt::lookup(const QString& word) const {
    UnidictCore::DictionaryEntry e; auto d = impl_.lookup(cs(word)); if (!d.empty()) { e.word = word; e.definition = qs(d); } return e;
}

QStringList StarDictParserQt::findSimilar(const QString& word, int maxResults) const {
    QStringList out; auto v = impl_.find_similar(cs(word), maxResults); out.reserve((int)v.size()); for (auto& s : v) out.append(qs(s)); return out;
}

QStringList StarDictParserQt::getAllWords() const { QStringList out; auto v = impl_.all_words(); out.reserve((int)v.size()); for (auto& s : v) out.append(qs(s)); return out; }

QString StarDictParserQt::getDictionaryName() const { return qs(impl_.dictionary_name()); }
QString StarDictParserQt::getDictionaryDescription() const { return qs(impl_.dictionary_description()); }
int StarDictParserQt::getWordCount() const { return impl_.word_count(); }
QString StarDictParserQt::getSourcePath() const { return sourcePath_; }
QString StarDictParserQt::getDictionaryId() const { return dictionaryId_; }
QString StarDictParserQt::getFormatName() const { return "StarDict"; }

} // namespace UnidictAdaptersQt
