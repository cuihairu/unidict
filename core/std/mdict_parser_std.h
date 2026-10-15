// Qt-free MDict parser (std-only). Supports multiple non-encrypted container
// layouts and best-effort SimpleXOR decryption (password via env).

#ifndef UNIDICT_MDICT_PARSER_STD_H
#define UNIDICT_MDICT_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include "dictionary_parser_std.h"
#include "mdict_decryptor_std.h"

namespace UnidictCoreStd {

class MdictParserStd : public DictionaryParserStd {
public:
    MdictParserStd();

    bool load_dictionary(const std::string& mdx_path) override;
    bool is_loaded() const override;

    std::string dictionary_name() const override;
    std::string dictionary_description() const override;
    int word_count() const;

    std::string lookup(const std::string& word) const override; // empty if not found
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const override;

    // P1 text-norm：换键归一化档位。作废 folded_ 缓存，下次查询按新档位重建
    void set_norm_options(const TextNorm::Options& opt) override;

private:
    bool load_companion_mdd(const std::string& mdx_path);
    bool load_resource_manifest();
    bool extract_and_cache_resources_from_mdd(const std::string& mdd_path);
    std::string render_entry_for_ui(const std::string& word, const std::string& definition) const;

    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::string encoding_;
    std::string compression_;
    std::string version_;
    bool encrypted_ = false;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;
    // fold_key 回退索引（惰建缓存）：fold_key(词头) -> 词头原形。层 0 查询
    // 的大小写/全角/重音变体经此命中——entries_ 以原始词形为键，直接
    // find 只有精确串能中（与 JsonParserStd::lookup 同口径）。mutable：
    // lookup 是 const 的，构建动作是缓存填充而非可观测状态变更。
    mutable std::unordered_map<std::string, std::string> folded_;
    mutable bool fold_dirty_ = true;
    // P1 text-norm：folded_ 缓存按此档位生成，换档置 dirty 重建
    TextNorm::Options norm_options_{};

    // Resource support (.mdd): best-effort extraction to cache directory.
    std::string dict_dir_;
    std::string resource_cache_root_;
    std::unordered_map<std::string, std::string> resource_file_by_key_; // normalized key -> absolute cached file path

    // Decryption support
    std::unique_ptr<MdictDecryptorStd> decryptor_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_MDICT_PARSER_STD_H
