#include "model_downloader.h"

#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent/QtConcurrentRun>
#include <QUrl>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

using namespace UnidictCoreStd;

namespace {

// 盘上现状（core 只做判断，不替壳查文件系统）
AssetState assetStateOf(const ModelAsset& asset, const std::string& dir) {
    std::error_code ec;
    AssetState st;
    const fs::path finalPath = fs::path(pron_asset_path(asset, dir));
    st.final_exists = fs::exists(finalPath, ec);
    if (st.final_exists) {
        st.final_size = static_cast<long long>(fs::file_size(finalPath, ec));
    }
    st.part_bytes =
        static_cast<long long>(fs::file_size(fs::path(pron_asset_part_path(asset, dir)), ec));
    return st;
}

QString humanSize(long long bytes) {
    if (bytes >= 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 GB")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
    if (bytes >= 1024LL * 1024) {
        return QStringLiteral("%1 MB")
            .arg(static_cast<double>(bytes) / (1024.0 * 1024.0), 0, 'f', 0);
    }
    if (bytes >= 1024) {
        return QStringLiteral("%1 KB").arg(static_cast<double>(bytes) / 1024.0, 0, 'f', 0);
    }
    return QStringLiteral("%1 字节").arg(bytes);
}

}  // namespace

QString pronBytesText(long long bytes) { return humanSize(bytes); }

QString pronModelSizeText() {
    return humanSize(pron_model_total_bytes());
}

ModelDownloader::ModelDownloader(QObject* parent) : QObject(parent) {
    net_ = new QNetworkAccessManager(this);
    installWatcher_ = new QFutureWatcher<PronInstallResult>(this);
    connect(installWatcher_, &QFutureWatcher<PronInstallResult>::finished, this,
            &ModelDownloader::onVerified);
}

ModelDownloader::~ModelDownloader() = default;

bool ModelDownloader::busy() const {
    return !queue_.empty() || reply_ != nullptr || installWatcher_->isRunning();
}

void ModelDownloader::startMissing() {
    if (busy()) {
        return;
    }
    dir_ = pron_model_dir();
    cancelled_ = false;
    restarted_ = false;
    index_ = 0;
    queue_.clear();
    const std::vector<std::string> missing = missing_pron_assets(dir_);
    for (const ModelAsset& a : pron_model_assets()) {
        if (std::find(missing.begin(), missing.end(), a.key) != missing.end()) {
            queue_.push_back(a);
        }
    }
    if (queue_.empty()) {
        emit finished(true, QStringLiteral("模型资产已齐"));
        return;
    }
    std::error_code ec;
    fs::create_directories(dir_, ec);
    if (ec) {
        const QString msg =
            QStringLiteral("建不了模型目录 %1：%2").arg(QString::fromStdString(dir_),
                                                       QString::fromStdString(ec.message()));
        queue_.clear();
        emit finished(false, msg);
        return;
    }
    fetchCurrent();
}

void ModelDownloader::cancel() {
    if (cancelled_) {
        return;
    }
    cancelled_ = true;
    queue_.clear();
    if (reply_) {
        reply_->abort();  // onReplyFinished 里会看到 cancelled_，只报"已取消"
    }
}

void ModelDownloader::fetchCurrent(bool keepRestartFlag) {
    if (cancelled_) {
        return;
    }
    if (index_ >= queue_.size()) {
        const QString msg =
            QStringLiteral("模型已就位（SHA-256 校验通过），可以评分了");
        queue_.clear();
        emit finished(true, msg);
        return;
    }
    current_ = queue_[index_];
    const FetchDecision d = plan_fetch(current_, dir_, assetStateOf(current_, dir_));
    if (d.plan == FetchPlan::kDoneVerified) {
        finishAsset(true, QString());
        return;
    }
    if (d.plan == FetchPlan::kVerifyPart) {
        // 断点已是完整尺寸：跳过传输，直接校验落地（进度条直接满格）
        received_ = current_.size_bytes;
        verifyAndInstall();
        return;
    }
    requestedOffset_ = (d.plan == FetchPlan::kResume) ? d.resume_from : 0;
    if (!keepRestartFlag) {
        restarted_ = false;
    }
    received_ = requestedOffset_;
    part_ = new QFile(QString::fromStdString(d.part_path), this);
    const QIODevice::OpenMode mode =
        requestedOffset_ > 0 ? (QIODevice::WriteOnly | QIODevice::Append)
                             : (QIODevice::WriteOnly | QIODevice::Truncate);
    if (!part_->open(mode)) {
        const QString msg = QStringLiteral("打不开断点文件 %1：%2")
                                .arg(QString::fromStdString(d.part_path),
                                     part_->errorString());
        part_->deleteLater();
        part_ = nullptr;
        abortAndReport(msg);
        return;
    }
    QNetworkRequest req{QUrl(QString::fromStdString(current_.url))};
    // HF 的 resolve 端点 302 到 CDN：跟随重定向，但不放行降级到 http
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("Range", QByteArrayLiteral("bytes=") +
                                  QByteArray::number(requestedOffset_) + '-');
    reply_ = net_->get(req);
    connect(reply_, &QNetworkReply::readyRead, this, &ModelDownloader::onReadyRead);
    connect(reply_, &QNetworkReply::finished, this, &ModelDownloader::onReplyFinished);
    emit progress(received_, current_.size_bytes, QString::fromStdString(current_.filename));
}

void ModelDownloader::onReadyRead() {
    if (!reply_ || !part_) {
        return;
    }
    const QByteArray buf = reply_->readAll();
    if (buf.isEmpty()) {
        return;
    }
    received_ += buf.size();
    part_->write(buf);
    emit progress(received_, current_.size_bytes, QString::fromStdString(current_.filename));
}

void ModelDownloader::onReplyFinished() {
    const QVariant statusVar =
        reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const long long status = statusVar.isValid() ? statusVar.toLongLong() : 0;
    const QNetworkReply::NetworkError err = reply_->error();
    const QString errText = reply_->errorString();
    const std::string partPath =
        part_ ? part_->fileName().toStdString() : std::string();
    const bool ignoredRange = !range_response_appends(requestedOffset_ > 0, status);
    reply_->deleteLater();
    reply_ = nullptr;
    if (part_) {
        part_->close();
        part_->deleteLater();
        part_ = nullptr;
    }
    if (cancelled_) {
        emit finished(false, QStringLiteral("已取消（断点已保留，重试可续传）"));
        return;
    }
    if (err != QNetworkReply::NoError) {
        if (requestedOffset_ > 0 && !restarted_) {
            // 续传失败：多半是服务器不支持 Range。断点留着只会让下一次
            // 接着接在可能已经错位的字节上，删掉从头来
            std::error_code rm;
            fs::remove(partPath, rm);
            restarted_ = true;
            requestedOffset_ = 0;
            fetchCurrent(true);
            return;
        }
        abortAndReport(QStringLiteral("下载失败：%1（断点已保留，重试可续传）")
                           .arg(errText));
        return;
    }
    if (ignoredRange && !restarted_) {
        // 请求了偏移却拿到 200 全量：全量接在半截后面就是"尺寸对但字节
        // 坏"的坏包，丢弃重来（core 的 range_response_appends 判的就是它）
        std::error_code rm;
        fs::remove(partPath, rm);
        restarted_ = true;
        requestedOffset_ = 0;
        fetchCurrent(true);
        return;
    }
    std::error_code ec;
    const long long got = static_cast<long long>(fs::file_size(partPath, ec));
    if (ec || got != current_.size_bytes) {
        abortAndReport(QStringLiteral("%1 只下到 %2 / %3 字节（断点已保留，"
                                      "重试可续传）")
                           .arg(QString::fromStdString(current_.filename))
                           .arg(got)
                           .arg(current_.size_bytes));
        return;
    }
    verifyAndInstall();
}

void ModelDownloader::verifyAndInstall() {
    // 校验 + 落地放后台线程：635MB 的 SHA-256 要一两秒，冻 UI 不可接受。
    // lambda 只抓值拷贝（资产/目录都是 POD），面板关掉也不会悬垂
    const ModelAsset asset = current_;
    const std::string dir = dir_;
    const QString label = QString::fromStdString(current_.filename);
    emit progress(received_, asset.size_bytes,
                  QStringLiteral("%1（校验中）").arg(label));
    installWatcher_->setFuture(QtConcurrent::run(
        [asset, dir]() -> PronInstallResult {
            PronInstallResult r;
            std::string err;
            r.status = install_part(asset, dir, err);
            r.message = QString::fromStdString(err);
            return r;
        }));
}

void ModelDownloader::onVerified() {
    if (cancelled_) {
        return;
    }
    const PronInstallResult r = installWatcher_->result();
    const QString label = QString::fromStdString(current_.filename);
    if (r.status == InstallStatus::kOk) {
        finishAsset(true, QStringLiteral("%1 已就位并通过 SHA-256 校验").arg(label));
        return;
    }
    if (r.status == InstallStatus::kVerifyFailed && !restarted_) {
        // 坏包：core 已删断点（留着只会让下一次续传接在烂字节上），重下
        restarted_ = true;
        requestedOffset_ = 0;
        fetchCurrent(true);
        return;
    }
    finishAsset(false, QStringLiteral("%1 落地失败：%2").arg(label, r.message));
}

void ModelDownloader::finishAsset(bool ok, const QString& message) {
    if (!ok) {
        queue_.clear();
        emit finished(false, message);
        return;
    }
    ++index_;
    if (index_ < queue_.size()) {
        fetchCurrent();
        return;
    }
    queue_.clear();
    emit finished(true, QStringLiteral("模型已就位（SHA-256 校验通过），可以评分了"));
}

void ModelDownloader::abortAndReport(const QString& message) {
    queue_.clear();
    emit finished(false, message);
}
