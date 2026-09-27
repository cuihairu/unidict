#pragma once

// 发音模型资产下载（gui 平台壳，M10）——QNetworkAccessManager 传字节，
// 决策全在 core/std pron_model_source_std（下/续传/校验/落地），这里只
// 做三件壳该做的事：把断点文件写出来、把进度报给面板、把人点了取消/
// 网络断了的善后接上。
//
// 为什么模型下载要进面板而不是让用户自己去 HuggingFace 下：评分的
// 全部价值都挂在这一个 635MB 资产上，"记得去下、放到约定目录、确认
// 没下坏"三步里错一步，用户看到的就只是一句加载失败。
//
// 分层纪律：网络 + 文件 IO + 事件循环，按仓库规矩是平台壳，不进任何
// 测试；可测的部分（下载计划/续传策略/校验/落地）全在 core/std。
#include <QObject>
#include <QString>
#include <QFutureWatcher>
#include <cstddef>
#include <string>
#include <vector>

#include "std/pron_model_source_std.h"

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

// 一个资产"校验 + 落地"的结果（跑在后台线程：635MB 的 SHA-256 要一两
// 秒，冻 UI 不可接受）
struct PronInstallResult {
    UnidictCoreStd::InstallStatus status = UnidictCoreStd::InstallStatus::kIoFailed;
    QString message;
};

// 字节数 → "606 MB" 这样给人看的话：core 只给字节数，格式化成话留在
// 壳里（按钮文案与下载进度共用同一个格式化，别各写一套）
QString pronBytesText(long long bytes);
QString pronModelSizeText();

class ModelDownloader : public QObject {
    Q_OBJECT
public:
    explicit ModelDownloader(QObject* parent = nullptr);
    ~ModelDownloader() override;

    // 取清单里缺的资产（已就位的自动跳过）。目录由 core 的约定/env
    // 决定，调用方不传路径
    void startMissing();
    bool busy() const;

public slots:
    // 用户取消：保留断点文件（下次接着下）
    void cancel();

signals:
    // label 是当前文件名（"正在下 model.onnx"），面板状态栏用
    void progress(qint64 received, qint64 total, const QString& label);
    // ok=false 时 message 是可读原因（面板状态栏直接用）
    void finished(bool ok, const QString& message);

private:
    void fetchCurrent(bool keepRestartFlag = false);
    void verifyAndInstall();
    void onReadyRead();
    void onReplyFinished();
    void onVerified();
    void finishAsset(bool ok, const QString& message);
    void abortAndReport(const QString& message);

    QNetworkAccessManager* net_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QFile* part_ = nullptr;
    QFutureWatcher<PronInstallResult>* installWatcher_ = nullptr;

    std::string dir_;
    std::vector<UnidictCoreStd::ModelAsset> queue_;
    size_t index_ = 0;
    UnidictCoreStd::ModelAsset current_;
    qint64 requestedOffset_ = 0;  // 本次请求带了 Range 偏移（回包判定用）
    qint64 received_ = 0;
    bool restarted_ = false;       // 服务器无视 Range / 坏包时只从头重来一次
    bool cancelled_ = false;
};
