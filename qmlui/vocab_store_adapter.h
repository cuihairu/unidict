#ifndef VOCAB_STORE_ADAPTER_H
#define VOCAB_STORE_ADAPTER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// 生词/历史/笔记面（TD-121 拆桥第三刀，从 LookupAdapter 整体迁出）：
// DataStoreQt 存储口径的薄转发（标签/笔记/技能标记/清空/导出），入库时
// 做富文本 → 纯文本降采样（stripHtmlForStorage），其余无本地状态。
class VocabStoreAdapter : public QObject {
    Q_OBJECT
public:
    explicit VocabStoreAdapter(QObject* parent = nullptr);

    Q_INVOKABLE void addToVocabulary(const QString& word, const QString& definition);
    Q_INVOKABLE QStringList searchHistory(int limit = 100) const;
    // 结构化历史（UI 批一 历史命中计数）：[{word,count,pinned}]，count=
    // 同词重复查词累计（旧数据兜 1）
    Q_INVOKABLE QVariantList searchHistoryEntries(int limit = 200) const;
    // 历史置顶（历史面最后一公里）：pin/unpin 转发 core 置顶块语义
    // （true=置顶块新尾，false=非置顶区头；词大小写不敏感，未命中返回假）
    Q_INVOKABLE bool setSearchHistoryPinned(const QString& word, bool pinned);
    Q_INVOKABLE QVariantList vocabulary() const;
    Q_INVOKABLE QVariantList vocabularyMeta() const; // [{word,definition,added_at,tags}]
    // M3-B 生词本编辑（qmlui 生词本页）：标签单条增删（词大小写不敏感、
    // add 幂等、空标签拒、remove 双命中才真）、按标签筛选（保持存储序）、
    // 笔记 upsert（空串即删）——core DataStore M3 口径的转发
    Q_INVOKABLE QVariantList vocabularyByTag(const QString& tag) const;
    Q_INVOKABLE bool addVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE bool removeVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE void setVocabNote(const QString& word, const QString& text);
    // 荧光笔多色（UI 批二）：色位 0=无色 1-4=色板位（语义同 core
    // set_note 3 参口径，color<0 = 保持既有色位）
    Q_INVOKABLE void setVocabNoteColored(const QString& word, const QString& text, int color);
    Q_INVOKABLE QString getVocabNote(const QString& word) const;
    Q_INVOKABLE int getVocabNoteColor(const QString& word) const; // 无笔记返回 0
    // 笔记内检索（roadmap Search within notes）：note 文本大小写不敏感
    // 子串过滤，[{word,text,updated_at}]；query 空 = 全部笔记
    Q_INVOKABLE QVariantList searchNotes(const QString& query) const;
    // 四技能标记（P-7 复习面写入口）：skill ∈
    // {listen,speak,read,write}（大小写不敏感）、level 0-2（0=未练
    // 1=不稳 2=稳），非法参数/词未命中返回假不动数据
    Q_INVOKABLE bool setVocabularySkill(const QString& word, const QString& skill, int level);
    Q_INVOKABLE void removeVocabularyWord(const QString& word);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void clearVocabulary();
    Q_INVOKABLE bool exportVocabCsv(const QString& path) const;
    // CSV 导入（导入导出批）：返回 {imported, skipped}；imported=-1 =
    // 文件不可读。跳过/幂等语义同 std 层（已存在整行跳过不覆盖）
    Q_INVOKABLE QVariantMap importVocabCsv(const QString& path);
    // 笔记导出（roadmap Export notes）：按扩展名分流 .pdf → PDF，其余 → HTML
    Q_INVOKABLE bool exportNotesHtml(const QString& path) const;
    Q_INVOKABLE bool exportNotesPdf(const QString& path) const;
};

#endif // VOCAB_STORE_ADAPTER_H
