// Qt-free minimal StarDict parser: supports .ifo/.idx/.dict (no .dz for now).

#ifndef UNIDICT_STARDICT_PARSER_STD_H
#define UNIDICT_STARDICT_PARSER_STD_H

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {

struct StarDictHeaderStd {
    std::string version;
    std::string book_name;
    int word_count = 0;
    int index_file_size = 0;
    int idx_offset_bits = 32; // 32 or 64
    std::string description;
};

class StarDictParserStd {
public:
    StarDictParserStd();
    ~StarDictParserStd();

    bool load_dictionary(const std::string& any_path);
    bool is_loaded() const;

    std::string dictionary_name() const;
    std::string dictionary_description() const;
    int word_count() const;

    std::string lookup(const std::string& word) const; // empty if not found
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool load_ifo(const std::string& ifo_path);
    bool load_idx(const std::string& idx_path);
    bool open_dict(const std::string& dict_path);

    static std::string base_without_ext(const std::string& path);
    static std::string dirname(const std::string& path);
    static bool ends_with(const std::string& s, const std::string& suf);
    static uint32_t be32(const unsigned char* p);
    static uint64_t be64(const unsigned char* p);
    static std::string lcase(const std::string& s);

    StarDictHeaderStd header_;
    std::unordered_map<std::string, std::pair<uint64_t, uint32_t>> index_; // word -> (offset, size)
    std::vector<std::string> words_;
    mutable std::ifstream dict_stream_;
    bool loaded_ = false;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_STARDICT_PARSER_STD_H

