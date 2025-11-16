#include "fulltext_manager_qt.h"

#include <QRegularExpression>
#include <QByteArray>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
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
    m["signature"] = QString::fromUtf8(ft.signature().c_str());
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

QString FullTextManagerQt::currentSignature() const {
    return QString::fromUtf8(mgr_->fulltext_signature().c_str());
}

QVariantMap FullTextManagerQt::currentStats() const {
    QVariantMap m;
    auto s = mgr_->fulltext_stats();
    m["version"] = s.version;
    m["docs"] = (qulonglong)s.docs;
    m["terms"] = (qulonglong)s.terms;
    m["postings"] = (qulonglong)s.postings;
    m["compressed_terms"] = (qulonglong)s.compressed_terms;
    m["compressed_bytes"] = (qulonglong)s.compressed_bytes;
    m["pairs_decompressed"] = (qulonglong)s.pairs_decompressed;
    m["avg_df"] = s.avg_df;
    m["signature"] = currentSignature();
    return m;
}

QString FullTextManagerQt::verifyIndexMatch(const QString& path) const {
    FullTextIndexStd ft;
    const std::string p = std::string(path.toUtf8().constData());
    if (!ft.load(p)) {
        return QString::fromUtf8(ft.last_error().c_str());
    }
    // Legacy v1 has no signature; treat as unverifiable-but-ok for quick checks
    if (ft.version() == 1 || ft.signature().empty()) {
        return {};
    }
    const std::string cur = mgr_->fulltext_signature();
    if (ft.signature() == cur) {
        return {};
    }
    return QStringLiteral("signature mismatch (file does not match currently loaded dictionaries)");
}

QVariantMap FullTextManagerQt::verifyIndexDetailed(const QString& path) const {
    QVariantMap out;
    out["ok"] = false;
    out["version"] = 0;
    out["match"] = false;
    out["fileSignature"] = "";
    out["currentSignature"] = QString::fromUtf8(mgr_->fulltext_signature().c_str());
    out["fileSigPrefix"] = "";
    out["currentSigPrefix"] = "";
    out["error"] = "";
    // Add dictionary summaries for diagnostics
    auto curMeta = mgr_->dictionaries_meta();
    {
        QVariantList arr;
        for (const auto& m : curMeta) {
            QVariantMap mm; mm["name"] = QString::fromUtf8(m.name.c_str()); mm["wordCount"] = m.word_count;
            arr.push_back(mm);
        }
        out["currentDicts"] = arr;
    }
    // Helper: parse signature payload into source file summaries
    auto parse_sources = [](const QString& sig) {
        QVariantList dicts;
        int bar = sig.indexOf('|');
        if (bar == -1) return dicts;
        QString payload = sig.mid(bar + 1);
        // segments separated by ';'
        const auto segments = payload.split(';', Qt::SkipEmptyParts);
        for (const auto& seg : segments) {
            const auto parts = seg.split('|', Qt::SkipEmptyParts);
            if (parts.size() < 2) continue;
            QVariantMap dm;
            dm["name"] = parts[0];
            // find sources after the first few fields (name, count, maybe first/last word), detect by '#'
            // We reconstruct source list by scanning for '#' delimiters.
            QVariantList files;
            QVariantList filesRaw;
            int hashPos = seg.indexOf('#');
            if (hashPos != -1) {
                // sources are like path|size|mtime#path|size|mtime#...
                QString sourcesPart = seg.mid(hashPos);
                const auto srcs = sourcesPart.split('#', Qt::SkipEmptyParts);
                int cap = 0;
                for (const auto& s : srcs) {
                    const auto f = s.split('|');
                    if (!f.isEmpty()) {
                        QString path = f.value(0);
                        QString sz = f.value(1);
                        QString mt = f.value(2);
                        files.push_back(QString("%1%2%3").arg(path, sz.isEmpty() ? "" : QString(" (") + sz + ")", (mt.isEmpty() ? "" : QString("," + mt + ")")));
                        QVariantMap fr; fr["path"] = path; fr["size"] = sz; fr["mtime"] = mt; filesRaw.push_back(fr);
                        if (++cap >= 3) break;
                    }
                }
            }
            dm["files"] = files;
            dm["filesRaw"] = filesRaw;
            dicts.push_back(dm);
            if (dicts.size() >= 3) break; // cap dicts
        }
        return dicts;
    };
    FullTextIndexStd ft;
    const std::string p = std::string(path.toUtf8().constData());
    if (!ft.load(p)) {
        out["error"] = QString::fromUtf8(ft.last_error().c_str());
        return out;
    }
    out["ok"] = true;
    out["version"] = ft.version();
    const std::string fs = ft.signature();
    out["fileSignature"] = QString::fromUtf8(fs.c_str());
    const QString cs = out["currentSignature"].toString();
    auto prefix = [](const QString& s)->QString { return s.left(32); };
    out["fileSigPrefix"] = prefix(out["fileSignature"].toString());
    out["currentSigPrefix"] = prefix(cs);
    // Source file summaries for file/current
    QVariantList fileSrc = parse_sources(out["fileSignature"].toString());
    QVariantList curSrc = parse_sources(cs);
    out["fileSources"] = fileSrc;
    out["currentSources"] = curSrc;
    // Compute source diffs (best-effort by path; ignores dict grouping)
    QHash<QString, QPair<QString,QString>> FM, CM; // path -> (size, mtime)
    QHash<QString, QString> FOwner, COwner; // path -> dict name
    auto collectMeta = [](const QVariantList& lst, QHash<QString,QPair<QString,QString>>& M, QHash<QString,QString>& Owner){
        for (const auto& v : lst) {
            QVariantMap dm = v.toMap();
            QString dname = dm.value("name").toString();
            QVariantList raws = dm.value("filesRaw").toList();
            for (const auto& r : raws) {
                QVariantMap m = r.toMap();
                QString p = m.value("path").toString();
                if (p.isEmpty()) continue;
                QString sz = m.value("size").toString();
                QString mt = m.value("mtime").toString();
                M.insert(p, {sz, mt});
                if (!Owner.contains(p)) Owner.insert(p, dname);
            }
        }
    };
    collectMeta(fileSrc, FM, FOwner);
    collectMeta(curSrc, CM, COwner);
    QStringList added, removed, changed;
    QVariantList addedDet, removedDet, changedDet;
    // Added: in file only
    for (auto it = FM.constBegin(); it != FM.constEnd(); ++it) {
        if (!CM.contains(it.key())) {
            added.append(it.key());
            QVariantMap d; d["path"] = it.key(); d["ownerFile"] = FOwner.value(it.key());
            d["sizeFile"] = it.value().first; d["mtimeFile"] = it.value().second;
            addedDet.push_back(d);
        }
    }
    // Removed: in current only
    for (auto it = CM.constBegin(); it != CM.constEnd(); ++it) {
        if (!FM.contains(it.key())) {
            removed.append(it.key());
            QVariantMap d; d["path"] = it.key(); d["ownerCurrent"] = COwner.value(it.key());
            d["sizeCurrent"] = it.value().first; d["mtimeCurrent"] = it.value().second;
            removedDet.push_back(d);
        }
    }
    // Changed: in both but size/mtime differ
    for (auto it = FM.constBegin(); it != FM.constEnd(); ++it) {
        auto jt = CM.find(it.key());
        if (jt != CM.end()) {
            if (it.value().first != jt.value().first || it.value().second != jt.value().second) {
                changed.append(it.key());
                QString reason;
                if (it.value().first != jt.value().first) reason += (reason.isEmpty() ? "" : ",") + QString("size");
                if (it.value().second != jt.value().second) reason += (reason.isEmpty() ? "" : ",") + QString("mtime");
                QVariantMap d; d["path"] = it.key();
                d["ownerFile"] = FOwner.value(it.key());
                d["ownerCurrent"] = COwner.value(it.key());
                d["reason"] = reason;
                d["sizeFile"] = it.value().first; d["mtimeFile"] = it.value().second;
                d["sizeCurrent"] = jt.value().first; d["mtimeCurrent"] = jt.value().second;
                changedDet.push_back(d);
            }
        }
    }
    out["addedSourcePaths"] = added;
    out["removedSourcePaths"] = removed;
    out["changedSourcePaths"] = changed;
    out["addedSourcesDetailed"] = addedDet;
    out["removedSourcesDetailed"] = removedDet;
    out["changedSourcesDetailed"] = changedDet;
    // Attempt to parse a minimal dict summary from signature (best-effort)
    // signature format: hex|payload; payload includes "name|word_count|..." segments
    QVariantList farr;
    int bar = out["fileSignature"].toString().indexOf('|');
    if (bar != -1) {
        QString payload = out["fileSignature"].toString().mid(bar + 1);
        // Split by ';' for dict segments, then for each segment split by '|'
        const auto segments = payload.split(';', Qt::SkipEmptyParts);
        for (const auto& seg : segments) {
            const auto parts = seg.split('|', Qt::SkipEmptyParts);
            if (parts.size() >= 2) {
                QVariantMap mm; mm["name"] = parts[0]; mm["wordCount"] = parts[1].toInt();
                farr.push_back(mm);
            }
        }
    }
    out["fileDicts"] = farr;
    // Legacy v1: no signature; treat as unverifiable-but-ok
    if (ft.version() == 1 || fs.empty()) {
        out["match"] = true;
        return out;
    }
    out["match"] = (out["fileSignature"].toString() == cs);
    if (!out["match"].toBool()) {
        out["error"] = QStringLiteral("signature mismatch");
    }
	    return out;
	}
	
	bool FullTextManagerQt::exportSourceDiff(const QVariantMap& verifyResult, const QString& outPath) const {
	    // Build a compact JSON document that captures the diff and basic context.
	    QJsonObject root;
	    root.insert("generatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	    root.insert("version", QJsonValue::fromVariant(verifyResult.value("version")));
	    root.insert("match", QJsonValue::fromVariant(verifyResult.value("match")));
	    root.insert("fileSignature", QJsonValue::fromVariant(verifyResult.value("fileSignature")));
	    root.insert("currentSignature", QJsonValue::fromVariant(verifyResult.value("currentSignature")));
	    auto toArr = [](const QVariantList& lst) {
	        QJsonArray a;
	        for (const auto& v : lst) {
	            a.push_back(QJsonObject::fromVariantMap(v.toMap()));
	        }
	        return a;
	    };
	    root.insert("added", toArr(verifyResult.value("addedSourcesDetailed").toList()));
	    root.insert("removed", toArr(verifyResult.value("removedSourcesDetailed").toList()));
	    root.insert("changed", toArr(verifyResult.value("changedSourcesDetailed").toList()));
	    // Optionally include a brief per-dictionary summary for changed items
	    // (best-effort using owner fields if present).
	    QHash<QString, int> perDict;
	    for (const auto& key : {"addedSourcesDetailed","removedSourcesDetailed","changedSourcesDetailed"}) {
	        for (const auto& v : verifyResult.value(key).toList()) {
	            const auto m = v.toMap();
	            const QString o1 = m.value("ownerFile").toString();
	            const QString o2 = m.value("ownerCurrent").toString();
	            if (!o1.isEmpty()) perDict[o1] += 1;
	            if (!o2.isEmpty() && o2 != o1) perDict[o2] += 1;
	        }
	    }
	    QJsonArray dictSummary;
	    for (auto it = perDict.constBegin(); it != perDict.constEnd(); ++it) {
	        QJsonObject o; o.insert("dict", it.key()); o.insert("count", it.value());
	        dictSummary.push_back(o);
	    }
	    root.insert("dictSummary", dictSummary);
	    // Write atomically
	    QSaveFile f(outPath);
	    if (!f.open(QIODevice::WriteOnly)) {
	        return false;
	    }
	    QJsonDocument doc(root);
	    if (f.write(doc.toJson(QJsonDocument::Indented)) < 0) {
	        return false;
	    }
	    return f.commit();
	}
	
	QVariantMap FullTextManagerQt::loadIndexDetailed(const QString& path, const QString& compatMode) {
	    QVariantMap out;
	    out["ok"] = false;
	    out["mode"] = compatMode;
	    out["version"] = 0;
    out["fileSignature"] = "";
    out["currentSignature"] = QString::fromUtf8(mgr_->fulltext_signature().c_str());
    out["error"] = "";
    const std::string p = std::string(path.toUtf8().constData());
    const std::string cm = std::string(compatMode.toUtf8().constData());
    int ver = 0; std::string err;
    if (cm == "strict") {
        if (mgr_->load_fulltext_index(p)) { out["ok"] = true; out["version"] = 3; return out; }
        out["error"] = "strict mode: signature mismatch or invalid index";
        return out;
    } else if (cm == "auto") {
        if (mgr_->load_fulltext_index(p)) { out["ok"] = true; out["version"] = 3; return out; }
        if (mgr_->load_fulltext_index_relaxed(p, &ver, &err)) {
            out["ok"] = (ver == 1); // only legacy v1 accepted in auto fallback
            out["version"] = ver;
            if (!out["ok"].toBool()) out["error"] = QString::fromUtf8(err.c_str());
            return out;
        }
        out["error"] = QString::fromUtf8(err.c_str());
        return out;
    } else { // loose
        if (mgr_->load_fulltext_index(p)) { out["ok"] = true; out["version"] = 3; return out; }
        if (mgr_->load_fulltext_index_relaxed(p, &ver, &err)) {
            out["ok"] = true;
            out["version"] = ver;
            return out;
        }
        out["error"] = QString::fromUtf8(err.c_str());
        return out;
    }
}

} // namespace UnidictAdaptersQt
