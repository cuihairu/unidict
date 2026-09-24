#ifndef UNIDICT_EPUB_PARSER_STD_H
#define UNIDICT_EPUB_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace UnidictCoreStd {

// EPUB 词典 parser（最小可用）。EPUB 是 zip 容器：
// META-INF/container.xml 声明 OPF 路径 → OPF manifest 列出内容文档 →
// 逐 XHTML 提取词条。
//
// 词条提取约定（词典类 epub 没有标准排版，这里取最常见的一种）：
// - <h1>~<h6> 的文本是词头；其后到下一个词头之间的可见文本是释义
// - 没有任何词头的文档无法可靠切词，整篇跳过
// - HTML 实体解码最小集 + ASCII 数字实体；剥其余标签、折叠空白
//
// 大小写语义：键按小写归一（大小写不敏感查询），返回词头保留原词形。
class EpubParserStd {
public:
    bool load_dictionary(const std::string& path);
    bool is_loaded() const { return loaded_; }

    std::string dictionary_name() const { return name_; }
    std::string dictionary_description() const { return description_; }
    int word_count() const;

    std::string lookup(const std::string& word) const;
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool parse_container(const std::string& container_xml, std::string& opf_path) const;
    // manifest 里全部 XHTML 文档的 zip 路径（相对 OPF 目录已归一）
    std::vector<std::string> collect_opf_documents(const std::string& opf_xml,
                                                   const std::string& opf_dir) const;
    void add_entries_from_html(const std::string& html);

    bool loaded_ = false;
    std::string name_;
    std::string description_;
    // lower(word) -> {原词形, 释义}：查询大小写不敏感，词头保留原词形
    std::unordered_map<std::string, std::pair<std::string, std::string>> entries_;
    std::vector<std::string> words_; // 原词形，插入序
};

} // namespace UnidictCoreStd

#endif // UNIDICT_EPUB_PARSER_STD_H
