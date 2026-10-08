#ifndef UNIDICT_SYNC_MANAGER_QT_H
#define UNIDICT_SYNC_MANAGER_QT_H

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "std/sync_crypto_std.h"
#include "std/sync_engine_std.h"

namespace UnidictAdaptersQt {

// B7 同步管理器（server_plan §7「同步设置页/默认关闭显式开启/导出加密
// 备份」的 Qt 面）：core/std 同步引擎与本机数据的设置页后端。
//
// 红线：enabled 缺省 false——不显式开启时本管理器不做任何组动作；
// 开启时 UI 先展示 scopeText()（同步什么/加密口径/中转只见密文）。
//
// B5 剩余接线（2026-10-08）：三形态传输绑定——selfhost=自建中转地址、
// hosted=官方托管 Worker 地址、lan=局域网直连（增量三接）。syncNow()
// 按形态解析地址 → HTTP 传输 → 密封层（组密钥环驻留本机，中转只见
// 密文）→ 引擎推拉。enable() 时机即建组：本机无钥时生成组密钥并落盘
// （换钥/第二设备入组配对面归 B7 续——配对未接前其他设备enable同组
// 会各自生成独立钥，UI 范围明示里写清）。
//
// 自救口：口令加密备份（core/std/sync_backup_std），覆盖本机生词本
// （词/分组标签/笔记）。恢复为并入语义：缺的补上，已有的不动。
class SyncManagerQt : public QObject {
    Q_OBJECT
    // QML 绑定面：enabled 与 QObject::enabled 撞名，绑定名用 syncEnabled
    Q_PROPERTY(bool syncEnabled READ enabled NOTIFY enabledChanged)
    Q_PROPERTY(QString groupId READ groupId NOTIFY enabledChanged)
    Q_PROPERTY(QString deviceId READ deviceId CONSTANT)
    Q_PROPERTY(QString relayUrl READ relayUrl WRITE setRelayUrl NOTIFY
                   enabledChanged)
    Q_PROPERTY(QString hostedUrl READ hostedUrl WRITE setHostedUrl NOTIFY
                   enabledChanged)
    Q_PROPERTY(QString transportForm READ transportForm WRITE setTransportForm
                   NOTIFY enabledChanged)
public:
    explicit SyncManagerQt(QObject* parent = nullptr);

    // —— 红线开关面 ——
    Q_INVOKABLE bool enabled() const { return enabled_; }
    // 开启：gid 须过 PROTOCOL §3 校验（^[A-Za-z0-9_-]{16,64}$）。通过
    // 则持久化并置位；组密钥按需建组（见类注释）；失败写 lastError
    // 返回 false。
    Q_INVOKABLE bool enable(const QString& gid);
    // 关闭：清开关位（保留组设置与密钥，重开免重填）
    Q_INVOKABLE void disable();
    // 范围明示（开启确认框展示：同步什么/加密口径/中转可见什么）
    Q_INVOKABLE QString scopeText() const;

    // —— 组/形态设置（持久化）——
    Q_INVOKABLE QString groupId() const;
    Q_INVOKABLE QString relayUrl() const;
    Q_INVOKABLE void setRelayUrl(const QString& url);
    Q_INVOKABLE QString hostedUrl() const;  // 官方托管 Worker 部署地址
    Q_INVOKABLE void setHostedUrl(const QString& url);
    Q_INVOKABLE QString transportForm() const;  // selfhost | hosted | lan
    Q_INVOKABLE void setTransportForm(const QString& form);
    // 本机设备标识（引擎生成 128-bit hex，随引擎状态文件持久化）
    Q_INVOKABLE QString deviceId() const;

    // —— 同步动作（B5 三形态绑定）——
    // 按形态解析地址 → 建组（幂等）→ 密封链 → 引擎推拉一轮 → 达阈值
    // 上传快照。任一步失败写 lastError 返回 false（outbox/位点不动，
    // 重试即续传）。lan 形态在直连发现接入前报「未配置」。
    Q_INVOKABLE bool syncNow();
    // 状态行：位点/待发/开关态/最近错误
    Q_INVOKABLE QString syncStatusText() const;
    Q_INVOKABLE bool hasGroupKey() const { return keyring_.has_current(); }

    // —— 自救口：口令加密备份（防设备全丢）——
    // 导出：生词本（词/分组标签/笔记）全量密封到 path。
    Q_INVOKABLE bool exportBackup(const QString& path, const QString& passphrase);
    // 恢复：并入语义（缺的补上/已有的不动）。返回
    // { ok, error?, words, notes, tags } 计数。
    Q_INVOKABLE QVariantMap restoreBackup(const QString& path,
                                          const QString& passphrase);

    Q_INVOKABLE QString lastError() const { return lastError_; }

signals:
    void enabledChanged();
    void syncStateChanged();
    void backupRestored();

private:
    static QString qs(const std::string& s);
    static std::string cs(const QString& s);
    void loadEngineState();
    void loadKeyring();
    bool saveKeyring();

    UnidictCoreStd::SyncEngineStd engine_;  // 本机设备标识与其断点续传态的宿主
    UnidictCoreStd::SyncKeyRingStd keyring_;  // 组密钥环（本机落盘）
    bool enabled_ = false;  // 红线：缺省关闭
    QString lastError_;
};

}  // namespace UnidictAdaptersQt

#endif  // UNIDICT_SYNC_MANAGER_QT_H
