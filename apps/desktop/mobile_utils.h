#ifndef MOBILEUTILS_H
#define MOBILEUTILS_H

#include <QObject>
#include <QString>
#include <QStringList>

class MobileUtils : public QObject
{
    Q_OBJECT

public:
    explicit MobileUtils(QObject *parent = nullptr);

    // 平台检测
    Q_INVOKABLE bool isAndroid() const;
    Q_INVOKABLE bool isIOS() const;
    Q_INVOKABLE bool isMobile() const;

    // Android专用方法
    Q_INVOKABLE void openDocumentPicker(const QString &title,
                                       const QStringList &mimeTypes,
                                       bool selectExisting = true);

    // iOS专用方法
    Q_INVOKABLE void openIOSDocumentPicker(const QString &title,
                                          const QStringList &extensions,
                                          bool selectExisting = true);

    // 通用文件操作
    Q_INVOKABLE QString getDocumentsPath() const;
    Q_INVOKABLE QString getCachePath() const;
    Q_INVOKABLE bool hasStoragePermission() const;
    Q_INVOKABLE bool requestStoragePermission();

signals:
    void documentSelected(const QString &url);
    void documentSelectionCancelled();
    void permissionGranted(const QString &permission);
    void permissionDenied(const QString &permission);

private:
    void setupAndroidConnections();
    void setupIOSConnections();

#ifdef Q_OS_ANDROID
    void handleAndroidActivityResult(int requestCode, int resultCode, const QAndroidJniObject &data);
#endif
};

#endif // MOBILEUTILS_H