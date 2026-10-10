// StarDict 资源表（图片/音频）：与 mdd_resource_std 对称的装载面，但
// StarDict 资源是纯文件（无二进制容器），解析器只做「键 → 文件路径」
// 映射。发现规则（StarDict 3.0 约定 + 真实世界散装形态）：
//   1. .ifo 的 res 键：
//      - 值是目录 → 递归收录，键 = 相对路径（POSIX 分隔符）
//      - 值是文件 → 逐行清单（每行一个文件名，可含子目录；空行与
//        # 注释跳过），路径 = 词典目录/行
//   2. 无 res 键 → 散装兜底：词典目录直属媒体文件（图片/音频扩展名，
//      .ifo/.idx/.dict/.dict.dz/.syn 骨架除外）+ 命名目录 res/ 递归
//      （真实世界词典常见 res/ 目录无键形态）
// 键归一口径与 MddResourceParser::normalize_key 逐条对齐（反斜杠、
// 前导斜杠、协议前缀、?query、#fragment、小写）。

#ifndef UNIDICT_STARDICT_RESOURCE_STD_H
#define UNIDICT_STARDICT_RESOURCE_STD_H

#include <string>
#include <unordered_map>

namespace UnidictCoreStd {

class StarDictResourceParser {
public:
    // ifo_path = 该词典的 .ifo 路径（资源发现以它所在目录为词典目录）
    bool load(const std::string& ifo_path);
    bool is_loaded() const { return loaded_; }

    // 归一化键查资源；命中返回词典目录下的真实文件路径，未命中空串
    std::string resource_path(const std::string& key) const;
    bool has_resource(const std::string& key) const;
    size_t resource_count() const { return resources_.size(); }

    // 与 MddResourceParser::normalize_key 同口径的键归一
    static std::string normalize_key(const std::string& key);

private:
    bool load_from_ifo(const std::string& ifo_path);
    bool load_res_directory(const std::string& dir_path);
    bool load_res_list_file(const std::string& list_path);
    void scan_implicit_media(const std::string& dict_dir);

    std::unordered_map<std::string, std::string> resources_;  // 归一键 → 文件路径
    bool loaded_ = false;
};

} // namespace UnidictCoreStd

#endif // UNIDICT_STARDICT_RESOURCE_STD_H
