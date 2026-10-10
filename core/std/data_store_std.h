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
    // P-7 四技能 LearningState 字段位（product-principles §11：预留防迁移，
    // 不是课程化）。每技能 0-2：0=未练 1=不稳 2=稳；旧文件无字段=0（未练），
    // 落盘只写非零。经 set_vocabulary_skill 写入，生词本条目级存储。
    int listen = 0; // 听
    int speak = 0;  // 说
    int read = 0;   // 读
    int write = 0;  // 写
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

// 搜索历史条目（P-7 双存储合一）：此前 Qt 面 legacy manager 与本存储各持
// 一份 history——manager 是结构化记录（查词元数据 + 置顶，落 state 文件），
// 本存储是纯词表（落 unidict.json）。收敛后单源在此：查词元数据
// （success/dictionary_name）与置顶语义下沉，Qt 壳与 legacy manager 一律转发。
struct SearchHistoryEntryStd {
    std::string query;
    bool success = true;
    std::string dictionary_name;
    bool pinned = false;
};

class DataStoreStd {
public:
    DataStoreStd();

    void set_storage_path(const std::string& file_path);
    std::string storage_path() const;

    // History
    // 简单面：只带查询词（success=true、无词典名、不置顶）
    void add_search_history(const std::string& word);
    // 结构化面：查词记录（P-7 起为单一事实源写入口）
    void add_search_history_entry(const SearchHistoryEntryStd& entry);
    std::vector<std::string> get_search_history(int limit = 100) const;
    std::vector<SearchHistoryEntryStd> get_search_history_entries(int limit = 100) const;
    // 置顶语义（与 legacy manager 一致）：pin/unpin 都把条目插到 pinned 块
    // 末尾——true 即成为置顶块新尾，false 即非置顶区头；词大小写不敏感，
    // 未命中返回假
    bool set_search_history_pinned(const std::string& query, bool pinned);
    // 大小写不敏感删除；命中删掉并返回真
    bool remove_search_history(const std::string& query);
    // 整表重建（同步回放面）：按给定序原样替换全部 history，单次落盘。
    // 序语义=存储序（index 0 为最新），与 get_search_history_entries 的
    // 返回序同轴——restore(get()) 无损往返
    void set_search_history(std::vector<SearchHistoryEntryStd> entries);
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
    // 四技能状态写入（P-7，product-principles §11 预留字段位的写入口）：
    // skill ∈ {"listen","speak","read","write"}（大小写不敏感），
    // level 0-2（0=未练 1=不稳 2=稳）。词命中且技能与等级合法才真；
    // 非法技能/等级返回假不动数据（不静默钳位）。读面走 get_vocabulary
    // 的条目字段。注意 add_vocabulary_item 的更新臂只换释义，技能状态保留。
    bool set_vocabulary_skill(const std::string& word, const std::string& skill,
                              int level);
    void clear_vocabulary();
    // CSV 导出（M3 口径）：UTF-8 带 BOM（Excel 兼容），表头
    // word,definition,tags,note；标签单元格 ';' 连接（标签约定不含分号），
    // 笔记按词（大小写不敏感）联查、无笔记为空串
    bool export_vocabulary_csv(const std::string& file_path) const;

    // 词条笔记：按词（大小写不敏感）upsert；text 空串即移除该词笔记
    void set_note(const std::string& word, const std::string& text);
    std::string get_note(const std::string& word) const;
    std::vector<NoteItemStd> get_notes() const;
    // 笔记内检索（roadmap Note-Taking "Search within notes"）：note 文本
    // 大小写不敏感子串匹配（与词头匹配同 tolower 字节口径——ASCII 大小
    // 写互通，非 ASCII 字节原样参与，中文等按子串直接命中）；query 空
    // 串 = 全部笔记。命中按存储序返回。
    std::vector<NoteItemStd> search_notes(const std::string& query) const;
    // 笔记导出（roadmap Export notes）：独立 HTML，转义/时间格式口径
    // 见 notes_export_std.h；false=输出路径不可写
    bool export_notes_html(const std::string& out_path) const;

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

    // 存储上限（legacy manager 口径）：超过裁最旧
    static constexpr size_t kMaxHistoryEntries = 100;

    std::string path_;
    mutable bool loaded_ = false;
    mutable std::vector<SearchHistoryEntryStd> history_;
    mutable std::vector<VocabItemStd> vocab_;
    mutable std::vector<NoteItemStd> notes_;
    mutable std::vector<PronRecordStd> pron_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DATA_STORE_STD_H
