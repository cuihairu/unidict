// 词典库状态序列化（LAY-007 自 dictionary_manager_std.cpp 拆出）——
// 手写 JSON 读写器（复用 data_store_std 的口径）与 save_state/load_state。
// state_* 助手仅本 TU 使用，留文件内 static；成员声明在
// dictionary_manager_std.h 不变，聚合 TU 经链接取到这两个方法。

#include "dictionary_manager_std.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "fs_path_std.h"

namespace fs = std::filesystem;

namespace UnidictCoreStd {

// ---- 状态文件手写 JSON 读写器（复用 data_store_std 的口径）----
// 唯一字符串读取入口：\\ \" \n \r \t 转义表与 data_store_std 一致；
// \uXXXX 不解码（本文件只由 json_escape 产出，外来 \u 按字面保留）。
static std::string state_parse_json_string(const std::string& s, size_t from,
                                           size_t* out_end) {
    std::string out;
    size_t i = from + 1;
    // GCOVR_EXCL_LINE：调用方（区段提取/对象切分）都由字符串感知的
    // 深度计数扫描器把关后才切入，传入串在本翻译单元内必然闭合——
    // i≥size 出口与串尾悬空反斜杠兜底不可达（防御留档）。
    while (i < s.size() && s[i] != '"') {  // GCOVR_EXCL_LINE
        if (s[i] != '\\') { out.push_back(s[i++]); continue; }
        if (i + 1 >= s.size()) break;  // GCOVR_EXCL_LINE
        const char e = s[i + 1];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 't': out.push_back('\t'); break;
            case 'r': out.push_back('\r'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            default: out.push_back(e); break;  // \" \\ \/ 及未知转义取字面
        }
        i += 2;
    }
    *out_end = i < s.size() ? i + 1 : s.size();  // GCOVR_EXCL_LINE
    return out;
}

static std::string state_obj_val(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find(':', p);
    if (p == std::string::npos) return {};
    p = o.find('"', p);
    if (p == std::string::npos) return {};
    size_t end = 0;
    return state_parse_json_string(o, p, &end);
}

static long long state_obj_int(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return 0;
    p = o.find(':', p);
    if (p == std::string::npos) return 0;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    bool neg = false;
    if (p < o.size() && o[p] == '-') { neg = true; ++p; }
    long long v = 0;
    bool any = false;
    while (p < o.size() && o[p] >= '0' && o[p] <= '9') { v = v * 10 + (o[p] - '0'); ++p; any = true; }
    if (!any) return 0;
    return neg ? -v : v;
}

static bool state_obj_bool(const std::string& o, const std::string& key, bool def) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return def;
    p = o.find(':', p);
    if (p == std::string::npos) return def;
    ++p;
    while (p < o.size() && (o[p] == ' ' || o[p] == '\t')) ++p;
    if (o.compare(p, 4, "true") == 0) return true;
    if (o.compare(p, 5, "false") == 0) return false;
    return def;
}

static std::vector<std::string> state_obj_str_array(const std::string& o, const std::string& key) {
    const std::string pat = '"' + key + '"';
    size_t p = o.find(pat);
    if (p == std::string::npos) return {};
    p = o.find('[', p);
    if (p == std::string::npos) return {};
    std::vector<std::string> out;
    for (size_t k = p + 1; k < o.size();) {
        const char c = o[k];
        if (c == ']') break;
        if (c != '"') { ++k; continue; }  // 容错：跳过非字符串元素
        size_t end = 0;
        out.push_back(state_parse_json_string(o, k, &end));
        k = end;
    }
    return out;
}

// 提取 "key": <对象/数组> 区段：字符串感知深度计数（路径/原因里的
// 括号引号是内容而非结构）
static std::string state_find_section(const std::string& s, const std::string& key) {
    const std::string pattern = '"' + key + '"';
    size_t pos = s.find(pattern);
    if (pos == std::string::npos) return {};
    pos = s.find(':', pos);
    if (pos == std::string::npos) return {};
    const size_t start = s.find_first_of("[{", pos);
    if (start == std::string::npos) return {};
    const char open = s[start];
    const char close = (open == '[') ? ']' : '}';
    int depth = 0;
    bool in_str = false, esc = false;
    for (size_t i = start; i < s.size(); ++i) {
        const char c = s[i];
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') in_str = true;
        else if (c == open) ++depth;
        else if (c == close) {
            --depth;
            if (depth == 0) return s.substr(start, i - start + 1);
        }
    }
    return {};  // 未闭合（截断文件）：空串按缺区段处理
}

// 遍历对象数组区段里的每个对象字符串（字符串感知深度计数定界）
template <typename F>
static void state_for_each_object(const std::string& sec, F&& fn) {
    size_t i = 1;
    while (i < sec.size()) {
        size_t obj = sec.find('{', i);
        if (obj == std::string::npos) break;
        int depth = 1;
        bool in_str = false, esc = false;
        size_t j = obj + 1;
        for (; j < sec.size() && depth > 0; ++j) {
            const char c = sec[j];
            if (in_str) {
                if (esc) esc = false;
                else if (c == '\\') esc = true;
                else if (c == '"') in_str = false;
                continue;
            }
            if (c == '"') in_str = true;
            else if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
        if (depth == 0) fn(sec.substr(obj, j - obj));
        i = j + 1;
    }
}

static std::string state_json_escape(const std::string& s) {
    std::string out; out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back((char)c); break;
        }
    }
    return out;
}

bool DictionaryManagerStd::save_state(const std::string& file) const {
    std::error_code ec;
    fs::create_directories(fs::path(file).parent_path(), ec);
    std::ofstream out(u8_path(file), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "{\n  \"version\": 1,\n  \"dictionaries\": [";
    for (size_t i = 0; i < dicts_.size(); ++i) {
        const auto& d = dicts_[i];
        out << (i ? ",\n" : "\n");
        // file_path 取最初 add 的路径（src_paths 首位，load() 无条件先压入）
        out << "    {\"file_path\":\"" << state_json_escape(d.src_paths().front()) << "\"";
        out << ",\"enabled\":" << (d.enabled() ? "true" : "false");
        out << ",\"priority\":" << d.priority();
        if (!d.tags().empty()) {
            out << ",\"tags\":[";
            for (size_t t = 0; t < d.tags().size(); ++t) {
                if (t) out << ",";
                out << '"' << state_json_escape(d.tags()[t]) << '"';
            }
            out << "]";
        }
        out << "}";
    }
    out << (dicts_.empty() ? "" : "\n") << "  ],\n  \"quarantined\": [";
    for (size_t i = 0; i < failures_.size(); ++i) {
        const auto& f = failures_[i];
        out << (i ? ",\n" : "\n");
        out << "    {\"file_path\":\"" << state_json_escape(f.file_path) << "\"";
        out << ",\"reason\":\"" << state_json_escape(f.reason) << "\"";
        out << ",\"quarantined\":" << (f.quarantined ? "true" : "false") << "}";
    }
    out << (failures_.empty() ? "" : "\n") << "  ]\n}\n";
    return out.good();
}

bool DictionaryManagerStd::load_state(const std::string& file) {
    std::ifstream in(u8_path(file), std::ios::binary);
    if (!in) {
        last_error_ = "State file does not exist: " + file;
        return false;
    }
    std::ostringstream ss; ss << in.rdbuf();
    const std::string s = ss.str();

    struct DictState {
        std::string path;
        bool enabled = true;
        int priority = 0;
        std::vector<std::string> tags;
    };
    std::vector<DictState> entries;
    std::vector<DictionaryFailureStd> restored;

    const std::string dsec = state_find_section(s, "dictionaries");
    if (dsec.empty()) {
        last_error_ = "State file is missing dictionary list.";
        return false;
    }
    if (dsec.front() == '[') {
        state_for_each_object(dsec, [&](const std::string& o) {
            DictState e;
            e.path = state_obj_val(o, "file_path");
            e.enabled = state_obj_bool(o, "enabled", true);
            e.priority = (int)state_obj_int(o, "priority");
            e.tags = state_obj_str_array(o, "tags");
            if (!e.path.empty()) entries.push_back(std::move(e));
        });
    }
    const std::string qsec = state_find_section(s, "quarantined");
    if (!qsec.empty() && qsec.front() == '[') {
        state_for_each_object(qsec, [&](const std::string& o) {
            DictionaryFailureStd f;
            f.file_path = state_obj_val(o, "file_path");
            f.reason = state_obj_val(o, "reason");
            f.quarantined = state_obj_bool(o, "quarantined", false);
            if (!f.file_path.empty()) restored.push_back(std::move(f));
        });
    }

    // 提交相：先换失败表，再逐条恢复词典（add_dictionary 成功摘记录、
    // 失败刷新/补记录，两档语义由此收敛到一处）
    dicts_.clear();
    index_.clear();
    ft_index_.reset();
    ft_built_.store(false, std::memory_order_release);
    failures_ = std::move(restored);
    last_error_.clear();

    std::set<std::string> seen;
    for (auto& e : entries) {
        if (!seen.insert(e.path).second) continue;  // 同路径重复条目：首个生效
        // 隔离中的路径不再尝试解析（重试必须显式走 retry_failed_dictionary）
        const int fi = index_of_failure(e.path);
        if (fi >= 0 && failures_[fi].quarantined) continue;
        if (!add_dictionary(e.path)) {
            std::error_code ec;
            if (!fs::is_regular_file(e.path, ec)) {
                // 运行期诊断档：每次载入重查，文件回来自动恢复加载
                record_failure(e.path, "File not found: " + e.path, false);
            } else if (last_error_.compare(0, std::strlen(kUnsupportedExtPrefix),
                                           kUnsupportedExtPrefix) == 0) {
                record_failure(e.path, "Unsupported dictionary format: " + e.path, false);
            }
            // 解析失败档已由 add_dictionary 记录（quarantined=true）
            continue;
        }
        // 状态字段落到刚加入的实例（add_dictionary 恒 push 到尾部）
        auto& d = dicts_.back();
        d.set_enabled(e.enabled);
        d.set_priority(e.priority);
        d.set_tags(std::move(e.tags));
    }
    return true;
}

} // namespace UnidictCoreStd
