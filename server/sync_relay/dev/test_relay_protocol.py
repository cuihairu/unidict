#!/usr/bin/env python3
"""同步中转协议 v1 契约符合性测试（PROTOCOL.md）。

对本目录的 dev 参考实现跑全契约（真实 HTTP 往返）：探活/建组幂等/
自动建组/服务端定序全序无重号/op_id 幂等去重/位点增量拉取分页/快照
存取与空洞拉取规则/限额与校验/并发追加不串号/落盘重启续存。

ctest 注册名 sync_relay_protocol；也可直接运行：
    python3 server/sync_relay/dev/test_relay_protocol.py
同一契约也约束官方托管形态（worker/，其单测见 worker/test/）与 C++ 版
relay（unidict-relay，ctest 注册名 sync_relay_cpp_protocol，经
UNIDICT_RELAY_EXTERNAL_BASE 环境变量把用例打到外部被测进程）。
"""

import base64
import json
import os
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sync_relay_dev import create_server  # noqa: E402

GID = "A" + "0123456789abcdefghij"  # 21 字符，符合 ^[A-Za-z0-9_-]{16,64}$
GID2 = "B" + "0123456789abcdefghij"


def b64(n):
    return base64.b64encode(bytes([n % 251] * 8)).decode()


class RelayProtocolTest(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # 外部被测实现（C++ relay 等）：设 UNIDICT_RELAY_EXTERNAL_BASE 即
        # 全量用例打到该地址——同一契约约束所有形态（PROTOCOL.md §4）
        external = os.environ.get("UNIDICT_RELAY_EXTERNAL_BASE")
        if external:
            cls.base = external
            return
        cls.server, _ = create_server("127.0.0.1", 0)
        cls.base = f"http://127.0.0.1:{cls.server.server_address[1]}"
        threading.Thread(target=cls.server.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        if not getattr(cls, "server", None):
            return
        cls.server.shutdown()
        cls.server.server_close()

    # ---- HTTP 助手：返回 (status, dict) ----

    def req(self, method, path, body=None):
        url = self.base + path
        data = json.dumps(body).encode() if body is not None else None
        r = urllib.request.Request(url, data=data, method=method)
        if data:
            r.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(r) as resp:
                return resp.status, json.loads(resp.read().decode())
        except urllib.error.HTTPError as e:
            with e:  # 显式关闭,避免 ResourceWarning 污染 ctest 输出
                return e.code, json.loads(e.read().decode())

    # ---- 探活 / 组生命周期 ----

    def test_ping(self):
        st, body = self.req("GET", "/api/sync/relay/ping")
        self.assertEqual(st, 200)
        self.assertEqual(body["service"], "unidict-sync-relay")
        self.assertEqual(body["protocol"], 1)

    def test_create_group_idempotent(self):
        gid = "J" + "0123456789abcdefghij"  # 独立组:共享 GID 已被追加用例推进
        st, m1 = self.req("PUT", f"/api/sync/groups/{gid}")
        self.assertEqual(st, 200)
        st, m2 = self.req("PUT", f"/api/sync/groups/{gid}")
        self.assertEqual((st, m2["latest_seq"]), (200, 0))
        self.assertEqual(m1, m2)

    def test_invalid_group_id(self):
        st, body = self.req("PUT", "/api/sync/groups/short")
        self.assertEqual((st, body["error"]), (400, "invalid_group_id"))
        st, body = self.req("PUT", "/api/sync/groups/..%2Fescape")
        self.assertEqual(st, 400)

    def test_unknown_group_404(self):
        for method, path in (("GET", f"/api/sync/groups/{GID2}/meta"),
                             ("GET", f"/api/sync/groups/{GID2}/ops"),
                             ("GET", f"/api/sync/groups/{GID2}/snapshot"),
                             ("PUT", f"/api/sync/groups/{GID2}/snapshot")):
            st, body = self.req(method, path,
                                {"up_to_seq": 1, "payload": b64(1)}
                                if method == "PUT" else None)
            self.assertEqual((st, body["error"]), (404, "group_not_found"),
                             f"{method} {path}")

    # ---- 追加 / 定序 / 幂等 ----

    def test_append_auto_create_and_sequence(self):
        st, r = self.req("POST", f"/api/sync/groups/{GID}/ops", {"ops": [
            {"op_id": "devA:1", "device_id": "devA", "payload": b64(1)},
            {"op_id": "devB:1", "device_id": "devB", "payload": b64(2)},
        ]})
        self.assertEqual(st, 200)
        self.assertEqual([a["seq"] for a in r["assigned"]], [1, 2])
        self.assertEqual(r["duplicate_op_ids"], [])
        st, m = self.req("GET", f"/api/sync/groups/{GID}/meta")
        self.assertEqual((m["latest_seq"], m["op_count"]), (2, 2))

    def test_idempotent_repost(self):
        op = {"op_id": "devA:2", "device_id": "devA", "payload": b64(3)}
        self.req("POST", f"/api/sync/groups/{GID}/ops", {"ops": [op]})
        st, m_before = self.req("GET", f"/api/sync/groups/{GID}/meta")
        # 同 op 重发（网络重试场景）→ 不追加不计新 seq
        st, r = self.req("POST", f"/api/sync/groups/{GID}/ops",
                         {"ops": [op]})
        self.assertEqual((st, r["assigned"]), (200, []))
        self.assertEqual(r["duplicate_op_ids"], ["devA:2"])
        # 新旧混合一批：只有新的分配 seq
        st, r = self.req("POST", f"/api/sync/groups/{GID}/ops", {"ops": [
            op,
            {"op_id": "devA:3", "device_id": "devA", "payload": b64(4)},
        ]})
        self.assertEqual([a["op_id"] for a in r["assigned"]], ["devA:3"])
        self.assertEqual(r["duplicate_op_ids"], ["devA:2"])
        st, m_after = self.req("GET", f"/api/sync/groups/{GID}/meta")
        self.assertEqual(m_after["op_count"], m_before["op_count"] + 1)

    def test_duplicate_within_request_400(self):
        op = {"op_id": "devX:1", "device_id": "devX", "payload": b64(1)}
        st, body = self.req("POST", f"/api/sync/groups/{GID}/ops",
                            {"ops": [op, dict(op)]})
        self.assertEqual((st, body["error"]),
                         (400, "duplicate_op_id_in_request"))

    def test_invalid_ops(self):
        cases = [
            {"op_id": "d:1", "device_id": "d"},                # 缺 payload
            {"op_id": "d:2", "payload": b64(1)},               # 缺 device_id
            {"device_id": "d", "payload": b64(1)},             # 缺 op_id
            {"op_id": "d:3", "device_id": "d", "payload": "!!"},  # 非 base64
            {"op_id": "", "device_id": "d", "payload": b64(1)},   # 空 op_id
            {"op_id": "d:4", "device_id": "d", "payload": "ab&cd"},  # 非 b64 字符
        ]
        for op in cases:
            st, body = self.req("POST", f"/api/sync/groups/{GID}/ops",
                                {"ops": [op]})
            self.assertEqual((st, body["error"]), (400, "invalid_op"),
                             str(op))
        st, body = self.req("POST", f"/api/sync/groups/{GID}/ops",
                            {"ops": []})
        self.assertEqual(st, 400)

    # ---- 拉取 / 位点 / 分页 ----

    def test_pull_since_and_pagination(self):
        gid = "C" + "0123456789abcdefghij"
        ops = [{"op_id": f"devP:{i}", "device_id": "devP", "payload": b64(i)}
               for i in range(1, 11)]
        self.req("POST", f"/api/sync/groups/{gid}/ops", {"ops": ops})
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=0")
        self.assertEqual([o["seq"] for o in r["ops"]], list(range(1, 11)))
        self.assertEqual((r["cursor"], r["has_more"]), (10, False))
        # 位点语义：since=N 严格返回 seq>N
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=7")
        self.assertEqual([o["seq"] for o in r["ops"]], [8, 9, 10])
        self.assertEqual([o["op_id"] for o in r["ops"]],
                         ["devP:8", "devP:9", "devP:10"])
        # 分页：limit 截断 + has_more 续拉
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=0&limit=4")
        self.assertEqual([o["seq"] for o in r["ops"]], [1, 2, 3, 4])
        self.assertTrue(r["has_more"])
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=4&limit=4")
        self.assertEqual([o["seq"] for o in r["ops"]], [5, 6, 7, 8])
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=8&limit=4")
        self.assertEqual([o["seq"] for o in r["ops"]], [9, 10])
        self.assertFalse(r["has_more"])
        # limit 超上限钳制、非法参数 400、负位点容错为 0
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?limit=99999")
        self.assertEqual(len(r["ops"]), 10)
        st, body = self.req("GET", f"/api/sync/groups/{gid}/ops?since=abc")
        self.assertEqual((st, body["error"]), (400, "invalid_param"))
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since=-5")
        self.assertEqual(st, 200)

    def test_pull_empty_existing_group(self):
        gid = "D" + "0123456789abcdefghij"
        self.req("PUT", f"/api/sync/groups/{gid}")
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops")
        self.assertEqual((st, r["ops"], r["cursor"]), (200, [], 0))

    # ---- 限额 ----

    def test_payload_limit_413(self):
        big = base64.b64encode(b"x" * (256 * 1024)).decode() + "x"  # 超限 1 字符
        st, body = self.req("POST", f"/api/sync/groups/{GID}/ops", {"ops": [
            {"op_id": "devL:1", "device_id": "devL", "payload": big}]})
        self.assertEqual((st, body["error"]), (413, "payload_too_large"))
        at_limit = base64.b64encode(b"x" * (192 * 1024)).decode()  # 恰好 256KiB
        st, r = self.req("POST", f"/api/sync/groups/{GID}/ops", {"ops": [
            {"op_id": "devL:2", "device_id": "devL", "payload": at_limit}]})
        self.assertEqual(st, 200)

    def test_too_many_ops_400(self):
        ops = [{"op_id": f"devM:{i}", "device_id": "devM", "payload": b64(i)}
               for i in range(257)]
        st, body = self.req("POST", f"/api/sync/groups/{GID}/ops",
                            {"ops": ops})
        self.assertEqual((st, body["error"]), (400, "too_many_ops"))

    # ---- 快照与空洞拉取规则 ----

    def test_snapshot_roundtrip_and_validation(self):
        gid = "E" + "0123456789abcdefghij"
        self.req("POST", f"/api/sync/groups/{gid}/ops", {"ops": [
            {"op_id": f"devS:{i}", "device_id": "devS", "payload": b64(i)}
            for i in range(1, 6)]})
        st, body = self.req("PUT", f"/api/sync/groups/{gid}/snapshot",
                            {"up_to_seq": 6, "payload": b64(0)})
        self.assertEqual((st, body["error"]), (400, "invalid_snapshot"))
        st, body = self.req("PUT", f"/api/sync/groups/{gid}/snapshot",
                            {"up_to_seq": 0, "payload": b64(0)})
        self.assertEqual(st, 400)
        st, r = self.req("PUT", f"/api/sync/groups/{gid}/snapshot",
                         {"up_to_seq": 3, "payload": b64(9)})
        self.assertEqual((st, r["up_to_seq"]), (200, 3))
        st, m = self.req("GET", f"/api/sync/groups/{gid}/meta")
        self.assertEqual(m["snapshot_up_to_seq"], 3)
        st, r = self.req("GET", f"/api/sync/groups/{gid}/snapshot")
        self.assertEqual((st, r["up_to_seq"], r["payload"]), (200, 3, b64(9)))
        # 最后写入者胜
        self.req("PUT", f"/api/sync/groups/{gid}/snapshot",
                 {"up_to_seq": 5, "payload": b64(8)})
        st, r = self.req("GET", f"/api/sync/groups/{gid}/snapshot")
        self.assertEqual(r["up_to_seq"], 5)

    def test_snapshot_hole_pull_rule(self):
        """PROTOCOL §2.8：位点落后于快照覆盖位 → 先快照后增量。"""
        gid = "F" + "0123456789abcdefghij"
        self.req("POST", f"/api/sync/groups/{gid}/ops", {"ops": [
            {"op_id": f"devH:{i}", "device_id": "devH", "payload": b64(i)}
            for i in range(1, 7)]})
        self.req("PUT", f"/api/sync/groups/{gid}/snapshot",
                 {"up_to_seq": 4, "payload": b64(7)})
        # 模拟落后设备（位点 0）的拉取决策
        st, meta = self.req("GET", f"/api/sync/groups/{gid}/meta")
        my_cursor = 0
        base = my_cursor
        snapshot = None
        if my_cursor < meta["snapshot_up_to_seq"]:
            st, snapshot = self.req("GET", f"/api/sync/groups/{gid}/snapshot")
            base = snapshot["up_to_seq"]
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops?since={base}")
        self.assertEqual(base, 4)
        self.assertEqual(snapshot["up_to_seq"], 4)
        self.assertEqual([o["seq"] for o in r["ops"]], [5, 6])

    # ---- 并发：全序无重号 ----

    def test_concurrent_appends_total_order(self):
        gid = "G" + "0123456789abcdefghij"
        n_threads, per_thread = 8, 25

        def worker(t):
            ops = [{"op_id": f"dev{t}:{i}", "device_id": f"dev{t}",
                    "payload": b64(i)} for i in range(per_thread)]
            st, r = self.req("POST", f"/api/sync/groups/{gid}/ops",
                             {"ops": ops})
            assert st == 200
            return [a["seq"] for a in r["assigned"]]

        with ThreadPoolExecutor(max_workers=n_threads) as ex:
            all_seqs = [s for seqs in ex.map(worker, range(n_threads))
                        for s in seqs]
        self.assertEqual(sorted(all_seqs),
                         list(range(1, n_threads * per_thread + 1)))
        st, r = self.req("GET", f"/api/sync/groups/{gid}/ops")
        ids = [o["op_id"] for o in r["ops"]]
        self.assertEqual(len(ids), len(set(ids)))   # 无重复指令
        seqs = [o["seq"] for o in r["ops"]]
        self.assertEqual(seqs, sorted(seqs))        # 升序全序

    # ---- 落盘重启续存（--data 形态） ----

    def test_persistence_across_restart(self):
        gid = "H" + "0123456789abcdefghij"
        with tempfile.TemporaryDirectory() as d:
            srv1, _ = create_server("127.0.0.1", 0, data_dir=d)
            threading.Thread(target=srv1.serve_forever, daemon=True).start()
            base1 = f"http://127.0.0.1:{srv1.server_address[1]}"
            body = json.dumps({"ops": [
                {"op_id": "devZ:1", "device_id": "devZ", "payload": b64(5)}]
            }).encode()
            req = urllib.request.Request(
                f"{base1}/api/sync/groups/{gid}/ops", data=body, method="POST")
            req.add_header("Content-Type", "application/json")
            with urllib.request.urlopen(req) as resp:
                self.assertEqual(resp.status, 200)
            srv1.shutdown()
            srv1.server_close()

            srv2, _ = create_server("127.0.0.1", 0, data_dir=d)
            threading.Thread(target=srv2.serve_forever, daemon=True).start()
            base2 = f"http://127.0.0.1:{srv2.server_address[1]}"
            with urllib.request.urlopen(
                    f"{base2}/api/sync/groups/{gid}/meta") as resp:
                m = json.loads(resp.read().decode())
            self.assertEqual((m["latest_seq"], m["op_count"]), (1, 1))
            srv2.shutdown()
            srv2.server_close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
