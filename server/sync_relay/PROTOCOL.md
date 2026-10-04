# Unidict 同步中转协议 v1（sync relay protocol）

> server_plan.md §7 B1 的契约面。三种部署形态（官方托管 Worker / 自带
> 中转 / 局域网直传）实现同一份契约；客户端（B2）只依赖本文，不依赖
> 任何具体形态。

## 0. 红线（全程有效）

- **中转只见密文**：`payload` 对中转是不透明 base64 串（B3 起为组密钥
  对称加密产物）。中转不解析、不索引、不做任何明文派生。
- **同步默认关闭**是客户端侧契约（B2/B7 落地）：中转本身是惰性基础设施，
  没有客户端显式配置就不会有任何数据流入。
- 不做内建 TLS（§3.1 口径：反代/隧道负责）；局域网形态自定传输安全。

## 1. 概念

| 概念 | 定义 |
|------|------|
| **同步组 group** | 共享指令流的设备集合。`group_id` = 客户端生成的 128 位随机 capability（base64url，22 字符）——**知道 group_id 即可读写该组**（密文无意义，B3 的组密钥才是数据边界；B3 预留请求签名防注入垃圾） |
| **指令 op** | `{"op_id","device_id","payload"}`。`payload` = base64 密文；`op_id` = 客户端保证组内唯一（契约形态 `"<device_id>:<本地序号>"`），是幂等去重键 |
| **binlog** | 组内全部指令按服务端到达定序的全序列表，`seq` 从 1 起单调递增。**回放即一致**：客户端从位点拉取重放 |
| **位点 cursor** | 客户端已重放到的最大 `seq`。增量拉取 = `GET ops?since=cursor` |
| **快照 snapshot** | 客户端上传的状态快照（base64 密文）+ 覆盖位点。用于压缩指令流（B2）与落后设备跳变（位点落后于快照覆盖位时，先取快照再从 `snapshot_up_to_seq` 续拉） |

## 2. 端点（全部 JSON over HTTP）

统一前缀 `/api/sync`。错误统一形态 `{"error": "<机器可读码>"}` + HTTP 状态码。

### 2.1 `GET /api/sync/relay/ping`

探活/协议版本协商。

```json
{"service": "unidict-sync-relay", "protocol": 1}
```

### 2.2 `PUT /api/sync/groups/{gid}`

显式建组（幂等：已存在则原样成功）。返回组元信息（同 2.3）。

### 2.3 `GET /api/sync/groups/{gid}/meta`

```json
{"gid": "…", "latest_seq": 42, "op_count": 42,
 "snapshot_up_to_seq": 30, "created_at": 1733300000}
```

无快照时 `snapshot_up_to_seq` 为 0。组不存在 → 404 `{"error":"group_not_found"}`。

### 2.4 `POST /api/sync/groups/{gid}/ops`

追加指令（**组不存在则自动建组**——离线重试零往返；typo 出来的孤儿组
只是空组，无副作用）。

请求体：

```json
{"ops": [
  {"op_id": "devA:17", "device_id": "devA", "payload": "<base64>"},
  {"op_id": "devA:18", "device_id": "devA", "payload": "<base64>"}
]}
```

响应：

```json
{"assigned": [{"op_id": "devA:17", "seq": 43}, {"op_id": "devA:18", "seq": 44}],
 "duplicate_op_ids": []}
```

语义：

- 服务端按到达顺序定序（组内全序），`seq` 单调无空洞。
- `op_id` 组内已存在 → 不追加、不计新 seq，`op_id` 进 `duplicate_op_ids`
  （客户端重试安全：「网络死了不知服务端收没收到」由此消解）。
- 同一请求内出现重复 `op_id` → 400 `{"error":"duplicate_op_id_in_request"}`。
- 字段缺失/类型错 → 400 `{"error":"invalid_op"}`。

### 2.5 `GET /api/sync/groups/{gid}/ops?since=N&limit=M`

按 `seq` 升序返回 `seq > N` 的指令，至多 `limit` 条。

```json
{"gid": "…",
 "ops": [{"seq": 41, "op_id": "devB:3", "device_id": "devB",
          "ts": 1733300001, "payload": "<base64>"}],
 "cursor": 42, "has_more": false}
```

- `cursor` = 组内当前最大 `seq`（不是本次返回的最大值——客户端一次
  拉取不完时以 `has_more` 判断续拉，以返回内容推进自己的位点）。
- `since` 缺省 0；`limit` 缺省 200、上限 1000（超出按 1000 处理）。
- 组不存在 → 404（区别于「存在但空」→ `ops: []`）。

### 2.6 `PUT /api/sync/groups/{gid}/snapshot`

```json
{"up_to_seq": 30, "payload": "<base64>"}
```

- 语义：快照覆盖 ≤30 的全部指令状态。最后写入者胜（新快照整体替换旧快照）。
- 校验：`1 ≤ up_to_seq ≤ latest_seq`（不能快照未来），违者 400。
- **v1 不做 binlog 裁剪**（B2 定压缩策略后随批落地）；本端点先行落地
  是为了让客户端压缩器有稳定依赖。

### 2.7 `GET /api/sync/groups/{gid}/snapshot`

```json
{"up_to_seq": 30, "payload": "<base64>"}
```

无快照 → 404 `{"error":"snapshot_not_found"}`。

### 2.8 拉取规则（客户端契约，含快照空洞）

```
meta = GET meta
if my_cursor < meta.snapshot_up_to_seq:      # 位点落后于快照覆盖位
    snapshot = GET snapshot                  # 先取快照重建状态
    base = snapshot.up_to_seq
else:
    base = my_cursor
ops = GET ops?since=base （has_more 则续拉）  # 再增量重放
```

## 3. 限额与校验

| 项 | 值 | 超限行为 |
|----|----|----------|
| `gid` 形态 | `^[A-Za-z0-9_-]{16,64}$` | 400 `invalid_group_id` |
| 单 op payload（base64 后） | ≤ 256 KiB | 413 `payload_too_large` |
| 单次 POST op 数 | ≤ 256 | 400 `too_many_ops` |
| 单次拉取 limit | ≤ 1000 | 钳到 1000 |
| 请求体大小 | ≤ 64 MiB（一次最多 256×256KiB 富余） | 413 |

官方托管形态（Worker）可在此之上加配额（§6「存储限配额防滥用」），
自建形态不限。

## 4. 各形态一致性要求

| 形态 | 存储 | 定序保证 |
|------|------|----------|
| **本地 dev**（参考实现） | 单进程 + 互斥锁 + 原子落盘 | 进程内串行，天然全序 |
| **官方托管**（Cloudflare Worker + D1） | D1 = 单写者 SQLite | `batch()` 事务内 `MAX(seq)+1` 子查询插入，写串行化保证无重号；`UNIQUE(gid,op_id)` 兜底幂等 |
| **自带中转 / 局域网直传**（B5） | 各自实现 | 必须满足：组内 seq 单调无空洞、`op_id` 幂等、拉取按 seq 升序——契约符合性测试（`dev/test_relay_protocol.py`）是验收口径 |

## 5. 预留（后续批次，不在 v1）

- **请求签名**（B3）：组密钥派生 HMAC 挂 header，防知道 group_id 的
  第三方注入垃圾密文；v1 依赖 capability 不可猜性。
- **binlog 裁剪**（B2）：快照上传后按设备位点最小值裁剪。
- **设备登记**（B4）：设备清单/备注/最近同步（目前 device_id 只是指令
  上的不透明标签）。
