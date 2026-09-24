// Qt-free lightweight data store for history and vocabulary.

#ifndef UNIDICT_DATA_STORE_STD_H
#define UNIDICT_DATA_STORE_STD_H

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
    std::vector<VocabItemStd> get_vocabulary() const;
    void clear_vocabulary();
    bool export_vocabulary_csv(const std::string& file_path) const;

    // 词条笔记：按词（大小写不敏感）upsert；text 空串即移除该词笔记
    void set_note(const std::string& word, const std::string& text);
    std::string get_note(const std::string& word) const;
    std::vector<NoteItemStd> get_notes() const;

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
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DATA_STORE_STD_H
