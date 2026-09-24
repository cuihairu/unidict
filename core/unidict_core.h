#ifndef UNIDICT_CORE_H
#define UNIDICT_CORE_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <QJsonObject>
#include <QPair>
#include <memory>
#include <vector>

#include "std/fulltext_index_std.h"
#include "std/mdd_resource_std.h"

namespace UnidictCore {

struct DictionaryEntry {
    QString word;
    QString definition;
    QString pronunciation;
    QStringList examples;
    QVariantMap metadata;
};

struct DictionaryInfo {
    QString id;
    QString name;
    QString description;
    QString filePath;
    QString format;
    QStringList tags;
    int wordCount = 0;
    bool enabled = true;
    int priority = 0;
};

// 加载失败词典的诊断记录（损坏词典检测）。持久化语义分两档：
// 解析失败（loadDictionary 返回假）→ 隔离：写进状态文件 quarantined，
//   重启后不再重复解析（大词典反复失败代价高），用户显式重试才再试；
// 文件丢失/扩展名不支持 → 仅运行期诊断：每次启动重查（开销为 stat），
//   文件回来了自动恢复加载，不落盘。
struct DictionaryFailure {
    QString filePath;
    QString reason;
    bool quarantined = false; // true=已持久化隔离；false=本次运行期诊断
};

struct DictionaryMatch {
    DictionaryEntry entry;
    QString dictionaryId;
    QString dictionaryName;
};

struct LookupResult {
    bool success = false;
    QString query;
    QString message;
    DictionaryEntry entry;
    QVector<DictionaryMatch> matches;
    QStringList suggestions;
    QString dictionaryId;
    QString dictionaryName;
};

struct SearchHistoryItem {
    QString query;
    bool success = false;
    QString dictionaryName;
    bool pinned = false;
};

class DictionaryParser {
public:
    virtual ~DictionaryParser() = default;
    
    virtual bool loadDictionary(const QString& filePath) = 0;
    virtual bool isLoaded() const = 0;
    virtual QStringList getSupportedExtensions() const = 0;
    
    virtual DictionaryEntry lookup(const QString& word) const = 0;
    virtual QStringList findSimilar(const QString& word, int maxResults = 10) const = 0;
    virtual QStringList getAllWords() const = 0;

    // 全部词条（词, 释义）：全文倒排索引的构建数据源。
    // 默认空实现——parser 可以不支持，此时该词典不参与全文检索。
    virtual QVector<QPair<QString, QString>> allEntries() const { return {}; }

    // 前缀匹配词表（大小写不敏感，返回原词形）：QCompleter 补全数据源。
    // 默认线性扫 getAllWords；拥有有序小写键存储的 parser 应 override
    // 走 lowerBound 二分。
    virtual QStringList prefixSearch(const QString& prefix, int maxResults = 20) const
    {
        QStringList out;
        const QString p = prefix.toLower();
        if (p.isEmpty() || maxResults <= 0) {
            return out;
        }
        const QStringList words = getAllWords();
        for (const QString& w : words) {
            if (w.toLower().startsWith(p)) {
                out.append(w);
                if (out.size() >= maxResults) {
                    break;
                }
            }
        }
        return out;
    }
    
    virtual QString getDictionaryName() const = 0;
    virtual QString getDictionaryDescription() const = 0;
    virtual int getWordCount() const = 0;
    virtual QString getSourcePath() const = 0;
    virtual QString getDictionaryId() const = 0;
    virtual QString getFormatName() const = 0;
};

class DictionaryManager {
public:
    static DictionaryManager& instance();
    
    bool addDictionary(const QString& filePath);
    int addDictionariesFromDirectory(const QString& directoryPath);
    bool removeDictionary(const QString& dictionaryId);
    bool setDictionaryEnabled(const QString& dictionaryId, bool enabled);
    bool setDictionaryTags(const QString& dictionaryId, const QStringList& tags);
    bool moveDictionaryUp(const QString& dictionaryId);
    bool moveDictionaryDown(const QString& dictionaryId);
    bool loadState(const QString& stateFilePath = QString());
    bool saveState(const QString& stateFilePath = QString()) const;
    QString defaultStateFilePath() const;
    void clear();
    bool hasDictionaries() const;
    QStringList getLoadedDictionaries() const;
    QVector<DictionaryInfo> getLoadedDictionaryInfos() const;
    // 加载失败词典（含隔离中的）诊断列表；词典管理对话框与 CLI --list 展示用
    QVector<DictionaryFailure> getFailedDictionaries() const;
    // 重试加载隔离中的词典：成功转正常（自动摘除隔离记录），失败留在隔离区并刷新原因
    bool retryFailedDictionary(const QString& filePath);
    // 放弃隔离中的词典：从隔离区与状态文件的 wanted 列表一并移除（不再重试）
    bool forgetFailedDictionary(const QString& filePath);
    // 读取词典附属 .mdd 资源（图片/音频等原始字节）。
    // 词典无 mdd 或资源未命中都返回空字节——调用方按空判处理。
    QByteArray loadDictionaryResource(const QString& dictionaryId,
                                      const QString& resourcePath) const;
    QVector<SearchHistoryItem> getSearchHistory(int maxItems = 50) const;
    void clearSearchHistory();
    bool removeSearchHistoryItem(const QString& query);
    bool setSearchHistoryPinned(const QString& query, bool pinned);
    bool exportSearchHistory(const QString& filePath) const;
    bool importSearchHistory(const QString& filePath, bool replaceExisting = false);
    
    LookupResult searchWord(const QString& word, const QStringList& tagFilter = {}) const;
    QStringList searchSimilar(const QString& word, int maxResults = 10,
                              const QStringList& tagFilter = {}) const;
    // 全部启用词典的词表合并去重（QCompleter 补全数据源；limit 防超大词典吃内存）
    QStringList getAllWords(int limit = 200000, const QStringList& tagFilter = {}) const;
    // 聚合搜索：所有启用词典中该词的条目（entry.metadata["dictionary"] 带来源名）
    QVector<DictionaryEntry> searchAll(const QString& word, const QStringList& tagFilter = {}) const;
    // 全文检索：对启用词典的释义建倒排索引（组合 std 引擎，惰性构建），
    // 按相关度返回命中的词条。词典集合变化后索引自动失效重建。
    // tagFilter 语义（下列所有查询一致）：空列表不过滤；非空时词典 tags
    // 与之有交集才参与。分组/profile 查询用，无需重建全文索引。
    QVector<DictionaryEntry> fullTextSearch(const QString& query, int maxResults = 20,
                                            const QStringList& tagFilter = {}) const;
    // 全文索引是否已在内存（构建/加载诊断用）
    bool isFulltextIndexBuilt() const;
    // 前缀补全：合并启用词典的 prefixSearch（去重，大小写不敏感，保序截断）
    QStringList prefixSearch(const QString& prefix, int maxResults = 20,
                             const QStringList& tagFilter = {}) const;
    // 正则搜索全部启用词典的词表（QRegularExpression 语义）
    QStringList regexSearch(const QString& pattern, int maxResults = 20,
                            const QStringList& tagFilter = {}) const;
    // 已加载（启用）词典的索引词总数
    int getIndexedWordCount() const;
    // 词典元数据列表（gui/qmlui 侧栏展示用）
    QVector<DictionaryInfo> getDictionariesMeta() const;
    // 仅清空已加载词典，不动搜索历史与持久化状态
    void clearDictionaries();
    QString lastError() const;
    
private:
    QString resolveStateFilePath(const QString& stateFilePath) const;
    QJsonObject toJson() const;
    bool loadFromJson(const QJsonObject& object);
    void recordSearch(const LookupResult& result);
    void invalidateFulltextIndex();
    void ensureFulltextIndexBuilt() const;

    struct DictionaryRecord {
        std::unique_ptr<DictionaryParser> parser;
        bool enabled = true;
        QStringList tags;
        // MDX 的同目录同名 .mdd 资源包（可选；加载失败不致命，保持空）
        std::unique_ptr<UnidictCoreStd::MddResourceParser> mdd;
    };

    // 分组过滤谓词：filter 空 = 不过滤；否则词典 tags 与 filter 有交集才可见
    static bool recordPassesTagFilter(const DictionaryRecord& record,
                                      const QStringList& filter);

    DictionaryManager() = default;
    std::vector<DictionaryRecord> m_parsers;
    QVector<DictionaryFailure> m_failures;
    QVector<SearchHistoryItem> m_history;
    QString m_lastError;

    // 按归一化路径找失败记录；带 out 参数返回下标，找不到返回 -1
    int indexOfFailure(const QString& filePath) const;
    // 插入或刷新失败记录（同路径幂等）；内容有变化返回 true
    bool recordFailure(const QString& filePath, const QString& reason, bool quarantined);
    // MDX 的同目录同名 .mdd 探测加载；无 mdd 或加载失败都不致命
    void loadMddCompanion(DictionaryRecord& record, const QString& dictionaryPath,
                          const QString& extension);

    // 全文倒排索引（std 引擎组合，词典型无 Qt）；m_ftDocs 与索引 doc 一一对应
    mutable std::unique_ptr<UnidictCoreStd::FullTextIndexStd> m_ftIndex;
    mutable std::vector<DictionaryEntry> m_ftDocs;
};

QString searchWord(const QString& word);
LookupResult lookupWord(const QString& word);
QString formatLookupResult(const LookupResult& result);

}

#endif // UNIDICT_CORE_H
