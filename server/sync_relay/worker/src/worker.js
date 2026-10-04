// Unidict 同步中转——Cloudflare Worker + D1（官方托管形态参考实现）。
//
// 与 dev/sync_relay_dev.py 实现 PROTOCOL.md v1 同一契约；契约符合性
// 验收口径是 dev/test_relay_protocol.py，本实现的单测（test/worker_
// test.mjs，node:sqlite 做 D1 shim）覆盖同一批契约断言。
//
// 定序保证（PROTOCOL.md §4）：D1 是单写者 SQLite，batch() 内语句
// 串行执行于一个事务；seq 由 `MAX(seq)+1` 子查询分配 + 主键 (gid,seq)
// 兜底，op_id 由 UNIQUE(gid,op_id) 幂等（INSERT ... DO NOTHING +
// meta.changes 判定是否新分配）。
//
// 红线（PROTOCOL.md §0）：中转只见密文——payload 是不透明 base64，
// 本实现只做形态校验（长度/字符集），不解析、不索引。

const PROTOCOL_VERSION = 1;

// PROTOCOL.md §3 限额（与 dev 形态一致）
const GID_RE = /^[A-Za-z0-9_-]{16,64}$/;
const B64_RE = /^[A-Za-z0-9+/=]*$/;
const MAX_PAYLOAD_B64 = 256 * 1024;
const MAX_OPS_PER_POST = 256;
const DEFAULT_PULL_LIMIT = 200;
const MAX_PULL_LIMIT = 1000;
const MAX_BODY_BYTES = 64 * 1024 * 1024;
const OP_ID_MAX = 200;
const DEVICE_ID_MAX = 128;
// 读回 op_id→seq 的 IN 查询分块：D1 单语句绑定参数上限 100，留 gid 一位
const IN_CHUNK = 90;

// 与 schema.sql 保持一致；首个请求惰性执行（每 isolate 一次），
// 自建部署零手工建表步骤。
const SCHEMA = [
  `CREATE TABLE IF NOT EXISTS groups (
     gid TEXT PRIMARY KEY,
     created_at INTEGER NOT NULL)`,
  `CREATE TABLE IF NOT EXISTS ops (
     gid TEXT NOT NULL,
     seq INTEGER NOT NULL,
     op_id TEXT NOT NULL,
     device_id TEXT NOT NULL,
     ts INTEGER NOT NULL,
     payload TEXT NOT NULL,
     PRIMARY KEY (gid, seq),
     UNIQUE (gid, op_id))`,
  `CREATE TABLE IF NOT EXISTS snapshots (
     gid TEXT PRIMARY KEY,
     up_to_seq INTEGER NOT NULL,
     ts INTEGER NOT NULL,
     payload TEXT NOT NULL)`,
];

class HttpError extends Error {
  constructor(status, code) {
    super(code);
    this.status = status;
    this.code = code;
  }
}

const ok = (obj) => Response.json(obj);
const err = (status, code) => Response.json({ error: code }, { status });

const mustGid = (gid) => {
  if (typeof gid !== "string" || !GID_RE.test(gid)) {
    throw new HttpError(400, "invalid_group_id");
  }
};

const now = () => Math.floor(Date.now() / 1000);

async function readJson(request) {
  const len = Number(request.headers.get("content-length") || 0);
  if (len > MAX_BODY_BYTES) throw new HttpError(413, "payload_too_large");
  let raw;
  try {
    raw = await request.text();
  } catch {
    throw new HttpError(400, "invalid_json");
  }
  if (raw.length > MAX_BODY_BYTES) throw new HttpError(413, "payload_too_large");
  try {
    return JSON.parse(raw);
  } catch {
    throw new HttpError(400, "invalid_json");
  }
}

function intParam(sp, name, fallback) {
  const raw = sp.get(name);
  if (raw === null) return fallback;
  const n = Number(raw);
  if (!Number.isFinite(n) || !Number.isInteger(n)) {
    throw new HttpError(400, "invalid_param");
  }
  return n;
}

function validateOp(op) {
  if (typeof op !== "object" || op === null) throw new HttpError(400, "invalid_op");
  for (const [val, cap] of [[op.op_id, OP_ID_MAX], [op.device_id, DEVICE_ID_MAX]]) {
    if (typeof val !== "string" || !val || val.length > cap) {
      throw new HttpError(400, "invalid_op");
    }
  }
  if (typeof op.payload !== "string" || !op.payload) {
    throw new HttpError(400, "invalid_op");
  }
  if (op.payload.length > MAX_PAYLOAD_B64) {
    throw new HttpError(413, "payload_too_large"); // 尺寸超限是 413，不是 400
  }
  if (!B64_RE.test(op.payload)) throw new HttpError(400, "invalid_op");
}

async function ensureSchema(env) {
  if (!env.__schemaReady) {
    env.__schemaReady = env.DB.batch(SCHEMA.map((s) => env.DB.prepare(s)));
  }
  await env.__schemaReady;
}

async function metaOf(env, gid) {
  const row = await env.DB.prepare(
    `SELECT g.gid,
            (SELECT COALESCE(MAX(seq), 0) FROM ops WHERE gid = g.gid) AS latest_seq,
            (SELECT COUNT(*) FROM ops WHERE gid = g.gid) AS op_count,
            COALESCE((SELECT up_to_seq FROM snapshots WHERE gid = g.gid), 0) AS snapshot_up_to_seq,
            g.created_at
     FROM groups g WHERE g.gid = ?1`)
    .bind(gid).first();
  if (!row) throw new HttpError(404, "group_not_found");
  return row;
}

async function createGroup(env, gid) {
  mustGid(gid);
  await env.DB.batch([
    env.DB.prepare("INSERT OR IGNORE INTO groups (gid, created_at) VALUES (?1, ?2)")
      .bind(gid, now()),
  ]);
  return ok(await metaOf(env, gid));
}

async function appendOps(env, gid, request) {
  mustGid(gid);
  const body = await readJson(request);
  const ops = body === null || typeof body !== "object" ? undefined : body.ops;
  if (!Array.isArray(ops) || ops.length === 0) throw new HttpError(400, "invalid_op");
  if (ops.length > MAX_OPS_PER_POST) throw new HttpError(400, "too_many_ops");
  const seen = new Set();
  for (const op of ops) {
    validateOp(op);
    if (seen.has(op.op_id)) throw new HttpError(400, "duplicate_op_id_in_request");
    seen.add(op.op_id);
  }

  // 一个 batch = 一个事务：建组（自动，幂等）→ 逐条 MAX(seq)+1 插入
  // （已存在的 op_id 被 DO NOTHING 跳过，不计新 seq）→ 分块读回新 seq。
  const ts = now();
  const stmts = [
    env.DB.prepare("INSERT OR IGNORE INTO groups (gid, created_at) VALUES (?1, ?2)")
      .bind(gid, ts),
    ...ops.map((op) => env.DB.prepare(
      `INSERT INTO ops (gid, seq, op_id, device_id, ts, payload)
       VALUES (?1, (SELECT COALESCE(MAX(seq), 0) + 1 FROM ops WHERE gid = ?1),
               ?2, ?3, ?4, ?5)
       ON CONFLICT(gid, op_id) DO NOTHING`)
      .bind(gid, op.op_id, op.device_id, ts, op.payload)),
  ];
  for (let i = 0; i < ops.length; i += IN_CHUNK) {
    const chunk = ops.slice(i, i + IN_CHUNK);
    stmts.push(env.DB.prepare(
      `SELECT op_id, seq FROM ops WHERE gid = ? AND op_id IN (${chunk.map(() => "?").join(",")})`)
      .bind(gid, ...chunk.map((o) => o.op_id)));
  }
  const results = await env.DB.batch(stmts);

  // meta.changes==0 → 该 op_id 已存在（幂等去重）
  const fresh = new Set();
  for (let i = 0; i < ops.length; i++) {
    if (results[1 + i].meta.changes > 0) fresh.add(ops[i].op_id);
  }
  const seqById = new Map();
  for (let i = 0; i < ops.length; i += IN_CHUNK) {
    for (const row of results[1 + ops.length + i / IN_CHUNK].results) {
      seqById.set(row.op_id, row.seq);
    }
  }
  return ok({
    assigned: ops.filter((o) => fresh.has(o.op_id))
      .map((o) => ({ op_id: o.op_id, seq: seqById.get(o.op_id) })),
    duplicate_op_ids: ops.map((o) => o.op_id).filter((id) => !fresh.has(id)),
  });
}

async function pullOps(env, gid, sp) {
  mustGid(gid);
  const since = intParam(sp, "since", 0);
  const limit = intParam(sp, "limit", DEFAULT_PULL_LIMIT);
  const lim = Math.min(Math.max(1, limit), MAX_PULL_LIMIT);
  const [existsRes, opsRes, latestRes] = await env.DB.batch([
    env.DB.prepare("SELECT 1 AS exists_ FROM groups WHERE gid = ?1").bind(gid),
    env.DB.prepare(
      `SELECT seq, op_id, device_id, ts, payload FROM ops
       WHERE gid = ?1 AND seq > ?2 ORDER BY seq LIMIT ?3`)
      .bind(gid, Math.max(0, since), lim),
    env.DB.prepare("SELECT COALESCE(MAX(seq), 0) AS m FROM ops WHERE gid = ?1").bind(gid),
  ]);
  if (!existsRes.results[0]) throw new HttpError(404, "group_not_found");
  const ops = opsRes.results;
  const latest = latestRes.results[0].m;
  return ok({
    gid,
    ops,
    cursor: latest, // 组内当前最大 seq（非本次返回的最大值，PROTOCOL §2.5）
    has_more: ops.length > 0 && ops[ops.length - 1].seq < latest,
  });
}

async function putSnapshot(env, gid, request) {
  mustGid(gid);
  const body = await readJson(request);
  const upTo = body === null || typeof body !== "object" ? undefined : body.up_to_seq;
  const payload = body === null || typeof body !== "object" ? undefined : body.payload;
  const g = await env.DB.prepare(
    `SELECT COALESCE((SELECT MAX(seq) FROM ops WHERE gid = ?1), 0) AS latest
     FROM groups WHERE gid = ?1`).bind(gid).first();
  if (!g) throw new HttpError(404, "group_not_found");
  if (!Number.isInteger(upTo) || upTo < 1 || upTo > g.latest) {
    throw new HttpError(400, "invalid_snapshot"); // 不能快照未来（PROTOCOL §2.6）
  }
  if (typeof payload !== "string" || !payload
    || payload.length > MAX_PAYLOAD_B64 || !B64_RE.test(payload)) {
    throw new HttpError(400, "invalid_op");
  }
  await env.DB.batch([ // 最后写入者胜：整体替换旧快照
    env.DB.prepare(
      `INSERT INTO snapshots (gid, up_to_seq, ts, payload) VALUES (?1, ?2, ?3, ?4)
       ON CONFLICT(gid) DO UPDATE SET up_to_seq = ?2, ts = ?3, payload = ?4`)
      .bind(gid, upTo, now(), payload),
  ]);
  return ok({ up_to_seq: upTo });
}

async function getSnapshot(env, gid) {
  mustGid(gid);
  const g = await env.DB.prepare("SELECT 1 AS exists_ FROM groups WHERE gid = ?1")
    .bind(gid).first();
  if (!g) throw new HttpError(404, "group_not_found");
  const s = await env.DB.prepare("SELECT up_to_seq, payload FROM snapshots WHERE gid = ?1")
    .bind(gid).first();
  if (!s) throw new HttpError(404, "snapshot_not_found");
  return ok(s);
}

export default {
  async fetch(request, env) {
    await ensureSchema(env);
    const url = new URL(request.url);
    const parts = url.pathname.split("/").filter(Boolean);
    try {
      if (parts.length === 4 && parts[0] === "api" && parts[1] === "sync"
        && parts[2] === "relay" && parts[3] === "ping" && request.method === "GET") {
        return ok({ service: "unidict-sync-relay", protocol: PROTOCOL_VERSION });
      }
      if (parts.length >= 3 && parts[0] === "api" && parts[1] === "sync"
        && parts[2] === "groups") {
        const gid = parts[3] ?? "";
        const tail = parts[4] ?? "";
        if (request.method === "PUT" && !tail) return await createGroup(env, gid);
        if (request.method === "GET" && tail === "meta") {
          mustGid(gid);
          return ok(await metaOf(env, gid));
        }
        if (request.method === "POST" && tail === "ops") {
          return await appendOps(env, gid, request);
        }
        if (request.method === "GET" && tail === "ops") {
          return await pullOps(env, gid, url.searchParams);
        }
        if (request.method === "PUT" && tail === "snapshot") {
          return await putSnapshot(env, gid, request);
        }
        if (request.method === "GET" && tail === "snapshot") {
          return await getSnapshot(env, gid);
        }
      }
      return err(404, "not_found");
    } catch (e) {
      if (e instanceof HttpError) return err(e.status, e.code);
      return err(500, "internal_error");
    }
  },
};
