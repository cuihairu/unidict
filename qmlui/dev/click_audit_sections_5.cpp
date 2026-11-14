// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 5/6 批）：
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

using namespace ui_audit;
using UnidictQml::Theme;

void audit_s19(AuditCtx& c) {
// ---- S19 词典库 tab（P-6）：目录导入 + 装载列表 + 隔离重试 ----
// 种子：临时目录两本有效 + 一本损坏。路径每次唯一 → 隔离档跨次
// 运行不复用，扫描恒报 3 候选（断言不依赖上次运行残留）。存活到
// main 尾（重试用例仍需 broken.json 在盘）
QTemporaryDir dictSeed(
    QDir::tempPath() + "/unidict_audit_dict_XXXXXX");
if (!dictSeed.isValid()) {
    audit(false, "词典库 tab 审计", "QTemporaryDir 种子创建失败");
} else {
    const QString seedDir = QDir(dictSeed.path()).filePath("seed_dicts");
    QDir().mkpath(seedDir);
    auto writeSeed = [&seedDir](const QString& name,
                                const QString& dictName) {
        QFile f(QDir(seedDir).filePath(name));
        f.open(QIODevice::WriteOnly);
        f.write(QString("{\"name\":\"%1\",\"entries\":[{\"word\":"
                        "\"seed\",\"definition\":\"x\"}]}")
                    .arg(dictName)
                    .toUtf8());
    };
    writeSeed("alpha.json", "Audit Alpha");
    writeSeed("beta.json", "Audit Beta");
    QFile brokenSeed(QDir(seedDir).filePath("broken.json"));
    brokenSeed.open(QIODevice::WriteOnly);
    brokenSeed.write("{ not valid json");

    if (c.drawer) c.drawer->setProperty("visible", true);
    settle(c.win);
    clickItem(c.win, item(c.win, "toolsTab2"));
    settle(c.win);
    audit(item(c.win, "importDirField")
              && item(c.win, "importDirField")->isVisible(),
          "抽屉「词典」tab 切换", "importDirField 可见");

    // 浏览按钮：FolderDialog 离屏不出窗，signal 级同「导出CSV」先例
    QQuickItem* browseButton = item(c.win, "importBrowseButton");
    ClickCounter browseCounter;
    QObject::connect(browseButton, SIGNAL(clicked()), &browseCounter,
                     SLOT(onClicked()));
    clickItem(c.win, browseButton);
    audit(browseCounter.count == 1,
          "「浏览」点击（signal 级：目录对话框离屏不出窗）",
          QString("clicked 计数=%1").arg(browseCounter.count));

    // 导入：真路径（startDirectoryImport 分块步进到 finishImport）。
    // 导入后 dictMetas/dictFailures 变更 → 两个 Repeater 重建委托，
    // 之后每步都要重新 item() 取活指针（旧指针会悬垂 UAF）
    const int metasBefore = c.adapter.dictionariesMeta().size();
    item(c.win, "importDirField")->setProperty("text", seedDir);
    clickItem(c.win, item(c.win, "importRunButton"));
    settle(c.win);   // QTimer 分块步进需真实事件循环时间
    const QQuickItem* statusItem = item(c.win, "importStatusLabel");
    const QString importLabel =
        statusItem ? statusItem->property("text").toString() : QString();
    const int metasAfter = c.adapter.dictionariesMeta().size();
    audit(metasAfter == metasBefore + 2
              && importLabel.contains(QStringLiteral("成功 2"))
              && importLabel.contains(QStringLiteral("失败 1")),
          "「导入」点击跑通目录导入（装载列表+状态行双证）",
          QString("metas %1→%2, label=%3")
              .arg(metasBefore).arg(metasAfter).arg(importLabel));

    // 隔离档渲染 + 重试：broken.json 仍损坏，重试拒绝并确认隔离
    QQuickItem* retryButton = item(c.win, "retryFailedButton");
    audit(retryButton != nullptr, "隔离档列表渲染",
          retryButton ? "retryFailedButton located"
                      : "retryFailedButton 不存在");
    if (retryButton) {
        // 重试按钮位于抽屉滚动区下方（scene y>抽屉高），先滚进视口再点
        scrollIntoView(c.win, retryButton);
        clickItem(c.win, retryButton);   // 重试 → 隔离档重建，retryButton 失效
        settle(c.win);
        const QQuickItem* retryStatus = item(c.win, "importStatusLabel");
        const QString retryLabel = retryStatus
                                       ? retryStatus->property("text").toString()
                                       : QString();
        audit(retryLabel.contains(QStringLiteral("重试仍失败"))
                  && c.adapter.failedDictionaries().size() == 1,
              "「重试」点击仍失败确认隔离",
              QString("label=%1").arg(retryLabel));
    }

    // 同目录重复导入：已装载/隔离去重 → 0 候选状态行。重试前把滚动
    // 区滚到了底部，导入按钮（首行）已出视口——先滚回来再点
    QQuickItem* runButton2 = item(c.win, "importRunButton");
    if (runButton2) scrollIntoView(c.win, runButton2);
    clickItem(c.win, runButton2);
    settle(c.win);
    const QQuickItem* dupStatus = item(c.win, "importStatusLabel");
    const QString dupLabel =
        dupStatus ? dupStatus->property("text").toString() : QString();
    // 空候选：scan 文案「未发现新词典」随即被 finish 文案覆盖，终态
    // 以 importFinished 为准（0/0）——重复导入去重的可观测证据
    audit(dupLabel.contains(QStringLiteral("成功 0"))
              && dupLabel.contains(QStringLiteral("失败 0"))
              && c.adapter.dictionariesMeta().size() == metasAfter,
          "同目录重复导入去重（0 候选）",
          QString("label=%1, metas=%2")
              .arg(dupLabel).arg(c.adapter.dictionariesMeta().size()));

    if (c.drawer) c.drawer->setProperty("visible", false);
    settle(c.win);
}

}

void audit_s20(AuditCtx& c) {
// ---- S20 AI 造句弹层（roadmap AI sentence generation）----
// 审计环境无 UNIDICT_AI_CMD → 走启发式 mock，输出自标 [Mock sentences]
{
    c.win->setProperty("currentWord", QStringLiteral("hello"));
    settle(c.win);
    clickItem(c.win, item(c.win, "aiSentenceAction"));
    settle(c.win);
    QObject* aiPopup = popupObj(c.win, "aiSentencePopup");
    audit(popupVisible(aiPopup), "「✨ 造句」轻入口打开 AI 例句弹层",
          "aiSentencePopup visible");
    if (popupVisible(aiPopup)) {
        QObject* textArea = aiPopup->findChild<QObject*>("aiSentenceText");
        const QString out =
            textArea ? textArea->property("text").toString() : QString();
        audit(out.contains(QStringLiteral("[Mock sentences]"))
                  && out.contains(QStringLiteral("\"hello\"")),
              "AI 例句内容命中（mock 输出注入词头）",
              QString("len=%1 head=%2").arg(out.size())
                  .arg(out.left(40)));
        clickItem(c.win, item(c.win, "aiSentenceCloseButton"));
        settle(c.win);
        audit(!popupVisible(aiPopup), "AI 例句弹层关闭按钮收起",
              "aiSentencePopup hidden after close");
    }
    // 复位查询态，避免污染后续/此前依赖默认词条的断言
    c.win->setProperty("currentWord", QString());
    settle(c.win);
}

}

void audit_s21(AuditCtx& c) {
// ---- S21 划词取词（X11 面）：非 xcb（offscreen）开关整块隐藏 ----
{
    // 开关住在 Drawer（Popup 层不在 contentItem 走查范围）——走
    // popupObj 的 findChild 整树找（S19 同款取法）
    auto* sw = qobject_cast<QQuickItem*>(popupObj(c.win, "selectionSwitch"));
    audit(sw && !sw->isVisible(), "划词开关非 xcb 平台隐藏（诚实门控）",
          QString("selectionSwitch found=%1 visible=%2")
              .arg(sw != nullptr)
              .arg(sw ? sw->isVisible() : false));
}

}

void audit_s21b(AuditCtx& c) {
// ---- S21b F13b macOS 焦点窗选区读取：非 macOS 整块隐藏 + 授权契约 ----
{
    // UI 面：mac 专属开关行在非 macOS 隐藏（诚实门控，同 S21 口径）
    auto* macSw = qobject_cast<QQuickItem*>(popupObj(c.win, "macSelectionSwitch"));
    audit(macSw && !macSw->isVisible(), "macOS 读选区开关非 macOS 隐藏（诚实门控）",
          QString("macSelectionSwitch found=%1 visible=%2")
              .arg(macSw != nullptr)
              .arg(macSw ? macSw->isVisible() : false));
    auto* macRow = qobject_cast<QQuickItem*>(popupObj(c.win, "macPermissionRow"));
    audit(macRow && !macRow->isVisible(), "macOS 授权引导行非 macOS 隐藏（诚实门控）",
          QString("macPermissionRow found=%1 visible=%2")
              .arg(macRow != nullptr)
              .arg(macRow ? macRow->isVisible() : false));

    // 数据面：深链常量全平台可断言（QML 侧据此开系统设置）
    const QString macUrl = c.platform.macAccessibilitySettingsUrl();
    audit(macUrl.startsWith(QStringLiteral("x-apple.systempreferences:"))
              && macUrl.contains(QStringLiteral("Privacy_Accessibility")),
          "macOS 系统设置深链指向辅助功能面板",
          QStringLiteral("url=%1").arg(macUrl));
    // 契约：非 macOS 恒未授权 / 恒不支持（不假装）
#ifndef Q_OS_MACOS
    audit(!c.platform.isMacAccessibilityTrusted(),
          "macOS 授权面非 macOS 恒未授权（契约）", "trusted=false");
    audit(!c.platform.isMacSelectionReaderSupported(),
          "macOS 读选区面非 macOS 恒不支持（契约）", "supported=false");
#endif
}

}

void audit_s22(AuditCtx& c) {
// ---- S22 UI 批一：命中词典 chip / 内容 tab 分区折叠 / 建议分组 /
// 历史命中计数（"help" 双词典精确命中撑 chip 行；S9 的 "hel" 探针
// 依赖「无精确命中→前缀层」，dict2 不收 "hel" 词头）----
{
    // (a) 命中词典 chip 概览：点击只看该词典，再点取消
    // （"help" 在两本 fixture 均为精确词头 → 词典 tab 恰 2 组）
    openWord(c.win, "help");
    clickItem(c.win, item(c.win, "contentTab_dict"));
    flushEvents();
    const QVariantList groups22 = paneProp(c.win, "groups").toList();
    audit(groups22.size() >= 2, "多词典命中（chip 概览前置）",
          QString("groups=%1").arg(groups22.size()));
    if (QQuickItem* chip0 = item(c.win, "dictChip_0")) {
        clickItem(c.win, chip0);
        settle(c.win);
        const QString solo = paneProp(c.win, "soloGid").toString();
        audit(!solo.isEmpty(), "命中词典 chip 点击只看该词典",
              QString("soloGid=%1").arg(solo));
        if (!solo.isEmpty()) {
            // 选中词典卡可见，其余卡整卡收起（组序=卡序）
            int soloIdx = -1, otherIdx = -1;
            for (int i = 0; i < groups22.size(); ++i) {
                const QVariantMap g = groups22[i].toMap();
                const QString gid = g.value("dictionaryId").toString().isEmpty()
                                        ? g.value("dictionary").toString()
                                        : g.value("dictionaryId").toString();
                if (gid == solo && soloIdx < 0) soloIdx = i;
                if (gid != solo && otherIdx < 0) otherIdx = i;
            }
            QQuickItem* soloCard = item(c.win, QString("groupCard_%1").arg(soloIdx)
                                                       .toUtf8().constData());
            QQuickItem* otherCard = otherIdx >= 0
                ? item(c.win, QString("groupCard_%1").arg(otherIdx).toUtf8().constData())
                : nullptr;
            audit(soloCard && soloCard->isVisible()
                      && (!otherCard || !otherCard->isVisible()),
                  "chip 聚焦过滤非选中词典整卡收起",
                  QString("soloCard=%1 visible=%2, otherCard=%3 visible=%4")
                      .arg(soloIdx)
                      .arg(soloCard ? soloCard->isVisible() : false)
                      .arg(otherIdx)
                      .arg(otherCard ? otherCard->isVisible() : false));
        }
        clickItem(c.win, chip0);
        settle(c.win);
        audit(paneProp(c.win, "soloGid").toString().isEmpty(),
              "命中词典 chip 再点取消聚焦", "soloGid cleared");
    } else {
        audit(false, "命中词典 chip 概览存在", "dictChip_0 未定位（检查 fixture 数）");
    }

    // (b) 内容 tab 分区折叠（例句 tab：分区头点折叠/再点展开；
    // 例句数据在 "hello" 的释义 Example: 标记里，(a) 的 "help" 不带）
    openWord(c.win, "hello");
    clickItem(c.win, item(c.win, "contentTab_examples"));
    settle(c.win);
    if (QQuickItem* exGroup = item(c.win, "examplesGroup_0")) {
        clickItem(c.win, exGroup);
        settle(c.win);
        const QVariantMap cc1 = paneProp(c.win, "contentCollapsed").toMap();
        bool anyTrue = false;
        for (auto it = cc1.constBegin(); it != cc1.constEnd(); ++it)
            if (it.value().toBool()) anyTrue = true;
        audit(anyTrue, "例句分区头点击折叠", "contentCollapsed 含 true");
        clickItem(c.win, exGroup);
        settle(c.win);
        const QVariantMap cc2 = paneProp(c.win, "contentCollapsed").toMap();
        bool allFalse = true;
        for (auto it = cc2.constBegin(); it != cc2.constEnd(); ++it)
            if (it.value().toBool()) allFalse = false;
        audit(allFalse && item(c.win, "exampleSpeaker") != nullptr,
              "例句分区头再点展开（卡内容恢复）", "contentCollapsed 全 false 且喇叭仍在");
    } else {
        audit(false, "例句分区折叠卡存在", "examplesGroup_0 未定位（检查 fixture）");
    }

    // (c) 搜索建议分组：help → 精确（"help" 词头）+ 前缀（helpdesk）
    // 两段；模糊模式全部归「词条」段（combo popup 吞点，模式切换走
    // signal 级注入）
    clickItem(c.win, item(c.win, "sideTab0"));
    settle(c.win);
    QQuickItem* searchField22 = item(c.win, "searchField");
    searchField22->setProperty("text", QStringLiteral("help"));
    settle(c.win);
    QList<QQuickItem*> secHeaders;
    visualFindAll(c.win->contentItem(), "suggestSectionHeader", secHeaders);
    QStringList sectionTexts;
    for (QQuickItem* h : secHeaders)
        if (h->isVisible()) sectionTexts << h->property("section").toString();
    audit(sectionTexts.contains(QStringLiteral("精确"))
              && sectionTexts.contains(QStringLiteral("前缀")),
          "搜索建议分组段头（精确/前缀）", sectionTexts.join(QLatin1Char('|')));
    c.win->setProperty("suggestMode", 2);  // 模糊
    QVariant fuzzyQ(QStringLiteral("helo"));
    QMetaObject::invokeMethod(c.win, "reloadSuggestions", Q_ARG(QVariant, fuzzyQ));
    settle(c.win);
    QList<QQuickItem*> fuzzyHeaders;
    visualFindAll(c.win->contentItem(), "suggestSectionHeader", fuzzyHeaders);
    QStringList fuzzyTexts;
    for (QQuickItem* h : fuzzyHeaders)
        if (h->isVisible()) fuzzyTexts << h->property("section").toString();
    audit(fuzzyTexts.contains(QStringLiteral("词条")),
          "模糊建议归词条段", fuzzyTexts.join(QLatin1Char('|')));
    c.win->setProperty("suggestMode", 0);

    // (d) 历史命中计数：同一词重查两次 → 历史行 ×N 胶囊（数据面 +
    // UI 面各钉一次）
    openWord(c.win, "helicopter");
    openWord(c.win, "helicopter");
    clickItem(c.win, item(c.win, "sideTab1"));
    settle(c.win);
    bool dataCountOk = false;
    const QVariantList hEntries = c.vocab.searchHistoryEntries(200);
    for (const QVariant& ev : hEntries) {
        const QVariantMap em = ev.toMap();
        if (em.value("word").toString() == QStringLiteral("helicopter")) {
            dataCountOk = em.value("count").toInt() == 2;
            break;
        }
    }
    audit(dataCountOk, "历史命中计数数据面（重查两次 count=2）",
          QString("entries=%1").arg(hEntries.size()));
    QQuickItem* countTarget = nullptr;
    QList<QQuickItem*> hItems22;
    visualFindAll(c.win->contentItem(), "historyItem", hItems22);
    for (QQuickItem* h : hItems22)
        if (h->property("text").toString() == QStringLiteral("helicopter")) {
            countTarget = h;
            break;
        }
    if (countTarget) {
        QQuickItem* chip = visualFind(countTarget, "historyCountChip");
        QQuickItem* chipText = chip ? visualFind(countTarget, "historyCountLabel")
                                    : nullptr;
        audit(chip && chip->isVisible() && chipText
                  && chipText->property("text").toString()
                         == QStringLiteral("×2"),
              "历史行命中计数胶囊显示",
              QString("chip=%1 text=%2")
                  .arg(chip != nullptr)
                  .arg(chipText ? chipText->property("text").toString()
                                : QString()));
    } else {
        audit(false, "历史行命中计数胶囊显示",
              "historyItem(helicopter) 未定位（列表裁剪？）");
    }

    // 复位查询态（S22 自产状态不外溢）
    c.win->setProperty("suggestMode", 0);
    QMetaObject::invokeMethod(c.win, "resetHome");
    settle(c.win);
}

}
