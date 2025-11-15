#include "mobile_utils.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

#ifdef Q_OS_ANDROID
#include <QtAndroid>
#include <QAndroidJniObject>
#include <QAndroidActivityResultReceiver>
#include <QAndroidIntent>

class AndroidActivityResultReceiver : public QAndroidActivityResultReceiver
{
public:
    AndroidActivityResultReceiver(MobileUtils *parent) : m_parent(parent) {}

    void handleActivityResult(int receiverRequestCode, int resultCode, const QAndroidJniObject &data) override
    {
        Q_UNUSED(receiverRequestCode)
        if (resultCode == -1) { // RESULT_OK
            if (data.isValid()) {
                QAndroidJniObject uri = data.callObjectMethod("getData", "()Landroid/net/Uri;");
                if (uri.isValid()) {
                    QString url = uri.toString();
                    emit m_parent->documentSelected(url);
                    return;
                }
            }
        }
        emit m_parent->documentSelectionCancelled();
    }

private:
    MobileUtils *m_parent;
};
#endif

#ifdef Q_OS_IOS
// iOS native code would go here
// For now, we'll use a placeholder
#endif

MobileUtils::MobileUtils(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
    setupAndroidConnections();
#elif defined(Q_OS_IOS)
    setupIOSConnections();
#endif
}

bool MobileUtils::isAndroid() const
{
#ifdef Q_OS_ANDROID
    return true;
#else
    return false;
#endif
}

bool MobileUtils::isIOS() const
{
#ifdef Q_OS_IOS
    return true;
#else
    return false;
#endif
}

bool MobileUtils::isMobile() const
{
    return isAndroid() || isIOS();
}

void MobileUtils::openDocumentPicker(const QString &title,
                                    const QStringList &mimeTypes,
                                    bool selectExisting)
{
#ifdef Q_OS_ANDROID
    Q_UNUSED(title)
    Q_UNUSED(selectExisting)

    QAndroidIntent intent;
    if (selectExisting) {
        intent = QAndroidIntent(QAndroidJniObject::fromString("android.intent.action.OPEN_DOCUMENT"));
    } else {
        intent = QAndroidIntent(QAndroidJniObject::fromString("android.intent.action.CREATE_DOCUMENT"));
    }

    intent.putExtra("android.intent.extra.ALLOW_MULTIPLE", false);

    if (!mimeTypes.isEmpty()) {
        if (mimeTypes.contains("*.csv") || mimeTypes.contains("CSV files (*.csv)")) {
            intent.putExtra("android.intent.extra.MIME_TYPES", QStringList() << "text/csv" << "text/plain");
        } else if (mimeTypes.contains("*.index") || mimeTypes.contains("Index (*.index)")) {
            intent.putExtra("android.intent.extra.MIME_TYPES", QStringList() << "application/octet-stream");
        } else {
            intent.putExtra("android.intent.extra.MIME_TYPES", QStringList() << "*/*");
        }
    }

    static AndroidActivityResultReceiver *receiver = new AndroidActivityResultReceiver(this);
    QtAndroid::startActivity(intent.handle(), 1001, receiver);
#else
    Q_UNUSED(title)
    Q_UNUSED(mimeTypes)
    Q_UNUSED(selectExisting)
    emit documentSelectionCancelled();
#endif
}

void MobileUtils::openIOSDocumentPicker(const QString &title,
                                       const QStringList &extensions,
                                       bool selectExisting)
{
#ifdef Q_OS_IOS
    // iOS document picker implementation would go here
    // This requires Objective-C++ code integration
    Q_UNUSED(title)
    Q_UNUSED(extensions)
    Q_UNUSED(selectExisting)
    qDebug() << "iOS document picker not implemented yet";
    emit documentSelectionCancelled();
#else
    Q_UNUSED(title)
    Q_UNUSED(extensions)
    Q_UNUSED(selectExisting)
    emit documentSelectionCancelled();
#endif
}

QString MobileUtils::getDocumentsPath() const
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QDir dir(path);
    if (!dir.exists("Unidict")) {
        dir.mkpath("Unidict");
    }
    return path + "/Unidict";
}

QString MobileUtils::getCachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
}

bool MobileUtils::hasStoragePermission() const
{
#ifdef Q_OS_ANDROID
    auto result = QtAndroid::checkPermission("android.permission.READ_EXTERNAL_STORAGE");
    return result == QtAndroid::PermissionResult::Granted;
#else
    return true; // iOS handles permissions differently
#endif
}

bool MobileUtils::requestStoragePermission()
{
#ifdef Q_OS_ANDROID
    QStringList permissions;
    permissions << "android.permission.READ_EXTERNAL_STORAGE"
                << "android.permission.WRITE_EXTERNAL_STORAGE";

    for (const QString &permission : permissions) {
        if (QtAndroid::checkPermission(permission) == QtAndroid::PermissionResult::Denied) {
            auto result = QtAndroid::requestPermissions(QStringList() << permission);
            // Note: This is synchronous call, but permission dialog is asynchronous
            return false; // Will be updated via callback
        }
    }
    return true;
#else
    return true;
#endif
}

void MobileUtils::setupAndroidConnections()
{
#ifdef Q_OS_ANDROID
    // Setup Android-specific connections here
    qDebug() << "Android utils initialized";
#endif
}

void MobileUtils::setupIOSConnections()
{
#ifdef Q_OS_IOS
    // Setup iOS-specific connections here
    qDebug() << "iOS utils initialized";
#endif
}