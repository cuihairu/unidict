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
    std::vector<VocabItemStd> get_vocabulary() const;
    void clear_vocabulary();
    bool export_vocabulary_csv(const std::string& file_path) const;

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
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DATA_STORE_STD_H
