#include "stardict_resource_std.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

static inline std::string rstrip_cr(std::string s) {
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

static inline std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t");
    if (b == std::string::npos) return {};
    size_t e = s.find_last_not_of(" \t");
    return s.substr(b, e - b + 1);
}

static std::string read_file_to_string(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string StarDictResourceParser::normalize_key(const std::string& key) {
    std::string result = key;

    // 反斜杠转正斜杠（Windows 形态的词典目录）
    std::replace(result.begin(), result.end(), '\\', '/');

    // 去前导斜杠
    size_t start = result.find_first_not_of("/");
    if (start != std::string::npos) result = result.substr(start);

    // 去协议前缀（与 .mdd 侧同清单：真实词条里 sound:// 直指资源文件）
    static const char* kPrefixes[] = {
        "file://", "sound://", "entry://", "bword://", "gxres://", "mdd://"
    };
    for (const char* prefix : kPrefixes) {
        std::string lower_prefix(prefix);
        std::transform(lower_prefix.begin(), lower_prefix.end(),
                       lower_prefix.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        std::string lower_result = result.substr(0, lower_prefix.size());
        std::transform(lower_result.begin(), lower_result.end(),
                       lower_result.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (lower_result == lower_prefix) {
            result = result.substr(lower_prefix.size());
            break;
        }
    }

    // 去 ?query 与 #fragment
    size_t query_pos = result.find('?');
    if (query_pos != std::string::npos) result = result.substr(0, query_pos);
    size_t frag_pos = result.find('#');
    if (frag_pos != std::string::npos) result = result.substr(0, frag_pos);

    // 小写（大小写不敏感文件系统口径与 .mdd 侧一致）
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return result;
}

bool StarDictResourceParser::load(const std::string& ifo_path) {
    resources_.clear();
    loaded_ = false;
    if (!load_from_ifo(ifo_path)) return false;
    loaded_ = true;
    return true;
}

bool StarDictResourceParser::load_from_ifo(const std::string& ifo_path) {
    const fs::path ifo(ifo_path);
    const fs::path dict_dir = ifo.parent_path();

    // .ifo 逐行找 res 键（键名大小写不包容——真实 .ifo 全小写键名，
    // 与 stardict_parser_std 的 lcase(key) 口径一致）
    std::string res_value;
    {
        std::istringstream is(read_file_to_string(ifo_path));
        std::string line;
        while (std::getline(is, line)) {
            line = rstrip_cr(std::move(line));
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;
            // 键名 trim：真实 .ifo 多为 "key=value" 无空格，但 "res = pics"
            // 带空格形态存在（Windows 工具产出），不 trim 会漏键
            std::string key = trim(line.substr(0, eq));
            std::transform(key.begin(), key.end(), key.begin(),
                           [](unsigned char c) { return (char)std::tolower(c); });
            if (key == "res") {
                res_value = trim(line.substr(eq + 1));
                break;
            }
        }
    }

    if (!res_value.empty()) {
        // res 键形态：目录 → 递归；文件 → 逐行清单
        fs::path target = dict_dir / res_value;
        std::error_code ec;
        if (fs::is_directory(target, ec)) {
            return load_res_directory(target.string());
        }
        if (fs::is_regular_file(target, ec)) {
            return load_res_list_file(target.string());
        }
        // res 键指向的目标不存在：显式声明优先，不静默兜底（可预期性）
        return true;
    }

    // 无 res 键 → 散装兜底
    scan_implicit_media(dict_dir.string());
    return true;
}

bool StarDictResourceParser::load_res_directory(const std::string& dir_path) {
    std::error_code ec;
    // 递归收录（res 目录形态常见多层子目录：res/sub/b.jpg）
    fs::recursive_directory_iterator it(dir_path, ec);
    if (ec) return false;
    const fs::path res_root = fs::path(dir_path);
    for (; it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) return false;
        const fs::path& p = it->path();
        if (!it->is_regular_file()) continue;
        // 双键索引：条目里的 <img src> 相对**资源根**（res 目录）书写
        //（StarDict 资源引用约定 → "a.png"/"sub/b.jpg"），而含 res 目录名
        // 的形态（清单文件逐行路径 / Goldendict 部分词典）也并存——两个
        // 键空间的碰撞只在文件真名以 res 目录名为前缀的病态形态出现
        std::string rel_res = fs::relative(p, res_root, ec).generic_string();
        if (rel_res.empty() || ec) continue;
        resources_[normalize_key(rel_res)] = p.string();
        std::string rel_dict =
            fs::relative(p, res_root.parent_path(), ec).generic_string();
        if (!rel_dict.empty() && !ec) {
            resources_[normalize_key(rel_dict)] = p.string();
        }
    }
    return true;
}

bool StarDictResourceParser::load_res_list_file(const std::string& list_path) {
    const fs::path dict_dir = fs::path(list_path).parent_path();
    std::ifstream in(list_path, std::ios::binary);
    if (!in) return false;
    std::string line;
    while (std::getline(in, line)) {
        line = rstrip_cr(std::move(line));
        const std::string name = trim(line);
        if (name.empty() || name[0] == '#') continue;
        const fs::path target = dict_dir / name;
        std::error_code ec;
        if (!fs::is_regular_file(target, ec)) continue;
        // 清单行即键（保持原相对形态，归一在查询侧）
        resources_[normalize_key(name)] = target.string();
    }
    return true;
}

void StarDictResourceParser::scan_implicit_media(const std::string& dict_dir) {
    static const char* kMediaExts[] = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp",
        ".wav", ".mp3", ".ogg", ".oga", ".m4a", ".flac", ".aac"
    };
    auto is_media = [&](const fs::path& p) {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        for (const char* e : kMediaExts) if (ext == e) return true;
        return false;
    };
    // 词典骨架文件不参与散装兜底（即便理论上是媒体扩展名）
    auto is_skeleton = [&](const std::string& fname) {
        static const char* kSkeleton[] = {".ifo", ".idx", ".dict", ".dict.dz", ".syn"};
        std::string lower = fname;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        for (const char* s : kSkeleton) {
            const size_t n = std::char_traits<char>::length(s);
            if (lower.size() >= n &&
                lower.compare(lower.size() - n, n, s) == 0) return true;
        }
        return false;
    };

    std::error_code ec;
    fs::directory_iterator it(dict_dir, ec);
    if (ec) return;
    for (; it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) return;
        const fs::path& p = it->path();
        if (it->is_regular_file()) {
            if (is_media(p) && !is_skeleton(p.filename().string())) {
                resources_[normalize_key(p.filename().string())] = p.string();
            }
        } else if (it->is_directory() && p.filename() == "res") {
            // res/ 命名目录无键形态：递归收录
            load_res_directory(p.string());
        }
    }
}

bool StarDictResourceParser::has_resource(const std::string& key) const {
    return resources_.find(normalize_key(key)) != resources_.end();
}

std::string StarDictResourceParser::resource_path(const std::string& key) const {
    auto it = resources_.find(normalize_key(key));
    if (it == resources_.end()) return {};
    // 出口绝对化：词典可能从相对路径装载（src_paths 原样存），调用方
    // （QML file:// 重写）需要绝对路径；词典目录下的真实路径语义不变
    return fs::absolute(it->second).string();
}

} // namespace UnidictCoreStd
