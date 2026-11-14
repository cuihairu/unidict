// 离屏 GUI 真点审计场景面（LAY-010 自 gui/click_audit_test.cpp 拆出）：
// 每个 include 方必须先 #include "main.cpp"——MainWindow 是其匿名命名
// 空间类，仅 TU 内可见。AuditCtx 携带窗口指针与三项实例状态（访问器
// 与 GuiClickAudit 私有助手同式，win_ -> win）；21 个场景函数由主 TU
// 的同名槽包装原序调用（click_audit_sections_{1..4}.cpp 提供）。

#ifndef UNIDICT_GUI_CLICK_AUDIT_SECTIONS_H
#define UNIDICT_GUI_CLICK_AUDIT_SECTIONS_H

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QString>
#include <QTabWidget>
#include <QTextBrowser>

namespace gui_audit {

struct AuditCtx {
    MainWindow* win = nullptr;
    QString scratch;
    QString noteWord;

    QLineEdit* input() { return win->findChild<QLineEdit*>("searchInput"); }
    QLabel* status() { return win->findChild<QLabel*>("statusLabel"); }
    QTabWidget* contentTabs() {
        return win->findChild<QTabWidget*>("contentTabs");
    }
    QTabWidget* sideTabs() { return win->findChild<QTabWidget*>("sideTabs"); }
    QTextBrowser* view(const char* name) {
        return win->findChild<QTextBrowser*>(QLatin1String(name));
    }
    QListWidget* list(const char* name) {
        return win->findChild<QListWidget*>(QLatin1String(name));
    }
};

void t02_lookupByReturn(AuditCtx& c);
void t03_completerSelect(AuditCtx& c);
void t04_contentTabsSweep(AuditCtx& c);
void t05_exampleAnchorJump(AuditCtx& c);
void t06_sideTabsSwitch(AuditCtx& c);
void t07_historyDblClick(AuditCtx& c);
void t08_historyPinMenu(AuditCtx& c);
void t09_historyRemoveMenu(AuditCtx& c);
void t10_starButton(AuditCtx& c);
void t11_vocabDblClick(AuditCtx& c);
void t12_vocabTagDialog(AuditCtx& c);
void t13_vocabGroupFilter(AuditCtx& c);
void t14_vocabRemoveMenu(AuditCtx& c);
void t15_toolbarToggles(AuditCtx& c);
void t16_themeCycle(AuditCtx& c);
void t17_fontDialog(AuditCtx& c);
void t18_noteDialog(AuditCtx& c);
void t19_pronPanel(AuditCtx& c);
void t20_dictManagerDialog(AuditCtx& c);
void t21_addFileDialog(AuditCtx& c);
void t22_hotkeyPlatform(AuditCtx& c);

} // namespace gui_audit

#endif // UNIDICT_GUI_CLICK_AUDIT_SECTIONS_H
