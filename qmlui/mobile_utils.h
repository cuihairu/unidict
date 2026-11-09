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
    class AndroidActivityResultReceiver;  // mobile_utils.cpp 内定义
    // 文件选择回执按实例持有、父属 this（深审 P2）：实例析构时子先亡，
    // 回调 emit m_parent 不可能触悬垂。旧 static 局部绑定首实例且从不
    // delete——首实例死后回调解引用悬垂，换实例还复用死指针
    AndroidActivityResultReceiver *m_resultReceiver = nullptr;
    void handleAndroidActivityResult(int requestCode, int resultCode, const QAndroidJniObject &data);
#endif
};

#endif // MOBILEUTILS_H