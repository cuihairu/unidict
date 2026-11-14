// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 6/6 批）：
// 场景清单与调用顺序见 click_audit_sections.h，共享面见 click_audit_helpers.h。

#include "click_audit_sections.h"
#include "click_audit_helpers.h"
#include "../theme.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVariant>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

using namespace ui_audit;
using UnidictQml::Theme;

void audit_s23(AuditCtx& c) {
// ---- S23 UI 批二：生词本表格/卡片双视图 + 词典元信息面板 + 笔记荧光笔多色 ----
{
    // a) 双视图往返：卡片（默认）→ 表格 → 卡片
    clickItem(c.win, item(c.win, "sideTab2"));
    settle(c.win);
    clickItem(c.win, item(c.win, "vocabViewTableButton"));
    settle(c.win);
    {
        QList<QQuickItem*> rows, cards;
        visualFindAll(c.win->contentItem(), "vocabTableRow", rows);
        visualFindAll(c.win->contentItem(), "vocabCard", cards);
        audit(rows.size() >= 1 && cards.empty(),
              "生词本「表格」视图切换",
              QString("rows=%1 cards=%2").arg(rows.size()).arg(cards.size()));
    }
    // 表格行点击查询（点中谁断言谁，同 historyItem 先例）
    if (auto* row = item(c.win, "vocabTableRow")) {
        const QString rowWord = row->property("text").toString();
        clickItem(c.win, row);
        audit(c.win->property("currentWord").toString() == rowWord,
              "表格行点击查询",
              QString("currentWord=%1, 期望=%2")
                  .arg(c.win->property("currentWord").toString(), rowWord));
    } else {
        audit(false, "表格行点击查询", "vocabTableRow 委托不存在");
    }
    clickItem(c.win, item(c.win, "vocabViewCardButton"));
    settle(c.win);
    {
        QList<QQuickItem*> rows, cards;
        visualFindAll(c.win->contentItem(), "vocabTableRow", rows);
        visualFindAll(c.win->contentItem(), "vocabCard", cards);
        audit(cards.size() >= 1 && rows.empty(),
              "生词本「卡片」视图切回",
              QString("rows=%1 cards=%2").arg(rows.size()).arg(cards.size()));
    }

    // b) 笔记荧光笔多色：对话框选色保存 → core 色位 → 卡片荧光回读。
    // 文本显式置串（首卡可能无笔记，空文本保存=删笔记，色位断言会空转）
    if (auto* noteBtn = item(c.win, "vocabNoteButton")) {
        clickItem(c.win, noteBtn);
        settle(c.win);
        QObject* nd = popupObj(c.win, "noteDialog");
        if (popupVisible(nd)) {
            const QString targetWord =
                nd->property("pendingWord").toString();
            if (QObject* area = nd->findChild<QObject*>("noteDialogArea"))
                area->setProperty("text", QStringLiteral("audit highlight"));
            clickItem(c.win, item(c.win, "noteColorSwatch_3"));
            QMetaObject::invokeMethod(nd, "accept");  // onAccepted 走保存链
            settle(c.win);   // 保存 → reloadVocabulary 重建委托
            audit(c.vocab.getVocabNoteColor(targetWord) == 3,
                  "笔记荧光选色保存（core 色位落库）",
                  QString("color(%1)=%2").arg(targetWord)
                      .arg(c.vocab.getVocabNoteColor(targetWord)));
            QList<QQuickItem*> cards;
            visualFindAll(c.win->contentItem(), "vocabCard", cards);
            QQuickItem* hit = nullptr;
            for (QQuickItem* ci : cards)
                if (ci->property("ownerWord").toString() == targetWord) {
                    hit = ci;
                    break;
                }
            audit(hit && hit->property("ownerNoteColor").toInt() == 3,
                  "卡片荧光色位回读",
                  QString("ownerNoteColor=%1")
                      .arg(hit ? hit->property("ownerNoteColor").toInt()
                               : -1));
        } else {
            audit(false, "笔记荧光选色保存（core 色位落库）",
                  "noteDialog 未弹出");
        }
    } else {
        audit(false, "笔记荧光选色保存（core 色位落库）",
              "vocabNoteButton 不存在");
    }

    // c) 词典元信息面板：抽屉 → 词典 tab → 索引状态行/启用勾选/打开目录
    QObject* drawer2 = popupObj(c.win, "toolsDrawer");
    if (drawer2) drawer2->setProperty("visible", true);
    settle(c.win);
    clickItem(c.win, item(c.win, "toolsTab2"));
    settle(c.win);
    {
        const QQuickItem* idxLabel = item(c.win, "dictIndexStatusLabel");
        const QString idxText = idxLabel
                                    ? idxLabel->property("text").toString()
                                    : QString();
        audit(idxText.contains(QStringLiteral("全文索引")),
              "词典页全文索引状态行渲染", QString("text=%1").arg(idxText));
    }
    // 启用勾选真点：停用 → 元信息 enabled 翻假 → 再点还原（Repeater
    // 随 stamp 重建，两步之间必须重新 item() 取活指针）
    {
        QQuickItem* chk = item(c.win, "dictEnabledCheck_0");
        if (chk && c.adapter.dictionariesMeta().size() > 0) {
            scrollIntoView(c.win, chk);
            clickItem(c.win, chk);
            settle(c.win);
            audit(c.adapter.dictionariesMeta().value(0).toMap()
                          .value("enabled").toBool() == false,
                  "词典卡「启用」勾选点击停用", "meta[0].enabled=false");
            chk = item(c.win, "dictEnabledCheck_0");
            if (chk) {
                clickItem(c.win, chk);
                settle(c.win);
            }
            audit(c.adapter.dictionariesMeta().value(0).toMap()
                          .value("enabled").toBool() == true,
                  "词典卡「启用」勾选再点还原", "meta[0].enabled=true");
        } else {
            audit(false, "词典卡「启用」勾选点击停用",
                  "dictEnabledCheck_0 不存在");
        }
    }
    // 打开目录：真实点击会拉起外部文件管理器（QDesktopServices）——
    // 离屏审计止步接线态（按钮存在且可点），同「导出CSV」信号级先例
    {
        QQuickItem* openBtn = item(c.win, "dictOpenFolderButton_0");
        audit(openBtn != nullptr && openBtn->isEnabled(),
              "词典卡「打开目录」按钮接线（结构级：外部文件管理器不进审计）",
              openBtn ? "dictOpenFolderButton_0 enabled" : "不存在");
    }
    // ---- S26 词典优先级 ↑↓（巡检续批 F2）：normalize-on-move——
    // 与邻位交换后整列 priority = n-1-位置（全零并列起步首移也真实
    // 换位）；P 标签渲染 / 上移交换 / 整列归一 / 下移还原 四查 ----
    {
        const QVariantList metas = c.adapter.dictionariesMeta();
        QQuickItem* upBtn1 = item(c.win, "dictPriorityUp_1");
        QQuickItem* pLabel0 = item(c.win, "dictPriorityLabel_0");
        if (metas.size() >= 2 && upBtn1 && pLabel0) {
            const QString name0 =
                metas.value(0).toMap().value("name").toString();
            const QString name1 =
                metas.value(1).toMap().value("name").toString();
            audit(pLabel0->property("text").toString()
                      == QStringLiteral("P")
                             + metas.value(0).toMap()
                                   .value("priority").toString(),
                  "词典卡 P 标签渲染",
                  QString("label=%1")
                      .arg(pLabel0->property("text").toString()));
            // 上移：第 2 本顶到首位（fixture 两本并列 P0 起步——正是
            // 归一口径要救的并列态）
            scrollIntoView(c.win, upBtn1);
            clickItem(c.win, upBtn1);
            settle(c.win);
            const QVariantList moved = c.adapter.dictionariesMeta();
            audit(moved.value(0).toMap().value("name").toString() == name1,
                  "词典优先级上移交换",
                  QString("meta[0]=%1 (原=%2)")
                      .arg(moved.value(0).toMap().value("name").toString())
                      .arg(name0));
            // 归一：无并列，priority = n-1-位置
            bool normalized = !moved.empty();
            QStringList plist;
            for (int i = 0; i < moved.size(); ++i) {
                const int p = moved.value(i).toMap()
                                  .value("priority").toInt();
                plist << QString::number(p);
                normalized = normalized && p == moved.size() - 1 - i;
            }
            audit(normalized, "词典优先级整列归一（无并列）",
                  QString("n=%1, p=[%2]").arg(moved.size())
                      .arg(plist.join(",")));
            // 还原：首位下移归位（Repeater 随 stamp 重建，重新取指针）
            QQuickItem* downBtn0 = item(c.win, "dictPriorityDown_0");
            if (downBtn0) {
                scrollIntoView(c.win, downBtn0);
                clickItem(c.win, downBtn0);
                settle(c.win);
                audit(c.adapter.dictionariesMeta().value(0).toMap()
                              .value("name").toString() == name0,
                      "词典优先级下移还原", "meta[0] 回原序");
            } else {
                audit(false, "词典优先级下移还原",
                      "dictPriorityDown_0 不存在");
            }
        } else {
            audit(false, "词典优先级上移交换",
                  QString("metas=%1, upBtn=%2, label=%3")
                      .arg(metas.size())
                      .arg(upBtn1 ? 1 : 0)
                      .arg(pLabel0 ? 1 : 0));
        }
    }
    // ---- S31 词典移除（巡检续批 F8）：真点 ✕ → 行数 -1 + 装载清单
    // 不再含该名。会话面移除（env 驱动无状态文件）不可在 run 内还原，
    // 故置于审计尾部，之后仅汇总 ----
    {
        const QVariantList metasTail = c.adapter.dictionariesMeta();
        const int n = metasTail.size();
        const QString victim =
            metasTail.value(n - 1).toMap().value("name").toString();
        const QByteArray rmName =
            QByteArray("dictRemoveButton_") + QByteArray::number(n - 1);
        QQuickItem* rmBtn = item(c.win, rmName.constData());
        if (n >= 1 && rmBtn) {
            scrollIntoView(c.win, rmBtn);
            clickItem(c.win, rmBtn);
            settle(c.win);
            const QVariantList after = c.adapter.dictionariesMeta();
            QStringList remain;
            for (int i = 0; i < after.size(); ++i)
                remain << after.value(i).toMap().value("name").toString();
            const bool rowOk = after.size() == n - 1;
            const bool metaGone = !remain.contains(victim);
            const bool loadedGone =
                !c.adapter.loadedDictionaries().contains(victim);
            const bool statusOk =
                c.win->property("statusText").toString()
                == QStringLiteral("已移除词典: ") + victim;
            audit(rowOk && metaGone && loadedGone && statusOk,
                  "词典移除（✕ 真点：行数-1 + 装载清单摘名 + 状态行）",
                  QString("n %1→%2, victim=%3, remain=[%4], row=%5 "
                          "meta=%6 loaded=%7 status=%8 (%9)")
                      .arg(n).arg(after.size()).arg(victim)
                      .arg(remain.join(","))
                      .arg(rowOk).arg(metaGone).arg(loadedGone)
                      .arg(statusOk)
                      .arg(c.win->property("statusText").toString()));
        } else {
            audit(false, "词典移除（✕ 真点：行数-1 + 装载清单摘名 + 状态行）",
                  QString("metas=%1, rmBtn=%2")
                      .arg(n).arg(rmBtn ? 1 : 0));
        }
    }
    // ---- S35 全文索引重建按钮（候选#4 F12）：先查一个词面/前缀/
    // 模糊全 miss 的词逼出全文兜底层把索引建起来，再真点「重建」→
    // 内存索引丢弃 docs 归零 + 状态行回惰建态 + 按钮反馈。置于尾部
    //（前面 S 步骤的全文态不受影响；索引随后按需重建，无需还原）----
    {
        QQuickItem* sf35 = item(c.win, "searchField");
        QQuickItem* go35 = item(c.win, "searchGoButton");
        if (sf35 && go35 && drawer2) {
            if (drawer2->property("visible").toBool() == false) {
                drawer2->setProperty("visible", true);
                settle(c.win);
            }
            // 全文兜底触发建索引（点搜索钮在抽屉外——抽屉
            // CloseOnPressOutside 会顺势关掉，后面重开再点）
            sf35->setProperty("text", QStringLiteral("zzqqxx"));
            flushEvents();
            clickItem(c.win, go35);
            settle(c.win);
            const int docsBefore =
                c.adapter.fulltextStats().value(QStringLiteral("docs"))
                    .toInt();
            audit(docsBefore > 0,
                  "全文索引按需建触发（全文兜底查询）",
                  QString("docs=%1").arg(docsBefore));
            // 重开抽屉 + 切词典 tab 真点「重建」
            if (!drawer2->property("visible").toBool()) {
                drawer2->setProperty("visible", true);
                settle(c.win);
            }
            clickItem(c.win, item(c.win, "toolsTab2"));
            settle(c.win);
            QQuickItem* rebuildBtn = item(c.win, "dictIndexRebuildButton");
            if (rebuildBtn) {
                scrollIntoView(c.win, rebuildBtn);
                clickItem(c.win, rebuildBtn);
                settle(c.win);
                const int docsAfter =
                    c.adapter.fulltextStats().value(QStringLiteral("docs"))
                        .toInt();
                QQuickItem* idxLabel = item(c.win, "dictIndexStatusLabel");
                const QString label =
                    idxLabel ? idxLabel->property("text").toString()
                             : QString();
                audit(docsAfter == 0
                          && c.win->property("statusText").toString()
                                 == QStringLiteral(
                                        "已重置全文索引（下次全文检索时按"
                                        "需重建）"),
                      "全文索引重建按钮（真点：索引丢弃+状态行）",
                      QString("docs %1→%2, status=%3")
                          .arg(docsBefore)
                          .arg(docsAfter)
                          .arg(c.win->property("statusText").toString()));
                audit(label.startsWith(
                          QStringLiteral("全文索引: 按需建")),
                      "全文索引状态行回惰建态",
                      QString("label=%1").arg(label));
            } else {
                audit(false, "全文索引重建按钮（真点：索引丢弃+状态行）",
                      "dictIndexRebuildButton 不存在");
            }
        } else {
            audit(false, "全文索引按需建触发（全文兜底查询）",
                  "searchField/searchGoButton 不存在");
        }
    }
    if (drawer2) drawer2->setProperty("visible", false);
    settle(c.win);
}

}

void audit_s37(AuditCtx& c) {
// ---- S37 移动端根（Main.qml）同步预览冲突裁决（remoteOnly 选择链）：
// 桌面面走 B 系列 SyncManager；previewDetails 选择链只在移动端根
// （Android 生产面，此前无任何测试加载，qmllint 仅语法闸门）。第二
// 引擎复用同批适配器实例；Vocab 页 StackLayout 无滚动容器，offscreen
// 大窗把折叠下方控件全量送进场景。夹具：本地只收 alpha；同步文件手写
// alpha+beta（beta 远端独有）；sync 上下文属性两引擎同实例。断言链：
// Preview Sync 真点 → Apply 构造 sel（remoteOnly 列表选择函数）→
// 确认摘要 "+ pull remote-only: 1" → accept 触发 applySelection 落库
{
    QQmlApplicationEngine engine2;
    registerAuditContext(engine2, c);
    engine2.load(QUrl("qrc:/Main.qml"));
    if (engine2.rootObjects().isEmpty()) {
        audit(false, "移动端根加载", "qrc:/Main.qml load 失败");
    } else {
        auto* win2 = qobject_cast<QQuickWindow*>(
            engine2.rootObjects().first());
        if (!win2) {
            audit(false, "移动端根加载", "root is not a QQuickWindow");
        } else {
            win2->show();
            settle(win2);
            win2->setProperty("currentPage", 2);
            settle(win2);

            c.vocab.addToVocabulary(QStringLiteral("alpha"),
                                  QStringLiteral("audit sync shared"));
            const QString syncPath =
                QDir(c.outDir).filePath("audit-sync-mobile.json");
            {
                QFile f(syncPath);
                f.open(QIODevice::WriteOnly | QIODevice::Truncate);
                f.write("{\"version\":1,\"history\":[],\"vocab\":["
                        "{\"word\":\"alpha\",\"definition\":\"audit sync "
                        "shared\"},{\"word\":\"beta\",\"definition\":"
                        "\"audit sync remote only\"}]}");
            }
            c.sync.setSyncFile(syncPath);

            QQuickItem* previewButton = item(win2, "syncPreviewButton");
            audit(previewButton != nullptr,
                  "移动端同步预览入口按钮存在",
                  previewButton ? "found" : "syncPreviewButton 不存在");
            // 入口按钮仅 existence 级（Vocab 页 Column 在 offscreen 有
            // anchors 警告、布局不保证点击落点，与回归目标无涉）；预览
            // 弹窗直驱：open() 后内容入树（S3 弹窗内容同律），再取
            // 节点 fill(adapter previewDiff 真算结果) → 真点 Apply
            QObject* previewDlg = popupObj(win2, "previewDialog");
            if (previewDlg) {
                QMetaObject::invokeMethod(previewDlg, "open");
                settle(win2);
            }
            QQuickItem* details = item(win2, "previewDetails");
            if (details && previewDlg) {
                QVariantMap m;
                QMetaObject::invokeMethod(&c.sync, "previewDiff",
                                          Q_RETURN_ARG(QVariantMap, m));
                QMetaObject::invokeMethod(details, "fill",
                                          Q_ARG(QVariant, m));
                settle(win2);
                audit(popupVisible(previewDlg)
                          && m.value("remoteOnly").toStringList()
                                 == QStringList{QStringLiteral("beta")},
                      "同步预览夹具（远端独有 beta）",
                      QString("remoteOnly=[%1]")
                          .arg(m.value("remoteOnly").toStringList().join(
                              QLatin1Char(','))));
                QQuickItem* applyBtn = item(win2, "previewApplyButton");
                if (applyBtn) {
                    scrollIntoView(win2, applyBtn);
                    clickItem(win2, applyBtn);
                    settle(win2);
                    QObject* confirmDlg =
                        popupObj(win2, "previewConfirmDialog");
                    const QString summary =
                        confirmDlg
                            ? confirmDlg->property("summaryText").toString()
                            : QString();
                    audit(confirmDlg && popupVisible(confirmDlg)
                              && summary.contains(
                                  QStringLiteral("remote-only: 1")),
                          "移动端同步预览 Apply 选择链（remoteOnly=1 摘要）",
                          summary.isEmpty() ? "previewConfirmDialog 未打开"
                                            : summary);
                    // 确认执行：applySelection 把远端独有 beta 拉进本地
                    if (confirmDlg && popupVisible(confirmDlg)) {
                        QMetaObject::invokeMethod(confirmDlg, "accept");
                        settle(win2);
                        bool alphaIn = false, betaIn = false;
                        for (const auto& v : c.vocab.vocabulary()) {
                            const QString w = v.toMap()
                                                  .value("word")
                                                  .toString()
                                                  .toLower();
                            if (w == QStringLiteral("alpha"))
                                alphaIn = true;
                            if (w == QStringLiteral("beta")) betaIn = true;
                        }
                        audit(alphaIn && betaIn,
                              "移动端同步预览确认后远端独有词落库",
                              QString("alpha=%1 beta=%2")
                                  .arg(alphaIn)
                                  .arg(betaIn));
                    }
                } else {
                    audit(false, "移动端同步预览 Apply 按钮",
                          "previewApplyButton 不存在");
                }
            } else {
                audit(false, "移动端同步预览弹窗装配",
                      QString("details=%1 dialog=%2")
                          .arg(details != nullptr)
                          .arg(previewDlg != nullptr));
            }
            // 清场：关残留弹窗（断链红态/应用失败时 previewDialog 会
            // 挡后续场景的模态点击）、夹具词、同步文件、路径复位
            if (QObject* pd = popupObj(win2, "previewDialog")) {
                if (popupVisible(pd))
                    QMetaObject::invokeMethod(pd, "close");
                settle(win2);
            }
            c.vocab.removeVocabularyWord(QStringLiteral("alpha"));
            c.vocab.removeVocabularyWord(QStringLiteral("beta"));
            c.sync.setSyncFile(QString());
            QFile::remove(syncPath);
        }
    }
}

}
