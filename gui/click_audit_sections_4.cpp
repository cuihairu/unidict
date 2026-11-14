// 离屏 GUI 真点审计场景（LAY-010 自 gui/click_audit_test.cpp 拆出，第 4/4 批）：
// 场景清单与调用顺序见 click_audit_sections.h，共享面见 click_audit_helpers.h。

#define UNIDICT_GUI_AUDIT_NO_MAIN
#include "main.cpp"

#include "click_audit_helpers.h"
#include "click_audit_sections.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QWidget>
#include <QtTest/QtTest>

namespace gui_audit {

void t15_toolbarToggles(AuditCtx& c) {
    // 剪贴板取词：真点翻转 + QSettings 记忆，再点还原
    QToolButton* clip = actionButton(c.win, QStringLiteral("剪贴板取词"));
    audit(clip != nullptr, QStringLiteral("工具栏剪贴板取词按钮就位"),
          clip ? QStringLiteral("located")
               : QStringLiteral("not found"));
    if (clip) {
        const bool was = QSettings()
                             .value(QStringLiteral("ui/clipboardLookup"))
                             .toBool();
        QTest::mouseClick(clip, Qt::LeftButton);
        flush(80);
        const bool after =
            QSettings()
                .value(QStringLiteral("ui/clipboardLookup"))
                .toBool();
        audit(after != was,
              QStringLiteral("剪贴板取词开关真点翻转+记忆"),
              QStringLiteral("%1→%2").arg(was ? 1 : 0).arg(after ? 1 : 0));
        QTest::mouseClick(clip, Qt::LeftButton);
        flush(80);
        audit(QSettings()
                  .value(QStringLiteral("ui/clipboardLookup"))
                  .toBool() == was,
              QStringLiteral("剪贴板取词开关还原"),
              QStringLiteral("回 %1").arg(was ? 1 : 0));
    }
}

void t16_themeCycle(AuditCtx& c) {
    QToolButton* theme = actionButton(c.win, QStringLiteral("主题: 跟随系统"));
    if (!theme) {
        // 当前态可能是浅色/深色——按 text 前缀找
        for (QToolButton* b : c.win->findChildren<QToolButton*>()) {
            if (b->text().startsWith(QStringLiteral("主题: "))) {
                theme = b;
            }
        }
    }
    audit(theme != nullptr,
          QStringLiteral("主题循环按钮就位"),
          theme ? theme->text() : QStringLiteral("not found"));
    if (theme) {
        const QString before = theme->text();
        QTest::mouseClick(theme, Qt::LeftButton);
        flush(80);
        const QString after = theme->text();
        audit(before != after,
              QStringLiteral("主题真点切换"),
              QStringLiteral("%1 → %2").arg(before).arg(after));
        // 循环到原态（三态循环最多再点 2 次）
        for (int i = 0; i < 2 && theme->text() != before; ++i) {
            QTest::mouseClick(theme, Qt::LeftButton);
            flush(60);
        }
        audit(theme->text() == before,
              QStringLiteral("主题循环还原"),
              QStringLiteral("回 %1").arg(before));
    }
}

void t17_fontDialog(AuditCtx& c) {
    QToolButton* font = actionButton(c.win, QStringLiteral("释义字体…"));
    audit(font != nullptr, QStringLiteral("释义字体按钮就位"),
          font ? QStringLiteral("located") : QStringLiteral("not found"));
    if (font) {
        modalSeen = false;
        onModal(120, [](QWidget* w) { w->close(); });  // 字体对话框取消路径
        QTest::mouseClick(font, Qt::LeftButton);
        flush(400);
        audit(modalSeen,
              QStringLiteral("释义字体真点弹字体对话框（取消路径）"),
              QStringLiteral("modalSeen=%1").arg(modalSeen ? 1 : 0));
    }
}

void t18_noteDialog(AuditCtx& c) {
    // 先保证有当前词（查询成功）
    if (c.input()->text().trimmed().isEmpty()) {
        c.input()->setText(QStringLiteral("hello"));
        QTest::keyClick(c.input(), Qt::Key_Return);
        flush(100);
    }
    QToolButton* note = actionButton(c.win, QStringLiteral("笔记"));
    audit(note != nullptr && note->isEnabled(),
          QStringLiteral("笔记按钮就位且可用"),
          note ? QStringLiteral("enabled=%1").arg(note->isEnabled() ? 1 : 0)
               : QStringLiteral("not found"));
    if (!note || !note->isEnabled()) {
        return;
    }
    modalSeen = false;
    onModal(100, [](QWidget* w) { inputAccept(w, QStringLiteral("audit note")); });
    QTest::mouseClick(note, Qt::LeftButton);
    flush(500);
    const QString noteText =
        UnidictAdaptersQt::DataStoreQt::instance().getNote(c.input()->text().trimmed());
    audit(modalSeen && noteText == QStringLiteral("audit note"),
          QStringLiteral("笔记真点代填保存落库"),
          QStringLiteral("modalSeen=%1 note=%2")
              .arg(modalSeen ? 1 : 0)
              .arg(noteText));
}

void t19_pronPanel(AuditCtx& c) {
    QToolButton* pron = actionButton(c.win, QStringLiteral("发音练习"));
    audit(pron != nullptr, QStringLiteral("发音练习按钮就位"),
          pron ? QStringLiteral("located") : QStringLiteral("not found"));
    if (!pron) {
        return;
    }
    modalSeen = false;
    onModal(150, [](QWidget* w) { w->close(); });
    QTest::mouseClick(pron, Qt::LeftButton);
    flush(600);
    audit(modalSeen,
          QStringLiteral("发音练习真点弹面板（离屏即关）"),
          QStringLiteral("modalSeen=%1").arg(modalSeen ? 1 : 0));
}

void t20_dictManagerDialog(AuditCtx& c) {
    QPushButton* manage = c.win->findChild<QPushButton*>("manageButton");
    audit(manage != nullptr, QStringLiteral("词典管理入口就位"),
          manage ? QStringLiteral("located") : QStringLiteral("not found"));
    if (!manage) {
        return;
    }
    modalSeen = false;
    bool buttonsOk = false;
    bool toggled = false;
    // timer 链：进 dialog exec → 记录按钮面 → 真点「启用/禁用」翻转 →
    // 真点「设置分组标签…」（先排下一级 timer 代填）→ 关 dialog
    QTimer::singleShot(150, [&buttonsOk, &toggled] {
        auto* dialog = QApplication::activeModalWidget();
        if (!dialog) {
            return;
        }
        modalSeen = true;
        const QStringList want = {
            QStringLiteral("添加词典文件…"), QStringLiteral("添加词典目录…"),
            QStringLiteral("启用/禁用"), QStringLiteral("上移"),
            QStringLiteral("下移"), QStringLiteral("移除"),
            QStringLiteral("设置分组标签…"), QStringLiteral("重试加载")};
        int found = 0;
        for (QPushButton* b : dialog->findChildren<QPushButton*>()) {
            if (want.contains(b->text())) {
                ++found;
            }
        }
        buttonsOk = found == 8;

        auto* listBox = dialog->findChild<QListWidget*>();
        if (listBox && listBox->count() > 0) {
            listBox->setCurrentRow(0);
            auto* mgr = &dictMgr();
            const QString id = listBox->item(0)->data(Qt::UserRole).toString();
            bool enabledBefore = false;
            for (const auto& meta : mgr->dictionaries_meta()) {
                if (QString::fromStdString(meta.name) == id) {
                    enabledBefore = meta.enabled;
                }
            }
            QPushButton* toggle = nullptr;
            QPushButton* tagsBtn = nullptr;
            for (QPushButton* b : dialog->findChildren<QPushButton*>()) {
                if (b->text() == QStringLiteral("启用/禁用")) {
                    toggle = b;
                }
                if (b->text() == QStringLiteral("设置分组标签…")) {
                    tagsBtn = b;
                }
            }
            qInfo().noquote() << "[diag] mgr list count=" << listBox->count()
                              << "currentRow=" << listBox->currentRow()
                              << "toggleEnabled="
                              << (toggle ? toggle->isEnabled() : -1)
                              << "toggleVisible="
                              << (toggle ? toggle->isVisible() : -1);
            if (toggle) {
                QTest::mouseClick(toggle, Qt::LeftButton);
                flush(80);
                bool enabledAfter = enabledBefore;
                for (const auto& meta : mgr->dictionaries_meta()) {
                    if (QString::fromStdString(meta.name) == id) {
                        enabledAfter = meta.enabled;
                    }
                }
                toggled = enabledAfter != enabledBefore;
                // 还原走 manager 直调：第二次按钮点击依赖的 currentItem 已被
                // refreshList 重建清掉（第一轮还原点击因此失效，enabled=false
                // 残留 state 污染下一轮）——被测行为是第一次翻转，卫生还原不占真点
                mgr->set_dictionary_enabled(id.toStdString(), enabledBefore);
            }
            listBox->setCurrentRow(0);  // toggle 还原触发 refreshList，重选行
            if (tagsBtn) {
                // 先排 QInputDialog 代填 timer，再点按钮（exec 内执行）
                QTimer::singleShot(120, [] {
                    if (QWidget* dlg = QApplication::activeModalWidget()) {
                        inputAccept(dlg, QStringLiteral("en"));
                    }
                });
                QTest::mouseClick(tagsBtn, Qt::LeftButton);
                flush(300);
            }
        }
        if (dialog) {
            dialog->close();
        }
    });
    QTest::mouseClick(manage, Qt::LeftButton);
    flush(900);

    QComboBox* group = c.win->findChild<QComboBox*>("groupBox");
    audit(modalSeen && buttonsOk,
          QStringLiteral("词典管理对话框真点打开，八按钮齐全"),
          QStringLiteral("modalSeen=%1 buttons=%2")
              .arg(modalSeen ? 1 : 0)
              .arg(buttonsOk ? 8 : -1));
    audit(toggled,
          QStringLiteral("词典管理「启用/禁用」真点翻转"),
          QStringLiteral("toggled=%1").arg(toggled ? 1 : 0));
    audit(group && group->count() >= 2,
          QStringLiteral("分组标签真点设置 → 分组下拉聚合"),
          QStringLiteral("groupBox items=%1")
              .arg(group ? group->count() : -1));
}

void t21_addFileDialog(AuditCtx& c) {
    QPushButton* manage = c.win->findChild<QPushButton*>("manageButton");
    if (!manage) {
        return;
    }
    modalSeen = false;
    bool fileDialogSeen = false;
    // 链：dialog exec → 真点「添加词典文件…」→ QFileDialog exec 里关掉
    QTimer::singleShot(150, [&fileDialogSeen] {
        auto* dialog = QApplication::activeModalWidget();
        if (!dialog) {
            return;
        }
        modalSeen = true;
        bool clickedAdd = false;
        for (QPushButton* b : dialog->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("添加词典文件…")) {
                QTimer::singleShot(150, [&fileDialogSeen] {
                    if (QWidget* fd = QApplication::activeModalWidget()) {
                        fileDialogSeen = true;
                        qInfo().noquote() << "[diag] inner modal=" << fd->metaObject()->className();
                        fd->close();  // 文件对话框取消路径
                    } else {
                        qInfo().noquote() << "[diag] inner modal=null (no active modal)";
                    }
                });
                QTest::mouseClick(b, Qt::LeftButton);
                flush(400);
                clickedAdd = true;
                qInfo().noquote() << "[diag] addFile clicked, activeModal now="
                                  << (QApplication::activeModalWidget()
                                          ? QApplication::activeModalWidget()->metaObject()->className()
                                          : QString("<null>"));
                break;
            }
        }
        if (!clickedAdd) {
            qInfo().noquote() << "[diag] addFile button NOT found in dialog";
        }
        if (dialog) {
            dialog->close();
        }
    });
    QTest::mouseClick(manage, Qt::LeftButton);
    flush(900);
    audit(modalSeen && fileDialogSeen,
          QStringLiteral("「添加词典文件…」真点弹文件对话框（取消路径）"),
          QStringLiteral("modalSeen=%1 fileDialog=%2")
              .arg(modalSeen ? 1 : 0)
              .arg(fileDialogSeen ? 1 : 0));
}

void t22_hotkeyPlatform(AuditCtx& c) {
    QToolButton* hotkey = actionButton(c.win, QStringLiteral("全局热键"));
    audit(hotkey != nullptr,
          QStringLiteral("全局热键按钮就位"),
          hotkey ? QStringLiteral("located") : QStringLiteral("not found"));
    if (hotkey) {
        // 无 X11 的 Linux 会话（Wayland）等不支持平台置灰是有意设计（tooltip 说明原因）；
        // Windows 上可点但注册失败自回弹。两种状态都算接线正确
        const bool supported = GlobalHotkeys::isPlatformSupported();
        audit(supported || (!hotkey->isEnabled() &&
                            hotkey->toolTip().contains(
                                QStringLiteral("暂不支持"))),
              QStringLiteral("全局热键平台态正确"),
              QStringLiteral("supported=%1 enabled=%2")
                  .arg(supported ? 1 : 0)
                  .arg(hotkey->isEnabled() ? 1 : 0));
    }
}

} // namespace gui_audit
