#include "std/sync_relay_state_std.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ctime>

namespace UnidictCoreStd {

namespace {

// PROTOCOL.md §3 限额（与 dev 参考实现同值）
constexpr std::size_t kGidMin = 16;
constexpr std::size_t kGidMax = 64;
constexpr std::size_t kMaxPayloadB64 = 256 * 1024;
constexpr std::size_t kMaxOpsPerPost = 256;
constexpr std::int64_t kMaxPullLimit = 1000;
constexpr std::size_t kOpIdMax = 200;
constexpr std::size_t kDeviceIdMax = 128;

// base64 形态校验（内容不解析——中转只见密文红线）；空串非法由调用方判
bool is_b64_shape(const std::string& s) {
    for (char c : s) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
        if (!ok) return false;
    }
    return true;
}

bool is_gid_shape(const std::string& gid) {
    if (gid.size() < kGidMin || gid.size() > kGidMax) return false;
    for (char c : gid) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

std::int64_t now_seconds() {
    return (std::int64_t)std::time(nullptr);
}

// ---- 落盘编解码：行式文本，字段空格分隔 ----
//   unidict-relay-state 1
//   G <gid> <created_at> <latest_seq> <snap_up_to|-> <snap_ts|-> <snap_payload|->
//   O <gid> <seq> <ts> <op_id_hex> <device_id_hex> <payload>
// gid 恒匹配 [A-Za-z0-9_-]{16,64}、payload 恒为 base64 文本，二者天然
// 不含空白可裸放；op_id/device_id 是客户端任意串，hex 编码保无歧义。
// 全部行解析失败即按坏文件空起（与 dev 形态同口径）。

std::string to_hex(const std::string& raw) {
    static const char* kDigits = "0123456789abcdef";
    std::string out;
    out.reserve(raw.size() * 2);
    for (unsigned char c : raw) {
        out += kDigits[c >> 4];
        out += kDigits[c & 0xf];
    }
    return out;
}

bool from_hex(const std::string& hex, std::string* out) {
    if (hex.size() % 2 != 0) return false;
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string raw;
    raw.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nib(hex[i]), lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        raw += (char)((hi << 4) | lo);
    }
    *out = raw;
    return true;
}

std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t pos = 0;
    while (true) {
        const std::size_t next = line.find(' ', pos);
        if (next == std::string::npos) {
            fields.push_back(line.substr(pos));
            break;
        }
        fields.push_back(line.substr(pos, next - pos));
        pos = next + 1;
    }
    return fields;
}

bool parse_i64(const std::string& s, std::int64_t* out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        i = 1;
        if (s.size() == 1) return false;
    }
    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        if (v > (INT64_MAX - (s[i] - '0')) / 10) return false;  // 坏文件防溢出
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return true;
}

const char* kStateMagic = "unidict-relay-state";
constexpr int kStateVersion = 1;

}  // namespace

SyncRelayStateStd::SyncRelayStateStd(const std::string& data_dir) {
    if (data_dir.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(data_dir, ec);
    if (ec) throw std::runtime_error("relay: cannot create data dir: " + data_dir);
    data_dir_ = data_dir;
    state_path_ = (std::filesystem::path(data_dir) / "relay_state.json").string();

    // 加载：坏文件/不存在按空起（dev 形态同口径）
    std::ifstream in(state_path_);
    if (!in) return;
    std::string line;
    if (!std::getline(in, line)) return;
    if (line != std::string(kStateMagic) + " " + std::to_string(kStateVersion)) return;
    bool valid = true;
    std::map<std::string, Group> loaded;
    while (valid && std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto f = split_fields(line);
        if (f.empty() || f[0].empty()) continue;
        if (f[0] == "G" && f.size() == 7) {
            Group g;
            if (!parse_i64(f[2], &g.created_at) || !parse_i64(f[3], &g.latest_seq)) {
                valid = false;
                break;
            }
            if (f[4] != "-") {
                if (!parse_i64(f[4], &g.snapshot_up_to_seq) ||
                    !parse_i64(f[5], &g.snapshot_ts) || f[6] == "-") {
                    valid = false;
                    break;
                }
                g.has_snapshot = true;
                g.snapshot_payload = f[6];
            }
            loaded[f[1]] = std::move(g);
        } else if (f[0] == "O" && f.size() == 7) {
            auto it = loaded.find(f[1]);
            if (it == loaded.end()) {
                valid = false;
                break;
            }
            SyncRelayOp op;
            if (!parse_i64(f[2], &op.seq) || !parse_i64(f[3], &op.ts) ||
                !from_hex(f[4], &op.op_id) || !from_hex(f[5], &op.device_id)) {
                valid = false;
                break;
            }
            op.payload = f[6];
            it->second.ops.push_back(std::move(op));
        } else {
            valid = false;
        }
    }
    if (valid) groups_ = std::move(loaded);
}

void SyncRelayStateStd::must_gid(const std::string& gid) {
    if (!is_gid_shape(gid)) throw SyncRelayErrorStd(400, "invalid_group_id");
}

void SyncRelayStateStd::must_payload(const std::string& payload) {
    // 长度超限是 413 不是 400（PROTOCOL §3）；形态坏是 400
    if (payload.size() > kMaxPayloadB64)
        throw SyncRelayErrorStd(413, "payload_too_large");
    if (!is_b64_shape(payload)) throw SyncRelayErrorStd(400, "invalid_op");
}

void SyncRelayStateStd::must_op(const SyncRelayOpIn& op) {
    if (op.op_id.empty() || op.op_id.size() > kOpIdMax ||
        op.device_id.empty() || op.device_id.size() > kDeviceIdMax ||
        op.payload.empty()) {
        throw SyncRelayErrorStd(400, "invalid_op");
    }
    must_payload(op.payload);
}

SyncRelayStateStd::Group& SyncRelayStateStd::must_group(const std::string& gid) {
    auto it = groups_.find(gid);
    if (it == groups_.end()) throw SyncRelayErrorStd(404, "group_not_found");
    return it->second;
}

void SyncRelayStateStd::persist_locked() {
    if (data_dir_.empty()) return;
    const std::string tmp = state_path_ + ".tmp";
    {
        // 打开失败或中途写失败都收敛到这一个 good() 判定（打开失败时
        // 后续写全是设 failbit 的空操作）
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << kStateMagic << " " << kStateVersion << "\n";
        for (const auto& [gid, g] : groups_) {
            out << "G " << gid << " " << g.created_at << " " << g.latest_seq;
            if (g.has_snapshot) {
                out << " " << g.snapshot_up_to_seq << " " << g.snapshot_ts << " "
                    << g.snapshot_payload;
            } else {
                out << " - - -";
            }
            out << "\n";
            for (const SyncRelayOp& op : g.ops) {
                out << "O " << gid << " " << op.seq << " " << op.ts << " "
                    << to_hex(op.op_id) << " " << to_hex(op.device_id) << " "
                    << op.payload << "\n";
            }
        }
        if (!out.good()) throw SyncRelayErrorStd(500, "persist_failed");
    }
    std::error_code ec;
    std::filesystem::rename(tmp, state_path_, ec);
    if (ec) throw SyncRelayErrorStd(500, "persist_failed");
}

SyncRelayMeta SyncRelayStateStd::create_group(const std::string& gid) {
    must_gid(gid);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = groups_.find(gid);
    if (it == groups_.end()) {
        Group g;
        g.created_at = now_seconds();
        it = groups_.emplace(gid, std::move(g)).first;
        persist_locked();
    }
    SyncRelayMeta m;
    m.gid = gid;
    m.latest_seq = it->second.latest_seq;
    m.op_count = (std::int64_t)it->second.ops.size();
    m.snapshot_up_to_seq = it->second.has_snapshot ? it->second.snapshot_up_to_seq : 0;
    m.created_at = it->second.created_at;
    return m;
}

SyncRelayMeta SyncRelayStateStd::group_meta(const std::string& gid) {
    must_gid(gid);
    std::lock_guard<std::mutex> lock(mutex_);
    const Group& g = must_group(gid);
    SyncRelayMeta m;
    m.gid = gid;
    m.latest_seq = g.latest_seq;
    m.op_count = (std::int64_t)g.ops.size();
    m.snapshot_up_to_seq = g.has_snapshot ? g.snapshot_up_to_seq : 0;
    m.created_at = g.created_at;
    return m;
}

SyncRelayAppendResult SyncRelayStateStd::append_ops(
    const std::string& gid, const std::vector<SyncRelayOpIn>& ops) {
    must_gid(gid);
    if (ops.empty()) throw SyncRelayErrorStd(400, "invalid_op");
    if (ops.size() > kMaxOpsPerPost) throw SyncRelayErrorStd(400, "too_many_ops");
    for (const SyncRelayOpIn& op : ops) must_op(op);
    // 同请求内重复 op_id → 400（客户端 bug 面）
    {
        std::map<std::string, bool> seen;
        for (const SyncRelayOpIn& op : ops) {
            if (seen.count(op.op_id))
                throw SyncRelayErrorStd(400, "duplicate_op_id_in_request");
            seen[op.op_id] = true;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = groups_.find(gid);
    if (it == groups_.end()) {
        Group g;  // 自动建组（PROTOCOL §2.4）
        g.created_at = now_seconds();
        it = groups_.emplace(gid, std::move(g)).first;
    }
    Group& g = it->second;

    std::map<std::string, bool> existing;
    for (const SyncRelayOp& op : g.ops) existing[op.op_id] = true;

    SyncRelayAppendResult result;
    for (const SyncRelayOpIn& op : ops) {
        if (existing.count(op.op_id)) {
            result.duplicate_op_ids.push_back(op.op_id);  // 幂等：不追加不计新 seq
            continue;
        }
        SyncRelayOp stored;
        stored.seq = ++g.latest_seq;
        stored.ts = now_seconds();
        stored.op_id = op.op_id;
        stored.device_id = op.device_id;
        stored.payload = op.payload;
        g.ops.push_back(std::move(stored));
        existing[op.op_id] = true;
        result.assigned.push_back({op.op_id, stored.seq});
    }
    if (!result.assigned.empty()) persist_locked();
    return result;
}

SyncRelayPull SyncRelayStateStd::pull_ops(const std::string& gid,
                                          std::int64_t since, std::int64_t limit) {
    must_gid(gid);
    std::lock_guard<std::mutex> lock(mutex_);
    const Group& g = must_group(gid);
    SyncRelayPull out;
    out.gid = gid;
    out.cursor = g.latest_seq;
    if (since < 0) since = 0;  // 负位点按 0（容错不报错）
    if (limit < 1) limit = 1;
    if (limit > kMaxPullLimit) limit = kMaxPullLimit;
    // v1 无裁剪：ops[i].seq == i+1，按位点直接切片
    const std::int64_t total = (std::int64_t)g.ops.size();
    if (since < total) {
        const std::int64_t end = std::min(total, since + limit);
        out.ops.assign(g.ops.begin() + since, g.ops.begin() + end);
    }
    out.has_more = since + (std::int64_t)out.ops.size() < total;
    return out;
}

std::int64_t SyncRelayStateStd::put_snapshot(const std::string& gid,
                                             std::int64_t up_to_seq,
                                             const std::string& payload) {
    must_gid(gid);
    std::lock_guard<std::mutex> lock(mutex_);
    Group& g = const_cast<Group&>(must_group(gid));
    // 不能快照未来：1 ≤ up_to_seq ≤ latest_seq
    if (up_to_seq < 1 || up_to_seq > g.latest_seq)
        throw SyncRelayErrorStd(400, "invalid_snapshot");
    must_payload(payload);  // 形态错按 invalid_op（与 dev 同码）
    g.has_snapshot = true;
    g.snapshot_up_to_seq = up_to_seq;
    g.snapshot_ts = now_seconds();
    g.snapshot_payload = payload;
    persist_locked();
    return up_to_seq;
}

SyncRelaySnapshot SyncRelayStateStd::get_snapshot(const std::string& gid) {
    must_gid(gid);
    std::lock_guard<std::mutex> lock(mutex_);
    const Group& g = must_group(gid);
    if (!g.has_snapshot) throw SyncRelayErrorStd(404, "snapshot_not_found");
    SyncRelaySnapshot s;
    s.up_to_seq = g.snapshot_up_to_seq;
    s.payload = g.snapshot_payload;
    return s;
}

}  // namespace UnidictCoreStd
