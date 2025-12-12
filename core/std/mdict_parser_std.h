// Qt-free MDict parser (std-only). Supports multiple non-encrypted container
// layouts and best-effort SimpleXOR decryption (password via env).

#ifndef UNIDICT_MDICT_PARSER_STD_H
#define UNIDICT_MDICT_PARSER_STD_H

#include <string>
#include <unordered_map>
#include <vector>
#include "mdict_decryptor_std.h"

namespace UnidictCoreStd {

class MdictParserStd {
public:
    MdictParserStd();

    bool load_dictionary(const std::string& mdx_path);
    bool is_loaded() const;

    std::string dictionary_name() const;
    std::string dictionary_description() const;
    int word_count() const;

    std::string lookup(const std::string& word) const; // empty if not found
    std::vector<std::string> find_similar(const std::string& word, int max_results) const;
    std::vector<std::string> all_words() const;

private:
    bool loaded_ = false;
    std::string name_;
    std::string desc_;
    std::string encoding_;
    std::string compression_;
    std::string version_;
    bool encrypted_ = false;
    std::unordered_map<std::string, std::string> entries_;
    std::vector<std::string> words_;

    // Decryption support
    std::unique_ptr<MdictDecryptorStd> decryptor_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_MDICT_PARSER_STD_H
