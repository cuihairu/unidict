// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 4/6 批）：
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

void audit_s16(AuditCtx& c) {
// ---- S16 页脚清历史 ----
clickItem(c.win, item(c.win, "clearHistoryButton"));
audit(statusText(c.win) == QStringLiteral("历史已清空")
          && c.vocab.searchHistory(200).isEmpty(),
      "页脚「清空历史」按钮",
      QString("statusText=%1, history=%2")
          .arg(statusText(c.win)).arg(c.vocab.searchHistory(200).size()));

}

void audit_s17(AuditCtx& c) {
// ---- S17 悬浮取词窗全控件 ----
QVariant qw(QStringLiteral("hello"));
QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qw));
settle(c.win);
auto* paneWin = c.win->findChild<QQuickWindow*>("quickLookupPane");
audit(paneWin && paneWin->isVisible(), "取词窗弹出", "quickLookupPane visible");
if (paneWin) {
    auto* paneItem = paneWin->contentItem();
    auto* qlSpeak = visualFind(paneItem, "qlSpeakButton");
    clickItem(paneWin, qlSpeak);
    audit(c.hasTts || statusText(c.win).contains("本地语音不可用"),
          "取词窗朗读按钮（TTS 缺失给明示）",
          QString("statusText=%1").arg(statusText(c.win)));
    clickItem(paneWin, visualFind(paneItem, "qlVocabButton"));
    audit(statusText(c.win).contains("已加入生词本"),
          "取词窗生词本按钮",
          QString("statusText=%1").arg(statusText(c.win)));
    clickItem(paneWin, visualFind(paneItem, "qlCloseButton"));
    audit(!paneWin->isVisible(), "取词窗 ✕ 关闭", "pane hidden");

    QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qw));
    settle(c.win);
    clickItem(paneWin, visualFind(paneWin->contentItem(), "qlOpenMainButton"));
    flushEvents();
    audit(!paneWin->isVisible()
              && c.win->property("currentWord").toString() == QStringLiteral("hello"),
          "取词窗「在主窗打开」收窗并落主窗词条卡",
          QString("pane hidden, currentWord=%1")
              .arg(c.win->property("currentWord").toString()));

    QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qw));
    settle(c.win);
    clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseBottomButton"));
    audit(!paneWin->isVisible(), "取词窗「关闭」按钮", "pane hidden");

    // ---- S24 取词窗信息层级对照（UI 批四：改看行/未收录面/多词典概览） ----
    // 非精确命中："helo" 聚合模糊层有组（fixture 口径）→ 改看行出现
    // 且讲清显示的是谁；点非当前词面就地重查（词面换、窗不收）
    QVariant qhel(QStringLiteral("helo"));
    QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qhel));
    settle(c.win);
    audit(paneWin->property("fuzzyHit").toBool(),
          "取词窗非精确命中识别",
          QString("fuzzyHit=%1, word=%2")
              .arg(paneWin->property("fuzzyHit").toBool() ? 1 : 0)
              .arg(paneWin->property("word").toString()));
    auto* qlAlt0 = visualFind(paneWin->contentItem(), "qlAltWord_0");
    audit(qlAlt0 != nullptr,
          "取词窗改看词面出现",
          QString("altWords=%1")
              .arg(paneWin->property("altWords").toList().size()));
    if (qlAlt0) {
        // 点一个非当前词面的改看词（首条词面=helo 的匹配词，换点下一个）
        QString altWord = qlAlt0->property("text").toString();
        if (altWord.compare(paneWin->property("word").toString(),
                            Qt::CaseInsensitive) == 0) {
            auto* alt1 = visualFind(paneWin->contentItem(), "qlAltWord_1");
            if (alt1) { qlAlt0 = alt1; altWord = alt1->property("text").toString(); }
        }
        clickItem(paneWin, qlAlt0);
        settle(c.win);
        audit(paneWin->isVisible()
                  && paneWin->property("word").toString().compare(
                         altWord, Qt::CaseInsensitive) == 0,
              "取词窗改看词点击就地重查",
              QString("word=%1, 期望=%2")
                  .arg(paneWin->property("word").toString()).arg(altWord));
        clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseButton"));
    }

    // 完全未收录面： gibberish 聚合全空 → 未收录标签出现（建议层
    // 同空，给主窗兜底指引）
    QVariant qmiss(QStringLiteral("zzzqqqxxx"));
    QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qmiss));
    settle(c.win);
    auto* qlNotFound = visualFind(paneWin->contentItem(), "qlNotFoundLabel");
    audit(paneWin->property("notFound").toBool() && qlNotFound != nullptr
              && qlNotFound->property("visible").toBool(),
          "取词窗完全未收录面",
          QString("notFound=%1")
              .arg(paneWin->property("notFound").toBool() ? 1 : 0));

    // 多词典概览："help" 双 fixture 精确命中 → 来源行带「等 N 词典」
    QVariant qhelp(QStringLiteral("help"));
    QMetaObject::invokeMethod(c.win, "showQuickLookupFor", Q_ARG(QVariant, qhelp));
    settle(c.win);
    auto* qlDictLabel = visualFind(paneWin->contentItem(), "qlDictLabel");
    audit(qlDictLabel
              && qlDictLabel->property("text").toString().contains("词典"),
          "取词窗多词典命中概览",
          QString("dictLabel=%1")
              .arg(qlDictLabel ? qlDictLabel->property("text").toString()
                               : QString()));
    if (paneWin->isVisible())
        clickItem(paneWin, visualFind(paneWin->contentItem(), "qlCloseButton"));
}

}

void audit_s28(AuditCtx& c) {
// ---- S28 外观三态（F4）：锁定深色 → Theme 翻暗 + 记忆落盘；
// 锁定态不随系统切换；回「跟随系统」还原（审计不留持久模式） ----
{
    if (c.drawer) c.drawer->setProperty("visible", true);
    settle(c.win);
    clickItem(c.win, item(c.win, "toolsTab5"));
    settle(c.win);
    QQuickItem* radio2 = item(c.win, "themeModeRadio2");
    if (radio2) {
        scrollIntoView(c.win, radio2);
        clickItem(c.win, radio2);
        settle(c.win);
        audit(Theme::instance().mode() == 2 && Theme::instance().dark()
                  && c.settings.getInt("ui/theme", -1) == 2,
              "外观锁定深色（翻暗+记忆落盘）",
              QString("mode=%1, dark=%2, stored=%3")
                  .arg(Theme::instance().mode())
                  .arg(Theme::instance().dark() ? 1 : 0)
                  .arg(c.settings.getInt("ui/theme", -1)));
        // 锁定态：系统报亮不跟随（真调 applySystemScheme 双证）
        Theme::instance().applySystemScheme(false);
        audit(Theme::instance().dark(),
              "外观锁定态不随系统切换",
              "applySystemScheme(false) 后仍暗");
        // 回跟随系统还原（记忆归零，审计无残留模式）
        QQuickItem* radio0 = item(c.win, "themeModeRadio0");
        if (radio0) {
            scrollIntoView(c.win, radio0);
            clickItem(c.win, radio0);
            settle(c.win);
        }
        audit(Theme::instance().mode() == 0
                  && c.settings.getInt("ui/theme", -1) == 0,
              "外观回跟随系统（记忆归零）",
              QString("mode=%1, stored=%2")
                  .arg(Theme::instance().mode())
                  .arg(c.settings.getInt("ui/theme", -1)));
    } else {
        audit(false, "外观锁定深色（翻暗+记忆落盘）",
              "themeModeRadio2 不存在");
    }
}

}

void audit_s30(AuditCtx& c) {
// ---- S30 释义字号（F7）：radio 真点 → defFontSize 翻面 + 落盘 +
// 结果卡正文字号实变（definitionBodyText font.pixelSize 双证）；
// 回设还原（审计不留持久字号）----
{
    // 收抽屉做一次真查询，保证结果卡在树（definitionBodyText 可寻址）
    if (c.drawer) c.drawer->setProperty("visible", false);
    settle(c.win);
    if (QQuickItem* sf = item(c.win, "searchField")) {
        sf->setProperty("text", QStringLiteral("hello"));
        flushEvents();
        clickItem(c.win, item(c.win, "searchGoButton"));
        settle(c.win);
    }
    if (c.drawer) c.drawer->setProperty("visible", true);
    settle(c.win);
    clickItem(c.win, item(c.win, "toolsTab5"));
    settle(c.win);
    const int sizeBefore = c.win->property("defFontSize").toInt();
    const int target = (sizeBefore == 18) ? 12 : 18;   // 必与当前异档
    QQuickItem* radio =
        item(c.win, target == 18 ? "defFontRadio18" : "defFontRadio12");
    if (radio) {
        scrollIntoView(c.win, radio);
        clickItem(c.win, radio);
        settle(c.win);
        const int sizeAfter = c.win->property("defFontSize").toInt();
        int rendered = 0;
        if (QQuickItem* body =
                visualFind(c.win->contentItem(), "definitionBodyText"))
            rendered = body->property("font").value<QFont>().pixelSize();
        audit(sizeAfter == target && rendered == target
                  && c.settings.getInt("ui/def_font_size", -1) == target,
              "释义字号档位翻面（属性+落盘+实渲染）",
              QString("def=%1, rendered=%2, stored=%3")
                  .arg(sizeAfter)
                  .arg(rendered)
                  .arg(c.settings.getInt("ui/def_font_size", -1)));
        // 回设原档（原档非 12/14/16/18 的脏值兜 14；与当前同档则已还原）
        const int restore =
            (sizeBefore == 12 || sizeBefore == 14 || sizeBefore == 16
             || sizeBefore == 18)
                ? sizeBefore
                : 14;
        if (restore != target) {
            const QString backName =
                "defFontRadio" + QString::number(restore);
            QQuickItem* back = item(c.win, backName.toUtf8().constData());
            if (back) {
                scrollIntoView(c.win, back);
                clickItem(c.win, back);
                settle(c.win);
            }
        }
        audit(c.win->property("defFontSize").toInt() == restore
                  && c.settings.getInt("ui/def_font_size", -1) == restore,
              "释义字号回设还原",
              QString("def=%1, stored=%2")
                  .arg(c.win->property("defFontSize").toInt())
                  .arg(c.settings.getInt("ui/def_font_size", -1)));
    } else {
        audit(false, "释义字号档位翻面（属性+落盘+实渲染）",
              QString("defFontRadio%1 不存在").arg(target));
    }
}

}

void audit_s38(AuditCtx& c) {
// ---- S38 查询键归一化（P1 text-norm）：radio 真点 → core 档位翻面
//（adapter.normProfile）+ QML 属性翻面 + 落盘三证；回宽容还原
//（审计不留持久档位）----
{
    if (c.drawer) c.drawer->setProperty("visible", true);
    settle(c.win);
    clickItem(c.win, item(c.win, "toolsTab5"));
    settle(c.win);
    QQuickItem* tolerant = item(c.win, "textNormTolerant");
    QQuickItem* exact = item(c.win, "textNormExact");
    QQuickItem* conservative = item(c.win, "textNormConservative");
    if (tolerant && exact && conservative) {
        scrollIntoView(c.win, tolerant);
        audit(c.win->property("normProfileValue").toInt() == 0
                  && tolerant->property("checked").toBool()
                  && c.adapter.normProfile() == 0,
              "键归一化默认宽容档（core/QML/checked 三证）",
              QString("win=%1, checked=%2, core=%3")
                  .arg(c.win->property("normProfileValue").toInt())
                  .arg(tolerant->property("checked").toBool() ? 1 : 0)
                  .arg(c.adapter.normProfile()));
        scrollIntoView(c.win, exact);
        clickItem(c.win, exact);
        settle(c.win);
        audit(c.adapter.normProfile() == 2
                  && c.win->property("normProfileValue").toInt() == 2
                  && c.settings.getInt("textnorm/profile", -1) == 2,
              "键归一化切精确档（core 翻面+属性+落盘）",
              QString("core=%1, win=%2, stored=%3")
                  .arg(c.adapter.normProfile())
                  .arg(c.win->property("normProfileValue").toInt())
                  .arg(c.settings.getInt("textnorm/profile", -1)));
        // 回宽容还原（审计不留持久档位）
        scrollIntoView(c.win, tolerant);
        clickItem(c.win, tolerant);
        settle(c.win);
        audit(c.adapter.normProfile() == 0
                  && c.settings.getInt("textnorm/profile", -1) == 0,
              "键归一化回宽容还原",
              QString("core=%1, stored=%2")
                  .arg(c.adapter.normProfile())
                  .arg(c.settings.getInt("textnorm/profile", -1)));
    } else {
        audit(false, "键归一化默认宽容档（core/QML/checked 三证）",
              "textNorm* radio 不存在");
    }
}

}

void audit_s18(AuditCtx& c) {
// ---- S18 同步备份 tab：红线开关面 + 自救口 ----
if (c.drawer) c.drawer->setProperty("visible", true);
settle(c.win);
clickItem(c.win, item(c.win, "toolsTab4"));
settle(c.win);

// 红线：同步默认关闭
QQuickItem* syncSwitch = item(c.win, "syncSwitch");
audit(syncSwitch && !syncSwitch->property("checked").toBool(),
      "同步开关默认关闭（红线）",
      syncSwitch ? "checked=false（缺省态）" : "syncSwitch 不存在");

// 点开关想开：被挡回（须走「确认开启」——显式开启明示范围）
if (syncSwitch) clickItem(c.win, syncSwitch);
settle(c.win);
audit(syncSwitch && !syncSwitch->property("checked").toBool(),
      "开关联动挡回（开启须经确认）", "点开不直通，范围明示常驻");

// 范围明示常驻可见
QQuickItem* scopeLabel = item(c.win, "syncScopeLabel");
audit(scopeLabel && scopeLabel->property("visible").toBool(),
      "同步范围明示常驻", scopeLabel ? "scopeText 可见" : "syncScopeLabel 不存在");

// 确认开启：非法 gid 拒绝 + 合法 gid 生效
QQuickItem* applyButton = item(c.win, "syncApplyButton");
QQuickItem* syncStateLabel = item(c.win, "syncStateLabel");
if (applyButton) {
    QQuickItem* gidField = item(c.win, "syncGidField");
    if (gidField)
        gidField->setProperty("text", QStringLiteral("short"));
    scrollIntoView(c.win, applyButton);
    clickItem(c.win, applyButton);
    settle(c.win);
    audit(syncSwitch && !syncSwitch->property("checked").toBool()
              && syncStateLabel
              && syncStateLabel->property("text").toString().contains(
                  QStringLiteral("不合法")),
          "非法组 ID 拒绝开启", "gid 形态校验拦下 short");

    if (gidField)
        gidField->setProperty("text",
                              QStringLiteral("audit-group-0123456789ab"));
    scrollIntoView(c.win, applyButton);
    clickItem(c.win, applyButton);
    settle(c.win);
    audit(syncSwitch && syncSwitch->property("checked").toBool(),
          "合法组 ID 显式开启", "确认开启后开关置位");

    // 关闭路径：开关拨回（apply 滚动后开关在视口上方，先滚回来再点）
    scrollIntoView(c.win, syncSwitch);
    if (syncSwitch) clickItem(c.win, syncSwitch);
    settle(c.win);
    audit(syncSwitch && !syncSwitch->property("checked").toBool(),
          "开关关闭即停用", "disable 后 checked=false");
} else {
    audit(false, "同步确认按钮", "syncApplyButton 不存在");
}

// 自救口：导出 → 错口令拒 → 对口令恢复（断言读抽屉内 syncState 标签）
QQuickItem* syncExportButton = item(c.win, "backupExportButton");
QQuickItem* syncRestoreButton = item(c.win, "backupRestoreButton");
QQuickItem* passField = item(c.win, "backupPassField");
QQuickItem* pathField = item(c.win, "backupPathField");
if (syncExportButton && syncRestoreButton && passField && pathField) {
    passField->setProperty("text", QStringLiteral("audit-pass-123"));
    pathField->setProperty(
        "text", c.outDir + QStringLiteral("/audit-backup.udbk"));
    scrollIntoView(c.win, syncExportButton);
    clickItem(c.win, syncExportButton);
    settle(c.win);
    QFile backupFile(c.outDir + QStringLiteral("/audit-backup.udbk"));
    audit(backupFile.exists() && backupFile.size() > 64
              && syncStateLabel
              && syncStateLabel->property("text").toString().contains(
                  QStringLiteral("备份已导出")),
          "备份导出落盘",
          QString("size=%1").arg(backupFile.size()));

    passField->setProperty("text", QStringLiteral("wrong-pass"));
    scrollIntoView(c.win, syncRestoreButton);
    clickItem(c.win, syncRestoreButton);
    settle(c.win);
    audit(syncStateLabel
              && syncStateLabel->property("text").toString().contains(
                  QStringLiteral("passphrase")),
          "错口令恢复被拒",
          QString("label=%1")
              .arg(syncStateLabel
                       ? syncStateLabel->property("text").toString()
                       : QString()));
    // 错口令不产生文件之外的半态：恢复计数为 0 的语义由适配器并入
    // 语义保证，这里只验拒绝文案已到位

    passField->setProperty("text", QStringLiteral("audit-pass-123"));
    scrollIntoView(c.win, syncRestoreButton);
    clickItem(c.win, syncRestoreButton);
    settle(c.win);
    audit(syncStateLabel
              && syncStateLabel->property("text").toString().contains(
                  QStringLiteral("已恢复")),
          "对口令恢复成功（并入语义）",
          QString("label=%1")
              .arg(syncStateLabel
                       ? syncStateLabel->property("text").toString()
                       : QString()));
} else {
    audit(false, "备份自救控件", "导出/恢复按钮或输入框不存在");
}

if (c.drawer) c.drawer->setProperty("visible", false);
settle(c.win);

}
