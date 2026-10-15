// Qt-free JSON dictionary parser (minimal tolerant parser for project format).

#ifndef UNIDICT_JSON_PARSER_STD_H
#define UNIDICT_JSON_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>

#include "dictionary_parser_std.h"

namespace UnidictCoreStd {

class JsonParserStd : public DictionaryParserStd {
public:
    JsonParserStd();

    bool load_dictionary(const std::string& file_path) override;
    bool is_loaded() const override;

    std::string name() const;
    std::string description() const;
    // DictionaryParserStd 接口别名（3.4 注册表面）：基面用 dictionary_* 命名
    std::string dictionary_name() const override { return name(); }
    std::string dictionary_description() const override { return description(); }
    int word_count() const;

    // 查词：精确 miss 时回退折叠键（小写），故 Hello 命中词头 hello
    // （BUGS.md BUG-005 的 std 面；Qt 面 JsonParser 早已同口径）
    std::string lookup(const std::string& word) const override;
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const override;

    // P1 text-norm：换键归一化档位。作废折叠缓存，下次查询按新档位重建
    void set_norm_options(const TextNorm::Options& opt) override;

private:
    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::unordered_map<std::string, std::string> entries_; // word -> definition
    // 折叠键（TextNorm::fold_key：大小写/全半角/重音）→ canonical 词形。
    // 惰建缓存（首查建一次，同折叠键多词形后写覆盖），按当前 norm_options_
    // 生成；换档或重载作废
    mutable std::unordered_map<std::string, std::string> lower_words_;
    mutable bool folded_built_ = false;
    TextNorm::Options norm_options_{};
    std::vector<std::string> words_;

    void ensure_folded() const;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_JSON_PARSER_STD_H

