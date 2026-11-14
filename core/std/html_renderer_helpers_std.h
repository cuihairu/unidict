// html_renderer_helpers_std.h —— html 渲染与资源解析两个 TU 共用的内部
// 小助手。仅限 core/std 内部实现使用，不是对外 API；inline 定义免单建 .cpp。

#ifndef UNIDICT_HTML_RENDERER_HELPERS_STD_H
#define UNIDICT_HTML_RENDERER_HELPERS_STD_H

#include <string>

namespace UnidictCoreStd {
namespace html_detail {

// file:// 链接发射统一归一（平台债③根因收口）：资源本地路径来自平台
// fs（Windows 反斜杠、盘符无前导 /），直接拼接会「路径分隔符进链接」
// ——分隔符转 /、非 / 开头补 /；POSIX 路径原样透传。与 mdict 侧
// make_file_url 同口径（mdict_parser_std.cpp）
inline std::string file_url_from_local_path(const std::string& local_path) {
    std::string p;
    p.reserve(local_path.size() + 1);
    for (char c : local_path) p += (c == '\\') ? '/' : c;
    if (!p.empty() && p[0] != '/') p.insert(p.begin(), '/');
    return "file://" + p;
}

} // namespace html_detail
} // namespace UnidictCoreStd

#endif // UNIDICT_HTML_RENDERER_HELPERS_STD_H
