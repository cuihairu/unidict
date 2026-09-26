// Qt-free minimal StarDict parser: supports .ifo/.idx/.dict and .dict.dz (decompressed to cache).

#ifndef UNIDICT_STARDICT_PARSER_STD_H
#define UNIDICT_STARDICT_PARSER_STD_H

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "std/charset_codec_std.h"

namespace UnidictCoreStd {

struct StarDictHeaderStd {
    std::string version;
    std::string book_name;
    int word_count = 0;
    int index_file_size = 0;
    int idx_offset_bits = 32; // 32 or 64
    std::string description;
    std::string same_type_sequence; // .ifo sametypesequence（空=词条自带类型字节）
    std::string charset;            // .ifo charset 原样字符串（供 UI 显示）
    CharsetCodec::Charset codec = CharsetCodec::Charset::Unknown;  // 解析后的编码
    // mutable：lookup() 是 const 的，但兜底解码这个事实需要记下来给 UI
    // 如实提示（"该词典编码没写对，已按 GBK 猜"）。这是诊断信息，不是
    // 词典内容，不影响 const 语义。
    mutable bool charset_salvaged = false;
};

class StarDictParserStd {
public:
    StarDictParserStd();
    ~StarDictParserStd();

    bool load_dictionary(const std::string& any_path);
    bool is_loaded() const;

    std::string dictionary_name() const;
    std::string dictionary_description() const;

    // 词典自报的编码（原样）与解析结果。
    // 解析结果为 Unknown 表示该编码暂不支持——此时词条按原始字节透传，
    // 界面上会是乱码，UI 应当据此提示而不是假装正常。
    std::string dictionary_charset() const;
    CharsetCodec::Charset dictionary_codec() const;
    // 声明的编码我们能处理吗。false = 词条按原始字节透传，界面上会是乱码，
    // UI 应据此提示"该词典编码暂不支持"而不是假装正常。
    bool is_supported_codec() const;
    // 声明的 charset 不可信（不是合法 UTF-8）但数据能按 GB18030 救回来。
    // 真实世界里 .ifo 漏写 charset 或错标 UTF-8 很常见，这个标志让调用方
    // 能如实告诉用户"已按 GBK 猜测解码"。
    bool charset_salvaged() const;
    int word_count() const;

    std::string lookup(const std::string& word) const; // 主释义文本（解码 sametypesequence + charset 后）；未找到返回空
    std::string lookup_raw(const std::string& word) const; // .dict 原始字节（未解码）
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool load_ifo(const std::string& ifo_path);
    bool load_idx(const std::string& idx_path);
    bool open_dict(const std::string& dict_path);

    // 从 .dict 原始词条字节中解码出主释义文本：
    // 剥离类型字节/size 前缀/\0 终止符，优先返回第一个释义类字段（m/l/g/x/h），
    // 再按 header_.codec 把字节转成 UTF-8。
    std::string decode_entry(const std::string& raw) const;

    // 按 header_.codec 把 .dict 原始字节转成 UTF-8。声明不可信时按字段类型
    // 兜底：'l' 走 CP1252（实测最常见的错标形态），其余走 GB18030 兜底。
    // 两条兜底都要求字节不是合法 UTF-8。
    std::string decode_charset(const std::string& bytes, char field_kind) const;

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
