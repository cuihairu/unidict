// SyncManagerQt::syncNow 重入闸门（审核批 2026-11-12）。
//
// 现场已定性的 P0：HTTP 传输等待应答时泵嵌套事件循环
// （sync_http_transport_qt request 的 QEventLoop），同线程的第二轮
// syncNow（连点按钮/自动同步定时器借循环提前派发）若放行，会撞进引擎
// 同一把 mu_ 的二次加锁——旧实现 UI 直接冻结。进程内罐头 relay（真
// HTTP 走真路由，应答固定 JSON）+ 零间隔单发定时器把重入插进第一轮的
// 嵌套循环；修后第二轮被 in-flight 闸门当场拒绝。
//
// 隔离：QStandardPaths 测试模式（AppDataLocation → ~/.qttest）+ init
// 清 sync/* 设置与同步目录。
#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "sync_manager_qt.h"

using UnidictAdaptersQt::SyncManagerQt;

namespace {

const char* kGid = "syncnowqt0000001";  // 16 字符过 §3 校验

// 必须与 SyncManagerQt::syncSettings() 同构造（IniFormat + 同 org/app）
void clearSyncSettings() {
    QSettings s(QSettings::IniFormat, QSettings::UserScope,
                QCoreApplication::organizationName(),
                QCoreApplication::applicationName());
    s.remove(QStringLiteral("sync"));
    s.sync();
}

// 罐头 relay：建组 200、meta 全零、ops 空——够 syncNow 空推拉一轮走完
class CannedRelay : public QObject {
    Q_OBJECT
public:
    CannedRelay() {
        connect(&server_, &QTcpServer::newConnection, this,
                &CannedRelay::onNewConnection);
    }
    bool start() { return server_.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server_.serverPort(); }

private:
    void onNewConnection() {
        while (QTcpSocket* s = server_.nextPendingConnection()) {
            connect(s, &QTcpSocket::readyRead, this,
                    [this, s] { tryRespond(s); });
            connect(s, &QTcpSocket::disconnected, s, &QObject::deleteLater);
            connect(s, &QObject::destroyed, this,
                    [this, s] { bufs_.remove(s); });
            tryRespond(s);
        }
    }

    // 未收齐就 return，等下一段 readyRead（顶层事件循环分发）
    void tryRespond(QTcpSocket* s) {
        QByteArray& buf = bufs_[s];
        buf.append(s->readAll());
        const qsizetype hdrEnd = buf.indexOf("\r\n\r\n");
        if (hdrEnd < 0) return;
        const QList<QByteArray> lines = buf.left(hdrEnd).split('\n');
        int len = 0;
        for (int i = 1; i < lines.size(); ++i) {
            const QByteArray l = lines.at(i).trimmed();
            if (l.toLower().startsWith("content-length:"))
                len = l.mid(l.indexOf(':') + 1).trimmed().toInt();
        }
        if (buf.size() < hdrEnd + 4 + len) return;

        const QByteArray first = lines.value(0).simplified();
        QByteArray body = "{}";
        if (first.contains("/meta")) {
            body = "{\"latest_seq\":0,\"snapshot_up_to_seq\":0}";
        } else if (first.contains("/ops")) {
            body = "{\"ops\":[],\"cursor\":0,\"has_more\":false}";
        }
        const QByteArray resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: " +
            QByteArray::number(static_cast<qint64>(body.size())) +
            "\r\nConnection: close\r\n\r\n" + body;
        s->write(resp);
        s->flush();
        s->disconnectFromHost();
        bufs_.remove(s);
    }

    QTcpServer server_;
    QHash<QTcpSocket*, QByteArray> bufs_;
};

class TestSyncManagerSyncNowQt : public QObject {
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

    // 重入复现：零间隔定时器借第一轮的嵌套事件循环派发第二轮 syncNow。
    // 修前：第二轮冲进引擎 mu_ 二次加锁（同线程递归加锁）→ UI 冻结、
    // 测试挂死；修后：in-flight 闸门当场拒绝，第一轮照常收工
    void syncNow_reentryRejectedWhileInFlight() {
        CannedRelay relay;
        QVERIFY(relay.start());

        SyncManagerQt m;
        QVERIFY(m.enable(QLatin1String(kGid)));
        m.setRelayUrl(
            QStringLiteral("http://127.0.0.1:%1").arg(relay.port()));
        m.setTransportForm(QStringLiteral("selfhost"));

        bool second = true;
        QTimer::singleShot(0, [&] { second = m.syncNow(); });
        QElapsedTimer clock;
        clock.start();
        QVERIFY(m.syncNow());
        QVERIFY2(clock.elapsed() < 10000, "first syncNow stalled");
        QVERIFY(!second);
        QVERIFY(m.lastError().contains(QStringLiteral("同步进行中")));
    }
};

}  // namespace

QTEST_GUILESS_MAIN(TestSyncManagerSyncNowQt)
#include "sync_manager_syncnow_qt_test.moc"
