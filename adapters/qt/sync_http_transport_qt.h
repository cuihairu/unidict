#ifndef UNIDICT_SYNC_HTTP_TRANSPORT_QT_H
#define UNIDICT_SYNC_HTTP_TRANSPORT_QT_H

#include <QObject>
#include <QString>

#include "std/sync_engine_std.h"

class QNetworkAccessManager;

namespace UnidictAdaptersQt {

// PROTOCOL.md v1 的 Qt HTTP 绑定（server_plan §7 B5）：官方托管（Worker，
// https 走 Qt 平台栈）与自建中转（unidict-relay，明文 http）两形态共用
// 一份实现；局域网直连（B5 增量三）同一路由面复用。payload 一律 base64
// 上线——本层对密封层产物不透明（红线：中转只见密文在密封层执行）。
//
// 同步桥：QNetworkReply + QEventLoop + 超时定时器（引擎是同步驱动的窄
// 接口口径）。gid 走 PROTOCOL §3 白名单字符集，路径拼接不做百分号编码。
class SyncHttpTransportQt : public QObject,
                            public UnidictCoreStd::SyncTransportStd {
    Q_OBJECT
public:
    explicit SyncHttpTransportQt(const QString& baseUrl, int timeoutMs = 10000,
                                 QObject* parent = nullptr);
    ~SyncHttpTransportQt() override;

    // PROTOCOL §2.2 显式建组（幂等）：首同步前调用——空 outbox 设备在
    // 新组上不至于 meta 404（POST ops 会自动建组，但引擎先 meta）
    Q_INVOKABLE bool ensureGroup(const QString& gid, QString* err);

    bool meta(const std::string& gid, UnidictCoreStd::GroupMetaStd* out,
              std::string* err) override;
    bool push_ops(const std::string& gid,
                  const std::vector<UnidictCoreStd::EnqueuedOpStd>& ops,
                  std::vector<std::string>* acked,
                  std::string* err) override;
    bool pull_ops(const std::string& gid, std::uint64_t since, std::size_t limit,
                  std::vector<UnidictCoreStd::RemoteOpStd>* out,
                  std::uint64_t* cursor, bool* has_more,
                  std::string* err) override;
    bool put_snapshot(const std::string& gid, std::uint64_t up_to_seq,
                      const std::string& payload, std::string* err) override;
    bool get_snapshot(const std::string& gid, std::uint64_t* up_to_seq,
                      std::string* payload, std::string* err) override;

private:
    struct HttpResult {
        bool ok = false;        // 收到完整 HTTP 应答（2xx 与否另判）
        int status = 0;
        QByteArray body;
        QString err;            // 网络/超时层错误
    };
    HttpResult request(const QByteArray& method, const QString& path,
                       const QByteArray& body);
    // 非法应答统一出口：HTTP 状态 + {"error":code} 机器码（可解析时）
    bool httpError(const HttpResult& r, std::string* err);

    QNetworkAccessManager* nam_;
    QString base_;
    int timeoutMs_;
};

}  // namespace UnidictAdaptersQt

#endif  // UNIDICT_SYNC_HTTP_TRANSPORT_QT_H
