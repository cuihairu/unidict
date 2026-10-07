#include "sync_manager_qt.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

#include "core/data_store.h"
#include "std/sync_backup_std.h"

using namespace UnidictCore;
using namespace UnidictCoreStd;

namespace UnidictAdaptersQt {

namespace {

constexpr const char* kKeyEnabled = "sync/enabled";
constexpr const char* kKeyGid = "sync/gid";
constexpr const char* kKeyRelayUrl = "sync/relayUrl";
constexpr const char* kKeyForm = "sync/transportForm";

QSettings syncSettings() {
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QCoreApplication::organizationName(),
                     QCoreApplication::applicationName());
}

QString engineStatePath() {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
        QStringLiteral("/sync");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/engine_state.json");
}

}  // namespace

SyncManagerQt::SyncManagerQt(QObject* parent) : QObject(parent) {
    loadEngineState();
    // 红线：开关位缺省关（QSettings 无值时不落 true）
    enabled_ = syncSettings().value(kKeyEnabled, false).toBool();
}

QString SyncManagerQt::qs(const std::string& s) {
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

std::string SyncManagerQt::cs(const QString& s) {
    return s.toUtf8().toStdString();
}

void SyncManagerQt::loadEngineState() {
    std::string err;
    const std::string path = cs(engineStatePath());
    if (!engine_.load_state(path, &err)) {
        // 无状态文件（首次运行）：把引擎自生成的 device_id 落盘钉住，
        // 之后每次启动都从文件恢复同一标识
        err.clear();
        engine_.save_state(path, &err);
    }
}

bool SyncManagerQt::enable(const QString& gid) {
    lastError_.clear();
    if (!SyncEngineStd::valid_gid(cs(gid))) {
        lastError_ = QStringLiteral("组 ID 形态不合法：16-64 位字母/数字/_/-");
        return false;
    }
    QSettings s = syncSettings();
    s.setValue(kKeyGid, gid);
    s.setValue(kKeyEnabled, true);
    s.sync();
    enabled_ = true;
    emit enabledChanged();
    return true;
}

void SyncManagerQt::disable() {
    QSettings s = syncSettings();
    s.setValue(kKeyEnabled, false);
    s.sync();
    enabled_ = false;
    emit enabledChanged();
}

QString SyncManagerQt::scopeText() const {
    return QStringLiteral(
        "开启后，以下数据将端到端加密同步到同步组：\n"
        "· 生词本（词与分组标签）\n"
        "· 词条笔记\n"
        "· 查词记录与偏好\n"
        "· 词典安装清单\n\n"
        "加密在你的设备上完成，中转服务器只见密文，密钥不出设备。\n"
        "同步默认关闭，可随时关闭。");
}

QString SyncManagerQt::groupId() const {
    return syncSettings().value(kKeyGid).toString();
}

QString SyncManagerQt::relayUrl() const {
    return syncSettings().value(kKeyRelayUrl).toString();
}

void SyncManagerQt::setRelayUrl(const QString& url) {
    QSettings s = syncSettings();
    s.setValue(kKeyRelayUrl, url);
    s.sync();
}

QString SyncManagerQt::transportForm() const {
    return syncSettings().value(kKeyForm, QStringLiteral("selfhost")).toString();
}

void SyncManagerQt::setTransportForm(const QString& form) {
    QSettings s = syncSettings();
    s.setValue(kKeyForm, form);
    s.sync();
}

QString SyncManagerQt::deviceId() const { return qs(engine_.device_id()); }

bool SyncManagerQt::exportBackup(const QString& path,
                                 const QString& passphrase) {
    lastError_.clear();
    if (passphrase.isEmpty()) {
        lastError_ = QStringLiteral("需要设置备份口令");
        return false;
    }
    try {
        SyncVocabStateStd st;
        const QVariantList vocab = DataStore::instance().getVocabularyMeta();
        for (const QVariant& v : vocab) {
            const QVariantMap m = v.toMap();
            const std::string word = cs(m.value("word").toString());
            if (word.empty()) continue;
            st.words.push_back(word);
            const QVariantList tags = m.value("tags").toList();
            if (tags.isEmpty()) continue;  // 空标签不建键（规范序口径）
            std::vector<std::string>& list = st.tags[word];
            for (const QVariant& t : tags) list.push_back(cs(t.toString()));
        }
        for (const QVariant& n : DataStore::instance().getNotes()) {
            const QVariantMap m = n.toMap();
            const std::string word = cs(m.value("word").toString());
            if (word.empty()) continue;
            st.notes[word] = cs(m.value("text").toString());
        }
        std::sort(st.words.begin(), st.words.end());
        for (auto& kv : st.tags) {
            std::sort(kv.second.begin(), kv.second.end());
        }
        const std::string blob = sync_backup_export(st, cs(passphrase));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            lastError_ = QStringLiteral("无法写入备份文件");
            return false;
        }
        f.write(blob.data(), static_cast<qint64>(blob.size()));
        f.close();
        return true;
    } catch (const std::exception& e) {
        lastError_ = QString::fromUtf8(e.what());
        return false;
    }
}

QVariantMap SyncManagerQt::restoreBackup(const QString& path,
                                         const QString& passphrase) {
    QVariantMap out;
    out[QStringLiteral("ok")] = false;
    out[QStringLiteral("words")] = 0;
    out[QStringLiteral("notes")] = 0;
    out[QStringLiteral("tags")] = 0;
    lastError_.clear();
    if (passphrase.isEmpty()) {
        lastError_ = QStringLiteral("需要输入备份口令");
        out[QStringLiteral("error")] = lastError_;
        return out;
    }
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        lastError_ = QStringLiteral("无法读取备份文件");
        out[QStringLiteral("error")] = lastError_;
        return out;
    }
    const QByteArray bytes = f.readAll();
    f.close();

    try {
        SyncVocabStateStd st;
        std::string err;
        if (!sync_backup_import(
                std::string(bytes.constData(),
                            static_cast<std::size_t>(bytes.size())),
                cs(passphrase), st, &err)) {
            lastError_ = qs(err);
            out[QStringLiteral("error")] = lastError_;
            return out;
        }
        DataStore& ds = DataStore::instance();
        // 并入语义：已有不动，缺的补上（对半新设备上的误恢复最安全；
        // 要完全对齐备份先在生词本里手动清空）
        QSet<QString> existing;
        const QVariantList vocab = ds.getVocabularyMeta();
        existing.reserve(static_cast<qsizetype>(vocab.size()));
        for (const QVariant& v : vocab) {
            existing.insert(v.toMap().value(QStringLiteral("word"))
                                .toString().toLower());
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        int words = 0, notes = 0, tags = 0;
        for (const std::string& w : st.words) {
            const QString word = qs(w);
            if (word.isEmpty() || existing.contains(word.toLower())) continue;
            // 定义留空：定义随本机词典重查（与同步面同模型）
            ds.addVocabularyItemWithTime(word, QString(), now);
            ++words;
        }
        for (const auto& kv : st.notes) {
            const QString word = qs(kv.first);
            const QString note = qs(kv.second);
            if (word.isEmpty() || note.isEmpty()) continue;
            if (!ds.getNote(word).isEmpty()) continue;  // 已有不动
            ds.setNote(word, note);
            ++notes;
        }
        for (const auto& kv : st.tags) {
            const QString word = qs(kv.first);
            for (const std::string& t : kv.second) {
                // false = 词不在或标签已有：并入语义下的自然幂等
                if (ds.addVocabularyItemTag(word, qs(t))) ++tags;
            }
        }
        out[QStringLiteral("ok")] = true;
        out[QStringLiteral("words")] = words;
        out[QStringLiteral("notes")] = notes;
        out[QStringLiteral("tags")] = tags;
        emit backupRestored();
    } catch (const std::exception& e) {
        lastError_ = QString::fromUtf8(e.what());
        out[QStringLiteral("error")] = lastError_;
    }
    return out;
}

}  // namespace UnidictAdaptersQt
