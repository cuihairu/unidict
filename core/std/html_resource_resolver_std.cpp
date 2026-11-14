// HTML 资源解析器与工厂实现（LAY-005 自 html_renderer_std.cpp 拆出）——
// DefaultResourceResolverStd（.mdd/目录资源 → file:// 解析 + data URL 缓存）、
// HtmlRendererFactory 预设与 ResourceResolverStd 基类默认实现独立成 TU；
// 渲染 TU 只留 HTML 清洗/分词/高亮主链。类声明仍在 html_renderer_std.h。

#include "html_renderer_std.h"
#include "html_renderer_helpers_std.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace UnidictCoreStd {

// ============================================================================
// DefaultResourceResolverStd Implementation
// ============================================================================

DefaultResourceResolverStd::DefaultResourceResolverStd() {
    // Set default cache directory
    const char* home = std::getenv("HOME");
    if (home) {
        cache_dir_ = std::string(home) + "/.cache/unidict/resources";
    } else {
        cache_dir_ = "/tmp/unidict_cache";
    }
}

ResourceResolverStd::ResourceInfo DefaultResourceResolverStd::resolve(
    const std::string& url, const std::string& dictionary_id) {

    ResourceInfo info;

    if (!is_dictionary_resource(url)) {
        return info;
    }

    std::string key = extract_resource_key(url);
    std::string normalized_key = normalize_key(key);

    // Find dictionary resource directory
    auto dict_it = dictionary_resources_.find(dictionary_id);
    if (dict_it == dictionary_resources_.end()) {
        return info;
    }

    // Try to find resource file
    std::string resource_path = find_resource_file(normalized_key, dictionary_id);
    // 同名目录不是资源：find_resource_file 用 ifstream 探测，而在这类
    // 文件系统上目录也能"打开"（tellg 报 INT64_MAX）——info.size 变成
    // 巨值后 get_data_url 会按它分配，直接 bad_alloc
    std::error_code fs_ec;
    if (!resource_path.empty() &&
        !std::filesystem::is_regular_file(resource_path, fs_ec)) {
        resource_path.clear();
    }
    if (!resource_path.empty()) {
        info.local_path = resource_path;
        info.is_cached = true;

        // Get file info
        std::ifstream file(resource_path, std::ios::binary | std::ios::ate);
        // GCOVR_EXCL_LINE：上方 840 行已确认是 regular file，且
        // find_resource_file 本身刚用 ifstream 成功打开过同一路径；
        // 单线程下重开必成，这里只是"读之前被删"的 TOCTOU 兜底
        // （与 get_data_url 里的同型守卫一致），结构不可达。
        if (file) {  // GCOVR_EXCL_LINE
            info.size = file.tellg();

            // Simple MIME type detection
            if (key.find(".png") != std::string::npos) {
                info.mime_type = "image/png";
            } else if (key.find(".jpg") != std::string::npos || key.find(".jpeg") != std::string::npos) {
                info.mime_type = "image/jpeg";
            } else if (key.find(".gif") != std::string::npos) {
                info.mime_type = "image/gif";
            } else if (key.find(".svg") != std::string::npos) {
                info.mime_type = "image/svg+xml";
            } else if (key.find(".mp3") != std::string::npos) {
                info.mime_type = "audio/mpeg";
            } else if (key.find(".wav") != std::string::npos) {
                info.mime_type = "audio/wav";
            } else if (key.find(".ogg") != std::string::npos) {
                info.mime_type = "audio/ogg";
            }
        }
    }

    return info;
}

bool DefaultResourceResolverStd::exists(const std::string& url, const std::string& dictionary_id) {
    auto info = resolve(url, dictionary_id);
    return !info.local_path.empty();
}

std::string DefaultResourceResolverStd::get_data_url(const std::string& url, const std::string& dictionary_id) {
    auto info = resolve(url, dictionary_id);

    if (info.local_path.empty() || info.mime_type.empty()) {
        return "";
    }

    // Read file and encode as base64
    std::ifstream file(info.local_path, std::ios::binary);
    // GCOVR_EXCL_START：resolve() 只在 find_resource_file 成功（即该路径确实
    // 能打开）时才填 local_path，所以这里打不开只可能是"resolve 之后、
    // 读之前文件被删"，单线程测试无法构造。
    if (!file) {  // GCOVR_EXCL_LINE
        return "";  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_STOP

    std::vector<uint8_t> buffer(info.size);
    file.read(reinterpret_cast<char*>(buffer.data()), info.size);

    // Simple base64 encoding (for production, use a proper library)
    static const char* b64_table =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result = "data:" + info.mime_type + ";base64,";

    for (size_t i = 0; i < buffer.size(); i += 3) {
        uint32_t triple = (buffer[i] << 16) |
                         (i + 1 < buffer.size() ? buffer[i + 1] << 8 : 0) |
                         (i + 2 < buffer.size() ? buffer[i + 2] : 0);

        result += b64_table[(triple >> 18) & 0x3F];
        result += b64_table[(triple >> 12) & 0x3F];
        result += (i + 1 < buffer.size()) ? b64_table[(triple >> 6) & 0x3F] : '=';
        result += (i + 2 < buffer.size()) ? b64_table[triple & 0x3F] : '=';
    }

    return result;
}

bool DefaultResourceResolverStd::preload_resources(
    const std::vector<std::string>& urls, const std::string& dictionary_id) {
    // For now, this is a no-op since resources are read directly from disk
    // In the future, this could extract and cache resources from .mdd files
    return true;
}

void DefaultResourceResolverStd::clear_cache(const std::string& dictionary_id) {
    // Clear cached resources for a specific dictionary or all
    if (dictionary_id.empty()) {
        dictionary_resources_.clear();
    } else {
        dictionary_resources_.erase(dictionary_id);
    }
}

std::vector<std::string> DefaultResourceResolverStd::get_cached_resources() const {
    std::vector<std::string> resources;
    for (const auto& entry : dictionary_resources_) {
        resources.push_back(entry.first);
    }
    return resources;
}

bool DefaultResourceResolverStd::is_dictionary_resource(const std::string& url) const {
    // Check for common dictionary resource patterns
    if (url.find("://") != std::string::npos) {
        std::string lower = url;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        // Allow entry:// for cross-references, but not for resources
        if (lower.find("entry://") == 0) {
            return false;
        }
        // data:, http:, https: are external resources
        return true;
    }
    // Relative URLs are assumed to be dictionary resources
    return true;
}

std::string DefaultResourceResolverStd::extract_resource_key(const std::string& url) const {
    // Remove protocol if present
    size_t proto_pos = url.find("://");
    if (proto_pos != std::string::npos) {
        return url.substr(proto_pos + 3);
    }
    return url;
}

void DefaultResourceResolverStd::register_dictionary(
    const std::string& dictionary_id, const std::string& resource_path) {
    dictionary_resources_[dictionary_id] = resource_path;
}

void DefaultResourceResolverStd::unregister_dictionary(const std::string& dictionary_id) {
    dictionary_resources_.erase(dictionary_id);
}

void DefaultResourceResolverStd::set_cache_directory(const std::string& cache_dir) {
    cache_dir_ = cache_dir;
}

std::string DefaultResourceResolverStd::find_resource_file(
    const std::string& key, const std::string& dictionary_id) const {

    auto dict_it = dictionary_resources_.find(dictionary_id);
    // GCOVR_EXCL_START：唯一调用方 resolve() 已经在调本函数之前查过同一个
    // map 并在缺失时早退，所以这里的"词典未注册"分支不可达。保留为私有
    // 函数的自我保护。
    if (dict_it == dictionary_resources_.end()) {  // GCOVR_EXCL_LINE
        return "";  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_STOP

    const std::string& resource_dir = dict_it->second;

    // Try exact match first
    std::string full_path = resource_dir + "/" + key;
    std::ifstream test(full_path, std::ios::binary);
    if (test) {
        return full_path;
    }

    // Try case-insensitive match
    // (Note: This requires directory listing; for now, just try common variations)
    std::string lower_key = key;
    std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);

    // Try with common extensions
    static const char* image_exts[] = {".png", ".jpg", ".jpeg", ".gif", ".svg"};
    static const char* audio_exts[] = {".mp3", ".wav", ".ogg", ".m4a"};

    std::string base_name = key;
    for (const auto* ext : image_exts) {
        if (base_name.length() > strlen(ext) &&
            base_name.substr(base_name.length() - strlen(ext)) == ext) {
            base_name = base_name.substr(0, base_name.length() - strlen(ext));
            break;
        }
    }

    for (const auto* ext : image_exts) {
        full_path = resource_dir + "/" + base_name + ext;
        test.open(full_path, std::ios::binary);
        if (test) return full_path;
    }

    for (const auto* ext : audio_exts) {
        full_path = resource_dir + "/" + base_name + ext;
        test.open(full_path, std::ios::binary);
        if (test) return full_path;
    }

    return "";
}

std::string DefaultResourceResolverStd::normalize_key(const std::string& key) const {
    std::string result = key;

    // Replace backslashes with forward slashes
    std::replace(result.begin(), result.end(), '\\', '/');

    // Remove leading slashes
    size_t start = result.find_first_not_of("/");
    if (start != std::string::npos) {
        result = result.substr(start);
    }

    // URL decode basic entities
    size_t pos = 0;
    while ((pos = result.find("%20", pos)) != std::string::npos) {
        result.replace(pos, 3, " ");
        pos += 1;
    }

    return result;
}

// ============================================================================
// HtmlRendererFactory Implementation
// ============================================================================

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_with_defaults() {
    auto resolver = std::make_shared<DefaultResourceResolverStd>();
    return std::make_unique<HtmlRendererStd>(resolver);
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_with_resource_resolver(
    std::shared_ptr<ResourceResolverStd> resolver) {
    return std::make_unique<HtmlRendererStd>(resolver);
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_strict() {
    auto renderer = std::make_unique<HtmlRendererStd>();

    // Minimal allowed tags
    renderer->remove_allowed_tag("script");
    renderer->remove_allowed_tag("iframe");
    renderer->remove_allowed_tag("object");
    renderer->remove_allowed_tag("embed");
    renderer->remove_allowed_tag("form");
    renderer->remove_allowed_tag("input");
    renderer->remove_allowed_tag("button");

    return renderer;
}

std::unique_ptr<HtmlRendererStd> HtmlRendererFactory::create_permissive() {
    auto renderer = std::make_unique<HtmlRendererStd>();

    // Add more permissive tags for trusted dictionaries
    renderer->add_allowed_tag("details");
    renderer->add_allowed_tag("summary");
    renderer->add_allowed_tag("figure");
    renderer->add_allowed_tag("figcaption");

    return renderer;
}

// ============================================================================
// ResourceResolverStd Implementation
// ============================================================================

std::string ResourceResolverStd::get_data_url(const std::string& url, const std::string& dictionary_id) {
    // Default implementation: resolve to local file and encode as data URL
    // Subclasses can override for more efficient implementations
    ResourceInfo info = resolve(url, dictionary_id);
    if (!info.local_path.empty()) {
        // For now, return file:// URL instead of full data URL encoding
        // to avoid memory overhead for large resources
        return html_detail::file_url_from_local_path(info.local_path);
    }
    return "";
}

bool ResourceResolverStd::preload_resources(const std::vector<std::string>& urls, const std::string& dictionary_id) {
    // Default implementation: do nothing
    (void)urls;
    (void)dictionary_id;
    return true;
}

void ResourceResolverStd::clear_cache(const std::string& dictionary_id) {
    // Default implementation: do nothing
    (void)dictionary_id;
}

std::vector<std::string> ResourceResolverStd::get_cached_resources() const {
    // Default implementation: return empty list
    return {};
}

bool ResourceResolverStd::is_dictionary_resource(const std::string& url) const {
    // Default implementation: check for common patterns
    return url.find("://") == std::string::npos;  // No protocol = likely local resource
}

std::string ResourceResolverStd::extract_resource_key(const std::string& url) const {
    // Default implementation: return URL as-is
    return url;
}
} // namespace UnidictCoreStd
