#include "parser_registry_std.h"

#include <algorithm>
#include <cctype>

#include "json_parser_std.h"
#include "stardict_parser_std.h"
#include "mdict_parser_std.h"
#include "dsl_parser_std.h"
#include "csv_parser_std.h"
#include "epub_parser_std.h"

namespace UnidictCoreStd {

ParserRegistryStd::ParserRegistryStd() {
    register_factory(".json", [] { return std::make_unique<JsonParserStd>(); });
    register_factory(".ifo", [] { return std::make_unique<StarDictParserStd>(); });
    register_factory(".mdx", [] { return std::make_unique<MdictParserStd>(); });
    register_factory(".dsl", [] { return std::make_unique<DslParserStd>(); });
    register_factory(".csv", [] { return std::make_unique<CsvParserStd>(); });
    register_factory(".tsv", [] { return std::make_unique<CsvParserStd>(); });
    register_factory(".txt", [] { return std::make_unique<CsvParserStd>(); });
    register_factory(".epub", [] { return std::make_unique<EpubParserStd>(); });
}

ParserRegistryStd& ParserRegistryStd::instance() {
    static ParserRegistryStd registry;
    return registry;
}

std::string ParserRegistryStd::normalize(const std::string& extension) {
    std::string e = extension;
    for (auto& c : e) c = (char)tolower((unsigned char)c);
    if (!e.empty() && e[0] != '.') e = "." + e;
    return e;
}

ParserRegistryStd::Factory ParserRegistryStd::register_factory(const std::string& extension, Factory factory) {
    const std::string e = normalize(extension);
    if (e.empty() || e == ".") return {};  // 空扩展名不注册
    Factory prev;
    const auto it = factories_.find(e);
    if (it != factories_.end()) prev = std::move(it->second);
    factories_[e] = std::move(factory);
    return prev;
}

std::unique_ptr<DictionaryParserStd> ParserRegistryStd::create(const std::string& extension) const {
    const auto it = factories_.find(normalize(extension));
    if (it == factories_.end()) return nullptr;
    // 注册方可能传了空工厂：等同未注册，不能调用（bad_function_call）
    if (!it->second) return nullptr;
    return it->second();
}

std::vector<std::string> ParserRegistryStd::supported_extensions() const {
    std::vector<std::string> out;
    out.reserve(factories_.size());
    for (const auto& kv : factories_) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace UnidictCoreStd