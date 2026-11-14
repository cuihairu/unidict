// BUG-010 gui 端真点审计（与 qmlui/dev/ui_click_audit.cpp 同口径）：
// 可点元素逐个发真实 QMouseEvent（QTest::mouseClick 走 widget 事件路径，
// 非直接调 slot），断言点击前后可观测状态差。模态入口（输入框/字体/
// 文件对话框/发音面板/词典管理）用提前排的 QTimer 在其 exec 事件循环里
// 关闭或代填——直接调用会同步阻塞测试线程。
//
// include main.cpp 复用 MainWindow（同编译单元匿名命名空间可见），
// main() 被 UNIDICT_GUI_AUDIT_NO_MAIN 守卫挡掉，入口由 QtTest 提供。

#define UNIDICT_GUI_AUDIT_NO_MAIN
#include "main.cpp"

#include <QtTest/QtTest>

#include <QApplication>
#include <QDialogButtonBox>
#include <QTabBar>
#include <QToolButton>

#include "click_audit_helpers.h"
#include "click_audit_sections.h"

using namespace gui_audit;

class GuiClickAudit : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void t02_lookupByReturn();
    void t03_completerSelect();
    void t04_contentTabsSweep();
    void t05_exampleAnchorJump();
    void t06_sideTabsSwitch();
    void t07_historyDblClick();
    void t08_historyPinMenu();
    void t09_historyRemoveMenu();
    void t10_starButton();
    void t11_vocabDblClick();
    void t12_vocabTagDialog();
    void t13_vocabGroupFilter();
    void t14_vocabRemoveMenu();
    void t15_toolbarToggles();
    void t16_themeCycle();
    void t17_fontDialog();
    void t18_noteDialog();
    void t19_pronPanel();
    void t20_dictManagerDialog();
    void t21_addFileDialog();
    void t22_hotkeyPlatform();
    void cleanupTestCase();

private:
    MainWindow* win_ = nullptr;
    QString scratch_;
    QString noteWord_;

    QLineEdit* input() { return win_->findChild<QLineEdit*>("searchInput"); }
    QLabel* status() { return win_->findChild<QLabel*>("statusLabel"); }
    QTabWidget* contentTabs() {
        return win_->findChild<QTabWidget*>("contentTabs");
    }
    QTabWidget* sideTabs() { return win_->findChild<QTabWidget*>("sideTabs"); }
    QTextBrowser* view(const char* name) {
        return win_->findChild<QTextBrowser*>(QLatin1String(name));
    }
    QListWidget* list(const char* name) {
        return win_->findChild<QListWidget*>(QLatin1String(name));
    }
};

void GuiClickAudit::initTestCase() {
    // 存储/设置全部钉 scratch（test mode 的 AppData + ini 重定向），
    // 不碰真实 ~/.config 与 data/unidict.json
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("unidict-audit"));
    QCoreApplication::setApplicationName(QStringLiteral("gui-click-audit"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    scratch_ = QDir::tempPath() + QStringLiteral("/unidict_gui_click_audit");
    QDir(scratch_).removeRecursively();  // 幂等：上轮的 tags/主题/几何记忆不清会污染本轮
    QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .removeRecursively();  // 同理：manager state.json 里 enabled=false/tags 残留
    QDir().mkpath(scratch_);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, scratch_);
    UnidictAdaptersQt::DataStoreQt::instance().setStoragePath(
        scratch_ + QStringLiteral("/data.json"));

    loadDefaultDictionaryLocations();
    win_ = new MainWindow(*static_cast<QApplication*>(QCoreApplication::instance()));
    win_->show();
    flush(150);

    audit(win_->findChild<QPushButton*>("starButton") != nullptr &&
              !win_->findChild<QPushButton*>("starButton")->isEnabled(),
          QStringLiteral("启动初始态：收藏按钮禁用（未查询）"),
          QStringLiteral("starButton enabled=0"));
    audit(input() != nullptr &&
              input()->placeholderText().contains(QStringLiteral("输入")),
          QStringLiteral("启动初始态：搜索框就位"),
          QStringLiteral("placeholder=%1").arg(input()->placeholderText()));
    audit(win_->findChild<QPushButton*>("manageButton") != nullptr,
          QStringLiteral("启动初始态：词典管理入口存在"),
          QStringLiteral("manageButton located"));
}

void GuiClickAudit::t02_lookupByReturn() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t02_lookupByReturn(c);
}

void GuiClickAudit::t03_completerSelect() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t03_completerSelect(c);
}

void GuiClickAudit::t04_contentTabsSweep() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t04_contentTabsSweep(c);
}

void GuiClickAudit::t05_exampleAnchorJump() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t05_exampleAnchorJump(c);
}

void GuiClickAudit::t06_sideTabsSwitch() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t06_sideTabsSwitch(c);
}

void GuiClickAudit::t07_historyDblClick() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t07_historyDblClick(c);
}

void GuiClickAudit::t08_historyPinMenu() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t08_historyPinMenu(c);
}

void GuiClickAudit::t09_historyRemoveMenu() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t09_historyRemoveMenu(c);
}

void GuiClickAudit::t10_starButton() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t10_starButton(c);
}

void GuiClickAudit::t11_vocabDblClick() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t11_vocabDblClick(c);
}

void GuiClickAudit::t12_vocabTagDialog() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t12_vocabTagDialog(c);
}

void GuiClickAudit::t13_vocabGroupFilter() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t13_vocabGroupFilter(c);
}

void GuiClickAudit::t14_vocabRemoveMenu() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t14_vocabRemoveMenu(c);
}

void GuiClickAudit::t15_toolbarToggles() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t15_toolbarToggles(c);
}

void GuiClickAudit::t16_themeCycle() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t16_themeCycle(c);
}

void GuiClickAudit::t17_fontDialog() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t17_fontDialog(c);
}

void GuiClickAudit::t18_noteDialog() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t18_noteDialog(c);
}

void GuiClickAudit::t19_pronPanel() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t19_pronPanel(c);
}

void GuiClickAudit::t20_dictManagerDialog() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t20_dictManagerDialog(c);
}

void GuiClickAudit::t21_addFileDialog() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t21_addFileDialog(c);
}

void GuiClickAudit::t22_hotkeyPlatform() {
    gui_audit::AuditCtx c{win_, scratch_, noteWord_};
    gui_audit::t22_hotkeyPlatform(c);
}

void GuiClickAudit::cleanupTestCase() {
    // 关窗：离屏无托盘 → closeEvent 直接走退出路径（几何已存 scratch）
    win_->close();
    flush(150);
    audit(!win_->isVisible(),
          QStringLiteral("关窗收口（离屏无托盘直退）"),
          QStringLiteral("visible=%1").arg(win_->isVisible() ? 1 : 0));
    delete win_;

    qInfo().noquote() << QStringLiteral("==== gui click audit: %1 passed, "
                                       "%2 failed ====")
                             .arg(g_pass)
                             .arg(g_fail);
    QCOMPARE(g_fail, 0);
}

QTEST_MAIN(GuiClickAudit)
#include "click_audit_test.moc"