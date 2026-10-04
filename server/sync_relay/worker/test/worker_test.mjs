// Unidict 同步中转——Worker(D1)实现契约测试。
//
// 用 node:sqlite 做 D1 shim(事务化 batch + meta.changes 语义一致),
// 对 src/worker.js 的 fetch 跑与 dev/test_relay_protocol.py 同一批契约
// 断言(传输层差异:直接调 fetch,不过真实 HTTP)。
//
// 运行:node --test server/sync_relay/worker/test/
//      (Node ≥ 22,需 node:sqlite)

import { test } from "node:test";
import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
import worker from "../src/worker.js";

// ---- D1 shim:node:sqlite 之上的最小 D1 面 ----

class D1Shim {
  constructor(db) {
    this.db = db;
  }

  prepare(sql) {
    const db = this.db;
    let params = [];
    const runOne = () => {
      const stmt = db.prepare(sql);
      if (/^\s*select/i.test(sql)) {
        return { results: stmt.all(...params), meta: { changes: 0 } };
      }
      const info = stmt.run(...params);
      return { results: [], meta: { changes: Number(info.changes) } };
    };
    return {
      bind(...args) {
        params = args;
        return this;
      },
      _runOne: runOne, // batch 内部复用（事务路径）
      async run() {
        return runOne();
      },
      async all() {
        return runOne();
      },
      async first() {
        return runOne().results[0] ?? null;
      },
    };
  }

  async batch(statements) {
    // 与 D1 一致:batch 内语句串行执行于单个事务
    this.db.exec("BEGIN");
    try {
      const results = statements.map((s) => s._runOne());
      this.db.exec("COMMIT");
      return results;
    } catch (e) {
      this.db.exec("ROLLBACK");
      throw e;
    }
  }
}

// ---- 测试基建:每测例独立内存库,零串扰 ----

function mkEnv() {
  return { DB: new D1Shim(new DatabaseSync(":memory:")) };
}

async function call(env, method, path, body) {
  const req = new Request(`https://relay.test${path}`, {
    method,
    body: body === undefined ? undefined : JSON.stringify(body),
    headers: body === undefined ? {} : { "content-type": "application/json" },
  });
  const res = await worker.fetch(req, env);
  return [res.status, await res.json()];
}

const b64 = (n) => Buffer.from([n % 251, n % 251, n % 251, n % 251]).toString("base64");
const op = (id, n) => ({ op_id: id, device_id: id.split(":")[0], payload: b64(n) });

// ---- 契约断言 ----

test("ping 探活 + 协议版本", async () => {
  const [st, body] = await call(mkEnv(), "GET", "/api/sync/relay/ping");
  assert.equal(st, 200);
  assert.deepEqual(body, { service: "unidict-sync-relay", protocol: 1 });
});

test("建组幂等,初始 meta 全零", async () => {
  const env = mkEnv();
  const [st1, m1] = await call(env, "PUT", "/api/sync/groups/A0123456789abcdefghij");
  const [st2, m2] = await call(env, "PUT", "/api/sync/groups/A0123456789abcdefghij");
  assert.equal(st1, 200);
  assert.equal(st2, 200);
  assert.deepEqual(m1, m2);
  assert.deepEqual(
    [m1.latest_seq, m1.op_count, m1.snapshot_up_to_seq], [0, 0, 0]);
});

test("非法 group_id → 400", async () => {
  const env = mkEnv();
  for (const gid of ["short", "含中文的组标识符abc", "a".repeat(65)]) {
    const [st, body] = await call(env, "PUT", `/api/sync/groups/${gid}`);
    assert.equal(st, 400);
    assert.equal(body.error, "invalid_group_id");
  }
});

test("未知组 → 404 group_not_found(区别于存在但空)", async () => {
  const env = mkEnv();
  for (const [method, path, body] of [
    ["GET", "/api/sync/groups/B0123456789abcdefghij/meta"],
    ["GET", "/api/sync/groups/B0123456789abcdefghij/ops"],
    ["GET", "/api/sync/groups/B0123456789abcdefghij/snapshot"],
    ["PUT", "/api/sync/groups/B0123456789abcdefghij/snapshot",
      { up_to_seq: 1, payload: b64(1) }],
  ]) {
    const [st, resp] = await call(env, method, path, body);
    assert.deepEqual([st, resp.error], [404, "group_not_found"], path);
  }
});

test("追加自动建组 + 服务端定序", async () => {
  const env = mkEnv();
  const [st, r] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [op("devA:1", 1), op("devB:1", 2)] });
  assert.equal(st, 200);
  assert.deepEqual(r, {
    assigned: [{ op_id: "devA:1", seq: 1 }, { op_id: "devB:1", seq: 2 }],
    duplicate_op_ids: [],
  });
  const [, m] = await call(env, "GET", "/api/sync/groups/A0123456789abcdefghij/meta");
  assert.deepEqual([m.latest_seq, m.op_count], [2, 2]);
});

test("op_id 幂等去重:重发不追加,混合批只分配新的", async () => {
  const env = mkEnv();
  await call(env, "POST", "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [op("devA:1", 1)] });
  const [, r1] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops", { ops: [op("devA:1", 1)] });
  assert.deepEqual(r1, { assigned: [], duplicate_op_ids: ["devA:1"] });
  const [, r2] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [op("devA:1", 1), op("devA:2", 2)] });
  assert.deepEqual(r2.assigned, [{ op_id: "devA:2", seq: 2 }]);
  assert.deepEqual(r2.duplicate_op_ids, ["devA:1"]);
});

test("同请求内重复 op_id → 400", async () => {
  const env = mkEnv();
  const [st, body] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [op("devX:1", 1), op("devX:1", 1)] });
  assert.deepEqual([st, body.error], [400, "duplicate_op_id_in_request"]);
});

test("非法指令形态 → 400 invalid_op;超量 → too_many_ops", async () => {
  const env = mkEnv();
  const cases = [
    { ops: [] },
    { ops: [{ op_id: "d:1", device_id: "d" }] },                    // 缺 payload
    { ops: [{ op_id: "d:2", payload: b64(1) }] },                   // 缺 device_id
    { ops: [{ device_id: "d", payload: b64(1) }] },                 // 缺 op_id
    { ops: [{ op_id: "", device_id: "d", payload: b64(1) }] },      // 空 op_id
    { ops: [{ op_id: "d:3", device_id: "d", payload: "不是b64" }] }, // 非 base64
  ];
  for (const body of cases) {
    const [st, resp] = await call(env, "POST",
      "/api/sync/groups/A0123456789abcdefghij/ops", body);
    assert.deepEqual([st, resp.error], [400, "invalid_op"], JSON.stringify(body));
  }
  const many = Array.from({ length: 257 }, (_, i) => op(`d:${i}`, i));
  const [st, resp] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops", { ops: many });
  assert.deepEqual([st, resp.error], [400, "too_many_ops"]);
});

test("payload 超限 413,恰好限额可过", async () => {
  const env = mkEnv();
  const big = Buffer.alloc(256 * 1024 + 4, 120).toString("base64");
  const [st, body] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [{ op_id: "devL:1", device_id: "devL", payload: big }] });
  assert.deepEqual([st, body.error], [413, "payload_too_large"]);
  const atLimit = Buffer.alloc(192 * 1024, 120).toString("base64"); // 恰 256KiB
  const [st2] = await call(env, "POST",
    "/api/sync/groups/A0123456789abcdefghij/ops",
    { ops: [{ op_id: "devL:2", device_id: "devL", payload: atLimit }] });
  assert.equal(st2, 200);
});

test("增量拉取:位点语义/分页 has_more/limit 钳制/参数校验", async () => {
  const env = mkEnv();
  const gid = "C0123456789abcdefghij";
  const ops = Array.from({ length: 10 }, (_, i) => op(`devP:${i + 1}`, i + 1));
  await call(env, "POST", `/api/sync/groups/${gid}/ops`, { ops });

  const [, all] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=0`);
  assert.deepEqual(all.ops.map((o) => o.seq), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
  assert.deepEqual([all.cursor, all.has_more], [10, false]);
  const [, tail] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=7`);
  assert.deepEqual(tail.ops.map((o) => o.seq), [8, 9, 10]);
  const [, p1] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=0&limit=4`);
  assert.deepEqual([p1.ops.map((o) => o.seq), p1.has_more], [[1, 2, 3, 4], true]);
  const [, p2] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=4&limit=4`);
  assert.deepEqual(p2.ops.map((o) => o.seq), [5, 6, 7, 8]);
  const [, p3] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=8&limit=4`);
  assert.deepEqual([p3.ops.map((o) => o.seq), p3.has_more], [[9, 10], false]);
  const [, clamped] = await call(env, "GET", `/api/sync/groups/${gid}/ops?limit=99999`);
  assert.equal(clamped.ops.length, 10);
  const [st, errBody] = await call(env, "GET",
    `/api/sync/groups/${gid}/ops?since=abc`);
  assert.deepEqual([st, errBody.error], [400, "invalid_param"]);
  const [stNeg] = await call(env, "GET", `/api/sync/groups/${gid}/ops?since=-5`);
  assert.equal(stNeg, 200); // 负位点容错按 0

  await call(env, "PUT", "/api/sync/groups/D0123456789abcdefghij");
  const [, empty] = await call(env, "GET",
    "/api/sync/groups/D0123456789abcdefghij/ops");
  assert.deepEqual([empty.ops, empty.cursor], [[], 0]);
});

test("快照:往返/校验/最后写入者胜/meta 回显", async () => {
  const env = mkEnv();
  const gid = "E0123456789abcdefghij";
  await call(env, "POST", `/api/sync/groups/${gid}/ops`,
    { ops: Array.from({ length: 5 }, (_, i) => op(`devS:${i + 1}`, i + 1)) });
  const [st1, e1] = await call(env, "PUT", `/api/sync/groups/${gid}/snapshot`,
    { up_to_seq: 6, payload: b64(0) }); // 不能快照未来
  assert.deepEqual([st1, e1.error], [400, "invalid_snapshot"]);
  const [st2] = await call(env, "PUT", `/api/sync/groups/${gid}/snapshot`,
    { up_to_seq: 0, payload: b64(0) });
  assert.equal(st2, 400);
  const [st3, r3] = await call(env, "PUT", `/api/sync/groups/${gid}/snapshot`,
    { up_to_seq: 3, payload: b64(9) });
  assert.deepEqual([st3, r3], [200, { up_to_seq: 3 }]);
  const [, m] = await call(env, "GET", `/api/sync/groups/${gid}/meta`);
  assert.equal(m.snapshot_up_to_seq, 3);
  const [, got] = await call(env, "GET", `/api/sync/groups/${gid}/snapshot`);
  assert.deepEqual(got, { up_to_seq: 3, payload: b64(9) });
  await call(env, "PUT", `/api/sync/groups/${gid}/snapshot`,
    { up_to_seq: 5, payload: b64(8) });
  const [, replaced] = await call(env, "GET", `/api/sync/groups/${gid}/snapshot`);
  assert.equal(replaced.up_to_seq, 5);
});

test("快照空洞拉取规则:位点落后于快照覆盖位 → 先快照后增量", async () => {
  const env = mkEnv();
  const gid = "F0123456789abcdefghij";
  await call(env, "POST", `/api/sync/groups/${gid}/ops`,
    { ops: Array.from({ length: 6 }, (_, i) => op(`devH:${i + 1}`, i + 1)) });
  await call(env, "PUT", `/api/sync/groups/${gid}/snapshot`,
    { up_to_seq: 4, payload: b64(7) });
  // 落后设备(位点 0)按 PROTOCOL §2.8 决策
  const [, meta] = await call(env, "GET", `/api/sync/groups/${gid}/meta`);
  let base = 0;
  let snapshot = null;
  if (0 < meta.snapshot_up_to_seq) {
    const [, s] = await call(env, "GET", `/api/sync/groups/${gid}/snapshot`);
    snapshot = s;
    base = s.up_to_seq;
  }
  const [, r] = await call(env, "GET",
    `/api/sync/groups/${gid}/ops?since=${base}`);
  assert.equal(base, 4);
  assert.equal(snapshot.up_to_seq, 4);
  assert.deepEqual(r.ops.map((o) => o.seq), [5, 6]);
});

test("交错追加保持全序(两设备多批)", async () => {
  const env = mkEnv();
  const gid = "G0123456789abcdefghij";
  let expect = 0;
  for (let i = 0; i < 5; i++) {
    const [, r1] = await call(env, "POST", `/api/sync/groups/${gid}/ops`,
      { ops: [op(`devA:${i}`, i)] });
    expect += 1;
    assert.deepEqual(r1.assigned.map((a) => a.seq), [expect]);
    const [, r2] = await call(env, "POST", `/api/sync/groups/${gid}/ops`,
      { ops: [op(`devB:${i}`, i)] });
    expect += 1;
    assert.deepEqual(r2.assigned.map((a) => a.seq), [expect]);
  }
  const [, all] = await call(env, "GET", `/api/sync/groups/${gid}/ops`);
  assert.deepEqual(all.ops.map((o) => o.seq), [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
});

test("未知路由 → 404 not_found", async () => {
  const env = mkEnv();
  const [st, body] = await call(env, "GET", "/api/other");
  assert.deepEqual([st, body.error], [404, "not_found"]);
});
