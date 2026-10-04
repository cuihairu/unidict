-- Unidict 同步中转——Cloudflare D1 schema（PROTOCOL.md v1）
-- 与 src/worker.js 内 SCHEMA 数组保持一致（worker 首个请求会惰性
-- 执行 IF NOT EXISTS 建表；此文件供手动初始化/审阅）：
--   wrangler d1 execute unidict-relay --remote --file=./schema.sql
--
-- 定序与幂等：
--   ops 主键 (gid, seq) —— 组内 seq 单调无空洞（batch 事务内
--   MAX(seq)+1 插入，D1 单写者保证事务串行）；
--   UNIQUE (gid, op_id) —— op_id 幂等去重键，重放 INSERT 被忽略。

CREATE TABLE IF NOT EXISTS groups (
    gid        TEXT PRIMARY KEY,          -- 128-bit capability（base64url）
    created_at INTEGER NOT NULL           -- unix 秒
);

CREATE TABLE IF NOT EXISTS ops (
    gid       TEXT NOT NULL,
    seq       INTEGER NOT NULL,           -- 组内全序，从 1 起单调
    op_id     TEXT NOT NULL,              -- 组内唯一，幂等去重键
    device_id TEXT NOT NULL,
    ts        INTEGER NOT NULL,           -- unix 秒（服务端到达时刻）
    payload   TEXT NOT NULL,              -- base64 密文（中转不解析）
    PRIMARY KEY (gid, seq),
    UNIQUE (gid, op_id)
);

CREATE TABLE IF NOT EXISTS snapshots (
    gid       TEXT PRIMARY KEY,           -- 每组至多一份，最后写入者胜
    up_to_seq INTEGER NOT NULL,           -- 快照覆盖 ≤up_to_seq 的全部指令
    ts        INTEGER NOT NULL,
    payload   TEXT NOT NULL
);
