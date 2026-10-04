// Qt-free dictionary instance: one loaded dictionary (parser + metadata +
// state + attached resources). The manager holds only this type and never
// touches parser internals.

#ifndef UNIDICT_DICTIONARY_STD_H
#define UNIDICT_DICTIONARY_STD_H

#include <memory>
#include <string>
#include <vector>

#include "json_parser_std.h"
#include "stardict_parser_std.h"
#include "mdict_parser_std.h"
#include "dsl_parser_std.h"
#include "csv_parser_std.h"
#include "epub_parser_std.h"
#include "mdd_resource_std.h"

namespace UnidictCoreStd {

// 一个已加载的词典实例：解析器 + 元数据（name/words/src_paths）+
// 管理状态（enabled/priority/tags，排序与过滤在 manager 侧消费）+
// 同名 .mdd 资源解析器。
//
// load() 按扩展名分派六个 std 解析器之一并加载词典体；伴生文件路径
// 进入 src_paths（全文索引签名绑定用）。合法用途是只在 load() 成功
// 后持有实例（manager 即如此），但默认构造（无解析器）也可安全调用
// lookup/description——空实例由测试直接覆盖。
class DictionaryStd {
public:
    DictionaryStd() = default;

    // 按扩展名分派解析器；失败返回 false（实例不可用，调用方丢弃）。
    // .mdx 的同名 .mdd 会尝试附加解析，资源损坏不影响词典加载。
    bool load(const std::string& path);

    const std::string& name() const { return name_; }
    const std::vector<std::string>& words() const { return words_; }
    const std::vector<std::string>& src_paths() const { return src_paths_; }

    bool enabled() const { return enabled_; }
    void set_enabled(bool v) { enabled_ = v; }
    // 优先级：数值越大越靠前（manager 排序用；默认 0）
    int priority() const { return priority_; }
    void set_priority(int v) { priority_ = v; }
    const std::vector<std::string>& tags() const { return tags_; }
    void set_tags(std::vector<std::string> v) { tags_ = std::move(v); }

    // 六解析器分派：释义与词典描述（无解析器的空实例返回空串）
    std::string lookup(const std::string& word) const;
    std::string description() const;

    // 加载成功的伴生 .mdd 资源解析器（.mdx 词典）
    const std::vector<std::unique_ptr<MddResourceParser>>& mdd_parsers() const { return mdd_parsers_; }

private:
    std::unique_ptr<JsonParserStd> json_;
    std::unique_ptr<StarDictParserStd> stardict_;
    std::unique_ptr<MdictParserStd> mdict_;
    std::unique_ptr<DslParserStd> dsl_;
    std::unique_ptr<CsvParserStd> csv_;
    std::unique_ptr<EpubParserStd> epub_;

    std::string name_;
    bool enabled_ = true;
    int priority_ = 0;
    std::vector<std::string> tags_;
    std::vector<std::string> src_paths_;  // 原始源路径（含伴生文件），签名绑定用
    std::vector<std::string> words_;
    std::vector<std::unique_ptr<MddResourceParser>> mdd_parsers_;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_STD_H