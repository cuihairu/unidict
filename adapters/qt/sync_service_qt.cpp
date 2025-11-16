#include "sync_service_qt.h"

#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTextStream>
#include <unordered_set>

#include "core/data_store.h"

using namespace UnidictCore;

namespace UnidictAdaptersQt {

SyncServiceQt::SyncServiceQt(QObject* parent) : QObject(parent) {}

void SyncServiceQt::setSyncFile(const QString& path) { syncPath_ = path; }
QString SyncServiceQt::syncFile() const { return syncPath_; }

static QStringList ordered_union(const QStringList& first, const QStringList& second) {
    QStringList out; out.reserve(first.size() + second.size());
    QSet<QString> seen;
    for (const auto& s : first) { if (!seen.contains(s)) { out.append(s); seen.insert(s); } }
    for (const auto& s : second) { if (!seen.contains(s)) { out.append(s); seen.insert(s); } }
    return out;
}

static bool read_remote(const QString& path, QStringList& remoteHist, QList<QVariantMap>& remoteVocab, QString* err) {
    remoteHist.clear(); remoteVocab.clear();
    QFile f(path);
    if (!f.exists()) return true; // treat as empty remote
    if (!f.open(QIODevice::ReadOnly)) { if (err) *err = "cannot open sync file"; return false; }
    const QByteArray bytes = f.readAll(); f.close();
    QJsonParseError pe{0,QJsonParseError::NoError};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { if (err) *err = "invalid JSON"; return false; }
    const QJsonObject obj = doc.object();
    const QJsonArray h = obj.value("history").toArray();
    for (const auto& v : h) { if (v.isString()) remoteHist.append(v.toString()); }
    const QJsonArray va = obj.value("vocab").toArray();
    for (const auto& it : va) {
        if (!it.isObject()) continue;
        const QJsonObject o = it.toObject();
        QVariantMap m; m["word"] = o.value("word").toString();
        m["definition"] = o.value("definition").toString();
        m["added_at"] = static_cast<qlonglong>(o.value("added_at").toVariant().toLongLong());
        if (!m.value("word").toString().isEmpty()) remoteVocab.append(m);
    }
    return true;
}

bool SyncServiceQt::syncNow() {
    if (syncPath_.isEmpty()) { lastError_ = "sync file not set"; return false; }
    // 1) Read local
    const QStringList localHist = DataStore::instance().getSearchHistory(1000000);
    const QVariantList localMeta = DataStore::instance().getVocabularyMeta();

    // 2) Read remote (if exists)
    QStringList remoteHist;
    QList<QVariantMap> remoteVocab;
    {
        QString err;
        if (!read_remote(syncPath_, remoteHist, remoteVocab, &err)) { lastError_ = err; return false; }
    }

    // 3) Merge
    QStringList mergedHist = ordered_union(localHist, remoteHist);
    struct Merged { QString word; QString def; qlonglong added_at; bool fromLocal; };
    QMap<QString, Merged> byWord; // key: lower(word)
    // Seed from local meta
    for (const auto& v : localMeta) {
        QVariantMap m = v.toMap();
        QString w = m.value("word").toString();
        if (w.isEmpty()) continue;
        QString d = m.value("definition").toString();
        qlonglong t = m.value("added_at").toLongLong(); // 0 if unknown
        byWord[w.toLower()] = { w, d, t, true };
    }
    // Merge remote with latest-wins (by added_at)
    for (const auto& r : remoteVocab) {
        const QString k = r.value("word").toString().toLower();
        auto it = byWord.find(k);
        if (it == byWord.end()) {
            byWord[k] = { r.value("word").toString(), r.value("definition").toString(), r.value("added_at").toLongLong(), false };
        } else {
            // If remote has newer timestamp, take remote value; else keep local
            if (r.value("added_at").toLongLong() > it->added_at) {
                it->word = r.value("word").toString();
                it->def = r.value("definition").toString();
                it->added_at = r.value("added_at").toLongLong();
                it->fromLocal = false;
            }
        }
    }
    // Serialize merged to simple list for local DataStore update
    QList<DictionaryEntry> mergedVocab;
    mergedVocab.reserve(byWord.size());
    for (auto it = byWord.begin(); it != byWord.end(); ++it) {
        DictionaryEntry e; e.word = it->word; e.definition = it->def;
        mergedVocab.append(e);
    }

    // 4) Write back to local
    DataStore::instance().clearHistory();
    for (const auto& s : mergedHist) DataStore::instance().addSearchHistory(s);
    DataStore::instance().clearVocabulary();
    for (auto it = byWord.begin(); it != byWord.end(); ++it) {
        DataStore::instance().addVocabularyItemWithTime(it->word, it->def, it->added_at);
    }

    // 5) Write to remote
    {
        QFile f(syncPath_);
        QDir().mkpath(QFileInfo(f).dir().absolutePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { lastError_ = "cannot open sync file for write"; return false; }
        QJsonObject obj;
        obj["version"] = 1;
        obj["synced_at"] = static_cast<qlonglong>(QDateTime::currentSecsSinceEpoch());
        QJsonArray h; for (const auto& s : mergedHist) h.append(s);
        obj["history"] = h;
        QJsonArray va;
        for (auto it = byWord.begin(); it != byWord.end(); ++it) {
            QJsonObject o; o["word"] = it->word; o["definition"] = it->def;
            if (it->added_at > 0) o["added_at"] = static_cast<qlonglong>(it->added_at);
            va.append(o);
        }
        obj["vocab"] = va;
        QJsonDocument doc(obj);
        f.write(doc.toJson(QJsonDocument::Indented));
        f.close();
    }
    return true;
}

QVariantMap SyncServiceQt::previewDiff() const {
    QVariantMap out; out["ok"] = false; out["error"] = "";
    if (syncPath_.isEmpty()) { out["error"] = "sync file not set"; return out; }
    // Local
    const QVariantList localMeta = DataStore::instance().getVocabularyMeta();
    QMap<QString, QPair<qlonglong, QString>> L; // lower(word) -> (ts, def)
    for (const auto& v : localMeta) {
        QVariantMap m = v.toMap();
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        L[w.toLower()] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
    }
    // Remote
    QStringList remoteHist;
    QList<QVariantMap> remoteVocab;
    QString err;
    if (!read_remote(syncPath_, remoteHist, remoteVocab, &err)) { out["error"] = err; return out; }
    QMap<QString, QPair<qlonglong, QString>> R;
    for (const auto& m : remoteVocab) {
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        R[w.toLower()] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
    }
    // Compute sets
    QStringList localOnly, remoteOnly;
    QVariantList remoteNewer, localNewer;
    QSet<QString> keys = QSet<QString>(L.keys().begin(), L.keys().end()) + QSet<QString>(R.keys().begin(), R.keys().end());
    for (const auto& k : keys) {
        bool inL = L.contains(k), inR = R.contains(k);
        if (inL && !inR) localOnly.append(k);
        else if (!inL && inR) remoteOnly.append(k);
        else if (inL && inR) {
            qlonglong tl = L[k].first, tr = R[k].first;
            if (tr > tl) {
                QVariantMap m; m["word"] = k; m["local_ts"] = tl; m["remote_ts"] = tr; remoteNewer.append(m);
            } else if (tl > tr) {
                QVariantMap m2; m2["word"] = k; m2["local_ts"] = tl; m2["remote_ts"] = tr; localNewer.append(m2);
            }
        }
    }
    out["ok"] = true;
    out["localOnly"] = localOnly;
    out["remoteOnly"] = remoteOnly;
    out["remoteNewer"] = remoteNewer;
    out["localNewer"] = localNewer;
    return out;
}

bool SyncServiceQt::applyPreview(bool takeRemoteNewer, bool takeLocalNewer,
                                 bool includeRemoteOnly, bool includeLocalOnly) {
    if (syncPath_.isEmpty()) { lastError_ = "sync file not set"; return false; }
    // Build maps
    const QVariantList localMeta = DataStore::instance().getVocabularyMeta();
    QMap<QString, QPair<qlonglong, QString>> L; // lower(word) -> (ts, def)
    QMap<QString, QString> LWord; // lower -> original word case
    for (const auto& v : localMeta) {
        QVariantMap m = v.toMap();
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        QString k = w.toLower();
        L[k] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
        LWord[k] = w;
    }
    QStringList remoteHist;
    QList<QVariantMap> remoteVocab;
    QString err;
    if (!read_remote(syncPath_, remoteHist, remoteVocab, &err)) { lastError_ = err; return false; }
    QMap<QString, QPair<qlonglong, QString>> R; // lower -> (ts, def)
    QMap<QString, QString> RWord;
    for (const auto& m : remoteVocab) {
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        QString k = w.toLower();
        R[k] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
        RWord[k] = w;
    }
    // Compute outputs
    // Local output starts from current local
    QMap<QString, QPair<qlonglong, QString>> Lout = L;
    QMap<QString, QString> LoutWord = LWord;
    // Track changes for logging
    QStringList pulledRemoteOnly, pushedLocalOnly, updatedLocalFromRemote, updatedRemoteFromLocal;
    // Pull remote-only
    if (includeRemoteOnly) {
        for (auto it = R.begin(); it != R.end(); ++it) {
            const QString& k = it.key();
            if (!Lout.contains(k)) {
                Lout[k] = it.value();
                LoutWord[k] = RWord.value(k, k);
                pulledRemoteOnly.append(LoutWord.value(k, k));
            }
        }
    }
    // Apply remote newer
    if (takeRemoteNewer) {
        for (auto it = R.begin(); it != R.end(); ++it) {
            const QString& k = it.key();
            if (Lout.contains(k)) {
                if (it.value().first > Lout[k].first) {
                    Lout[k] = it.value();
                    LoutWord[k] = RWord.value(k, k);
                    updatedLocalFromRemote.append(LoutWord.value(k, k));
                }
            }
        }
    }
    // Remote output starts from current remote
    QMap<QString, QPair<qlonglong, QString>> Rout = R;
    QMap<QString, QString> RoutWord = RWord;
    // Push local-only
    if (includeLocalOnly) {
        for (auto it = L.begin(); it != L.end(); ++it) {
            const QString& k = it.key();
            if (!Rout.contains(k)) {
                Rout[k] = it.value();
                RoutWord[k] = LWord.value(k, k);
                pushedLocalOnly.append(RoutWord.value(k, k));
            }
        }
    }
    // Apply local newer
    if (takeLocalNewer) {
        for (auto it = L.begin(); it != L.end(); ++it) {
            const QString& k = it.key();
            if (Rout.contains(k)) {
                if (it.value().first > Rout[k].first) {
                    Rout[k] = it.value();
                    RoutWord[k] = LWord.value(k, k);
                    updatedRemoteFromLocal.append(RoutWord.value(k, k));
                }
            }
        }
    }
    // Write back local
    DataStore::instance().clearVocabulary();
    for (auto it = Lout.begin(); it != Lout.end(); ++it) {
        const QString word = LoutWord.value(it.key(), it.key());
        DataStore::instance().addVocabularyItemWithTime(word, it.value().second, it.value().first);
    }
    // History: keep ordered union (same as syncNow)
    const QStringList localHist = DataStore::instance().getSearchHistory(1000000);
    QStringList mergedHist = ordered_union(localHist, remoteHist);
    DataStore::instance().clearHistory();
    for (const auto& s : mergedHist) DataStore::instance().addSearchHistory(s);
    // Write back remote
    QFile f(syncPath_);
    QDir().mkpath(QFileInfo(f).dir().absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) { lastError_ = "cannot open sync file for write"; return false; }
    QJsonObject obj;
    obj["version"] = 1;
    obj["synced_at"] = static_cast<qlonglong>(QDateTime::currentSecsSinceEpoch());
    QJsonArray h; for (const auto& s : mergedHist) h.append(s);
    obj["history"] = h;
    QJsonArray va;
    for (auto it = Rout.begin(); it != Rout.end(); ++it) {
        const QString word = RoutWord.value(it.key(), it.key());
        QJsonObject o; o["word"] = word; o["definition"] = it.value().second;
        if (it.value().first > 0) o["added_at"] = static_cast<qlonglong>(it.value().first);
        va.append(o);
    }
    obj["vocab"] = va;
    // Attach last_changes
    {
        QJsonObject lc;
        lc["includeRemoteOnly"] = includeRemoteOnly;
        lc["includeLocalOnly"] = includeLocalOnly;
        lc["takeRemoteNewer"] = takeRemoteNewer;
        lc["takeLocalNewer"] = takeLocalNewer;
        auto toArr = [](const QStringList& lst) {
            QJsonArray a; for (const auto& s : lst) a.append(s); return a;
        };
        lc["pulled_remote_only"] = toArr(pulledRemoteOnly);
        lc["pushed_local_only"] = toArr(pushedLocalOnly);
        lc["updated_local_from_remote"] = toArr(updatedLocalFromRemote);
        lc["updated_remote_from_local"] = toArr(updatedRemoteFromLocal);
        obj["last_changes"] = lc;
    }
    QJsonDocument doc(obj);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QVariantMap SyncServiceQt::lastChanges() const {
    QVariantMap out; out["ok"] = false; out["error"] = ""; out["changes"] = QVariantMap{};
    if (syncPath_.isEmpty()) { out["error"] = "sync file not set"; return out; }
    QFile f(syncPath_);
    if (!f.exists()) { out["ok"] = true; return out; } // nothing to show
    if (!f.open(QIODevice::ReadOnly)) { out["error"] = "cannot open sync file"; return out; }
    const QByteArray bytes = f.readAll(); f.close();
    QJsonParseError pe{0,QJsonParseError::NoError};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { out["error"] = "invalid JSON"; return out; }
    QJsonObject obj = doc.object();
    if (!obj.contains("last_changes")) { out["ok"] = true; return out; }
    out["ok"] = true;
    out["changes"] = obj.value("last_changes").toVariant();
    return out;
}

bool SyncServiceQt::applySelection(const QVariantMap& selection) {
    if (syncPath_.isEmpty()) return false;
    // Build maps
    const QVariantList localMeta = DataStore::instance().getVocabularyMeta();
    QMap<QString, QPair<qlonglong, QString>> L; // lower(word) -> (ts, def)
    QMap<QString, QString> LWord; // lower -> original word case
    for (const auto& v : localMeta) {
        QVariantMap m = v.toMap();
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        QString k = w.toLower();
        L[k] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
        LWord[k] = w;
    }
    QStringList remoteHist;
    QList<QVariantMap> remoteVocab;
    QString err;
    if (!read_remote(syncPath_, remoteHist, remoteVocab, &err)) return false;
    QMap<QString, QPair<qlonglong, QString>> R; // lower -> (ts, def)
    QMap<QString, QString> RWord;
    for (const auto& m : remoteVocab) {
        QString w = m.value("word").toString(); if (w.isEmpty()) continue;
        QString k = w.toLower();
        R[k] = { m.value("added_at").toLongLong(), m.value("definition").toString() };
        RWord[k] = w;
    }
    // Parse selection lists (case-insensitive keys)
    auto toLowerSet = [](const QVariant& v) {
        QSet<QString> s; for (const auto& it : v.toStringList()) s.insert(it.toLower()); return s;
    };
    QSet<QString> selRemoteOnly = toLowerSet(selection.value("remoteOnly"));
    QSet<QString> selLocalOnly = toLowerSet(selection.value("localOnly"));
    QSet<QString> selRemoteNewer = toLowerSet(selection.value("remoteNewer"));
    QSet<QString> selLocalNewer = toLowerSet(selection.value("localNewer"));
    // Outputs
    QMap<QString, QPair<qlonglong, QString>> Lout = L;
    QMap<QString, QString> LoutWord = LWord;
    QMap<QString, QPair<qlonglong, QString>> Rout = R;
    QMap<QString, QString> RoutWord = RWord;
    // Track changes
    QStringList pulledRemoteOnly, pushedLocalOnly, updatedLocalFromRemote, updatedRemoteFromLocal;
    // Apply remoteOnly selection: add to local
    for (const auto& k : selRemoteOnly) {
        if (R.contains(k) && !Lout.contains(k)) {
            Lout[k] = R[k];
            LoutWord[k] = RWord.value(k, k);
            pulledRemoteOnly.append(LoutWord.value(k, k));
        }
    }
    // Apply localOnly selection: add to remote
    for (const auto& k : selLocalOnly) {
        if (L.contains(k) && !Rout.contains(k)) {
            Rout[k] = L[k];
            RoutWord[k] = LWord.value(k, k);
            pushedLocalOnly.append(RoutWord.value(k, k));
        }
    }
    // Apply remoteNewer selection: update local from remote (force override)
    for (const auto& k : selRemoteNewer) {
        if (R.contains(k)) {
            Lout[k] = R[k];
            LoutWord[k] = RWord.value(k, k);
            updatedLocalFromRemote.append(LoutWord.value(k, k));
        }
    }
    // Apply localNewer selection: update remote from local (force override)
    for (const auto& k : selLocalNewer) {
        if (L.contains(k)) {
            Rout[k] = L[k];
            RoutWord[k] = LWord.value(k, k);
            updatedRemoteFromLocal.append(RoutWord.value(k, k));
        }
    }
    // Write back local
    DataStore::instance().clearVocabulary();
    for (auto it = Lout.begin(); it != Lout.end(); ++it) {
        const QString word = LoutWord.value(it.key(), it.key());
        DataStore::instance().addVocabularyItemWithTime(word, it.value().second, it.value().first);
    }
    // History union
    const QStringList localHist = DataStore::instance().getSearchHistory(1000000);
    QStringList mergedHist = ordered_union(localHist, remoteHist);
    DataStore::instance().clearHistory();
    for (const auto& s : mergedHist) DataStore::instance().addSearchHistory(s);
    // Write back remote
    QFile f(syncPath_);
    QDir().mkpath(QFileInfo(f).dir().absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonObject obj;
    obj["version"] = 1;
    obj["synced_at"] = static_cast<qlonglong>(QDateTime::currentSecsSinceEpoch());
    QJsonArray h; for (const auto& s : mergedHist) h.append(s);
    obj["history"] = h;
    QJsonArray va;
    for (auto it = Rout.begin(); it != Rout.end(); ++it) {
        const QString word = RoutWord.value(it.key(), it.key());
        QJsonObject o; o["word"] = word; o["definition"] = it.value().second;
        if (it.value().first > 0) o["added_at"] = static_cast<qlonglong>(it.value().first);
        va.append(o);
    }
    obj["vocab"] = va;
    // Store last_changes with selection lists
    {
        QJsonObject lc;
        auto toArr = [](const QStringList& lst) { QJsonArray a; for (const auto& s : lst) a.append(s); return a; };
        lc["selection_remote_only"] = toArr(selection.value("remoteOnly").toStringList());
        lc["selection_local_only"] = toArr(selection.value("localOnly").toStringList());
        lc["selection_remote_newer"] = toArr(selection.value("remoteNewer").toStringList());
        lc["selection_local_newer"] = toArr(selection.value("localNewer").toStringList());
        lc["pulled_remote_only"] = toArr(pulledRemoteOnly);
        lc["pushed_local_only"] = toArr(pushedLocalOnly);
        lc["updated_local_from_remote"] = toArr(updatedLocalFromRemote);
        lc["updated_remote_from_local"] = toArr(updatedRemoteFromLocal);
        obj["last_changes"] = lc;
    }
    QJsonDocument doc(obj);
    f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

bool SyncServiceQt::exportSelection(const QVariantMap& selection, const QString& path) const {
    QJsonObject obj;
    auto toArr = [](const QStringList& lst) { QJsonArray a; for (const auto& s : lst) a.append(s); return a; };
    obj["remoteOnly"] = toArr(selection.value("remoteOnly").toStringList());
    obj["localOnly"] = toArr(selection.value("localOnly").toStringList());
    obj["remoteNewer"] = toArr(selection.value("remoteNewer").toStringList());
    obj["localNewer"] = toArr(selection.value("localNewer").toStringList());
    obj["exported_at"] = static_cast<qlonglong>(QDateTime::currentSecsSinceEpoch());
    QFile f(path);
    QDir().mkpath(QFileInfo(f).dir().absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc(obj);
    const auto n = f.write(doc.toJson(QJsonDocument::Indented));
    f.close();
    return n >= 0;
}

QVariantMap SyncServiceQt::importSelection(const QString& path) const {
    QVariantMap out; out["ok"] = false; out["error"] = "";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { out["error"] = "cannot open file"; return out; }
    const QByteArray bytes = f.readAll(); f.close();
    QJsonParseError pe{0,QJsonParseError::NoError};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) { out["error"] = "invalid JSON"; return out; }
    const QJsonObject obj = doc.object();
    QVariantMap sel;
    auto toStrList = [](const QJsonArray& a) {
        QStringList lst; for (const auto& v : a) if (v.isString()) lst.append(v.toString()); return lst;
    };
    sel["remoteOnly"] = toStrList(obj.value("remoteOnly").toArray());
    sel["localOnly"] = toStrList(obj.value("localOnly").toArray());
    sel["remoteNewer"] = toStrList(obj.value("remoteNewer").toArray());
    sel["localNewer"] = toStrList(obj.value("localNewer").toArray());
    out["selection"] = sel;
    out["ok"] = true;
    return out;
}

} // namespace UnidictAdaptersQt
