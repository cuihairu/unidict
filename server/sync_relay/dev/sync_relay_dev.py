#!/usr/bin/env python3
"""Unidict 同步中转——本地 dev 形态（server_plan.md §7 B1 参考实现）。

单进程、仅标准库；实现 PROTOCOL.md v1 全契约：指令收发（op_id 幂等
去重）、服务端定序（组内全序单调 seq）、按位点增量拉取、快照存取。
官方托管形态（worker/）实现同一契约；契约符合性测试
（test_relay_protocol.py）对两种实现跑同一套断言口径。

默认只听 127.0.0.1（server_plan §3.1 隐私口径：对外暴露是显式动作）；
不带 --data 时纯内存运行（自测/临时用途），带 --data 时状态原子落盘
（tmp + rename）跨重启保留。
"""

import argparse
import base64
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

PROTOCOL_VERSION = 1

# PROTOCOL.md §3 限额
GID_RE = re.compile(r"^[A-Za-z0-9_-]{16,64}$")
MAX_PAYLOAD_B64 = 256 * 1024
MAX_OPS_PER_POST = 256
DEFAULT_PULL_LIMIT = 200
MAX_PULL_LIMIT = 1000
MAX_BODY_BYTES = 64 * 1024 * 1024
OP_ID_MAX = 200
DEVICE_ID_MAX = 128
B64_RE = re.compile(r"^[A-Za-z0-9+/=]*$")


class RelayError(Exception):
    """契约错误：http_status + 机器可读码（PROTOCOL.md §2 错误形态）。"""

    def __init__(self, status, code):
        super().__init__(code)
        self.status = status
        self.code = code


class SyncRelayState:
    """组状态与全部契约语义（与 HTTP 传输解耦，便于复用与测试）。

    线程模型：一把 RLock 串行化全部读写——组内定序全序无重号由此
    保证（PROTOCOL.md §4「进程内串行」）。
    """

    def __init__(self, data_dir=None):
        self._lock = threading.RLock()
        # gid -> {"created_at", "latest_seq", "ops": [op...按 seq-1 对位],
        #         "snapshot": {"up_to_seq","ts","payload"} | None}
        self._groups = {}
        self._data_dir = data_dir
        if data_dir:
            os.makedirs(data_dir, exist_ok=True)
            self._path = os.path.join(data_dir, "relay_state.json")
            self._load()

    # ---- 持久化（原子替换；内存形态为空操作） ----

    def _load(self):
        try:
            with open(self._path, "r", encoding="utf-8") as f:
                raw = json.load(f)
            if isinstance(raw, dict):
                self._groups = raw.get("groups", {})
        except (OSError, ValueError):
            self._groups = {}  # 坏文件按空起（dev 形态容忍）

    def _save(self):
        if not self._data_dir:
            return
        tmp = self._path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump({"protocol": PROTOCOL_VERSION,
                       "groups": self._groups}, f)
        os.replace(tmp, self._path)

    # ---- 契约操作（全部持锁；抛 RelayError 表契约违规） ----

    def ping(self):
        return {"service": "unidict-sync-relay", "protocol": PROTOCOL_VERSION}

    def _must_gid(self, gid):
        if not isinstance(gid, str) or not GID_RE.match(gid):
            raise RelayError(400, "invalid_group_id")
        return gid

    def create_group(self, gid):
        with self._lock:
            self._must_gid(gid)
            if gid not in self._groups:
                self._groups[gid] = {
                    "created_at": int(time.time()),
                    "latest_seq": 0,
                    "ops": [],
                    "snapshot": None,
                }
                self._save()
            return self.group_meta(gid)

    def group_meta(self, gid):
        with self._lock:
            self._must_gid(gid)
            g = self._groups.get(gid)
            if g is None:
                raise RelayError(404, "group_not_found")
            return {
                "gid": gid,
                "latest_seq": g["latest_seq"],
                "op_count": len(g["ops"]),
                "snapshot_up_to_seq": (g["snapshot"]["up_to_seq"]
                                       if g["snapshot"] else 0),
                "created_at": g["created_at"],
            }

    @staticmethod
    def _validate_op(op):
        if not isinstance(op, dict):
            raise RelayError(400, "invalid_op")
        op_id, device_id, payload = op.get("op_id"), op.get("device_id"), \
            op.get("payload")
        for field, val, cap in (("op_id", op_id, OP_ID_MAX),
                                ("device_id", device_id, DEVICE_ID_MAX)):
            if not isinstance(val, str) or not val or len(val) > cap:
                raise RelayError(400, "invalid_op")
        if not isinstance(payload, str) or not payload:
            raise RelayError(400, "invalid_op")
        if len(payload) > MAX_PAYLOAD_B64:  # 尺寸超限是 413,不是 400
            raise RelayError(413, "payload_too_large")
        if not B64_RE.match(payload):  # base64 形态校验（内容不解析）
            raise RelayError(400, "invalid_op")

    def append_ops(self, gid, ops):
        with self._lock:
            self._must_gid(gid)
            if not isinstance(ops, list) or not ops:
                raise RelayError(400, "invalid_op")
            if len(ops) > MAX_OPS_PER_POST:
                raise RelayError(400, "too_many_ops")
            for op in ops:
                self._validate_op(op)
            seen = set()
            for op in ops:  # 同请求内重复 op_id → 400（客户端 bug 面）
                if op["op_id"] in seen:
                    raise RelayError(400, "duplicate_op_id_in_request")
                seen.add(op["op_id"])

            g = self._groups.get(gid)
            if g is None:  # 自动建组（离线重试零往返，PROTOCOL §2.4）
                g = {"created_at": int(time.time()), "latest_seq": 0,
                     "ops": [], "snapshot": None}
                self._groups[gid] = g

            existing = {o["op_id"] for o in g["ops"]}
            assigned, duplicates = [], []
            for op in ops:
                if op["op_id"] in existing:
                    duplicates.append(op["op_id"])  # 幂等：不追加不计新 seq
                    continue
                g["latest_seq"] += 1
                g["ops"].append({
                    "seq": g["latest_seq"],
                    "op_id": op["op_id"],
                    "device_id": op["device_id"],
                    "ts": int(time.time()),
                    "payload": op["payload"],
                })
                existing.add(op["op_id"])
                assigned.append({"op_id": op["op_id"],
                                 "seq": g["latest_seq"]})
            if assigned:
                self._save()
            return {"assigned": assigned, "duplicate_op_ids": duplicates}

    def pull_ops(self, gid, since, limit):
        with self._lock:
            self._must_gid(gid)
            g = self._groups.get(gid)
            if g is None:
                raise RelayError(404, "group_not_found")
            since = max(0, since)          # 负位点按 0（容错不报错）
            limit = min(max(1, limit), MAX_PULL_LIMIT)
            # v1 无裁剪：ops 与 seq 连续对位；按 seq 升序切片
            picked = g["ops"][since:since + limit]
            has_more = since + len(picked) < len(g["ops"])
            return {
                "gid": gid,
                "ops": picked,
                "cursor": g["latest_seq"],
                "has_more": has_more,
            }

    def put_snapshot(self, gid, up_to_seq, payload):
        with self._lock:
            self._must_gid(gid)
            g = self._groups.get(gid)
            if g is None:
                raise RelayError(404, "group_not_found")
            if (not isinstance(up_to_seq, int) or isinstance(up_to_seq, bool)
                    or up_to_seq < 1 or up_to_seq > g["latest_seq"]):
                raise RelayError(400, "invalid_snapshot")
            if not isinstance(payload, str) or not payload \
                    or len(payload) > MAX_PAYLOAD_B64 \
                    or not B64_RE.match(payload):
                raise RelayError(400, "invalid_op")
            g["snapshot"] = {"up_to_seq": up_to_seq,
                             "ts": int(time.time()),
                             "payload": payload}
            self._save()
            return {"up_to_seq": up_to_seq}

    def get_snapshot(self, gid):
        with self._lock:
            self._must_gid(gid)
            g = self._groups.get(gid)
            if g is None:
                raise RelayError(404, "group_not_found")
            if g["snapshot"] is None:
                raise RelayError(404, "snapshot_not_found")
            return {"up_to_seq": g["snapshot"]["up_to_seq"],
                    "payload": g["snapshot"]["payload"]}


def make_handler(state):
    """HTTP 壳：路由 + JSON 编解码，语义全在 SyncRelayState。"""

    def _int_param(query, name, default):
        raw = query.get(name)
        if raw is None:
            return default
        try:
            return int(raw)
        except ValueError:
            raise RelayError(400, "invalid_param")

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def _send(self, status, obj):
            body = json.dumps(obj).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def _read_body(self):
            length = int(self.headers.get("Content-Length") or 0)
            if length > MAX_BODY_BYTES:
                raise RelayError(413, "payload_too_large")
            raw = self.rfile.read(length) if length else b""
            if not raw:
                raise RelayError(400, "invalid_json")
            try:
                return json.loads(raw.decode("utf-8"))
            except (ValueError, UnicodeDecodeError):
                raise RelayError(400, "invalid_json")

        def _dispatch(self, method):
            url = urlparse(self.path)
            parts = [p for p in url.path.split("/") if p]
            query = {k: v[0] for k, v in parse_qs(url.query).items()}
            try:
                # /api/sync/relay/ping
                if parts == ["api", "sync", "relay", "ping"] \
                        and method == "GET":
                    return self._send(200, state.ping())
                # /api/sync/groups[/{gid}[/meta|ops|snapshot]]
                if len(parts) >= 3 and parts[:3] == ["api", "sync", "groups"]:
                    gid = parts[3] if len(parts) > 3 else ""
                    tail = parts[4] if len(parts) > 4 else ""
                    if method == "PUT" and not tail:
                        return self._send(200, state.create_group(gid))
                    if method == "GET" and tail == "meta":
                        return self._send(200, state.group_meta(gid))
                    if method == "POST" and tail == "ops":
                        body = self._read_body()
                        if not isinstance(body, dict) \
                                or not isinstance(body.get("ops"), list):
                            raise RelayError(400, "invalid_op")
                        return self._send(200, state.append_ops(gid, body["ops"]))
                    if method == "GET" and tail == "ops":
                        since = _int_param(query, "since", 0)
                        limit = _int_param(query, "limit", DEFAULT_PULL_LIMIT)
                        return self._send(200, state.pull_ops(gid, since, limit))
                    if method == "PUT" and tail == "snapshot":
                        body = self._read_body()
                        if not isinstance(body, dict):
                            raise RelayError(400, "invalid_json")
                        return self._send(200, state.put_snapshot(
                            gid, body.get("up_to_seq"), body.get("payload")))
                    if method == "GET" and tail == "snapshot":
                        return self._send(200, state.get_snapshot(gid))
                raise RelayError(404, "not_found")
            except RelayError as e:
                return self._send(e.status, {"error": e.code})

        def do_GET(self):
            self._dispatch("GET")

        def do_POST(self):
            self._dispatch("POST")

        def do_PUT(self):
            self._dispatch("PUT")

        def log_message(self, fmt, *args):  # 安静模式（ctest 输出卫生）
            pass

    return Handler


def create_server(host="127.0.0.1", port=0, data_dir=None):
    """起服务（port=0 由内核挑空闲口）；返回 (server, state)。"""
    state = SyncRelayState(data_dir)
    server = ThreadingHTTPServer((host, port), make_handler(state))
    return server, state


def main(argv=None):
    ap = argparse.ArgumentParser(description="Unidict sync relay (dev)")
    ap.add_argument("--host", default="127.0.0.1",
                    help="绑定地址（默认 127.0.0.1，对外暴露是显式动作）")
    ap.add_argument("--port", type=int, default=8788)
    ap.add_argument("--data", default=None,
                    help="状态落盘目录（缺省纯内存）")
    args = ap.parse_args(argv)
    server, _ = create_server(args.host, args.port, args.data)
    host, port = server.server_address[:2]
    print(f"unidict sync relay (dev, protocol {PROTOCOL_VERSION}) "
          f"listening on http://{host}:{port}", file=sys.stderr)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
