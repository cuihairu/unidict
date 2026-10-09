// 本地词典导出/打包（roadmap Dictionary Management 面）：把指定已装载
// 词典打包成项目自定义 JSON 词典格式（与 examples/dict.json 同构，
// JsonParserStd 可回读）。全保真 round-trip 由解析器侧受限转义解码 +
// 字符串感知对象扫描保证（json_parser_std.cpp 同日配套）。

#ifndef UNIDICT_DICTIONARY_EXPORT_STD_H
#define UNIDICT_DICTIONARY_EXPORT_STD_H

#include <string>

#include "dictionary_manager_std.h"

namespace UnidictCoreStd {

struct DictionaryExportResultStd {
    bool ok = false;
    std::string error;    // ok=false 时的可读原因
    int entry_count = 0;  // ok=true 时写入的词条数
};

// 导出指定词典到 out_path（父目录不存在则创建）。禁用词典同样导出
// （数据面操作，不走查询过滤）；未知词典名 → ok=false。description
// 取 dictionaries_meta 同源（解析器 description()）。
DictionaryExportResultStd export_dictionary_json(const DictionaryManagerStd& manager,
                                                 const std::string& dict_name,
                                                 const std::string& out_path);

} // namespace UnidictCoreStd

#endif // UNIDICT_DICTIONARY_EXPORT_STD_H
