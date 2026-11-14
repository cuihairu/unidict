// 离屏 UI 真点审计场景（LAY-008 自 ui_click_audit.cpp 拆出，第 2/6 批）：
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

void audit_s11(AuditCtx& c) {
// ---- S11 侧栏：搜索+查按钮 ----
QQuickItem* searchField = item(c.win, "searchField");
searchField->setProperty("text", QStringLiteral("hello"));
flushEvents();
clickItem(c.win, item(c.win, "searchGoButton"));
audit(c.win->property("currentWord").toString() == QStringLiteral("hello"),
      "侧栏「查」按钮点击提交查询",
      QString("currentWord=%1").arg(c.win->property("currentWord").toString()));

// 建议列表（suggestTimer 120ms 后有项）。UI 批一建议分组后，首条
// 委托可能是「精确」段的词头，不再必然等于 suggestPrefix 首候选——
// 改为读点击目标自身的词（text=原始词，内容 Text 是富文本高亮串）
searchField->setProperty("text", QStringLiteral("hel"));
settle(c.win);
if (QQuickItem* sugItem = item(c.win, "suggestItem")) {
    const QString sugWord = sugItem->property("text").toString();
    clickItem(c.win, sugItem);
    audit(c.win->property("currentWord").toString() == sugWord,
          "建议列表项点击查询",
          QString("currentWord=%1, 期望=%2")
              .arg(c.win->property("currentWord").toString(), sugWord));
} else {
    audit(false, "建议列表项点击查询", "suggestItem 委托不存在（检查词典）");
}

}

void audit_s12(AuditCtx& c) {
// ---- S12 侧栏 tab：历史 ----
clickItem(c.win, item(c.win, "sideTab1"));
settle(c.win);   // StackLayout 切页后委托实例化需要一帧
audit(lpProp(c.win, "currentTabIndex").toInt() == 1,
      "侧栏「历史」tab 点击", "currentTabIndex=1");
const QStringList history = c.vocab.searchHistory(200);
if (!history.isEmpty()) {
    // 委托 childItems 序≠model 序（ListView 复用/重排），点中谁断言谁
    // 的 text 落词——验证「委托点击 → openWord」接线本身
    if (auto* hItem = item(c.win, "historyItem")) {
        const QString hWord = hItem->property("text").toString();
        clickItem(c.win, hItem);
        audit(c.win->property("currentWord").toString() == hWord,
              "历史项点击查询",
              QString("currentWord=%1, 期望=%2")
                  .arg(c.win->property("currentWord").toString(), hWord));
    } else {
        audit(false, "历史项点击查询", "historyItem 委托不存在");
    }
} else {
    audit(false, "历史项点击查询", "searchHistory 为空（前置查询应已入史）");
}

}

void audit_s13(AuditCtx& c) {
// ---- S13 侧栏 tab：生词本（chips/卡/对话框） ----
clickItem(c.win, item(c.win, "sideTab2"));
settle(c.win);   // StackLayout 切页后 ListView 委托实例化需要一帧
audit(lpProp(c.win, "currentTabIndex").toInt() == 2,
      "侧栏「生词本」tab 点击", "currentTabIndex=2");
clickItem(c.win, item(c.win, "vocabFilterChip_greeting"));
audit(lpProp(c.win, "vocabTagFilter").toString()
          == QStringLiteral("greeting"),
      "标签筛选 chip 点击过滤",
      QString("vocabTagFilter=%1")
          .arg(lpProp(c.win, "vocabTagFilter").toString()));
// 卡片集断言：reloadVocabulary 读根属性过滤，裸名遮蔽会让属性值
// 看似更新但列表不过滤（属性值断言抓不到这类断线）
settle(c.win);   // 模型重建后委托实例化需一帧
{
    QList<QQuickItem*> tagCards;
    visualFindAll(c.win->contentItem(), "vocabCard", tagCards);
    QStringList tagWords;
    for (QQuickItem* ci : tagCards)
        tagWords << ci->property("ownerWord").toString();
    tagWords.sort();
    audit(tagWords == QStringList{QStringLiteral("hello")},
          "标签筛选卡片集收敛",
          QString("tagCards=[%1]").arg(tagWords.join(QLatin1Char('|'))));
}
clickItem(c.win, item(c.win, "vocabFilterChip_all"));
audit(lpProp(c.win, "vocabTagFilter").toString().isEmpty(),
      "「全部」chip 点击清筛选", "vocabTagFilter 为空");

const QVariantList vocabMeta = c.vocab.vocabularyMeta();
const QString vocabWord0 = vocabMeta.value(0).toMap().value("word").toString();
// 过滤切换触发模型重建，布局稳一帧再点卡——press→release 间隔内
// 控件位移会让 AbstractButton 判定界外释放、不发 clicked
settle(c.win);
// 委托 childItems 序≠model 序：点中谁断言谁的 ownerWord（验证
// 「生词卡点击 → openWord」接线本身）
if (auto* vCard = item(c.win, "vocabCard")) {
    const QString cardWord = vCard->property("ownerWord").toString();
    // 点词头 Label 区域（左上）：卡中心落在第二行笔记/标签 chip 上，
    // 那是另一个可点元素
    clickItemRel(c.win, vCard, 0.15, 0.15);
    audit(c.win->property("currentWord").toString() == cardWord,
          "生词卡点击查询",
          QString("currentWord=%1, 期望=%2")
              .arg(c.win->property("currentWord").toString(), cardWord));
} else {
    audit(false, "生词卡点击查询", "vocabCard 委托不存在");
}

clickItem(c.win, item(c.win, "vocabAddTagButton"));
QObject* tagDialog = popupObj(c.win, "tagDialog");
audit(popupVisible(tagDialog), "生词卡「+标签」打开标签对话框",
      "tagDialog visible");
if (popupVisible(tagDialog)) {
    QMetaObject::invokeMethod(tagDialog, "close");
    settle(c.win);   // modal 关闭过渡未完会吃掉后续点击
}

clickItem(c.win, item(c.win, "vocabNoteButton"));
QObject* noteDialog = popupObj(c.win, "noteDialog");
audit(popupVisible(noteDialog), "生词卡「笔记」打开笔记对话框",
      "noteDialog visible");
if (popupVisible(noteDialog)) {
    QMetaObject::invokeMethod(noteDialog, "close");
    settle(c.win);
}

QQuickItem* exportButton = item(c.win, "vocabExportButton");
ClickCounter exportCounter;
QObject::connect(exportButton, SIGNAL(clicked()), &exportCounter,
                 SLOT(onClicked()));
clickItem(c.win, exportButton);
audit(exportCounter.count == 1,
      "「导出CSV」点击（signal 级：native 文件对话框离屏不出窗）",
      QString("clicked 计数=%1").arg(exportCounter.count));

// 笔记导出按钮：同 CSV 先例（文件对话框离屏不出窗，signal 级验证接线）
QQuickItem* notesExportButton = item(c.win, "notesExportButton");
ClickCounter notesExportCounter;
QObject::connect(notesExportButton, SIGNAL(clicked()), &notesExportCounter,
                 SLOT(onClicked()));
clickItem(c.win, notesExportButton);
audit(notesExportCounter.count == 1,
      "「导出笔记」点击（signal 级：native 文件对话框离屏不出窗）",
      QString("clicked 计数=%1").arg(notesExportCounter.count));

}

void audit_s25(AuditCtx& c) {
// ---- S25 生词本 CSV 导入（导入导出批）：按钮 signal 级（对话框
// 离屏不出窗）+ 导入链真数据直驱（新词落库/已存在跳过/笔记还原） ----
QQuickItem* importButton = item(c.win, "vocabImportButton");
audit(importButton != nullptr,
      "「导入CSV」按钮存在",
      importButton ? "found" : "vocabImportButton 未定位");
if (importButton) {
    ClickCounter importCounter;
    QObject::connect(importButton, SIGNAL(clicked()), &importCounter,
                     SLOT(onClicked()));
    clickItem(c.win, importButton);
    audit(importCounter.count == 1,
          "「导入CSV」点击（signal 级：文件对话框离屏不出窗）",
          QString("clicked 计数=%1").arg(importCounter.count));
}
{
    const QString csvPath = QDir(c.outDir).filePath("audit_import.csv");
    {
        QFile f(csvPath);
        f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write("\xEF\xBB\xBFword,definition,tags,note\n"
                "\"audit import word\",\"from csv\",\"csvtag\","
                "\"csv note\"\n"
                "\"hello\",\"should skip\",\"\",\"\"\n");
    }
    const int before = c.vocab.vocabulary().size();
    const QVariantMap r = c.vocab.importVocabCsv(csvPath);
    audit(r.value("imported").toInt() == 1
              && r.value("skipped").toInt() == 1,
          "CSV 导入链（新词落库+已存在跳过）",
          QString("imported=%1, skipped=%2")
              .arg(r.value("imported").toInt())
              .arg(r.value("skipped").toInt()));
    audit(c.vocab.vocabulary().size() == before + 1
              && c.vocab.getVocabNote("audit import word")
                     == QStringLiteral("csv note"),
          "CSV 导入落库（笔记还原/计数）",
          QString("size=%1, note=%2")
              .arg(c.vocab.vocabulary().size())
              .arg(c.vocab.getVocabNote("audit import word")));
    c.vocab.removeVocabularyWord("audit import word");
}

}

void audit_s13b(AuditCtx& c) {
// ---- S13-b 笔记内检索（Search within notes）：输入非空 → 列表切
// 笔记命中集（hello 有笔记「常用问候语」，world 无笔记应被滤掉） ----
if (QQuickItem* noteSearch = item(c.win, "vocabNoteSearchInput")) {
    noteSearch->setProperty("text", QString::fromUtf8("问候"));
    settle(c.win);   // onTextChanged → reloadVocabulary 重建委托
    QList<QQuickItem*> cards;
    visualFindAll(c.win->contentItem(), "vocabCard", cards);
    QStringList words;
    for (QQuickItem* ci : cards)
        words << ci->property("ownerWord").toString();
    words.sort();
    audit(words == QStringList{QStringLiteral("hello")},
          "笔记检索命中集过滤",
          QString("命中卡=[%1]").arg(words.join(QLatin1Char('|'))));
    // 清空 → 恢复全量
    noteSearch->setProperty("text", QString());
    settle(c.win);
    QList<QQuickItem*> allCards;
    visualFindAll(c.win->contentItem(), "vocabCard", allCards);
    audit(allCards.size() >= 2,
          "笔记检索清空恢复全量",
          QString("恢复后卡数=%1").arg(allCards.size()));
} else {
    audit(false, "笔记检索命中集过滤", "vocabNoteSearchInput 不存在");
}

}

void audit_s27(AuditCtx& c) {
// ---- S27 历史置顶（历史面最后一公里）：真点 pin → 置顶块插队双证
// （adapter entries[0] 变被点词且 pinned=true）→ 再点还原 ----
{
    // 种两词条史（真实命中链入史；world 在 fixture 无独立词头，
    // miss 不入史），防审计前段查词数不足两行
    c.adapter.lookupDefinition(QStringLiteral("helmet"));
    c.adapter.lookupDefinition(QStringLiteral("helpdesk"));
    clickItem(c.win, item(c.win, "headerHistoryButton"));
    settle(c.win);   // 切历史 tab 即 reloadHistory（onCurrentIndexChanged）
    QList<QQuickItem*> rows;
    visualFindAll(c.win->contentItem(), "historyItem", rows);
    if (rows.size() >= 2) {
        QQuickItem* row = rows[1];
        const QString w2 = row->property("text").toString();
        QQuickItem* pinBtn = visualFind(row, "historyPinButton");
        audit(pinBtn != nullptr,
              "历史行置顶按钮存在",
              pinBtn ? "historyPinButton found" : "不存在");
        if (pinBtn) {
            clickItem(c.win, pinBtn);
            settle(c.win);
            const QVariantList hist = c.vocab.searchHistoryEntries(5);
            audit(hist.size() > 0
                      && hist.value(0).toMap().value("word").toString()
                             .compare(w2, Qt::CaseInsensitive) == 0
                      && hist.value(0).toMap().value("pinned").toBool(),
                  "历史置顶点击（置顶块插队）",
                  QString("top=%1, pinned=%2")
                      .arg(hist.size() > 0
                               ? hist.value(0).toMap().value("word")
                                     .toString()
                               : QString())
                      .arg(hist.size() > 0
                               && hist.value(0).toMap()
                                      .value("pinned").toBool()
                               ? 1
                               : 0));
            // 还原：置顶行已重排（模型重建），重新枚举找同词行再点
            QList<QQuickItem*> rows2;
            visualFindAll(c.win->contentItem(), "historyItem", rows2);
            for (QQuickItem* r2 : rows2) {
                if (r2->property("text").toString()
                        .compare(w2, Qt::CaseInsensitive) == 0) {
                    if (QQuickItem* pb =
                            visualFind(r2, "historyPinButton")) {
                        scrollIntoView(c.win, pb);
                        clickItem(c.win, pb);
                    }
                    break;
                }
            }
            settle(c.win);
            bool restored = false;
            const QVariantList hist2 = c.vocab.searchHistoryEntries(50);
            for (const QVariant& e : hist2) {
                if (e.toMap().value("word").toString()
                        .compare(w2, Qt::CaseInsensitive) == 0)
                    restored = !e.toMap().value("pinned").toBool();
            }
            audit(restored, "历史置顶再点还原",
                  QString("pinned(%1)=0").arg(w2));
        }
    } else {
        audit(false, "历史行置顶按钮存在",
              QString("历史行不足: %1").arg(rows.size()));
    }
}

}

void audit_s29(AuditCtx& c) {
// ---- S29 历史逐条删除（F5）：真点 ✕ → 行消失 + adapter entries
// 不再含该词（core 落库真删，非仅 UI 隐藏）----
{
    // 防御性补种（真实命中链入史；miss 不入史），保证删除后仍有残行
    c.adapter.lookupDefinition(QStringLiteral("helmet"));
    c.adapter.lookupDefinition(QStringLiteral("helpdesk"));
    clickItem(c.win, item(c.win, "headerHistoryButton"));
    settle(c.win);   // 切历史 tab 即 reloadHistory（onCurrentIndexChanged）
    QList<QQuickItem*> rows;
    visualFindAll(c.win->contentItem(), "historyItem", rows);
    if (!rows.isEmpty()) {
        QQuickItem* row = rows.last();
        const QString wDel = row->property("text").toString();
        // 删除前基准：entries 确含该词（had）+ 总条数
        const QVariantList before = c.vocab.searchHistoryEntries(200);
        bool had = false;
        for (const QVariant& e : before) {
            if (e.toMap().value("word").toString()
                    .compare(wDel, Qt::CaseInsensitive) == 0) {
                had = true;
                break;
            }
        }
        QQuickItem* delBtn = visualFind(row, "historyDeleteButton");
        audit(delBtn != nullptr,
              "历史行删除按钮存在",
              delBtn ? "historyDeleteButton found" : "不存在");
        if (delBtn) {
            scrollIntoView(c.win, delBtn);
            clickItem(c.win, delBtn);
            settle(c.win);   // reloadHistory 重建委托
            const QVariantList hist = c.vocab.searchHistoryEntries(200);
            bool gone = true;
            for (const QVariant& e : hist) {
                if (e.toMap().value("word").toString()
                        .compare(wDel, Qt::CaseInsensitive) == 0) {
                    gone = false;
                    break;
                }
            }
            QList<QQuickItem*> rows2;
            visualFindAll(c.win->contentItem(), "historyItem", rows2);
            audit(had && gone && rows2.size() == rows.size() - 1,
                  "历史逐条删除（行消失 + 落库删）",
                  QString("word=%1, rows %2→%3, entries %4→%5")
                      .arg(wDel)
                      .arg(rows.size())
                      .arg(rows2.size())
                      .arg(before.size())
                      .arg(hist.size()));
        }
    } else {
        audit(false, "历史行删除按钮存在", "历史行不足");
    }
}

}
