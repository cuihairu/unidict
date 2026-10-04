// Qt-free parser factory registry (P-3 3.4): extension -> factory.
// Built-in formats register at first use; Developer dictionaries add or
// override extensions through the same surface (last registration wins).

#ifndef UNIDICT_PARSER_REGISTRY_STD_H
#define UNIDICT_PARSER_REGISTRY_STD_H

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "dictionary_parser_std.h"

namespace UnidictCoreStd {

// 解析器工厂注册表：扩展名 → 工厂。内建六格式（json/ifo/mdx/dsl/
// csv·tsv·txt/epub）在首次 instance() 时注册；Developer dictionaries
// 经 register_factory 追加新扩展名或覆盖同名扩展名（覆盖即差异化，
// 后注册者胜）。归一化：大小写不敏感、前导点可有可无。
// 非线程安全：注册仅限启动期（查询期只读）。
class ParserRegistryStd {
public:
    using Factory = std::function<std::unique_ptr<DictionaryParserStd>()>;

    static ParserRegistryStd& instance();

    // 注册/覆盖：后注册者胜。返回该扩展名先前的工厂（未注册过返回空），
    // 覆盖方可用返回值恢复原状。
    Factory register_factory(const std::string& extension, Factory factory);
    // 未注册扩展名（或注册的是空工厂）返回 nullptr
    std::unique_ptr<DictionaryParserStd> create(const std::string& extension) const;
    // 排序输出（稳定、可预期）
    std::vector<std::string> supported_extensions() const;

private:
    ParserRegistryStd();  // 内建六格式注册
    static std::string normalize(const std::string& extension);
    std::unordered_map<std::string, Factory> factories_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_PARSER_REGISTRY_STD_H