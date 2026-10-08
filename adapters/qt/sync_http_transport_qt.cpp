#include "sync_http_transport_qt.h"

#include <QByteArray>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

using UnidictCoreStd::EnqueuedOpStd;
using UnidictCoreStd::GroupMetaStd;
using UnidictCoreStd::RemoteOpStd;

namespace UnidictAdaptersQt {

namespace {

constexpr const char* kGroupPrefix = "/api/sync/groups/";

QString cs(const std::string& s) {
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

std::string qs(const QString& s) {
    return s.toUtf8().toStdString();
}

// 严格 base64（坏字符/坏填充即拒——中转侧异形应答不做宽容解码）
bool strict_b64(const QString& s, QByteArray* out) {
    if (s.isEmpty()) {
        out->clear();
        return true;
    }
    const auto res = QByteArray::fromBase64Encoding(
        s.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    if (res.decodingStatus != QByteArray::Base64DecodingStatus::Ok) return false;
    *out = res.decoded;
    return true;
}

}  // namespace

SyncHttpTransportQt::SyncHttpTransportQt(const QString& baseUrl, int timeoutMs,
                                         QObject* parent)
    : QObject(parent), timeoutMs_(timeoutMs) {
    base_ = baseUrl;
    while (base_.endsWith('/')) base_.chop(1);
    nam_ = new QNetworkAccessManager(this);
}

SyncHttpTransportQt::~SyncHttpTransportQt() = default;

bool SyncHttpTransportQt::ensureGroup(const QString& gid, QString* err) {
    const HttpResult r =
        request("PUT", QStringLiteral("%1%2").arg(kGroupPrefix, gid), {});
    if (!r.ok) {
        if (err) *err = r.err;
        return false;
    }
    if (r.status != 200) {
        std::string se;
        httpError(r, &se);  // 透出 {"error":code} 机器码
        if (err) *err = QStringLiteral("建组失败：%1")
                            .arg(QString::fromStdString(se));
        return false;
    }
    return true;
}

SyncHttpTransportQt::HttpResult SyncHttpTransportQt::request(
    const QByteArray& method, const QString& path, const QByteArray& body) {
    HttpResult out;
    QNetworkRequest req{QUrl(base_ + path)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = nullptr;
    if (method == "GET") {
        reply = nam_->get(req);
    } else if (method == "POST") {
        reply = nam_->post(req, body);
    } else if (method == "PUT") {
        reply = nam_->put(req, body);
    } else {
        out.err = QStringLiteral("unsupported method");
        return out;
    }

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs_);
    loop.exec();

    if (!reply->isFinished()) {  // 超时：中断并报错
        reply->abort();
        out.err = QStringLiteral("同步请求超时（%1 ms）").arg(timeoutMs_);
        reply->deleteLater();
        return out;
    }
    const QVariant status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (reply->error() != QNetworkReply::NoError && status.isNull()) {
        // 传输层错误（连接拒绝/DNS 等）；带状态码的 HTTP 错误另路上报
        out.err = reply->errorString();
        reply->deleteLater();
        return out;
    }
    out.ok = true;
    out.status = status.toInt();
    out.body = reply->readAll();
    reply->deleteLater();
    return out;
}

// 非法应答统一出口：HTTP 状态 + {"error":code} 机器码（解析不出就带
// 状态码，防异形应答吞现场）
bool SyncHttpTransportQt::httpError(const HttpResult& r, std::string* err) {
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &parse);
    QString code;
    if (parse.error == QJsonParseError::NoError && doc.isObject()) {
        code = doc.object().value(QLatin1String("error")).toString();
    }
    const QString text =
        code.isEmpty()
            ? QStringLiteral("HTTP %1").arg(r.status)
            : QStringLiteral("HTTP %1 %2").arg(r.status).arg(code);
    if (err) *err = qs(text);
    return false;
}

bool SyncHttpTransportQt::meta(const std::string& gid, GroupMetaStd* out,
                               std::string* err) {
    const QString egid = cs(gid);
    const HttpResult r =
        request("GET", QStringLiteral("%1%2/meta").arg(kGroupPrefix, egid), {});
    if (!r.ok) {
        if (err) *err = qs(r.err);
        return false;
    }
    if (r.status != 200) return httpError(r, err);
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "meta: malformed JSON";
        return false;
    }
    const QJsonObject obj = doc.object();
    out->latest_seq = static_cast<std::uint64_t>(
        obj.value(QLatin1String("latest_seq")).toInteger());
    out->snapshot_up_to_seq = static_cast<std::uint64_t>(
        obj.value(QLatin1String("snapshot_up_to_seq")).toInteger());
    return true;
}

bool SyncHttpTransportQt::push_ops(const std::string& gid,
                                   const std::vector<EnqueuedOpStd>& ops,
                                   std::vector<std::string>* acked,
                                   std::string* err) {
    if (ops.empty()) return true;
    QJsonObject body;
    QJsonArray arr;
    for (const EnqueuedOpStd& op : ops) {
        QJsonObject o;
        const QString op_id = cs(op.op_id);
        o.insert(QLatin1String("op_id"), op_id);
        // device_id = op_id 前缀（PROTOCOL §1 幂等键形态）
        const qsizetype colon = op_id.indexOf(':');
        o.insert(QLatin1String("device_id"),
                 colon > 0 ? op_id.left(colon) : QString());
        o.insert(QLatin1String("payload"),
                 QString::fromLatin1(
                     QByteArray(op.payload.data(),
                                static_cast<qsizetype>(op.payload.size()))
                         .toBase64()));
        arr.append(o);
    }
    body.insert(QLatin1String("ops"), arr);
    const QString egid = cs(gid);
    HttpResult r =
        request("POST", QStringLiteral("%1%2/ops").arg(kGroupPrefix, egid),
                QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (!r.ok) {
        if (err) *err = qs(r.err);
        return false;
    }
    if (r.status != 200) return httpError(r, err);
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "push: malformed JSON";
        return false;
    }
    const QJsonObject obj = doc.object();
    // acked = assigned ∪ duplicate_op_ids（对引擎同义：服务端已有）
    const QJsonArray assigned = obj.value(QLatin1String("assigned")).toArray();
    for (const QJsonValue& v : assigned) {
        acked->push_back(
            qs(v.toObject().value(QLatin1String("op_id")).toString()));
    }
    const QJsonArray dups =
        obj.value(QLatin1String("duplicate_op_ids")).toArray();
    for (const QJsonValue& v : dups) acked->push_back(qs(v.toString()));
    return true;
}

bool SyncHttpTransportQt::pull_ops(const std::string& gid, std::uint64_t since,
                                   std::size_t limit,
                                   std::vector<RemoteOpStd>* out,
                                   std::uint64_t* cursor, bool* has_more,
                                   std::string* err) {
    const QString egid = cs(gid);
    const QString path = QStringLiteral("%1%2/ops?since=%3&limit=%4")
                             .arg(kGroupPrefix, egid)
                             .arg(static_cast<qulonglong>(since))
                             .arg(static_cast<qulonglong>(limit));
    const HttpResult r = request("GET", path, {});
    if (!r.ok) {
        if (err) *err = qs(r.err);
        return false;
    }
    if (r.status != 200) return httpError(r, err);
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "pull: malformed JSON";
        return false;
    }
    const QJsonObject obj = doc.object();
    out->clear();
    const QJsonArray arr = obj.value(QLatin1String("ops")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        RemoteOpStd op;
        op.seq =
            static_cast<std::uint64_t>(o.value(QLatin1String("seq")).toInteger());
        op.op_id = qs(o.value(QLatin1String("op_id")).toString());
        op.device_id = qs(o.value(QLatin1String("device_id")).toString());
        op.ts = o.value(QLatin1String("ts")).toInteger();
        QByteArray bin;
        if (!strict_b64(o.value(QLatin1String("payload")).toString(), &bin)) {
            if (err) *err = "pull: malformed base64 payload";
            out->clear();
            return false;
        }
        op.payload.assign(bin.constData(), static_cast<std::size_t>(bin.size()));
        out->push_back(op);
    }
    *cursor =
        static_cast<std::uint64_t>(obj.value(QLatin1String("cursor")).toInteger());
    *has_more = obj.value(QLatin1String("has_more")).toBool();
    return true;
}

bool SyncHttpTransportQt::put_snapshot(const std::string& gid,
                                       std::uint64_t up_to_seq,
                                       const std::string& payload,
                                       std::string* err) {
    QJsonObject body;
    body.insert(QLatin1String("up_to_seq"), static_cast<qint64>(up_to_seq));
    body.insert(QLatin1String("payload"),
                QString::fromLatin1(
                    QByteArray(payload.data(),
                               static_cast<qsizetype>(payload.size()))
                        .toBase64()));
    const QString egid = cs(gid);
    HttpResult r =
        request("PUT", QStringLiteral("%1%2/snapshot").arg(kGroupPrefix, egid),
                QJsonDocument(body).toJson(QJsonDocument::Compact));
    if (!r.ok) {
        if (err) *err = qs(r.err);
        return false;
    }
    if (r.status != 200) return httpError(r, err);
    return true;
}

bool SyncHttpTransportQt::get_snapshot(const std::string& gid,
                                       std::uint64_t* up_to_seq,
                                       std::string* payload, std::string* err) {
    const QString egid = cs(gid);
    const HttpResult r = request(
        "GET", QStringLiteral("%1%2/snapshot").arg(kGroupPrefix, egid), {});
    if (!r.ok) {
        if (err) *err = qs(r.err);
        return false;
    }
    if (r.status != 200) return httpError(r, err);
    QJsonParseError parse{};
    const QJsonDocument doc = QJsonDocument::fromJson(r.body, &parse);
    if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) *err = "snapshot: malformed JSON";
        return false;
    }
    const QJsonObject obj = doc.object();
    *up_to_seq = static_cast<std::uint64_t>(
        obj.value(QLatin1String("up_to_seq")).toInteger());
    QByteArray bin;
    if (!strict_b64(obj.value(QLatin1String("payload")).toString(), &bin)) {
        if (err) *err = "snapshot: malformed base64 payload";
        return false;
    }
    payload->assign(bin.constData(), static_cast<std::size_t>(bin.size()));
    return true;
}

}  // namespace UnidictAdaptersQt
