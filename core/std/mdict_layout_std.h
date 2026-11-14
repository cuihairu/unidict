// mdict_layout_std.h —— MDICT 布局/编解码内部面（LAY-006 自
// mdict_parser_std.cpp 拆出）：KIDX/RDEF、KEYB/RECB、KBIX/RBIX、
// MDXK/MDXR 各布局编解码、启发式兜底与头部/键规范化小助手。仅限
// core/std 内部实现使用，不是对外 API；定义在 mdict_layout_std.cpp，
// 解析 TU 经 using 引入保持调用点原样。

#ifndef UNIDICT_MDICT_LAYOUT_STD_H
#define UNIDICT_MDICT_LAYOUT_STD_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace UnidictCoreStd {
namespace mdict_detail {

inline uint64_t fnv1a64(const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

inline std::string lcase_ascii(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

inline std::string url_encode_component(const std::string& s) {
    std::ostringstream out;
    out << std::hex << std::uppercase;
    for (unsigned char c : s) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            out << (char)c;
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << (int)c;
        }
    }
    return out.str();
}

inline std::string make_file_url(const std::string& abs_path) {
    std::string p = abs_path;
    // Windows 路径分隔符归一：能走到这里的绝对路径全部来自 manifest rel /
    // dict_dir 拼接，而二者都经过 sanitize_relative_path（反斜杠已转 /），
    // POSIX 构建下 c=='\\' 恒假，分支不可达（mdict 分支缺口收口时标注）
    for (auto& c : p) if (c == '\\') c = '/';  // GCOVR_EXCL_LINE
    std::ostringstream out;
    out << "file://";
    // 本函数只收 fs::absolute 的产物（manifest rel / dict_dir 拼接后再取绝对
    // 路径），POSIX 下恒非空且以 / 开头，补 '/' 的两个真侧分支不可达
    // （mdict 分支缺口收口时标注）
    if (!p.empty() && p[0] != '/') out << '/';  // GCOVR_EXCL_LINE
    for (unsigned char c : p) {
        if (c == ' ') out << "%20";
        // 资源键在 normalize_resource_key 处已按 ?# 截断，缓存/词典目录两
        // 层来源的路径都不可能再含 '#'，此臂不可达（mdict 分支缺口收口时标注）
        else if (c == '#') out << "%23";  // GCOVR_EXCL_LINE
        else out << (char)c;
    }
    return out.str();
}

inline std::string normalize_resource_key(std::string s) {
    if (auto q = s.find_first_of("?#"); q != std::string::npos) s.resize(q);
    auto strip_prefix = [&](std::string_view pre) {
        if (s.size() >= pre.size() && std::equal(pre.begin(), pre.end(), s.begin(),
            [](char a, char b){ return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); })) {
            s.erase(0, pre.size());
        }
    };
    strip_prefix("file://");
    strip_prefix("sound://");
    strip_prefix("entry://");
    strip_prefix("bword://");

    while (!s.empty() && (s[0] == '/' || s[0] == '\\')) s.erase(0, 1);
    for (auto& c : s) if (c == '\\') c = '/';
    while (s.size() >= 2 && s[0] == '.' && s[1] == '/') s.erase(0, 2);
    return s;
}

inline std::string sanitize_relative_path(const std::string& key) {
    std::string n = normalize_resource_key(key);
    std::vector<std::string> parts;
    size_t i = 0;
    while (i <= n.size()) {
        size_t j = n.find('/', i);
        std::string part = n.substr(i, (j == std::string::npos) ? std::string::npos : (j - i));
        if (part.empty() || part == ".") {
            // skip
        } else if (part == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            for (auto& c : part) if (c == ':') c = '_';
            parts.push_back(part);
        }
        if (j == std::string::npos) break;
        i = j + 1;
    }
    std::string out;
    for (size_t k = 0; k < parts.size(); ++k) {
        if (k) out.push_back('/');
        out += parts[k];
    }
    return out;
}

inline std::string read_head(const std::string& path, size_t max_bytes = 256 * 1024) {
    // 调用点（load_dictionary / companion 提取）刚刚用 fs::exists 验证过
    // 同一路径，要让这次 open 失败只能靠"验证与打开之间文件被删"，
    // 单线程测试无法构造（同下方 load_dictionary 内 795 处的既定口径）
    std::ifstream in(path, std::ios::binary);  // GCOVR_EXCL_LINE
    if (!in) return {};  // GCOVR_EXCL_LINE
    std::string buf; buf.resize(max_bytes);
    in.read(buf.data(), (std::streamsize)max_bytes);
    buf.resize((size_t)in.gcount());
    return buf;
}

inline std::string utf16_bom_to_utf8(const std::string& bytes) {
    // UTF-16LE/BE BOM header → UTF-8：BMP 全量 + 代理对，非法代理单元
    // 吸收（与 mdx_v2_reader 侧同语义）。真实词典标题/描述大量非 ASCII
    //（中文市场词典 Title 惯例），此前 ASCII-only 截断把词典名退回
    // 文件名；属性名恒为 ASCII，值侧放开不影响键读取。
    if (bytes.size() < 2) return {};
    bool le = false, be = false;
    if ((unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE) le = true; // LE BOM
    else if ((unsigned char)bytes[0] == 0xFE && (unsigned char)bytes[1] == 0xFF) be = true; // BE BOM
    if (!le && !be) return {};
    auto unit = [&](size_t i) -> unsigned int {
        const unsigned char b1 = (unsigned char)bytes[i];
        const unsigned char b2 = (unsigned char)bytes[i + 1];
        return le ? ((static_cast<unsigned int>(b2) << 8) | b1)
                  : ((static_cast<unsigned int>(b1) << 8) | b2);
    };
    std::string out;
    auto push_cp = [&](unsigned int cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    };
    for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
        const unsigned int cp = unit(i);
        if (cp == 0) continue; // ignore nulls
        if (cp >= 0xD800 && cp <= 0xDBFF) { // 高位代理：配对则合一，孤立吸收
            if (i + 3 < bytes.size()) {
                const unsigned int lo = unit(i + 2);
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    push_cp(0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00));
                    i += 2;
                    continue;
                }
            }
            continue;
        }
        if (cp >= 0xDC00 && cp <= 0xDFFF) continue; // 孤立低位代理：吸收
        push_cp(cp);
    }
    return out;
}

bool parse_simple_kv(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_kidx_rdef(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_keyb_recb(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_kbix_rbix(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_kbix_multirb(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_mdxk_mdxr(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
bool parse_mdx_heuristic_real(const std::string& buf, std::unordered_map<std::string,std::string>& entries, std::vector<std::string>& words);
std::string extract_attr(const std::string& s, const char* key);
const char* get_mdict_password_env();
bool parse_mdict_body_from_file_best_effort(const std::string& path, std::unordered_map<std::string, std::string>& entries, std::vector<std::string>& words);

} // namespace mdict_detail
} // namespace UnidictCoreStd

#endif // UNIDICT_MDICT_LAYOUT_STD_H
