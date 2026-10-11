#ifndef LOOKUP_ADAPTER_H
#define LOOKUP_ADAPTER_H

#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>
#include <memory>

// Forward declarations
namespace UnidictCoreStd {
class DictionaryManagerStd;
}

class LookupAdapter : public QObject {
    Q_OBJECT
    Q_PROPERTY(int dictionariesStamp READ dictionariesStamp NOTIFY dictionariesStampChanged)
public:
    explicit LookupAdapter(QObject* parent = nullptr);
    ~LookupAdapter() override;

    int dictionariesStamp() const { return dictionariesStamp_; }

    Q_INVOKABLE QString lookupDefinition(const QString& word);
    Q_INVOKABLE QStringList suggestPrefix(const QString& prefix, int maxResults = 20) const;
    Q_INVOKABLE QStringList suggestFuzzy(const QString& word, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchWildcard(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList searchRegex(const QString& pattern, int maxResults = 20) const;
    Q_INVOKABLE QStringList loadedDictionaries() const;
    Q_INVOKABLE bool loadDictionariesFromEnv();
    Q_INVOKABLE bool reloadDictionariesFromEnv();

    // P-6 词典库导入面：目录扫描 → 逐个装载的分块导入（主线程
    // QTimer 步进，与查询天然互斥、进度条可刷新；单词典解析期间
    // 事件循环冻结与既有同步装载同级）。扫描用 manager::scan_directory
    // （registry 扩展名 + 已装载/隔离去重）。取消置标志，队列清空收尾。
    Q_INVOKABLE void startDirectoryImport(const QString& dir);
    Q_INVOKABLE void cancelDirectoryImport();
    Q_INVOKABLE bool isDirectoryImportRunning() const { return importActive_; }
    Q_INVOKABLE bool setMdictPassword(const QString& password);
    Q_INVOKABLE void clearMdictPassword();
    Q_INVOKABLE bool hasMdictPassword() const;
    Q_INVOKABLE void addToVocabulary(const QString& word, const QString& definition);
    Q_INVOKABLE QStringList searchHistory(int limit = 100) const;
    Q_INVOKABLE QVariantList vocabulary() const;
    Q_INVOKABLE QVariantList vocabularyMeta() const; // [{word,definition,added_at,tags}]
    // M3-B 生词本编辑（qmlui 生词本页）：标签单条增删（词大小写不敏感、
    // add 幂等、空标签拒、remove 双命中才真）、按标签筛选（保持存储序）、
    // 笔记 upsert（空串即删）——core DataStore M3 口径的转发
    Q_INVOKABLE QVariantList vocabularyByTag(const QString& tag) const;
    Q_INVOKABLE bool addVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE bool removeVocabTag(const QString& word, const QString& tag);
    Q_INVOKABLE void setVocabNote(const QString& word, const QString& text);
    Q_INVOKABLE QString getVocabNote(const QString& word) const;
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
    Q_INVOKABLE int indexedWordCount() const;
    Q_INVOKABLE bool exportVocabCsv(const QString& path) const;
    // 笔记导出（roadmap Export notes）：按扩展名分流 .pdf → PDF，其余 → HTML
    Q_INVOKABLE bool exportNotesHtml(const QString& path) const;
    Q_INVOKABLE bool exportNotesPdf(const QString& path) const;
    Q_INVOKABLE QVariantList dictionariesMeta() const;
    // P-6 词典库面：失败/隔离项与显式重试（成功摘档建索引发 stamp；
    // 再失败刷新原因并确认隔离——std 口径透传）
    Q_INVOKABLE QVariantList failedDictionaries() const;
    Q_INVOKABLE bool retryFailedDictionary(const QString& filePath);

    // P0 专业词典功能
    // HTML 渲染和安全过滤
    Q_INVOKABLE QString sanitizeHtml(const QString& html) const;
    Q_INVOKABLE QString extractTextFromHtml(const QString& html) const;
    // 文本节点级命中词标注（P-4）：sanitize 后 HTML 的文本节点内按
    // 大小写不敏感匹配 word，命中片段包 <span class="udict-hl">（显示
    // 层换主题高亮色）；标签/属性明文不动。空 word 原样返回。
    Q_INVOKABLE QString highlightHtml(const QString& html, const QString& word) const;
    Q_INVOKABLE QString rewriteResourceUrls(const QString& html, const QString& dictionaryId) const;
    Q_INVOKABLE QString rewriteCrossReferenceLinks(const QString& html, const QString& dictionaryId) const;

    // .mdd 资源直取：rewriteResourceUrls 负责把 HTML 里的相对 src 换成
    // file:// 缓存路径（图片/内嵌音视频够用了）。下面三个给"需要原始字节"
    // 的场景——发音音频要喂给 QML MediaPlayer、调用方要自己判断资源是否存在。
    // key 用 .mdd 里的原始键（如 "sounds/hello.mp3"），大小写与前导斜杠
    // 由 core/std 的 normalize_key 归一。
    Q_INVOKABLE bool hasDictionaryResource(const QString& dictionaryId, const QString& key) const;
    Q_INVOKABLE QByteArray loadDictionaryResourceData(const QString& dictionaryId, const QString& key) const;
    // 资源在本地缓存里的 file:// URL；未命中返回空串
    Q_INVOKABLE QString dictionaryResourceUrl(const QString& dictionaryId, const QString& key) const;

    // 一站式词条呈现管线：上面四步合成一次调用，顺序固定为
    // 清洗 → 交叉引用链接 → 资源 URL 重写。返回：
    //   html       —— 可直接交给富文本组件的 HTML
    //   text       —— 纯文本回退（搜索高亮/朗读/剪贴板用）
    //   resources  —— [{key, url, found}]，found=false 表示 .mdd 里没这张图
    Q_INVOKABLE QVariantMap presentEntry(const QString& rawDefinition,
                                         const QString& dictionaryId) const;

    // 交叉引用导航
    Q_INVOKABLE bool canGoBack() const;
    Q_INVOKABLE bool canGoForward() const;
    Q_INVOKABLE QString goBack();
    Q_INVOKABLE QString goForward();
    Q_INVOKABLE void navigateToWord(const QString& word, const QString& dictionaryId = "");
    Q_INVOKABLE void clearNavigationHistory();
    Q_INVOKABLE int navigationHistorySize() const;

    // 多词典聚合查询（分组形态：[{dictionary, dictionaryId, entries:[…]}]，
    // core searchGrouped 三层降级 + 组内 headword 去重）
    Q_INVOKABLE QVariantList aggregateLookup(const QString& word, const QVariantMap& options = QVariantMap());
    // 词条卡头英/美音标：从释义开头的"英 […] 美 […]"惯例提取（{british, american}）
    Q_INVOKABLE QVariantMap extractPhonetics(const QString& definition) const;
    // 全文检索 tab：释义包含目标词的词条（[{word, definition, dictionary}]）
    Q_INVOKABLE QVariantList fullTextLookup(const QString& word, int maxResults = 20) const;
    // 内容 tab：phrases=以查询词开头的词组条目（带释义）；
    // related=近义/联想候选词表（[{word}]，词头链接形态）
    Q_INVOKABLE QVariantList relatedLookup(const QString& word, const QString& kind) const;
    Q_INVOKABLE QVariantList getDictionariesByCategory(const QString& category) const;
    Q_INVOKABLE void setDictionaryPriority(const QString& dictionaryId, int priority);
    Q_INVOKABLE void setDictionaryEnabled(const QString& dictionaryId, bool enabled);

    // 语音面（TTS/在线发音/自动播放 28 个 Q_INVOKABLE + pronOnlineStatus
    // 信号）已迁出至 VoiceAdapter（TD-121 拆桥第二刀，qmlui/voice_adapter.h），
    // QML 侧 context property 名 "voice"；自动朗读经 entryLookedUp 信号接入。

signals:
    void dictionariesStampChanged();
    // 查词命中（lookupDefinition/aggregateLookup 有结果时）——VoiceAdapter
    // 接此信号做自动朗读（main.cpp 装配，两侧互不感知内部实现）
    void entryLookedUp(const QString& word);

    // 导入管道（startDirectoryImport）：
    //   importScanFinished(found) —— 扫描完成，found=0 时随后即发 importFinished
    //   importProgress(imported, failed, total, currentFile) —— 每装完一个
    //   importFinished(imported, failed) —— 收尾（含取消/空目录）
    void importScanFinished(int found);
    void importProgress(int imported, int failed, int total, const QString& currentFile);
    void importFinished(int imported, int failed);

private:
    // 导入步进：装载队列头一个并排下一步（队列空时收尾建索引+发完成）
    void importStep();
    // 导入终局（正常收尾/取消共用）：ok>0 时建索引+发 stamp，再发完成
    void finishImport();

    int dictionariesStamp_ = 0;
    // 目录导入分块队列（主线程 QTimer 步进；见 startDirectoryImport 注释）
    QStringList importQueue_;
    int importTotal_ = 0;
    int importOk_ = 0;
    int importFail_ = 0;
    bool importActive_ = false;
    // 词典面已切 core/std（P-6）：装载/查询/索引/资源定位都走
    // DictionaryManagerStd；DataStore（历史/生词本）仍经既有转发器
    std::unique_ptr<UnidictCoreStd::DictionaryManagerStd> m_dictMgr;

    // P0 模块实例 (forward declared, 使用 std 模块)
    // 为了避免 Qt 依赖 std 模块，这里使用 pimpl 模式
    class P0Modules;
    std::unique_ptr<P0Modules> m_p0;
};

#endif // LOOKUP_ADAPTER_H
