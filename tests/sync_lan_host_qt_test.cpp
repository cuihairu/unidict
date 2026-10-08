// §7 B5 剩余增量三：局域网直传宿主 SyncLanHostStd（Qt-free，server 侧）
// 的行为面测试。Qt 只做测试工具（QUdpSocket/QTcpSocket 打真端口）：
// 发现 query→reply 往返、HTTP 契约面与 unidict-relay 同路由（探活 +
// 建组推拉）、停机重启、组状态落盘续存、杂包忽略。
#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QHostAddress>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include <string>

#include "lan_host_std.h"
#include "std/sync_crypto_std.h"
#include "std/sync_engine_std.h"
#include "std/sync_sealed_transport_std.h"
#include "sync_http_transport_qt.h"

using namespace UnidictRelay;
using namespace UnidictCoreStd;
using namespace UnidictAdaptersQt;

namespace {

const char* kGid = "lanhostqt0000001";  // 16 字符过 §3 校验

QString make_query() {
    return QStringLiteral("{\"service\":\"unidict-sync-lan\",\"protocol\":1}");
}

// 单播一条发现 query，收 reply（timeoutMs 内无回返回空串）
QString discover(quint16 udp_port, int timeoutMs = 1500) {
    QUdpSocket sock;
    const QByteArray q = make_query().toUtf8();
    if (sock.writeDatagram(q, QHostAddress::LocalHost, udp_port) == -1) {
        return {};
    }
    QByteArray out;
    for (int waited = 0; waited < timeoutMs; waited += 50) {
        if (sock.hasPendingDatagrams()) {
            out.resize(int(sock.pendingDatagramSize()));
            sock.readDatagram(out.data(), out.size());
            break;
        }
        QTest::qWait(50);
    }
    return QString::fromUtf8(out);
}

}  // namespace

class TestSyncLanHostQt : public QObject {
    Q_OBJECT

private slots:
    void discoverRoundTrip() {
        SyncLanHostStd host;
        std::string err;
        QVERIFY2(host.start("127.0.0.1", 0, 0, temp_.path().toStdString(),
                            QStringLiteral("书房台式机").toStdString(), &err),
                 err.c_str());
        QVERIFY(host.running());
        QVERIFY(host.http_port() > 0);
        QVERIFY(host.udp_port() > 0);

        const QString reply = discover(quint16(host.udp_port()));
        QVERIFY2(reply.contains("\"service\":\"unidict-sync-relay\""),
                 qUtf8Printable(reply));
        QVERIFY(reply.contains("\"protocol\":1"));
        QVERIFY(reply.contains("\"device\":\"书房台式机\""));
        QVERIFY(reply.contains(
            QStringLiteral("\"port\":%1").arg(host.http_port())));

        // query 形态不匹配（服务名错/协议错）→ 忽略不回
        QUdpSocket probe;
        const QByteArray bad =
            QStringLiteral("{\"service\":\"other\",\"protocol\":1}").toUtf8();
        QCOMPARE(probe.writeDatagram(bad, QHostAddress::LocalHost,
                                     quint16(host.udp_port())),
                 bad.size());
        QTest::qWait(300);
        QVERIFY(!probe.hasPendingDatagrams());

        host.stop();
        QVERIFY(!host.running());
        host.stop();  // 幂等
    }

    void httpContractFace() {
        SyncLanHostStd host;
        std::string err;
        QVERIFY2(host.start("127.0.0.1", 0, 0, {}, "h2", &err), err.c_str());

        // 探活端点（宿主与 unidict-relay 同路由面）
        QTcpSocket ping;
        ping.connectToHost(QHostAddress::LocalHost, quint16(host.http_port()));
        QVERIFY(ping.waitForConnected(3000));
        ping.write("GET /api/sync/relay/ping HTTP/1.1\r\n"
                   "Host: localhost\r\nConnection: close\r\n\r\n");
        QVERIFY(ping.waitForReadyRead(3000));
        const QByteArray resp = ping.readAll();
        QVERIFY(resp.startsWith("HTTP/1.1 200"));
        QVERIFY(resp.contains("\"service\":\"unidict-sync-relay\""));
        QVERIFY(resp.contains("\"protocol\":1"));

        // 契约面：宿主 = 中转同一份路由（建组→推→拉，密封链直打）
        SyncHttpTransportQt t(
            QStringLiteral("http://127.0.0.1:%1").arg(host.http_port()));
        QVERIFY(t.ensureGroup(kGid, nullptr));
        SyncKeyRingStd ring;
        ring.import_key(1, std::string(32, '\x44'));
        SyncSealedTransportStd sealed(t, ring);
        SyncEngineStd a("devA");
        QVERIFY(!a.enqueue(SyncOpType::AddEntry, "lanword").empty());
        QVERIFY2(a.sync(sealed, kGid, &err), err.c_str());
        SyncEngineStd b("devB");
        QVERIFY2(b.sync(sealed, kGid, &err), err.c_str());
        QCOMPARE(b.state().words,
                 std::vector<std::string>{"lanword"});

        host.stop();
    }

    void dataDirSurvivesRestart() {
        SyncLanHostStd host;
        std::string err;
        const std::string dir = (temp_.path() + "/state").toStdString();
        QVERIFY2(host.start("127.0.0.1", 0, 0, dir, "h3", &err), err.c_str());
        SyncHttpTransportQt t(
            QStringLiteral("http://127.0.0.1:%1").arg(host.http_port()));
        QVERIFY(t.ensureGroup(kGid, nullptr));
        GroupMetaStd meta;
        QVERIFY2(t.meta(kGid, &meta, &err), err.c_str());
        QCOMPARE(meta.latest_seq, std::uint64_t{0});
        host.stop();

        // 重启（新端口）：组状态从落盘恢复
        QVERIFY2(host.start("127.0.0.1", 0, 0, dir, "h3", &err), err.c_str());
        SyncHttpTransportQt t2(
            QStringLiteral("http://127.0.0.1:%1").arg(host.http_port()));
        GroupMetaStd meta2;
        QVERIFY2(t2.meta(kGid, &meta2, &err), err.c_str());
        QCOMPARE(meta2.latest_seq, std::uint64_t{0});  // 组仍在（无指令）
        host.stop();
    }

private:
    QTemporaryDir temp_;
};

QTEST_GUILESS_MAIN(TestSyncLanHostQt)
#include "sync_lan_host_qt_test.moc"
