// §7 B5 剩余增量二：PROTOCOL.md v1 的 Qt HTTP 绑定测试。
//
// 进程内迷你 relay：QTcpServer + core SyncRelayStateStd（真契约语义层）
// + relay_http 的 JSON 面——不派生子进程（Windows CI 无 cmd 引号坑），
// 真 HTTP 走真路由。覆盖：建组/元信息/推拉/快照全端点、密封链穿透
// （中转侧只见 base64 密文）、双引擎收敛、失败矩阵（连接拒绝/404 机器
// 码/坏 JSON/坏 base64/超时）、尾斜杠地址归一。
#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>

#include <string>

#include "relay_http.h"
#include "std/sync_crypto_std.h"
#include "std/sync_engine_std.h"
#include "std/sync_relay_state_std.h"
#include "std/sync_sealed_transport_std.h"
#include "sync_http_transport_qt.h"

using namespace UnidictCoreStd;
using namespace UnidictAdaptersQt;

namespace {

const char* kGid = "transportqt00001";  // 16 字符过 §3 校验

// ---- 迷你 relay 路由（与 server/sync_relay/cpp/main.cpp 同路由面） ----

std::string meta_json(const SyncRelayMeta& m) {
    return "{\"gid\":" + UnidictRelay::json_quote(m.gid) +
           ",\"latest_seq\":" + std::to_string(m.latest_seq) +
           ",\"op_count\":" + std::to_string(m.op_count) +
           ",\"snapshot_up_to_seq\":" + std::to_string(m.snapshot_up_to_seq) +
           ",\"created_at\":" + std::to_string(m.created_at) + "}";
}

std::string append_result_json(const SyncRelayAppendResult& r) {
    std::string out = "{\"assigned\":[";
    for (std::size_t i = 0; i < r.assigned.size(); ++i) {
        if (i) out += ",";
        out += "{\"op_id\":" + UnidictRelay::json_quote(r.assigned[i].op_id) +
               ",\"seq\":" + std::to_string(r.assigned[i].seq) + "}";
    }
    out += "],\"duplicate_op_ids\":[";
    for (std::size_t i = 0; i < r.duplicate_op_ids.size(); ++i) {
        if (i) out += ",";
        out += UnidictRelay::json_quote(r.duplicate_op_ids[i]);
    }
    out += "]}";
    return out;
}

std::string pull_json(const SyncRelayPull& p, bool corrupt_payload) {
    std::string out =
        "{\"gid\":" + UnidictRelay::json_quote(p.gid) + ",\"ops\":[";
    for (std::size_t i = 0; i < p.ops.size(); ++i) {
        const SyncRelayOp& op = p.ops[i];
        if (i) out += ",";
        const std::string payload =
            corrupt_payload ? "!!not-base64!!" : op.payload;
        out += "{\"seq\":" + std::to_string(op.seq) +
               ",\"op_id\":" + UnidictRelay::json_quote(op.op_id) +
               ",\"device_id\":" + UnidictRelay::json_quote(op.device_id) +
               ",\"ts\":" + std::to_string(op.ts) +
               ",\"payload\":" + UnidictRelay::json_quote(payload) + "}";
    }
    out += "],\"cursor\":" + std::to_string(p.cursor) +
           ",\"has_more\":" + (p.has_more ? "true" : "false") + "}";
    return out;
}

std::string snapshot_json(const SyncRelaySnapshot& s, bool corrupt_payload) {
    const std::string payload =
        corrupt_payload ? "!!not-base64!!" : s.payload;
    return "{\"up_to_seq\":" + std::to_string(s.up_to_seq) +
           ",\"payload\":" + UnidictRelay::json_quote(payload) + "}";
}

bool parse_i64(const std::string& s, long long* out) {
    if (s.empty()) return false;
    long long v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        if (v > 900000000000000000LL) return false;
        v = v * 10 + (c - '0');
    }
    *out = v;
    return true;
}

struct RouteResult {
    int status = 200;
    std::string body;
};

}  // namespace

// 进程内迷你 relay：一连接一请求（Connection: close），阻塞读写（内部
// 转事件，同线程对端 QNAM 的写不受阻）
class MiniRelay : public QObject {
    Q_OBJECT
public:
    explicit MiniRelay(QObject* parent = nullptr) : QObject(parent) {
        connect(&server_, &QTcpServer::newConnection, this,
                &MiniRelay::onNewConnection);
    }

    bool start() {
        return server_.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const { return server_.serverPort(); }
    void stop() { server_.close(); }

    // 故障注入：坏 JSON 应答 / 损坏 payload 应答（次数制）
    int bad_json_next = 0;
    int corrupt_payload_next = 0;

private:
    void onNewConnection() {
        while (QTcpSocket* s = server_.nextPendingConnection()) {
            handle(s);
            s->deleteLater();
        }
    }

    // 轮询读：转事件循环（QNAM 侧写事件同样被处理），不依赖 readyRead
    // 补发（连接与数据同包到达时 waitForReadyRead 有空等怪癖）
    bool pump(QTcpSocket* s, QByteArray* buf, qsizetype need) {
        for (int i = 0; i < 100 && buf->size() < need; ++i) {
            buf->append(s->readAll());
            if (buf->size() >= need) break;
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        buf->append(s->readAll());
        return buf->size() >= need;
    }

    void handle(QTcpSocket* s) {
        QByteArray req;
        if (!pump(s, &req, 4) || !req.contains("\r\n\r\n")) return;
        const qsizetype hdrEnd = req.indexOf("\r\n\r\n");
        const QByteArray head = req.left(hdrEnd);
        QByteArray body = req.mid(hdrEnd + 4);
        const QList<QByteArray> lines = head.split('\n');
        const QList<QByteArray> first =
            lines.value(0).simplified().split(' ');
        const QByteArray method = first.value(0);
        QString target = QString::fromLatin1(first.value(1));
        QByteArray cl = "0";
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray l = lines.at(i).trimmed();
            if (l.toLower().startsWith("content-length:")) {
                cl = l.mid(l.indexOf(':') + 1).trimmed();
            }
        }
        const int len = cl.toInt();
        QByteArray more;
        if (!pump(s, &more, len - body.size())) return;
        body += more;

        const RouteResult r = route(method, target, body);
        const QByteArray resp =
            "HTTP/1.1 " + QByteArray::number(r.status) + " OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " +
            QByteArray::number(static_cast<qint64>(r.body.size())) +
            "\r\nConnection: close\r\n\r\n" +
            QByteArray(r.body.data(), static_cast<qsizetype>(r.body.size()));
        s->write(resp);
        s->flush();
        s->disconnectFromHost();
    }

    RouteResult route(const QByteArray& method, QString target,
                      const QByteArray& body) {
        RouteResult out;
        const qsizetype qpos = target.indexOf('?');
        const QString path = qpos >= 0 ? target.left(qpos) : target;
        const QString query = qpos >= 0 ? target.mid(qpos + 1) : QString();
        QStringList parts;
        for (const QString& seg : path.split('/'))
            if (!seg.isEmpty()) parts.append(seg);

        try {
            if (method == "PUT" && parts.size() == 4 && parts[0] == "api" &&
                parts[1] == "sync" && parts[2] == "groups") {
                // 四级路径 = gid 本体：PUT 建组（PROTOCOL §2.2 幂等）
                out.body =
                    meta_json(state_.create_group(parts[3].toStdString()));
                return out;
            }
            if (parts.size() == 5 && parts[0] == "api" && parts[1] == "sync" &&
                parts[2] == "groups") {
                const std::string gid = parts[3].toStdString();
                const QString leaf = parts[4];
                if (method == "GET" && leaf == "meta") {
                    // 注入旗标在目标分支内消费（引擎一轮多请求，顶部
                    // 消费会被 meta 等前序请求吞掉）
                    if (bad_json_next > 0 && !--bad_json_next) {
                        out.status = 200;
                        out.body = "not-json{";
                        return out;
                    }
                    out.body = meta_json(state_.group_meta(gid));
                    return out;
                }
                if (method == "POST" && leaf == "ops") {
                    UnidictRelay::JsonValue parsed;
                    const std::string text(body.constData(),
                                            static_cast<std::size_t>(body.size()));
                    if (!UnidictRelay::json_parse(text, &parsed) ||
                        !parsed.is_object()) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_json\"}";
                        return out;
                    }
                    const UnidictRelay::JsonValue* ops =
                        parsed.find("ops");
                    if (!ops || !ops->is_array()) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_op\"}";
                        return out;
                    }
                    std::vector<SyncRelayOpIn> list;
                    for (const UnidictRelay::JsonValue& item : ops->items) {
                        const UnidictRelay::JsonValue* op_id = item.find("op_id");
                        const UnidictRelay::JsonValue* dev =
                            item.find("device_id");
                        const UnidictRelay::JsonValue* payload =
                            item.find("payload");
                        if (!op_id || !dev || !payload || !op_id->is_string() ||
                            !dev->is_string() || !payload->is_string()) {
                            out.status = 400;
                            out.body = "{\"error\":\"invalid_op\"}";
                            return out;
                        }
                        list.push_back({op_id->str, dev->str, payload->str});
                    }
                    out.body = append_result_json(state_.append_ops(gid, list));
                    return out;
                }
                if (method == "GET" && leaf == "ops") {
                    long long since = 0, limit = 200;
                    const auto q = [&query](const char* key) {
                        const int at = query.indexOf(
                            QLatin1String(key) + QLatin1Char('='));
                        if (at < 0) return QString();
                        const int end = query.indexOf('&', at);
                        return query.mid(
                            at + qstrlen(key) + 1,
                            end < 0 ? -1 : end - at - qstrlen(key) - 1);
                    };
                    if (q("since").isEmpty() || !parse_i64(q("since").toStdString(), &since)) {
                        if (!q("since").isEmpty()) {
                            out.status = 400;
                            out.body = "{\"error\":\"invalid_param\"}";
                            return out;
                        }
                    }
                    if (!q("limit").isEmpty() &&
                        !parse_i64(q("limit").toStdString(), &limit)) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_param\"}";
                        return out;
                    }
                    out.body = pull_json(
                        state_.pull_ops(gid, since, limit),
                        corrupt_payload_next > 0 && !--corrupt_payload_next);
                    return out;
                }
                if (method == "PUT" && leaf == "snapshot") {
                    UnidictRelay::JsonValue parsed;
                    const std::string text(body.constData(),
                                           static_cast<std::size_t>(body.size()));
                    if (!UnidictRelay::json_parse(text, &parsed) ||
                        !parsed.is_object()) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_json\"}";
                        return out;
                    }
                    const UnidictRelay::JsonValue* up = parsed.find("up_to_seq");
                    const UnidictRelay::JsonValue* payload =
                        parsed.find("payload");
                    if (!up || !up->is_int()) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_snapshot\"}";
                        return out;
                    }
                    if (!payload || !payload->is_string()) {
                        out.status = 400;
                        out.body = "{\"error\":\"invalid_op\"}";
                        return out;
                    }
                    const std::int64_t placed = state_.put_snapshot(
                        gid, up->integer, payload->str);
                    out.body = "{\"up_to_seq\":" + std::to_string(placed) + "}";
                    return out;
                }
                if (method == "GET" && leaf == "snapshot") {
                    out.body = snapshot_json(
                        state_.get_snapshot(gid),
                        corrupt_payload_next > 0 && !--corrupt_payload_next);
                    return out;
                }
            }
            out.status = 404;
            out.body = "{\"error\":\"not_found\"}";
            return out;
        } catch (const SyncRelayErrorStd& e) {
            out.status = e.status();
            out.body =
                "{\"error\":" + UnidictRelay::json_quote(e.code()) + "}";
            return out;
        } catch (const std::exception&) {
            out.status = 500;
            out.body = "{\"error\":\"internal\"}";
            return out;
        }
    }

    QTcpServer server_;
    SyncRelayStateStd state_;
};

// 超时用：accept 后不回包
class StallServer : public QObject {
    Q_OBJECT
public:
    explicit StallServer(QObject* parent = nullptr) : QObject(parent) {
        connect(&server_, &QTcpServer::newConnection, this,
                [this] {
                    while (QTcpSocket* s = server_.nextPendingConnection())
                        clients_.append(s);
                });
    }
    bool start() { return server_.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server_.serverPort(); }
    void stop() { server_.close(); }

private:
    QTcpServer server_;
    QList<QTcpSocket*> clients_;
};

class TestSyncTransportQt : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QVERIFY(relay_.start());
        QVERIFY(stall_.start());
    }

    void cleanupTestCase() {
        relay_.stop();
        stall_.stop();
    }

    void ensureGroupAndMeta() {
        SyncHttpTransportQt t(url(relay_.port()));
        QString qerr;
        QVERIFY2(t.ensureGroup(kGid, &qerr), qUtf8Printable(qerr));
        // 幂等：重复建组仍成功
        QVERIFY(t.ensureGroup(kGid, &qerr));
        GroupMetaStd meta;
        std::string err;
        QVERIFY2(t.meta(kGid, &meta, &err), err.c_str());
        QCOMPARE(meta.latest_seq, std::uint64_t{0});
        QCOMPARE(meta.snapshot_up_to_seq, std::uint64_t{0});
        // 未建组 → 404 机器码
        QVERIFY(!t.meta("othergroup000011", &meta, &err));
        QVERIFY(QString::fromStdString(err).contains("group_not_found"));
    }

    void trailingSlashNormalized() {
        SyncHttpTransportQt t(url(relay_.port()) + QStringLiteral("///"));
        GroupMetaStd meta;
        std::string err;
        QVERIFY2(t.meta(kGid, &meta, &err), err.c_str());
    }

    void sealedEngineConverge() {
        const QString gid = "sealedconverge01";
        SyncHttpTransportQt t(url(relay_.port()));
        QVERIFY(t.ensureGroup(gid, nullptr));

        SyncKeyRingStd ring;
        ring.import_key(1, std::string(32, '\x11'));
        SyncSealedTransportStd sealed(t, ring);
        SyncEngineStd a("devA"), b("devB");
        QVERIFY(!a.enqueue(SyncOpType::AddEntry, "mathematics").empty());
        QVERIFY(!a.enqueue(SyncOpType::UpdateNote, "mathematics", "笔记").empty());
        std::string err;
        QVERIFY2(a.sync(sealed, gid.toStdString(), &err), err.c_str());

        // 中转侧落库：post 密封 + base64，无明文痕迹
        std::vector<RemoteOpStd> raw;
        uint64_t cur = 0;
        bool more = false;
        QVERIFY2(t.pull_ops(gid.toStdString(), 0, 100, &raw, &cur, &more, &err),
                 err.c_str());
        QCOMPARE(static_cast<int>(raw.size()), 2);
        for (const RemoteOpStd& op : raw) {
            QVERIFY(op.payload.find("mathematics") == std::string::npos);
            QVERIFY(op.payload.size() >= 44);  // 密封开销
        }

        QVERIFY2(b.sync(sealed, gid.toStdString(), &err), err.c_str());
        QCOMPARE(b.state().words, a.state().words);
        QCOMPARE(b.state().notes.at("mathematics"), std::string("笔记"));
    }

    void snapshotJumpAndCipher() {
        const QString gid = "snapshottqt00001";
        SyncHttpTransportQt t(url(relay_.port()));
        QVERIFY(t.ensureGroup(gid, nullptr));
        SyncKeyRingStd ring;
        ring.import_key(1, std::string(32, '\x22'));
        SyncSealedTransportStd sealed(t, ring);
        SyncEngineStd a("devA");
        for (int i = 0; i < 3; ++i)
            QVERIFY(!a.enqueue(SyncOpType::AddEntry, "w" + std::to_string(i))
                         .empty());
        std::string err;
        QVERIFY2(a.sync(sealed, gid.toStdString(), &err), err.c_str());
        QVERIFY2(
            a.maybe_snapshot(sealed, gid.toStdString(), 3, &err), err.c_str());

        // 快照落库是 base64 密文（明文序列化特征不出现）
        uint64_t up = 0;
        std::string snap;
        QVERIFY2(t.get_snapshot(gid.toStdString(), &up, &snap, &err), err.c_str());
        QCOMPARE(up, std::uint64_t{3});
        QVERIFY(snap.find("\"words\"") == std::string::npos);

        // 落后引擎走快照跳变 → 收敛
        SyncEngineStd b("devB");
        QVERIFY2(b.sync(sealed, gid.toStdString(), &err), err.c_str());
        QCOMPARE(b.state().words, a.state().words);
        QCOMPARE(b.cursor(), std::uint64_t{3});
    }

    void failureMatrix() {
        // 连接拒绝
        SyncHttpTransportQt refused(QStringLiteral("http://127.0.0.1:1"), 800);
        GroupMetaStd meta;
        std::string err;
        QVERIFY(!refused.meta(kGid, &meta, &err));
        QVERIFY(!err.empty());

        // 超时（accept 不回包）
        SyncHttpTransportQt slow(url(stall_.port()), 300);
        QVERIFY(!slow.meta(kGid, &meta, &err));
        QVERIFY(QString::fromStdString(err).contains("超时"));

        // 坏 JSON 应答
        SyncHttpTransportQt t(url(relay_.port()));
        QVERIFY(t.ensureGroup(kGid, nullptr));
        relay_.bad_json_next = 1;
        QVERIFY(!t.meta(kGid, &meta, &err));
        QVERIFY(QString::fromStdString(err).contains("malformed JSON"));

        // 坏 base64 payload：A 先正常落一条（回拉干净），注入只击中
        // B 的拉取
        SyncKeyRingStd ring;
        ring.import_key(1, std::string(32, '\x33'));
        SyncSealedTransportStd sealed(t, ring);
        SyncEngineStd a("devA");
        QVERIFY(!a.enqueue(SyncOpType::AddEntry, "seedword").empty());
        std::string pusherr;
        QVERIFY2(a.sync(sealed, kGid, &pusherr), pusherr.c_str());  // 先落一条
        relay_.corrupt_payload_next = 1;
        SyncEngineStd b("devB");
        QVERIFY(!b.sync(sealed, kGid, &err));
        QVERIFY(QString::fromStdString(err).contains("base64"));
    }

private:
    static QString url(quint16 port) {
        return QStringLiteral("http://127.0.0.1:%1").arg(port);
    }
    MiniRelay relay_;
    StallServer stall_;
};

QTEST_GUILESS_MAIN(TestSyncTransportQt)
#include "sync_transport_qt_test.moc"
