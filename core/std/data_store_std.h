// Qt-free lightweight data store for history and vocabulary.

#ifndef UNIDICT_DATA_STORE_STD_H
#define UNIDICT_DATA_STORE_STD_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace UnidictCoreStd {

struct VocabItemStd {
    std::string word;
    std::string definition;
    long long added_at = 0; // epoch seconds; 0 if unknown
    std::vector<std::string> tags; // 生词分组标签（可空；往返持久化，旧文件无此字段）
};

// 词条笔记：随词存储的任意文本（学习备注等），空文本即无笔记
struct NoteItemStd {
    std::string word;
    std::string text;
    long long updated_at = 0; // epoch seconds; 0 if unknown
};

// 发音练习记录（M9）：同一个词练过之后的累计——"跟自己的进步比"的
// 基线（评分绝对值对重口音用户偏严，单次分说明不了进步）。规则在
// core/std/pron_history_std（累计/进步幅度/清单次序），这里只负责存。
struct PronRecordStd {
    std::string word;
    double last_score = 0.0;  // 最近一次词分（0-1）
    double best_score = 0.0;  // 历史最佳
    int attempts = 0;         // 累计练习次数
    long long last_at = 0;    // epoch seconds; 0 if unknown
};

class DataStoreStd {
public:
    DataStoreStd();

    void set_storage_path(const std::string& file_path);
    std::string storage_path() const;

    // History
    void add_search_history(const std::string& word);
    std::vector<std::string> get_search_history(int limit = 100) const;
    void clear_history();

    // Vocabulary
    void add_vocabulary_item(const VocabItemStd& item);
    void remove_vocabulary_item(const std::string& word);
    // 按词（大小写不敏感）设置分组标签；命中返回真，未命中返回假不动数据
    bool set_vocabulary_item_tags(const std::string& word,
                                  const std::vector<std::string>& tags);
    // M3 标签管理：按词增/删单个标签（词大小写不敏感，标签精确匹配）。
    // add：命中词条即真（同标签幂等不重复）；空标签假。
    // remove：词条与标签都命中才真（删完保存）。
    bool add_vocabulary_item_tag(const std::string& word, const std::string& tag);
    bool remove_vocabulary_item_tag(const std::string& word, const std::string& tag);
    // 标签筛选：含该标签的条目（保持存储序；标签精确匹配）
    std::vector<VocabItemStd> get_vocabulary_by_tag(const std::string& tag) const;
    std::vector<VocabItemStd> get_vocabulary() const;
    void clear_vocabulary();
    // CSV 导出（M3 口径）：UTF-8 带 BOM（Excel 兼容），表头
    // word,definition,tags,note；标签单元格 ';' 连接（标签约定不含分号），
    // 笔记按词（大小写不敏感）联查、无笔记为空串
    bool export_vocabulary_csv(const std::string& file_path) const;

    // 词条笔记：按词（大小写不敏感）upsert；text 空串即移除该词笔记
    void set_note(const std::string& word, const std::string& text);
    std::string get_note(const std::string& word) const;
    std::vector<NoteItemStd> get_notes() const;

    // 发音练习记录（M9）：按词（大小写不敏感）upsert，word 空串即忽略
    // （没有键就没有记录）；查询/列举/清空
    void set_pron_record(const PronRecordStd& record);
    std::optional<PronRecordStd> get_pron_record(const std::string& word) const;
    std::vector<PronRecordStd> get_pron_records() const;
    void clear_pron_records();

    // Persistence
    bool load();
    bool save() const;

private:
    void ensure_loaded() const;
    static std::string json_escape(const std::string& s);

    std::string path_;
    mutable bool loaded_ = false;
    mutable std::vector<std::string> history_;
    mutable std::vector<VocabItemStd> vocab_;
    mutable std::vector<NoteItemStd> notes_;
    mutable std::vector<PronRecordStd> pron_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DATA_STORE_STD_H
