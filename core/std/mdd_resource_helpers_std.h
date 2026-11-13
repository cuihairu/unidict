// mdd_resource_helpers_std.h —— mdd_resource 解析与缓存两个 TU 共用的
// 内部小助手。仅限 core/std 内部实现使用，不是对外 API；inline 定义
// 免去单独 .cpp。

#ifndef UNIDICT_MDD_RESOURCE_HELPERS_STD_H
#define UNIDICT_MDD_RESOURCE_HELPERS_STD_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace UnidictCoreStd {
namespace mdd_detail {

// FNV-1a-64 摘要：超长缓存键压名用——纯截断会让不同超长键挤到同一个
// 文件名（后写的覆盖先写的），摘要保证仍一一映射。
uint64_t fnv1a64(const void* data, size_t len) {
    const unsigned char* p = static_cast<const unsigned char*>(data);
    uint64_t h = 1469598103934665603ULL;
    for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

// 取扩展名（含点），异常长的（>12 字节，说明键本身畸形）当没有
std::string file_extension(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || name.size() - dot > 12) {
        return {};
    }
    return name.substr(dot);
}

} // namespace mdd_detail
} // namespace UnidictCoreStd

#endif // UNIDICT_MDD_RESOURCE_HELPERS_STD_H
