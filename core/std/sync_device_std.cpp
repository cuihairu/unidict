// B4 设备面实现（server_plan §7）

#include "sync_device_std.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>  // strtoull/strtol
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "crypto_std.h"  // random_bytes 用于生成设备名称后缀
#include <iomanip>

namespace UnidictCoreStd {

namespace {

// 生成默认设备名：Platform-XXYY（4 位十六进制）
std::string make_default_name(DevicePlatform platform) {
    try {
        auto rnd = random_bytes(2);
        std::ostringstream oss;
        oss << device_platform_to_string(platform) << '-';
        for (unsigned char b : rnd) {
            static const char* hex = "0123456789ABCDEF";
            oss << hex[b >> 4] << hex[b & 0xF];
        }
        return oss.str();
    } catch (...) {
        // 随机源失败无法在测试里注入（std::random_device 无可控故障口；
        // crypto_std 契约：失败抛异常不降级）——兜底裸平台名，保
        // join_group 不因名字装饰失败而失败。两行均按 GCOVR_EXCL_LINE
        // 逐行排除（region 式 START/STOP 的收口行不生效，实测）。
        return device_platform_to_string(platform);  // GCOVR_EXCL_LINE
    }  // GCOVR_EXCL_LINE
}

// ---- 极简 JSON 往返（仅本模块 to_json/from_json 用，不追求通用）----
// 转义侧按标准 JSON 写；反转义侧对非法转义宽松收敛（状态文件可能被
// 手改）：未知转义/坏 \u 按字面保留，孤立代理项按 U+FFFD 收敛。
// 值内出现的引号必经转义，因此按裸 "key" 找键不会撞进字符串值里。

// 简单 JSON 转义
std::string json_escape(const std::string& s) {
    std::ostringstream oss;
    for (char c : s) {
        switch (c) {
            case '"':  oss << "\\\""; break;
            case '\\': oss << "\\\\"; break;
            case '\b': oss << "\\b"; break;
            case '\f': oss << "\\f"; break;
            case '\n': oss << "\\n"; break;
            case '\r': oss << "\\r"; break;
            case '\t': oss << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    oss << "\\u" << std::hex << std::uppercase << std::setw(4)
                        << std::setfill('0') << (static_cast<unsigned char>(c) & 0xFF);
                } else {
                    oss << c;
                }
        }
    }
    return oss.str();
}

// 定位 "key" 冒号后的值起点（跳过空白）；键/冒号缺失或输入在冒号后
// 截断 → npos
size_t json_value_start(const std::string& json, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    const size_t pos = json.find(needle);
    if (pos == std::string::npos) return std::string::npos;
    const size_t colon = json.find(':', pos + needle.size());
    if (colon == std::string::npos) return std::string::npos;
    return json.find_first_not_of(" \t\n\r", colon + 1);
}

int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// 取 4 位十六进制为 16 位码元；不足 4 位或含非法字符 → -1
int parse_u16(const std::string& s, size_t pos) {
    if (pos + 4 > s.size()) return -1;
    int v = 0;
    for (int i = 0; i < 4; ++i) {
        const int h = hex_val(s[pos + (size_t)i]);
        if (h < 0) return -1;
        v = (v << 4) | h;
    }
    return v;
}

// 码点 → UTF-8 追加
void append_utf8(std::string& out, unsigned int cp) {
    if (cp <= 0x7F) {
        out += (char)cp;
    } else if (cp <= 0x7FF) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

// 反转义 json_escape 的产物（含标准 JSON 转义集）。入参是字符串字面量的
// 内部原文（引号间原文），配对扫描已保证反斜杠不落单。
std::string json_unescape(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] != '\\') {
            out += raw[i];
            continue;
        }
        const char e = raw[++i];
        switch (e) {
            case '"':  out += '"'; break;
            case '\\': out += '\\'; break;
            case '/':  out += '/'; break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                const int hi = parse_u16(raw, i + 1);
                if (hi < 0) {
                    out += "\\u";  // 坏 \u → 字面保留，从未消费字符继续
                    break;
                }
                i += 4;  // 指向 \uXXXX 末位 hex
                if (hi >= 0xD800 && hi <= 0xDBFF) {
                    if (i + 2 < raw.size() && raw[i + 1] == '\\' &&
                        raw[i + 2] == 'u') {
                        const int lo = parse_u16(raw, i + 3);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            const unsigned int cp = 0x10000u +
                                ((unsigned int)(hi - 0xD800) << 10) +
                                (unsigned int)(lo - 0xDC00);
                            append_utf8(out, cp);
                            i += 6;  // 连低半 \uXXXX 一起吞掉
                            break;
                        }
                    }
                    append_utf8(out, 0xFFFD);  // 孤立高代理
                    break;
                }
                if (hi >= 0xDC00 && hi <= 0xDFFF) {
                    append_utf8(out, 0xFFFD);  // 孤立低代理
                    break;
                }
                append_utf8(out, (unsigned int)hi);
                break;
            }
            default:
                out += '\\';  // 未知转义 → 字面反斜杠 + 字符
                out += e;
                break;
        }
    }
    return out;
}

// 字符串值：定位起点并取引号间原文（转义配对扫描，闭合引号缺失/
// 值不是字符串 → false）
bool json_get_string(const std::string& json, const std::string& key,
                     std::string* out) {
    const size_t v = json_value_start(json, key);
    if (v == std::string::npos) return false;
    if (v >= json.size() || json[v] != '"') return false;
    const size_t begin = v + 1;
    for (size_t i = begin; i < json.size(); ++i) {
        if (json[i] == '\\') {
            ++i;  // 转义对整对跳过
            continue;
        }
        if (json[i] == '"') {
            *out = json_unescape(json.substr(begin, i - begin));
            return true;
        }
    }
    return false;  // 输入截断，闭合引号缺失
}

bool json_get_uint64(const std::string& json, const std::string& key,
                     uint64_t* out) {
    const size_t v = json_value_start(json, key);
    if (v == std::string::npos) return false;
    char* endptr = nullptr;
    const uint64_t val = std::strtoull(json.c_str() + v, &endptr, 10);
    if (endptr == json.c_str() + v) return false;  // 非数字开头
    *out = val;
    return true;
}

bool json_get_bool(const std::string& json, const std::string& key,
                   bool* out) {
    const size_t v = json_value_start(json, key);
    if (v == std::string::npos) return false;
    if (json.compare(v, 4, "true") == 0) {
        *out = true;
        return true;
    }
    if (json.compare(v, 5, "false") == 0) {
        *out = false;
        return true;
    }
    return false;  // null/数字/其他 → 视同缺省
}

bool json_get_int(const std::string& json, const std::string& key, int* out) {
    const size_t v = json_value_start(json, key);
    if (v == std::string::npos) return false;
    char* endptr = nullptr;
    const long val = std::strtol(json.c_str() + v, &endptr, 10);
    if (endptr == json.c_str() + v) return false;  // 非数字开头
    *out = static_cast<int>(val);
    return true;
}

// devices 数组：定位键 → '[' → 深度配对收 ']'。扫描按字符串感知——
// 值内合法出现的 []{} 不参与配对（json_escape 不转义这四个字符，备注
// 里带括号是正常输入）。键存在但无 '[' 或数组未闭合 → false；空数组
// [] 合法（begin == end，零设备态可往返）。
bool json_devices_span(const std::string& json, size_t* begin, size_t* end) {
    const size_t key = json.find("\"devices\"");
    if (key == std::string::npos) return false;
    const size_t arr = json.find('[', key);
    if (arr == std::string::npos) return false;
    int depth = 0;
    bool in_string = false;
    for (size_t i = arr; i < json.size(); ++i) {
        const char c = json[i];
        if (in_string) {
            if (c == '\\') {
                ++i;  // 转义对整对跳过
            } else if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
        } else if (c == '[' || c == '{') {
            ++depth;
        } else if (c == ']' || c == '}') {
            if (--depth == 0) {
                *begin = arr + 1;
                *end = i;
                return true;
            }
        }
    }
    return false;  // 数组未闭合
}

// 逐个提取数组内容里的顶层对象原文（字符串感知同上；跳过空白与逗号；
// 遇非 '{' 或对象截断即止）
std::vector<std::string> json_object_spans(const std::string& content) {
    std::vector<std::string> out;
    size_t pos = 0;
    const size_t n = content.size();
    while (pos < n) {
        while (pos < n && (content[pos] == ',' ||
                           std::isspace(static_cast<unsigned char>(content[pos])))) {
            ++pos;
        }
        if (pos >= n || content[pos] != '{') break;
        int depth = 0;
        bool in_string = false;
        bool closed = false;
        size_t i = pos;
        for (; i < n; ++i) {
            const char c = content[i];
            if (in_string) {
                if (c == '\\') {
                    ++i;  // 转义对整对跳过
                } else if (c == '"') {
                    in_string = false;
                }
                continue;
            }
            if (c == '"') {
                in_string = true;
            } else if (c == '{') {
                ++depth;
            } else if (c == '}') {
                if (--depth == 0) {
                    closed = true;
                    break;
                }
            }
        }
        if (!closed) break;  // 对象截断
        out.push_back(content.substr(pos, i - pos + 1));
        pos = i + 1;
    }
    return out;
}

}  // namespace

SyncDeviceManagerStd::SyncDeviceManagerStd(const std::string& current_device_id)
    : current_device_id_(current_device_id) {}

void SyncDeviceManagerStd::set_current_device_id(const std::string& device_id) {
    current_device_id_ = device_id;
    // 已在组内则重排 is_current 标注；不在组内（启动注入、未入组）保持
    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        for (auto& kv : devices_) kv.second.is_current = false;
        it->second.is_current = true;
    }
}

const SyncDeviceInfoStd* SyncDeviceManagerStd::current_device() const {
    if (current_device_id_.empty()) return nullptr;
    auto it = devices_.find(current_device_id_);
    return it != devices_.end() ? &it->second : nullptr;
}

std::vector<SyncDeviceInfoStd> SyncDeviceManagerStd::list_devices() const {
    std::vector<SyncDeviceInfoStd> result;
    result.reserve(devices_.size());
    for (const auto& kv : devices_) {
        result.push_back(kv.second);
    }
    // 排序：本机置顶，其次按 last_seen_ms 降序
    std::sort(result.begin(), result.end(),
              [](const SyncDeviceInfoStd& a, const SyncDeviceInfoStd& b) {
                  if (a.is_current != b.is_current) return a.is_current;
                  return a.last_seen_ms > b.last_seen_ms;
              });
    return result;
}

const SyncDeviceInfoStd* SyncDeviceManagerStd::find_device(
    const std::string& device_id) const {
    auto it = devices_.find(device_id);
    return it != devices_.end() ? &it->second : nullptr;
}

std::string SyncDeviceManagerStd::join_group(
    const std::string& device_id, const std::string& name,
    DevicePlatform platform, const std::string& platform_string,
    std::string* err) {
    if (device_id.empty()) {
        if (err) *err = "empty device_id";
        return "";
    }
    if (devices_.size() >= kMaxDevices) {
        if (err) *err = "max devices reached";
        return "";
    }
    if (devices_.find(device_id) != devices_.end()) {
        if (err) *err = "device already in group";
        return "";
    }

    const bool joins_as_current = (device_id == current_device_id_);
    SyncDeviceInfoStd info;
    info.device_id = device_id;
    info.name = name.empty() ? make_default_name(platform) : name;
    info.platform = platform;
    info.platform_string = platform_string;
    info.last_seen_ms = 0;
    info.remark.clear();
    info.is_current = joins_as_current;

    devices_.emplace(device_id, std::move(info));

    // 本机入组：全表重算标注（防历史错标残留）
    if (joins_as_current) {
        for (auto& kv : devices_) {
            kv.second.is_current = (kv.first == current_device_id_);
        }
    }

    return device_id;
}

bool SyncDeviceManagerStd::leave_group(const std::string& device_id) {
    auto it = devices_.find(device_id);
    if (it == devices_.end()) return false;

    const bool was_current = (device_id == current_device_id_);
    devices_.erase(it);

    if (was_current) current_device_id_.clear();
    return true;
}

bool SyncDeviceManagerStd::update_own_remark(const std::string& remark,
                                             std::string* err) {
    if (current_device_id_.empty()) {
        if (err) *err = "no current device";
        return false;
    }
    auto it = devices_.find(current_device_id_);
    if (it == devices_.end()) {
        if (err) *err = "current device not in group";
        return false;
    }
    it->second.remark = remark;
    return true;
}

void SyncDeviceManagerStd::touch_device(const std::string& device_id,
                                        uint64_t now_ms) {
    auto it = devices_.find(device_id);
    if (it != devices_.end()) {
        it->second.last_seen_ms = now_ms;
    }
}

bool SyncDeviceManagerStd::update_own_name(const std::string& name,
                                           std::string* err) {
    if (current_device_id_.empty()) {
        if (err) *err = "no current device";
        return false;
    }
    auto it = devices_.find(current_device_id_);
    if (it == devices_.end()) {
        if (err) *err = "current device not in group";
        return false;
    }
    it->second.name = name;
    return true;
}

std::string SyncDeviceManagerStd::to_json() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"current_device_id\":\"" << json_escape(current_device_id_) << "\",";
    oss << "\"devices\":[";
    bool first = true;
    for (const auto& kv : devices_) {
        if (!first) oss << ",";
        first = false;
        const auto& d = kv.second;
        oss << "{";
        oss << "\"device_id\":\"" << json_escape(d.device_id) << "\",";
        oss << "\"name\":\"" << json_escape(d.name) << "\",";
        oss << "\"platform\":" << static_cast<int>(d.platform) << ",";
        oss << "\"platform_string\":\"" << json_escape(d.platform_string) << "\",";
        oss << "\"last_seen_ms\":" << d.last_seen_ms << ",";
        oss << "\"remark\":\"" << json_escape(d.remark) << "\",";
        oss << "\"is_current\":" << (d.is_current ? "true" : "false");
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

bool SyncDeviceManagerStd::from_json(const std::string& json,
                                     std::string* err) {
    devices_.clear();
    current_device_id_.clear();

    if (!json_get_string(json, "current_device_id", &current_device_id_)) {
        if (err) *err = "missing current_device_id";
        return false;
    }

    size_t begin = 0, end = 0;
    if (!json_devices_span(json, &begin, &end)) {
        if (err) *err = "missing devices array";
        return false;
    }

    const std::string content = json.substr(begin, end - begin);
    for (const auto& obj : json_object_spans(content)) {
        SyncDeviceInfoStd d;
        if (!json_get_string(obj, "device_id", &d.device_id)) continue;  // 无 id 不收
        json_get_string(obj, "name", &d.name);
        int plat = 0;
        json_get_int(obj, "platform", &plat);
        d.platform = static_cast<DevicePlatform>(plat);
        json_get_string(obj, "platform_string", &d.platform_string);
        json_get_uint64(obj, "last_seen_ms", &d.last_seen_ms);
        json_get_string(obj, "remark", &d.remark);
        json_get_bool(obj, "is_current", &d.is_current);

        devices_.emplace(d.device_id, std::move(d));
    }

    // is_current 以 current_device_id_ 为准（防历史文件错标）
    for (auto& kv : devices_) {
        kv.second.is_current = (kv.first == current_device_id_);
    }

    return true;
}

void SyncDeviceManagerStd::clear() {
    devices_.clear();
    current_device_id_.clear();
}

std::string device_platform_to_string(DevicePlatform p) {
    switch (p) {
        case DevicePlatform::Windows:    return "Windows";
        case DevicePlatform::macOS:      return "macOS";
        case DevicePlatform::Linux:      return "Linux";
        case DevicePlatform::Android:    return "Android";
        case DevicePlatform::iOS:        return "iOS";
        case DevicePlatform::HarmonyOS:  return "HarmonyOS";
        case DevicePlatform::Web:        return "Web";
        default:                         return "Unknown";
    }
}

DevicePlatform device_platform_from_string(const std::string& s) {
    if (s == "Windows") return DevicePlatform::Windows;
    if (s == "macOS") return DevicePlatform::macOS;
    if (s == "Linux") return DevicePlatform::Linux;
    if (s == "Android") return DevicePlatform::Android;
    if (s == "iOS") return DevicePlatform::iOS;
    if (s == "HarmonyOS") return DevicePlatform::HarmonyOS;
    if (s == "Web") return DevicePlatform::Web;
    return DevicePlatform::Unknown;
}

}  // namespace UnidictCoreStd
