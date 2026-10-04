// B2 客户端同步引擎实现。JSON 手写读写器与 data_store_std /
// dictionary_manager_std 同口径（文件局部静态助手；\uXXXX 不解码，
// 转义表只写 \\ \" \n \r \t）。

#include "sync_engine_std.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

namespace fs = std::filesystem;

namespace UnidictCoreStd {

SyncTransportStd::~SyncTransportStd() = default;

namespace {

// ---- JSON 字符串助手（转义表与 data_store_std 一致）----
std::string sy_json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (const char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

// 唯一字符串读取入口：\\ \" \n \r \t \b \f 解码，未知转义取字面
std::string sy_parse_json_string(const std::string& s, size_t from,
                                 size_t* out_end) {
    std::string out;
    size_t i = from + 1;
    // GCOVR_EXCL_LINE：调用方（字段提取/区段遍历）都由字符串感知的
    // 扫描器把关后才切入，传入串在本翻译单元内必然闭合——i≥size 出口
    // 与串尾悬空反斜杠兜底不可达（防御留档，同 data_store_std）。
    while (i < s.size() && s[i] != '"') {  // GCOVR_EXCL_LINE
        if (s[i] != '\\') {
            out.push_back(s[i++]);
            continue;
        }
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

// 顺序扫描对象成员（字符串感知），命中键即回调 (值token, 是否字符串)。
// 不用朴素 find——词值可能是 "n"/"t" 这类键名同形词，子串匹配会被
// 值截胡；顺序扫描保证键匹配先于同形值。
template <typename F>
bool sy_find_member(const std::string& o, const std::string& key, F&& visit) {
    if (o.size() < 2 || o.front() != '{') return false;
    size_t i = 1;
    while (i < o.size()) {
        if (o[i] != '"') {
            ++i;
            continue;
        }
        size_t end = 0;
        const std::string k = sy_parse_json_string(o, i, &end);
        i = end;
        while (i < o.size() && o[i] != ':') ++i;
        if (i >= o.size()) return false;  // 畸形：截断收尾
        ++i;
        while (i < o.size() && (o[i] == ' ' || o[i] == '\t')) ++i;
        if (i >= o.size()) return false;
        if (o[i] == '"') {
            const std::string v = sy_parse_json_string(o, i, &end);
            if (k == key) {
                visit(v, true);
                return true;
            }
            i = end;
        } else {
            // 数值/字面量 token：读到结构性逗号或对象尾
            size_t j = i;
            while (j < o.size() && o[j] != ',' && o[j] != '}') ++j;
            if (k == key) {
                visit(o.substr(i, j - i), false);
                return true;
            }
            i = j;
        }
    }
    return false;
}

// 从对象字符串提取字符串/整数字段（无该字段返回空/0）
std::string sy_obj_val(const std::string& o, const std::string& key) {
    std::string out;
    sy_find_member(o, key,
                   [&out](const std::string& v, bool is_string) {
                       if (is_string) out = v;
                   });
    return out;
}

long long sy_obj_int(const std::string& o, const std::string& key) {
    long long out = 0;
    sy_find_member(o, key, [&out](const std::string& v, bool is_string) {
        if (!is_string) out = std::atoll(v.c_str());
    });
    return out;
}

// 提取 "key": <对象/数组> 区段：字符串感知深度计数（词/笔记里的
// 括号引号是内容而非结构）
std::string sy_find_section(const std::string& s, const std::string& key) {
    const std::string pattern = '"' + key + '"';
    const size_t pos = s.find(pattern);
    if (pos == std::string::npos) return {};
    const size_t colon = s.find(':', pos);
    if (colon == std::string::npos) return {};
    const size_t start = s.find_first_of("[{", colon);
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

// 区段若是字符串数组（"[...]"），取出全部元素
std::vector<std::string> sy_str_array(const std::string& sec) {
    std::vector<std::string> out;
    if (sec.size() < 2 || sec.front() != '[') return out;
    for (size_t k = 1; k + 1 < sec.size();) {
        const char c = sec[k];
        if (c != '"') {
            ++k;
            continue;
        }
        size_t end = 0;
        out.push_back(sy_parse_json_string(sec, k, &end));
        k = end;
    }
    return out;
}

// 遍历对象区段的每个成员，回调 (key, 值, is_string)。字符串成员给
// 解码后的值；数组成员给原始区段 token，由调用方按形态解析。
template <typename F>
void sy_for_each_member(const std::string& obj, F&& fn) {
    if (obj.size() < 2 || obj.front() != '{') return;
    size_t i = 1;
    while (i < obj.size()) {
        const char c = obj[i];
        if (c == '}') break;
        if (c != '"') {
            ++i;
            continue;
        }
        size_t end = 0;
        const std::string key = sy_parse_json_string(obj, i, &end);
        i = end;
        while (i < obj.size() && obj[i] != ':') ++i;
        if (i >= obj.size()) break;  // 畸形成员：截断收尾
        ++i;                         // 过 ':'
        while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) ++i;
        if (i >= obj.size()) break;
        if (obj[i] == '"') {
            const std::string value = sy_parse_json_string(obj, i, &end);
            fn(key, value, true);
            i = end;
        } else if (obj[i] == '[') {
            // 字符串感知深度计数取数组区段
            int depth = 0;
            bool in_str = false, esc = false;
            size_t j = i;
            for (; j < obj.size(); ++j) {
                const char d = obj[j];
                if (in_str) {
                    if (esc) esc = false;
                    else if (d == '\\') esc = true;
                    else if (d == '"') in_str = false;
                    continue;
                }
                if (d == '"') in_str = true;
                else if (d == '[') ++depth;
                else if (d == ']') {
                    --depth;
                    if (depth == 0) break;
                }
            }
            if (j >= obj.size()) break;  // 未闭合：截断收尾
            fn(key, obj.substr(i, j - i + 1), false);
            i = j + 1;
        } else if (obj[i] == '{') {
            // 对象成员：整段作为一个 token 回调（不递归内层成员）
            int depth = 0;
            bool in_str = false, esc = false;
            size_t j = i;
            for (; j < obj.size(); ++j) {
                const char d = obj[j];
                if (in_str) {
                    if (esc) esc = false;
                    else if (d == '\\') esc = true;
                    else if (d == '"') in_str = false;
                    continue;
                }
                if (d == '"') in_str = true;
                else if (d == '{' || d == '[') ++depth;
                else if (d == '}' || d == ']') {
                    --depth;
                    if (depth == 0) break;
                }
            }
            if (j >= obj.size()) break;  // 未闭合：截断收尾
            fn(key, obj.substr(i, j - i + 1), false);
            i = j + 1;
        } else {
            ++i;  // 其它字面量成员：跳过
        }
    }
}

// 遍历数组区段（"[...]"）的每个元素 token（元素是 "[...]" 或 "{...}"，
// 标量元素忽略）。字符串感知：payload 串内的裸括号不算结构。
template <typename F>
void sy_for_each_array_item(const std::string& sec, F&& fn) {
    if (sec.size() < 2 || sec.front() != '[') return;
    int depth = 0;
    bool in_str = false, esc = false;
    size_t start = 0;
    for (size_t i = 1; i < sec.size(); ++i) {
        const char c = sec[i];
        if (in_str) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') in_str = false;
            continue;
        }
        if (c == '"') in_str = true;
        else if (c == '[' || c == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (c == ']' || c == '}') {
            --depth;
            if (depth == 0) {
                fn(sec.substr(start, i - start + 1));
                start = 0;
            }
        }
    }
}

// ---- 规范指令 payload（B2 指令集；键序固定：t 在首、w 次之）----
std::string op_payload(SyncOpType type, const std::string& a,
                       const std::string& b, long long ts) {
    std::ostringstream o;
    switch (type) {
        case SyncOpType::AddEntry:
            o << "{\"t\":\"add\",\"w\":\"" << sy_json_escape(a) << "\"}";
            break;
        case SyncOpType::RemoveEntry:
            o << "{\"t\":\"rem\",\"w\":\"" << sy_json_escape(a) << "\"}";
            break;
        case SyncOpType::UpdateNote:
            // 单行语句：多行表达式的首行在 GCC gcov 下会丢行归属
            o << "{\"t\":\"note\",\"w\":\"" << sy_json_escape(a) << "\",\"n\":\"" << sy_json_escape(b) << "\"}";
            break;
        case SyncOpType::AddTag:
            o << "{\"t\":\"tag+\",\"w\":\"" << sy_json_escape(a) << "\",\"g\":\"" << sy_json_escape(b) << "\"}";
            break;
        case SyncOpType::RemoveTag:
            o << "{\"t\":\"tag-\",\"w\":\"" << sy_json_escape(a) << "\",\"g\":\"" << sy_json_escape(b) << "\"}";
            break;
        case SyncOpType::RecordHistory:
            o << "{\"t\":\"hist\",\"w\":\"" << sy_json_escape(a) << "\",\"ts\":" << ts << "}";
            break;
        case SyncOpType::SetPref:
            o << "{\"t\":\"pref\",\"k\":\"" << sy_json_escape(a) << "\",\"v\":\"" << sy_json_escape(b) << "\"}";
            break;
    }
    return o.str();
}

bool contains_word(const SyncVocabStateStd& s, const std::string& w) {
    return std::find(s.words.begin(), s.words.end(), w) != s.words.end();
}

// ---- 状态序列化/解析（快照与引擎持久化共用；逐字节确定）----
std::string serialize_state(const SyncVocabStateStd& s) {
    std::ostringstream o;
    o << "{\"words\":[";
    for (size_t i = 0; i < s.words.size(); ++i) {
        if (i) o << ',';
        o << '"' << sy_json_escape(s.words[i]) << '"';
    }
    o << "],\"notes\":{";
    bool first = true;
    for (const auto& kv : s.notes) {
        if (!first) o << ',';
        first = false;
        o << '"' << sy_json_escape(kv.first) << "\":\""
          << sy_json_escape(kv.second) << '"';
    }
    o << "},\"tags\":{";
    first = true;
    for (const auto& kv : s.tags) {
        if (!first) o << ',';
        first = false;
        o << '"' << sy_json_escape(kv.first) << "\":[";
        for (size_t i = 0; i < kv.second.size(); ++i) {
            if (i) o << ',';
            o << '"' << sy_json_escape(kv.second[i]) << '"';
        }
        o << ']';
    }
    o << "},\"history\":[";
    for (size_t i = 0; i < s.history.size(); ++i) {
        if (i) o << ',';
        o << "[\"" << sy_json_escape(s.history[i].first) << "\","
          << s.history[i].second << ']';
    }
    o << "],\"prefs\":{";
    first = true;
    for (const auto& kv : s.prefs) {
        if (!first) o << ',';
        first = false;
        o << '"' << sy_json_escape(kv.first) << "\":\""
          << sy_json_escape(kv.second) << '"';
    }
    o << "}}";
    return o.str();
}

SyncVocabStateStd parse_state(const std::string& sec) {
    SyncVocabStateStd s;
    s.words = sy_str_array(sy_find_section(sec, "words"));
    sy_for_each_member(sy_find_section(sec, "notes"),
                       [&s](const std::string& k, const std::string& v,
                            bool is_string) {
                           if (is_string && !v.empty()) s.notes[k] = v;
                       });
    sy_for_each_member(sy_find_section(sec, "tags"),
                       [&s](const std::string& k, const std::string& v,
                            bool is_string) {
                           if (!is_string) s.tags[k] = sy_str_array(v);
                       });
    sy_for_each_array_item(
        sy_find_section(sec, "history"), [&s](const std::string& item) {
            // ["w",ts]；畸形元素（空数组/非串头）跳过
            if (item.size() < 3 || item[1] != '"') return;
            size_t end = 0;
            const std::string w = sy_parse_json_string(item, 1, &end);
            long long ts = 0;
            const size_t comma = item.find(',', end);
            if (comma != std::string::npos) {
                ts = std::atoll(item.c_str() + comma + 1);
            }
            s.history.emplace_back(w, ts);
        });
    sy_for_each_member(sy_find_section(sec, "prefs"),
                       [&s](const std::string& k, const std::string& v,
                            bool is_string) {
                           if (is_string) s.prefs[k] = v;  // 空值合法
                       });
    return s;
}

std::string make_device_id() {
    std::random_device rd;
    const unsigned long long a =
        (static_cast<unsigned long long>(rd()) << 32) ^ rd();
    const unsigned long long b =
        (static_cast<unsigned long long>(rd()) << 32) ^ rd();
    char buf[33];
    std::snprintf(buf, sizeof buf, "%016llx%016llx", a, b);
    return buf;
}

}  // namespace

// ---- SyncVocabStateStd ----

void SyncVocabStateStd::clear() { *this = SyncVocabStateStd(); }

bool SyncVocabStateStd::operator==(const SyncVocabStateStd& other) const {
    return words == other.words && notes == other.notes &&
           tags == other.tags && history == other.history &&
           prefs == other.prefs;
}

// ---- SyncEngineStd ----

SyncEngineStd::SyncEngineStd(const std::string& device_id)
    : device_id_(device_id.empty() ? make_device_id() : device_id) {}

std::string SyncEngineStd::enqueue(SyncOpType type, const std::string& a,
                                   const std::string& b, long long ts) {
    last_error_.clear();
    const std::string payload = op_payload(type, a, b, ts);
    if (payload.size() > kMaxPayloadBytes) {
        last_error_ = "payload too large";
        return "";
    }
    ++local_seq_;
    EnqueuedOpStd op;
    op.op_id = device_id_ + ":" + std::to_string(local_seq_);
    op.payload = payload;
    outbox_.push_back(op);
    apply(payload);  // 本地立即生效；服务端回显重放时幂等
    return op.op_id;
}

bool SyncEngineStd::sync(SyncTransportStd& transport, const std::string& gid,
                         std::string* err) {
    last_error_.clear();
    if (!valid_gid(gid)) {
        last_error_ = "invalid group id";
        if (err) *err = last_error_;
        return false;
    }
    if (!push_outbox(transport, gid, err)) return false;
    return pull_and_replay(transport, gid, err);
}

bool SyncEngineStd::push_outbox(SyncTransportStd& transport,
                                const std::string& gid, std::string* err) {
    while (!outbox_.empty()) {
        const size_t n = std::min(kMaxOpsPerPost, outbox_.size());
        const std::vector<EnqueuedOpStd> batch(outbox_.begin(),
                                               outbox_.begin() + n);
        std::vector<std::string> acked;
        if (!transport.push_ops(gid, batch, &acked, err)) {
            last_error_ = err && !err->empty() ? *err : "push failed";
            return false;  // outbox 原样保留：重试即续传
        }
        std::set<std::string> ackset(acked.begin(), acked.end());
        std::vector<EnqueuedOpStd> rest;
        // 未确认的本批指令回到队首（保序），尾部随后
        for (size_t k = 0; k < outbox_.size(); ++k) {
            if (k >= n || ackset.count(outbox_[k].op_id) == 0) {
                rest.push_back(outbox_[k]);
            }
        }
        outbox_.swap(rest);
        if (acked.empty()) break;  // 协议下不该发生（正常响应必带 ack）：
                                   // 防御性收口，避免无进展死循环
    }
    return true;
}

bool SyncEngineStd::pull_and_replay(SyncTransportStd& transport,
                                    const std::string& gid, std::string* err) {
    GroupMetaStd meta;
    if (!transport.meta(gid, &meta, err)) {
        last_error_ = err && !err->empty() ? *err : "meta failed";
        return false;
    }
    uint64_t base = cursor_;
    if (cursor_ < meta.snapshot_up_to_seq) {
        // 位点落后于快照覆盖位：先取快照重建，再从覆盖位增量（PROTOCOL §2.8）
        uint64_t up = 0;
        std::string snap;
        if (!transport.get_snapshot(gid, &up, &snap, err)) {
            last_error_ = err && !err->empty() ? *err : "snapshot failed";
            return false;
        }
        state_ = parse_state(snap);
        cursor_ = up;
        base = up;
    }
    for (;;) {
        std::vector<RemoteOpStd> ops;
        uint64_t server_cursor = 0;
        bool has_more = false;
        if (!transport.pull_ops(gid, base, std::min(pull_limit_, kMaxPullLimit),
                                &ops, &server_cursor, &has_more, err)) {
            last_error_ = err && !err->empty() ? *err : "pull failed";
            return false;
        }
        for (const RemoteOpStd& op : ops) {
            apply(op.payload);  // seq 升序重放；自身回显幂等
            cursor_ = op.seq;
            ++applied_since_snapshot_;
        }
        if (ops.empty() || !has_more) break;
        base = ops.back().seq;
    }
    return true;
}

void SyncEngineStd::apply(const std::string& payload) {
    const std::string t = sy_obj_val(payload, "t");
    const std::string w = sy_obj_val(payload, "w");
    if (t == "add") {
        // 规范序插入（字典序）：回放次序无关，两台设备指令集相同即
        // 状态逐字节相同
        if (!w.empty()) {
            auto it = std::lower_bound(state_.words.begin(), state_.words.end(),
                                       w);
            if (it == state_.words.end() || *it != w) state_.words.insert(it, w);
        }
    } else if (t == "rem") {
        // remove-erase 成套：词不存在时删零个（幂等）
        state_.words.erase(std::remove(state_.words.begin(), state_.words.end(),
                                       w),
                           state_.words.end());
        state_.notes.erase(w);
        state_.tags.erase(w);
    } else if (t == "note") {
        if (contains_word(state_, w)) {
            const std::string n = sy_obj_val(payload, "n");
            if (n.empty()) state_.notes.erase(w);  // 空笔记 = 清除
            else state_.notes[w] = n;
        }
    } else if (t == "tag+") {
        if (contains_word(state_, w) && !sy_obj_val(payload, "g").empty()) {
            const std::string g = sy_obj_val(payload, "g");
            std::vector<std::string>& list = state_.tags[w];
            auto it = std::lower_bound(list.begin(), list.end(), g);
            if (it == list.end() || *it != g) list.insert(it, g);  // 规范序
        }
    } else if (t == "tag-") {
        auto it = state_.tags.find(w);
        if (it != state_.tags.end()) {
            const std::string g = sy_obj_val(payload, "g");
            it->second.erase(std::remove(it->second.begin(), it->second.end(),
                                         g),
                             it->second.end());
            if (it->second.empty()) state_.tags.erase(it);  // 空标签表收口
        }
    } else if (t == "hist") {
        // 规范序 (ts, word)：各设备本地先记的条目次序不同也收敛
        const std::pair<std::string, long long> entry = {
            w, sy_obj_int(payload, "ts")};
        auto it = std::lower_bound(
            state_.history.begin(), state_.history.end(), entry,
            [](const std::pair<std::string, long long>& x,
               const std::pair<std::string, long long>& y) {
                return x.second != y.second ? x.second < y.second
                                            : x.first < y.first;
            });
        if (it == state_.history.end() || *it != entry) {
            state_.history.insert(it, entry);
        }
    } else if (t == "pref") {
        state_.prefs[sy_obj_val(payload, "k")] = sy_obj_val(payload, "v");
    }
    // 未知 t：忽略（前向兼容——旧客户端遇到新指令不炸）
}

bool SyncEngineStd::maybe_snapshot(SyncTransportStd& transport,
                                   const std::string& gid,
                                   size_t applied_threshold, std::string* err) {
    last_error_.clear();
    if (applied_since_snapshot_ < applied_threshold) return true;  // 未达阈值
    if (!transport.put_snapshot(gid, cursor_, serialize_state(state_), err)) {
        last_error_ = err && !err->empty() ? *err : "snapshot upload failed";
        return false;
    }
    applied_since_snapshot_ = 0;
    return true;
}

bool SyncEngineStd::save_state(const std::string& path, std::string* err) {
    std::ostringstream o;
    o << "{\"version\":1,\"device_id\":\"" << sy_json_escape(device_id_)
      << "\",\"local_seq\":" << local_seq_ << ",\"cursor\":" << cursor_
      << ",\"applied_since_snapshot\":" << applied_since_snapshot_
      << ",\"outbox\":[";
    for (size_t i = 0; i < outbox_.size(); ++i) {
        if (i) o << ',';
        o << "{\"op_id\":\"" << sy_json_escape(outbox_[i].op_id)
          << "\",\"payload\":\"" << sy_json_escape(outbox_[i].payload)
          << "\"}";
    }
    o << "],\"state\":" << serialize_state(state_) << "}";

    std::error_code ec;
    const fs::path parent = fs::path(path).parent_path();
    if (!parent.empty() && !fs::exists(parent)) {
        fs::create_directories(parent, ec);  // 目录建不出时写文件自然报错
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good()) {
        last_error_ = "cannot write state file";
        if (err) *err = last_error_;
        return false;
    }
    const std::string body = o.str();
    out.write(body.data(), static_cast<std::streamsize>(body.size()));
    out.close();
    // GCOVR_EXCL_LINE：写回失败（close 后 good() 为假）需要磁盘故障注
    // 入——Linux 可用 /dev/full 但 macOS CI 无此设备，无法确定性复现；
    // 防御留档，同 data_store_std 写路径口径。
    if (!out.good()) {  // GCOVR_EXCL_LINE
        last_error_ = "cannot write state file";  // GCOVR_EXCL_LINE
        if (err) *err = last_error_;  // GCOVR_EXCL_LINE
        return false;  // GCOVR_EXCL_LINE
    }
    return true;
}

bool SyncEngineStd::load_state(const std::string& path, std::string* err) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        last_error_ = "state file does not exist";
        if (err) *err = last_error_;
        return false;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    const std::string body = buf.str();

    if (sy_obj_int(body, "version") != 1) {
        last_error_ = "unsupported state version";
        if (err) *err = last_error_;
        return false;
    }
    const std::string dev = sy_obj_val(body, "device_id");
    if (!dev.empty()) device_id_ = dev;  // 畸形缺省：保留本机标识
    local_seq_ = static_cast<uint64_t>(sy_obj_int(body, "local_seq"));
    cursor_ = static_cast<uint64_t>(sy_obj_int(body, "cursor"));
    applied_since_snapshot_ = static_cast<uint64_t>(
        sy_obj_int(body, "applied_since_snapshot"));
    outbox_.clear();
    sy_for_each_member(body,
                       [this](const std::string& k, const std::string& v,
                              bool is_string) {
                           if (k == "outbox" && !is_string) {
                               // v 是对象数组区段：字符串感知逐元素取
                               // op_id/payload（payload 串内有裸 { }，
                               // 朴素括号配对会截断）
                               sy_for_each_array_item(
                                   v, [this](const std::string& item) {
                                       EnqueuedOpStd op;
                                       op.op_id = sy_obj_val(item, "op_id");
                                       op.payload =
                                           sy_obj_val(item, "payload");
                                       if (!op.op_id.empty()) {
                                           outbox_.push_back(op);
                                       }
                                   });
                           }
                       });
    const std::string state = sy_find_section(body, "state");
    if (!state.empty()) state_ = parse_state(state);
    return true;
}

bool SyncEngineStd::valid_gid(const std::string& gid) {
    if (gid.size() < 16 || gid.size() > 64) return false;
    for (const char c : gid) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

}  // namespace UnidictCoreStd
