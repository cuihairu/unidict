// Qt-free dictionary parser interface (P-3 3.4).
// The uniform surface the std manager consumes. Concrete std parsers
// inherit it; the parser registry (parser_registry_std.h) maps extensions
// to factories of this interface — Developer dictionaries register their
// own parsers through the same surface.

#ifndef UNIDICT_DICTIONARY_PARSER_STD_H
#define UNIDICT_DICTIONARY_PARSER_STD_H

#include <string>
#include <vector>

#include "text_norm_std.h"

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

    // 键归一化档位（P1 text-norm UI）：带折叠回退缓存的解析器（json/
    // stardict/mdict）覆写后作废缓存按新档位重建；其余实现为 no-op。
    // 换档后不换词表，只换键——原形直查路径不受影响
    virtual void set_norm_options(const TextNorm::Options&) {}
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_PARSER_STD_H