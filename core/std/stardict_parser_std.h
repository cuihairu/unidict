// Qt-free minimal StarDict parser: supports .ifo/.idx/.dict and .dict.dz (decompressed to cache).

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
    std::string same_type_sequence; // .ifo sametypesequence（空=词条自带类型字节）
    std::string charset;            // .ifo charset（暂存，供后续归一化使用）
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

    std::string lookup(const std::string& word) const; // 主释义文本（解码 sametypesequence 后）；未找到返回空
    std::string lookup_raw(const std::string& word) const; // .dict 原始字节（未解码）
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool load_ifo(const std::string& ifo_path);
    bool load_idx(const std::string& idx_path);
    bool open_dict(const std::string& dict_path);

    // 从 .dict 原始词条字节中解码出主释义文本：
    // 剥离类型字节/size 前缀/\0 终止符，优先返回第一个释义类字段（m/l/g/x/h），
    // Latin-1 转为 UTF-8。
    std::string decode_entry(const std::string& raw) const;

    static std::string latin1_to_utf8(const std::string& s);

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
