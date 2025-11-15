#include "fulltext_manager_qt.h"

#include <QRegularExpression>
#include <QByteArray>
#include <cstdlib>

namespace UnidictAdaptersQt {

using namespace UnidictCoreStd;

static QStringList split_env_paths(const QString& env) {
    if (env.isEmpty()) return {};
    return env.split(QRegularExpression("[:;]"), Qt::SkipEmptyParts);
}

FullTextManagerQt::FullTextManagerQt(QObject* parent)
    : QObject(parent)
    , mgr_(new DictionaryManagerStd) {}

bool FullTextManagerQt::loadDictionariesFromEnv() {
    const QString env = qEnvironmentVariable("UNIDICT_DICTS");
    if (env.isEmpty()) return false;
    const auto paths = split_env_paths(env);
    bool ok = false;
    for (const auto& p : paths) ok |= mgr_->add_dictionary(p.toUtf8().constData());
    mgr_->build_index();
    return ok;
}

bool FullTextManagerQt::saveIndex(const QString& path) const {
    return mgr_->save_fulltext_index(std::string(path.toUtf8().constData()));
}

QVariantMap FullTextManagerQt::statsFromFile(const QString& path) const {
    QVariantMap m;
    FullTextIndexStd ft;
    if (!ft.load(std::string(path.toUtf8().constData()))) {
        m["error"] = QString::fromUtf8(ft.last_error().c_str());
        return m;
    }
    auto s = ft.stats();
    m["version"] = s.version;
    m["docs"] = (qulonglong)s.docs;
    m["terms"] = (qulonglong)s.terms;
    m["postings"] = (qulonglong)s.postings;
    m["compressed_terms"] = (qulonglong)s.compressed_terms;
    m["compressed_bytes"] = (qulonglong)s.compressed_bytes;
    m["pairs_decompressed"] = (qulonglong)s.pairs_decompressed;
    m["avg_df"] = s.avg_df;
    return m;
}

QString FullTextManagerQt::loadIndex(const QString& path, const QString& compatMode) {
    const std::string p = std::string(path.toUtf8().constData());
    const std::string cm = std::string(compatMode.toUtf8().constData());
    if (cm == "strict") {
        if (!mgr_->load_fulltext_index(p)) return QStringLiteral("signature mismatch or invalid index");
        return {};
    } else if (cm == "auto") {
        if (mgr_->load_fulltext_index(p)) return {};
        int ver = 0; std::string err;
        if (mgr_->load_fulltext_index_relaxed(p, &ver, &err)) {
            if (ver == 1) return {}; // legacy loaded without signature
            return QString::fromUtf8(err.c_str());
        }
        return QString::fromUtf8(err.c_str());
    } else { // loose
        if (mgr_->load_fulltext_index(p)) return {};
        int ver = 0; std::string err;
        if (mgr_->load_fulltext_index_relaxed(p, &ver, &err)) return {};
        return QString::fromUtf8(err.c_str());
    }
}

bool FullTextManagerQt::upgrade(const QString& inPath, const QString& outPath) {
    FullTextIndexStd ft;
    if (!ft.load(std::string(inPath.toUtf8().constData()))) return false;
    return mgr_->save_fulltext_index(std::string(outPath.toUtf8().constData()));
}

} // namespace UnidictAdaptersQt

