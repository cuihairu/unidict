// Qt 层 100% 收口：qmlui/mobile_utils.cpp——移动平台工具的桌面可编译面
//（平台旗标恒假、文档选择器降级为取消信号、路径/权限的桌面语义）。
// Android/iOS 专属实现（JNI/权限对话框/结果回调）被 Q_OS_* 预处理器从
// 桌面构建中移除，两个空 setup 函数只在移动构建可达，源码内以
// GCOVR_EXCL_LINE 标注理由。QStandardPaths 设测试模式隔离沙箱。

#include <QDir>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

#include "mobile_utils.h"

class MobileUtilsTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    // 平台旗标：桌面构建下 Android/iOS/移动 三查恒假
    void qt100_platform_flags();
    // 选择器降级：非移动平台两个 picker 都只发取消信号
    void qt100_picker_degrades();
    // 路径与权限的桌面语义：Documents/Unidict 目录落地、缓存路径透传、
    // 权限桌面端视为已授予
    void qt100_paths_and_permissions();
};

void MobileUtilsTest::initTestCase() {
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("unidict-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("mobile-utils-test"));
    // 测试模式的 Documents 目录跨进程持久：清掉上次运行留下的 Unidict
    // 子目录，让 getDocumentsPath 的"目录缺失 → mkpath"分支每次真实走过
    QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
        .rmdir(QStringLiteral("Unidict"));
}

void MobileUtilsTest::qt100_platform_flags() {
    MobileUtils mu;
    QCOMPARE(mu.isAndroid(), false);
    QCOMPARE(mu.isIOS(), false);
    QCOMPARE(mu.isMobile(), false);
}

void MobileUtilsTest::qt100_picker_degrades() {
    MobileUtils mu;
    QSignalSpy cancelled(&mu, &MobileUtils::documentSelectionCancelled);

    // Android/iOS 专属选择器在桌面构建里只剩非 Q_OS_* 分支：
    // 不弹任何界面，直接发 documentSelectionCancelled
    mu.openDocumentPicker(QStringLiteral("导入词典"), QStringList() << "*.csv",
                          true);
    mu.openDocumentPicker(QStringLiteral("导出"), QStringList(), false);
    mu.openIOSDocumentPicker(QStringLiteral("导入"), QStringList() << "csv",
                             true);
    QCOMPARE(cancelled.count(), 3);
}

void MobileUtilsTest::qt100_paths_and_permissions() {
    MobileUtils mu;

    // 首次调用落地 Unidict 子目录，返回路径以 /Unidict 结尾
    const QString docs = mu.getDocumentsPath();
    QVERIFY(docs.endsWith(QStringLiteral("/Unidict")));
    QVERIFY(QDir(docs).exists());

    // 再次调用走目录已存在分支，结果稳定
    QCOMPARE(mu.getDocumentsPath(), docs);

    // 缓存路径透传 QStandardPaths
    QCOMPARE(mu.getCachePath(),
             QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

    // 权限：桌面构建不做运行时授权，视为已授予
    QCOMPARE(mu.hasStoragePermission(), true);
    QCOMPARE(mu.requestStoragePermission(), true);
}

QTEST_MAIN(MobileUtilsTest)
#include "mobile_utils_test.moc"
