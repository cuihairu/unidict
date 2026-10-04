// Qt-free dictionary parser interface (P-3 3.4).
// The uniform surface the std manager consumes. Concrete std parsers
// inherit it; the parser registry (parser_registry_std.h) maps extensions
// to factories of this interface — Developer dictionaries register their
// own parsers through the same surface.

#ifndef UNIDICT_DICTIONARY_PARSER_STD_H
#define UNIDICT_DICTIONARY_PARSER_STD_H

#include <string>
#include <vector>

namespace UnidictCoreStd {

class DictionaryParserStd {
public:
    virtual ~DictionaryParserStd() = default;

    virtual bool load_dictionary(const std::string& path) = 0;
    virtual bool is_loaded() const = 0;
    // 未命中返回空串
    virtual std::string lookup(const std::string& word) const = 0;
    virtual std::string dictionary_name() const = 0;
    virtual std::string dictionary_description() const = 0;
    virtual std::vector<std::string> all_words() const = 0;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_PARSER_STD_H