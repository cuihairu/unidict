# Unidict 同步中转（sync relay）

个人词库同步的指令流中转面（server_plan.md §6/§7 B1）。职责只有三件事：
**收指令（op_id 幂等去重）、定序（组内全序 seq）、按位点增量拉取**——
外加快照存取（压缩指令流用）。

- 协议契约：[`PROTOCOL.md`](./PROTOCOL.md)（v1，客户端 B2 只依赖本文）
- 红线：**中转只见密文**（payload 是不透明 base64）；**同步默认关闭**是
  客户端侧契约，中转是惰性基础设施，没有客户端显式配置不会有任何数据流入
- 无账号：`group_id` 即 128 位 capability，知道即可读写该组

## 实现与部署形态

| 形态 | 目录 | 用途 |
|------|------|------|
| **本地 dev** | [`dev/`](./dev/) | 单进程 Python 参考实现（仅标准库），开发自测/局域网临时用 |
| **官方托管** | [`worker/`](./worker/) | Cloudflare Worker + D1，免费层零运维；开源可自建 |
| 自带中转（C++ `unidict-relay`） | 规划中（B5） | 与 core 同栈单二进制 |
| 局域网直传 | 规划中（B5） | 同网发现后直连 |

三种形态实现同一契约，验收口径 = 同一套契约符合性测试。

## 本地 dev 形态

```bash
# 纯内存（自测）
python3 server/sync_relay/dev/sync_relay_dev.py --port 8788

# 状态落盘（跨重启保留，原子写）
python3 server/sync_relay/dev/sync_relay_dev.py --data ~/.unidict/relay

# 探活
curl http://127.0.0.1:8788/api/sync/relay/ping
```

默认只听 `127.0.0.1`——对外暴露是显式动作（`--host 0.0.0.0`）；
不做内建 TLS，公网部署请前置反向代理（caddy/nginx）。

## 官方托管形态（Worker + D1）

```bash
cd server/sync_relay/worker
npx wrangler d1 create unidict-relay     # 把返回的 database_id 填入 wrangler.toml
npx wrangler d1 execute unidict-relay --remote --file=./schema.sql  # 可选,首个请求也会惰性建表
npx wrangler deploy
```

自建即免费层额度内零成本；托管方只见密文（E2E 在 B3 落地组密钥后成立）。

## 契约符合性测试

```bash
# dev 形态全契约（16 用例,真实 HTTP 往返）
python3 server/sync_relay/dev/test_relay_protocol.py

# Worker 形态(node ≥ 22, node:sqlite 做 D1 shim)
node server/sync_relay/worker/test/worker_test.mjs
```

dev 套件同时注册为 ctest `sync_relay_protocol`，随仓库门禁全量跑。
B5 的其余形态（C++ relay/局域网直传）落地时必须通过同一套断言。

## 配额与滥用

协议内建限额见 `PROTOCOL.md` §3（payload ≤256KiB、单 POST ≤256 ops、
拉取 ≤1000 条等）。官方托管形态可在此基础上加配额防滥用；自建形态不限。
