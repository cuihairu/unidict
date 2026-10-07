// unidict sync relay——C++ 版自带中转（server_plan §7 B5；PROTOCOL.md v1）。
//
// 单二进制、仅标准库（Qt-free），契约语义复用 core 的 SyncRelayStateStd：
// 与 dev 参考实现（Python）同一份契约符合性套件验收（run_conformance.py
// 经 UNIDICT_RELAY_EXTERNAL_BASE 打到本进程）。
//
// 默认只听 127.0.0.1（server_plan §3.1 隐私口径：对外暴露是显式动作）；
// 不带 --data 纯内存运行，带 --data 状态原子落盘跨重启保留。
#include <cstdio>
#include <cstring>
#include <string>

#include "relay_http.h"
#include "std/sync_relay_state_std.h"

using namespace UnidictRelay;

namespace {

// ---- 应答序列化（整数直排，字符串经 json_quote） ----

std::string meta_json(const UnidictCoreStd::SyncRelayMeta& m) {
    return "{\"gid\":" + json_quote(m.gid) +
           ",\"latest_seq\":" + std::to_string(m.latest_seq) +
           ",\"op_count\":" + std::to_string(m.op_count) +
           ",\"snapshot_up_to_seq\":" + std::to_string(m.snapshot_up_to_seq) +
           ",\"created_at\":" + std::to_string(m.created_at) + "}";
}

std::string append_result_json(const UnidictCoreStd::SyncRelayAppendResult& r) {
    std::string out = "{\"assigned\":[";
    for (std::size_t i = 0; i < r.assigned.size(); ++i) {
        if (i) out += ",";
        out += "{\"op_id\":" + json_quote(r.assigned[i].op_id) +
               ",\"seq\":" + std::to_string(r.assigned[i].seq) + "}";
    }
    out += "],\"duplicate_op_ids\":[";
    for (std::size_t i = 0; i < r.duplicate_op_ids.size(); ++i) {
        if (i) out += ",";
        out += json_quote(r.duplicate_op_ids[i]);
    }
    out += "]}";
    return out;
}

std::string pull_json(const UnidictCoreStd::SyncRelayPull& p) {
    std::string out = "{\"gid\":" + json_quote(p.gid) + ",\"ops\":[";
    for (std::size_t i = 0; i < p.ops.size(); ++i) {
        const UnidictCoreStd::SyncRelayOp& op = p.ops[i];
        if (i) out += ",";
        out += "{\"seq\":" + std::to_string(op.seq) +
               ",\"op_id\":" + json_quote(op.op_id) +
               ",\"device_id\":" + json_quote(op.device_id) +
               ",\"ts\":" + std::to_string(op.ts) +
               ",\"payload\":" + json_quote(op.payload) + "}";
    }
    out += "],\"cursor\":" + std::to_string(p.cursor) +
           ",\"has_more\":" + (p.has_more ? "true" : "false") + "}";
    return out;
}

std::string snapshot_get_json(const UnidictCoreStd::SyncRelaySnapshot& s) {
    return "{\"up_to_seq\":" + std::to_string(s.up_to_seq) +
           ",\"payload\":" + json_quote(s.payload) + "}";
}

// 错误应答统一形态（PROTOCOL §2）
HttpResponse error_response(int status, const std::string& code) {
    return {status, "{\"error\":" + json_quote(code) + "}"};
}

// 整数 query 参数：只收 [-]digits（dev 的 int() 同口径从严——空串/杂字
// 符一律 invalid_param）
bool parse_i64_param(const std::string& s, long long* out) {
    if (s.empty()) return false;
    std::size_t i = 0;
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        i = 1;
        if (s.size() == 1) return false;
    }
    long long v = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        if (v > (9223372036854775807LL - (s[i] - '0')) / 10) return false;
        v = v * 10 + (s[i] - '0');
    }
    *out = neg ? -v : v;
    return true;
}

class RelayApp {
public:
    explicit RelayApp(const std::string& data_dir) : state_(data_dir) {}

    HttpResponse handle(const HttpRequest& req) {
        try {
            return route(req);
        } catch (const UnidictCoreStd::SyncRelayErrorStd& e) {
            return error_response(e.status(), e.code());
        } catch (const std::exception&) {
            return error_response(500, "internal");
        }
    }

private:
    // ops 数组元素 → SyncRelayOpIn（非对象/字段非字符串 → invalid_op，
    // 与 dev 的 isinstance 校验同口径）
    std::vector<UnidictCoreStd::SyncRelayOpIn> to_ops(const JsonValue& arr) {
        std::vector<UnidictCoreStd::SyncRelayOpIn> ops;
        ops.reserve(arr.items.size());
        for (const JsonValue& item : arr.items) {
            if (!item.is_object())
                throw UnidictCoreStd::SyncRelayErrorStd(400, "invalid_op");
            const JsonValue* op_id = item.find("op_id");
            const JsonValue* device_id = item.find("device_id");
            const JsonValue* payload = item.find("payload");
            if (!op_id || !device_id || !payload || !op_id->is_string() ||
                !device_id->is_string() || !payload->is_string()) {
                throw UnidictCoreStd::SyncRelayErrorStd(400, "invalid_op");
            }
            ops.push_back({op_id->str, device_id->str, payload->str});
        }
        return ops;
    }

    HttpResponse route(const HttpRequest& req) {
        // path 切段（过滤空段：/api//sync/… 与 /api/sync/… 同视）
        std::vector<std::string> parts;
        {
            std::size_t pos = 0;
            while (pos < req.target.size()) {
                const std::size_t slash = req.target.find('/', pos);
                const std::string seg = req.target.substr(
                    pos, slash == std::string::npos ? std::string::npos
                                                    : slash - pos);
                if (!seg.empty()) parts.push_back(seg);
                if (slash == std::string::npos) break;
                pos = slash + 1;
            }
        }
        if (parts.size() == 4 && parts[0] == "api" && parts[1] == "sync" &&
            parts[2] == "relay" && parts[3] == "ping" && req.method == "GET") {
            return {200, std::string("{\"service\":\"") +
                         UnidictCoreStd::kRelayServiceName +
                         "\",\"protocol\":" +
                         std::to_string(UnidictCoreStd::kRelayProtocolVersion) +
                         "}"};
        }
        if (parts.size() >= 3 && parts[0] == "api" && parts[1] == "sync" &&
            parts[2] == "groups") {
            const std::string gid = parts.size() > 3 ? parts[3] : "";
            const std::string tail = parts.size() > 4 ? parts[4] : "";
            if (req.method == "PUT" && tail.empty() && parts.size() == 4) {
                return {200, meta_json(state_.create_group(gid))};
            }
            if (req.method == "GET" && tail == "meta") {
                return {200, meta_json(state_.group_meta(gid))};
            }
            if (req.method == "POST" && tail == "ops") {
                JsonValue body;
                if (!json_parse(req.body, &body) || !body.is_object()) {
                    return error_response(400, "invalid_json");
                }
                const JsonValue* ops = body.find("ops");
                if (!ops || !ops->is_array()) {
                    return error_response(400, "invalid_op");
                }
                return {200, append_result_json(state_.append_ops(gid, to_ops(*ops)))};
            }
            if (req.method == "GET" && tail == "ops") {
                long long since = 0, limit = 200;
                const auto it_since = req.query.find("since");
                if (it_since != req.query.end() &&
                    !parse_i64_param(it_since->second, &since)) {
                    return error_response(400, "invalid_param");
                }
                const auto it_limit = req.query.find("limit");
                if (it_limit != req.query.end() &&
                    !parse_i64_param(it_limit->second, &limit)) {
                    return error_response(400, "invalid_param");
                }
                return {200, pull_json(state_.pull_ops(gid, since, limit))};
            }
            if (req.method == "PUT" && tail == "snapshot") {
                JsonValue body;
                if (!json_parse(req.body, &body) || !body.is_object()) {
                    return error_response(400, "invalid_json");
                }
                // up_to_seq 缺失/非整数 → invalid_snapshot（dev 的
                // isinstance 校验同口径）；payload 非字符串 → invalid_op
                const JsonValue* up = body.find("up_to_seq");
                const JsonValue* payload = body.find("payload");
                if (!up || !up->is_int()) {
                    return error_response(400, "invalid_snapshot");
                }
                if (!payload || !payload->is_string()) {
                    return error_response(400, "invalid_op");
                }
                const std::int64_t placed = state_.put_snapshot(
                    gid, up->integer, payload->str);
                return {200, "{\"up_to_seq\":" + std::to_string(placed) + "}"};
            }
            if (req.method == "GET" && tail == "snapshot") {
                return {200, snapshot_get_json(state_.get_snapshot(gid))};
            }
        }
        return error_response(404, "not_found");
    }

    UnidictCoreStd::SyncRelayStateStd state_;
};

}  // namespace

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    int port = 8788;
    std::string data_dir;
    for (int i = 1; i < argc; ++i) {
        const auto need_value = [&](const char** out) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s 需要一个值\n", argv[i]);
                std::exit(2);
            }
            *out = argv[++i];
        };
        if (std::strcmp(argv[i], "--host") == 0) {
            const char* v;
            need_value(&v);
            host = v;
        } else if (std::strcmp(argv[i], "--port") == 0) {
            const char* v;
            need_value(&v);
            port = std::atoi(v);
        } else if (std::strcmp(argv[i], "--data") == 0) {
            const char* v;
            need_value(&v);
            data_dir = v;
        } else {
            std::fprintf(stderr,
                         "用法: unidict-relay [--host H] [--port P] [--data DIR]\n");
            return 2;
        }
    }

    RelayApp app(data_dir);
    HttpServerStd server;
    server.set_handler([&app](const HttpRequest& req) { return app.handle(req); });
    std::string err;
    if (!server.start(host, port, &err)) {
        std::fprintf(stderr, "unidict-relay: %s\n", err.c_str());
        return 1;
    }
    std::fprintf(stderr,
                 "unidict sync relay (cpp, protocol %d) listening on http://%s:%d\n",
                 UnidictCoreStd::kRelayProtocolVersion, host.c_str(), server.port());
    server.run();  // 不返回
    return 0;
}
