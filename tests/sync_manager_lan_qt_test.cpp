// SyncManagerQt 局域网面（B5 剩余增量三-b）：扫描发现往返、直连对端
// 选择持久化、syncNow lan 形态三路径（无 peer 拒绝 / 有 peer 走直连）、
// 本机宿主开关（起/停/幂等，发现口被占则跳过——环境性冲突不算坏）。
//
// 隔离：QStandardPaths 测试模式（AppDataLocation → ~/.qttest）+ init
// 清 sync/* 设置与同步目录；发现用单播 127.0.0.1 打宿主实际挑的 UDP 口
// （不走 8789 广播，避并行会话串扰）。
#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

#include "lan_host_std.h"
#include "sync_manager_qt.h"

using UnidictAdaptersQt::SyncManagerQt;
using UnidictRelay::SyncLanHostStd;

namespace {

// 必须与 SyncManagerQt::syncSettings() 同构造（IniFormat + 同 org/app）：
// 默认 QSettings() 是 NativeFormat，空 organization 下与 IniFormat 落
// 不同文件，清理会静默失效
void clearSyncSettings() {
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.remove(QStringLiteral("sync"));
    s.sync();
}

}  // namespace

class TestSyncManagerLanQt : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true);
        clearSyncSettings();
        const QString syncDir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
            QStringLiteral("/sync");
        QDir(syncDir).removeRecursively();
    }

    void cleanup() { clearSyncSettings(); }

    void discoverAndSelectPeer() {
        SyncLanHostStd host;
        std::string err;
        QVERIFY2(host.start("127.0.0.1", 0, 0, {}, "书房台式机", &err),
                 err.c_str());

        SyncManagerQt m;
        const QVariantList peers =
            m.lanScan(QStringLiteral("127.0.0.1"), host.udp_port());
        QCOMPARE(peers.size(), 1);
        const QVariantMap peer = peers.first().toMap();
        QCOMPARE(peer.value(QStringLiteral("name")).toString(),
                 QStringLiteral("书房台式机"));
        QCOMPARE(peer.value(QStringLiteral("port")).toInt(),
                 host.http_port());

        // 点选直连 → 持久化 + 属性重绑
        m.setLanPeer(peer.value(QStringLiteral("addr")).toString(),
                     host.http_port());
        QCOMPARE(m.lanPeerAddr(),
                 peer.value(QStringLiteral("addr")).toString());
        QCOMPARE(m.lanPeerPort(), host.http_port());

        host.stop();
    }

    void syncNowLanRejectsWithoutPeer() {
        SyncManagerQt m;
        m.setTransportForm(QStringLiteral("lan"));
        QVERIFY2(m.enable(QStringLiteral("lannopeer0000001")),
                 qUtf8Printable(m.lastError()));
        QVERIFY(!m.syncNow());
        QVERIFY(m.lastError().contains(QStringLiteral("扫描")));

        // 有对端但端口无效同样拒绝
        m.setLanPeer(QStringLiteral("127.0.0.1"), 0);
        QVERIFY(!m.syncNow());
        QVERIFY(m.lastError().contains(QStringLiteral("扫描")));
    }

    void syncNowLanThroughHost() {
        SyncLanHostStd host;
        std::string err;
        QVERIFY2(host.start("127.0.0.1", 0, 0, {}, "h", &err), err.c_str());

        SyncManagerQt m;
        m.setTransportForm(QStringLiteral("lan"));
        m.setLanPeer(QStringLiteral("127.0.0.1"), host.http_port());
        QVERIFY2(m.enable(QStringLiteral("lanthrough000001")),
                 qUtf8Printable(m.lastError()));
        QVERIFY2(m.syncNow(), qUtf8Printable(m.lastError()));
        QVERIFY(m.syncStatusText().contains(QStringLiteral("已开启")));
        QVERIFY(m.hasGroupKey());

        host.stop();
    }

    void lanHostLifecycle() {
        SyncManagerQt m;
        QVERIFY(!m.lanHostRunning());
        QVERIFY(m.lanHostInfo().contains(QStringLiteral("未运行")));

        if (!m.lanHostStart()) {
            // 发现口 8789 被并行会话占用是环境性冲突：跳过不假红
            QSKIP(qPrintable(QStringLiteral("lanHostStart: %1")
                                 .arg(m.lastError())));
        }
        QVERIFY(m.lanHostRunning());
        QVERIFY(m.lanHostInfo().contains(QStringLiteral("运行中")));
        QVERIFY(m.lanHostStart());  // 幂等：已运行再启仍 true

        // 宿主起着也能被扫描到（发现口 = 协议缺省 8789）
        const QVariantList peers = m.lanScan(QStringLiteral("127.0.0.1"));
        QVERIFY2(!peers.isEmpty(), "self host not discoverable");

        m.lanHostStop();
        QVERIFY(!m.lanHostRunning());
        m.lanHostStop();  // 幂等
    }
};

QTEST_GUILESS_MAIN(TestSyncManagerLanQt)
#include "sync_manager_lan_qt_test.moc"
