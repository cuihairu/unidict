# 同步引擎设计（云级：账号、增量同步、端到端加密）

> 状态：设计稿（2026-09-27）。本文档只做设计定稿，**不含实现**；
> 实施按 §7 分阶段推进，每阶段独立成提交。
> 关联：[roadmap](../roadmap.md)「Core Architecture → Sync engine design」
> 与「Advanced Features → Data Sync」；文件同步 MVP 见
> `adapters/qt/sync_service_qt`（保留为无云回退，见 §6.4）。

## 0. 范围与非交互假设（本轮标注）

按既定口径，缺信息自行假设并就地标注；以下假设影响设计但可替换，
替换时只需改对应小节，不动整体骨架。

- **A1 后端**：假设为 **S3 兼容对象存储**（自托管 MinIO 或任意云厂商）+
  一个极薄的账户服务（只管认证与设备登记，不碰业务数据）。
  理由：无状态对象存储运维成本最低；协议只依赖「按 key 存取不可变
  对象 + 列举」，避开自行开发有状态同步服务器。
- **A2 网络栈**：core（无 Qt 约束）不碰网络；HTTP 客户端放 adapters
  （Qt `QNetworkAccessManager`）。同步**协议与合并逻辑**是纯 std 可测的
  core 代码，传输只是其薄外壳。
- **A3 密码学库**：假设 vendor **monocypher**（BSD-2，单文件级体积，
  提供 argon2id / XChaCha20-Poly1305 / Ed25519 / X25519）到
  `third_party/`。引入第三方源码属仓库政策决定，列入 §9 开放问题。
- **A4 账户形态**：假设自建极简账户服务（opaque user id + 短期令牌）；
  是否改用第三方 OIDC 见 §9。协议只依赖「每个用户有一个受认证保护的
  命名空间」，不依赖具体认证方式。
- **A5 同步范围**：只同步**用户数据**——生词、笔记、发音练习记录、
  搜索历史、词典分组配置（profiles）与核心设置子集。**词典资产
  （.mdx/.mdd 等）不入同步**：体积大且有版权/分发问题，多设备各自导入。

## 1. 目标与非目标

**目标**

1. 离线优先：无网时一切功能照常；联网后自动收敛，无手工干预。
2. 多设备：≥3 台设备长期并行使用，最终一致。
3. 增量：日常同步流量与「本轮变更量」成正比，不与库总量成正比。
4. E2EE：服务器（含被入侵的服务器）只见密文与不可读元数据。
5. 冲突可控：每类数据有确定性的合并语义，可解释、可测试；
   绝不静默丢数据。
6. 可自托管：后端两件套（对象存储 + 账户服务）可独立部署。

**非目标**

- 实时协作/多人共享词库（v2+ 再议，密钥体系预留设备间授权位）。
- 词典文件分发/在线词库（roadmap 另有条目，与本项目只共享账号）。
- 服务器端搜索/服务端计算（与 E2EE 天然互斥）。
- 同步词典 UI 配置的全部细节（仅 §2 列出的核心设置子集）。

## 2. 数据模型：实体、标识与版本

### 2.1 集合（collection）

| collection | 对应现有存储 | 记录粒度 | 合并语义 |
|---|---|---|---|
| `vocab` | `DataStoreStd::VocabItemStd` | 每词一条 | 字段级（§4.5） |
| `notes` | `DataStoreStd::NoteItemStd` | 每词一条 | LWW（整条） |
| `pron` | `DataStoreStd::PronRecordStd` | 每词一条 | 字段级（§4.5） |
| `history` | `DataStoreStd` 搜索历史 | 单独一条快照记录 | 有序并集（§4.5） |
| `profiles` | `AggregateLookupStd::DictionaryProfile` | 每个 profile 一条 | LWW（整条） |
| `settings` | Qt 设置的核心子集（词典序/组开关） | 单条 KV | LWW |

词典索引（`IndexEngineStd`）、全文索引、mdd 缓存是**派生数据**，
不入同步，各设备重建（§6.2）。

### 2.2 记录信封（明文态，加密前）

```text
SyncRecord {
  id          : UUIDv7          // 稳定主键，创建时生成，永不复用
  collection  : enum            // §2.1
  hlc         : u64             // 混合逻辑时钟（§4.4），最后修改时间
  deleted     : bool            // 墓碑标记（§4.6）
  body        : bytes           // 集合特定的明文载荷（JSON）
}
```

- `body` 示例：`vocab` 即 `{word, definition, added_at, tags[]}`，
  与现有 `VocabItemStd` 一一对应，**不发明新业务字段**。
- 现有存储以「词（大小写不敏感）」为主键，没有稳定 id。迁移规则
  （§6.1）：存量数据一次性补发 UUIDv7 落盘；**首次多设备对接**时按
  词键去重合并（沿用文件 MVP 的 preview 语义），此后 id 即权威键。

### 2.3 设备

```text
Device {
  device_id  : 16B 随机数（展示为指纹）
  name       : 用户可见名（"台式机"、"手机"）
  created_at : 首次注册时间
  last_seen  : 最近一次推送的 hlc（账户服务记录，仅用于设备列表展示）
  revoked    : bool
}
```

## 3. 账号与设备模型

### 3.1 账号

- 注册：邮箱/用户名 + **主口令（passphrase）**。服务器存
  `argon2id` 盐与认证校验值（用于登录，**不是**数据密钥，见 §5.1）。
- 会话：登录换短期访问令牌（15 min）+ 刷新令牌（30 天，可撤销）。
  令牌只授权访问 `users/<uid>/` 命名空间。

### 3.2 设备注册与撤销

- 注册：新设备用账号登录后，生成 `device_id`，向账户服务登记，
  并把**账号主密钥的wrapped 形式**拉回本地（§5.2）。
- 撤销：账户服务标记 `revoked`；被撤设备的令牌与刷新令牌立即失效，
  对象存储前缀授权由账户服务下发的短期存储令牌携带（账户服务在
  每次发令牌时校验设备状态）。已同步到被撤设备上的历史数据仍可读
  ——撤销防的是**未来**写入，不是抹除历史；需要抹历史走密钥轮换
  （§5.4）。

### 3.3 本地/匿名模式

不注册账号也能用：导出加密快照文件（复用文件 MVP 入口），快照格式
与云端 chunk 相同（§5.3）。未来注册账号时该文件可原样导入。

## 4. 存储布局与增量同步协议

### 4.1 后端对象布局（全部为密文，见 §5.3）

```text
users/<uid>/
  wrapped_key                  # wrapped account key（§5.2），登录后可读
  head                         # {seq, manifest_hash}——当前清单指针
  manifests/<seq 8 位零填充>    # 清单快照（不可变，新序号新对象）
  chunks/<sha256>              # 内容寻址记录块（不可变，天然去重）
```

- 命名空间内对象对持有令牌者可读写；`head` 是唯一被并发覆盖的对象。

### 4.2 清单（manifest）

```text
Manifest {                    // 整体作为单个 chunk 加密存储
  seq                : u64        // 单调递增
  prev_manifest_hash : 32B        // 上一清单哈希，构成链（§5.5 回滚检测）
  records            : [ { id, collection, hlc, deleted, chunk_ref } ]
  device_id          : 16B        // 产生本清单的设备
}
```

`records` 只含信封元数据；`chunk_ref` 指向密文载荷。

### 4.3 同步循环（pull-merge-push，状态基，非操作基）

选型：**状态基**（每条记录整条上行）而非 op-log。理由：状态基天然
幂等、无需本地待发队列、离线多天后的合并逻辑与在线一小 时完全相同；
代价（无细粒度操作历史）对本数据规模（词表级，单条 < 数 KB）可忽略。

```text
loop（触发：启动、退到前台、定时 5 min、本地变更后防抖 3 s）：
  1. pull    : GET head → 有新 seq 则 GET manifest → 比对本地已知
               (id, hlc) → 对本地缺失/较旧的记录 GET chunks
  2. merge   : 本地状态 ∪ 远端状态，逐记录按 §4.5 合并，产出新本地态
               与待上行差异
  3. push    : 有差异则 PUT 新 chunks（幂等）→ PUT manifests/<seq+1>
               → CAS head（见下）→ 本地 cursor = seq+1
  4. 失败退避：指数退避（1s 起，上限 5 min）；网络错误不改变任何
               本地状态，可无限重试
```

**head 并发控制**：不要求服务器支持条件写。push 三步中前两步写入
不可变对象（天然幂等，重试安全）；第三步覆写 `head` 后**回读校验**：
若读回的 seq < 自己写的 seq（输给了并发设备），重新从步骤 1 开始
（新清单必然已含自己刚传的 manifest 对象，merge 后重 push 即收敛）。
若服务器支持 If-Match（A1 备注可选项），第三步退化为真正的 CAS，
省一次回读。

**游标**：本地持久化 `cursor = 已完整合并的 manifest seq`。游标只做
加速（跳过已知清单），正确性不依赖它——清掉游标从 head 全量重放
合并函数，结果不变（合并函数对重复输入幂等，§8.2）。

### 4.4 时钟：混合逻辑时钟（HLC）

- `hlc = (physical_ms << 16) | counter`，物理毫秒取本地墙钟，
  同毫秒内 counter 递增；收到更大 hlc 时跳钟。
- 比较序：hlc 大者新；hlc 相等比 `device_id`（全序，保证所有设备
  对同一对记录算出**同一个胜者**——收敛的前提）。
- 设备时钟回拨/漂移只影响「时间观感」，不影响正确性（全序由
  hlc+device_id 保证）。墙钟偏差上限仅影响 LWW 的语义合理性，
  多设备时钟差 > 阈值时 UI 提示校时。

### 4.5 合并函数（逐集合，纯函数）

输入是同一 `id`（或 `history` 的同键）下的本地/远端两个状态，
输出合并结果。**全部是 core/std 纯函数**，表驱动测试（§8.1）。

| 集合 | 字段 | 规则 |
|---|---|---|
| `vocab` | `definition` | LWW（hlc+device_id 大者胜） |
| | `added_at` | 取较早（min），保留"最早收录"语义 |
| | `tags` | 并集（大小写不敏感去重） |
| `notes` | `text` | LWW 整条；败方文本进「冲突副本」字段保留一次（UI 可查看恢复，防静默丢稿） |
| `pron` | `last_score`/`last_at` | LWW |
| | `best_score` | max |
| | `attempts` | 相加（两侧都练习过则累加） |
| `history` | — | 有序并集：按 hlc 排序去重，截断上限 200（现查询单次 100） |
| `profiles` / `settings` | — | LWW 整条 |

墓碑参与同样的比较：`deleted` 记录按其 hlc 与活记录比较，新者胜
（删了又改的次序问题由 hlc 全序裁决，不靠时间猜）。

### 4.6 删除与墓碑 GC

- 删除 = 本地置 `deleted=true`、hlc 前进、照常同步。
- 墓碑保留 **90 天**后可在本地物理清除；清单中的墓碑条目由
  seq 最小的设备在 GC 触发（本地起，无须服务器智能）时批量清理并
  推新清单。清除前该记录的 chunk 对象随清单不再被引用，对象存储
  生命周期规则异步回收（A1：桶规则 90 天，与墓碑窗口对齐）。

## 5. E2EE 密钥体系与威胁模型

### 5.1 密钥层级

```text
passphrase ──argon2id(salt≈每账号随机, 服务器存)──▶ KEK
随机 256bit ─────────────────────────────────────▶ account_key（账号主密钥）
account_key ──HKDF-SHA256(info="unidict/collection/<id>")──▶ collection_key
collection_key + 随机 24B nonce ──XChaCha20-Poly1305──▶ chunk 密文
account_key ──HKDF(info="unidict/manifest")──▶ manifest_key
account_key ──X25519──▶ 设备密钥对（v2 共享/授权预留，本轮不用）
```

- **数据全部由 account_key 派生的密钥加密，与登录口令解耦**：
  改口令 = 服务器上重写 `wrapped_key` 一个对象，已上传的 chunk
  **无需重加密**。
- `wrapped_key = XChaCha20-Poly1305(KEK, account_key)`。登录后拉回
  解包；改口令时用新 KEK 重包同一 account_key。

### 5.2 设备开通流

1. 新设备登录（口令 → KEK）。
2. GET `wrapped_key` → 解包得 account_key。
3. 校验：用 account_key 解密 head 指向的 manifest 成功即视为密钥
   正确（AEAD 认证标签承担校验职责，不需要额外的 magic）。
4. 登记设备（§3.2），account_key 存入平台安全存储（Linux libsecret /
   macOS Keychain / Windows DPAPI；Qt 层职责，core 不碰）。

### 5.3 密文对象格式

```text
chunk = version(1B) || nonce(24B) || AEAD 密文 || tag(16B, monocypher 布局)
AAD   = record_id || collection || hlc || deleted   （信封元数据绑定）
```

- 元数据绑进 AAD：服务器篡改清单里的 id/hlc 会导致解密失败
  （完整性），服务器**看到**的清单本身也是密文（机密性）。
- `head`/`wrapped_key` 之外的文件名与对象 key 不含明文信息：
  chunk 用内容哈希、manifest 用 seq（序号非敏感），collection 归属
  在密文内。
- 密文载荷（body）为集合特定 JSON，UTF-8，与 DataStoreStd 现有
  持久化格式字段同名，序列化器共享。

### 5.4 密钥轮换与设备撤销后的数据防护

- 改口令：只重写 `wrapped_key`（§5.1）。
- 怀疑某设备泄露：撤销（§3.2）后执行**数据轮换**——用新
  account_key 重加密全部 chunk 并推新清单（数据量小，一次遍历），
  旧 account_key 废弃；被撤设备即使留有旧密钥，也读不到轮换后的
  新增/修改数据（其未修改的旧数据仍在旧密文里，接受该残留，
  UI 明示「撤销不抹除已下发历史」）。

### 5.5 威胁模型

| 威胁 | 防线 | 残余风险（接受并注明） |
|---|---|---|
| 服务器/运营商窥探 | E2EE：服务器只见密文 chunk、密文 manifest、不可读对象名 | 看得到对象数量/大小与时序（元数据流量分析；可选 padding，§7 S5） |
| 服务器被 full 入侵 | 同上 + manifest 链（seq 单调 + prev_hash）使**篡改**可检测 | 服务器可**回滚**（向新设备供应旧 head）；本地有状态设备靠链拒绝降级，全新设备接受「看到旧快照」为残余风险 |
| 设备失窃 | 平台安全存储锁 account_key；撤销阻断未来写入 | 已解锁状态下物理提取内存/磁盘属超出威胁模型 |
| 弱口令 | argon2id（m=64MiB, t=3, p=4）+ 口令强度 UI | 弱口令仍可被离线爆破——服务器存盐不存口令，但 wrapped_key 暴露给爆破；UI 强制最低熵 |
| 口令遗忘 | 无恢复后门（E2EE 的代价） | 数据不可恢复，注册时明示；可选纸质恢复码（= 口令本身，只是提醒保存） |
| 中间人 | TLS + 对象 ETag/内容哈希校验 | 首次登录的账号服务信任建立依赖 CA 体系（不重造） |
| 恶意对端设备 | 设备各自持有完整 account_key，无细粒度隔离 | 家庭互信场景成立；不可信对端属 v2 授权体系问题 |
| 重放合并结果 | 合并函数幂等 + hlc 全序 | 无 |

明确不做：服务器端去重按密文哈希（内容寻址已达成）、零知识账户
恢复、后量子（算法位留 version 字段可平滑换）。

## 6. 与 core 现有模块的接缝

### 6.1 DataStoreStd（`core/std/data_store_std.h`）

- **迁移**：`VocabItemStd`/`NoteItemStd`/`PronRecordStd` 各加
  `id`（UUIDv7）与 `hlc` 字段；旧文件首次 load 时缺省补发并
  `save()` 一次（现有 `ensure_loaded` 惯例内做，JSON 增量字段向后
  兼容：旧字段全保留）。`history` 无需 id（整库一条快照记录）。
- **新增接口（设计示意，非本轮实现）**：

```cpp
// core/std/sync_engine_std.h（新文件，无 Qt、无网络）
namespace UnidictCoreStd {
struct SyncRecord { std::string id, collection, body; long long hlc; bool deleted; };
class SyncEngineStd {
public:
    // DataStoreStd 变更导出：hlc > cursor 的记录（增量上行）
    std::vector<SyncRecord> collect_changes(long long cursor) const;
    // 远端状态并入：逐 id 走 §4.5 合并函数，返回是否发生修改
    bool merge_remote(const std::vector<SyncRecord>& remote);
    long long cursor() const;               // 持久化于存储文件 meta 段
    void advance_cursor(long long seq);
};
}
```

- DataStoreStd 的**查询 API 不变**（`get_vocabulary` 等）——同步是
  存储层之下的能力，UI 无感。
- 词键大小写不敏感语义保留在 DataStoreStd 内部（id 是外键），
  首次对接去重走既有 preview 合并语义（词键）。

### 6.2 IndexEngineStd（`core/std/index_engine_std.h`）

**不参与同步**。索引是从词典文件构建的派生数据，各设备本地重建；
同步完成后（生词等用户数据变化不影响词典索引，二者解耦；仅
`settings/profiles` 变化时）通知上层按需重建/失效聚合查询缓存。

### 6.3 AggregateLookupStd（profiles）

`DictionaryProfile` 序列化为 `profiles` 集合记录（`profile_id` 即
记录 id），合并语义 LWW；`enabled_profiles` 引用的 profile 被远端
删除时按「引用悬空」处理：本地保留但标记未启用（不静默删组）。

### 6.4 adapters 与文件同步 MVP

- `sync_service_qt`（文件 MVP）**保留**：降级路径（无账号/无网导出）
  与「加密快照导出」（§3.3）共用其 UI 入口；其合并语义已被 §4.5
  覆盖（history 有序并集、vocab 本地胜 = LWW 的特例），长期看把
  其 preview/apply UI 接到 SyncEngineStd 的 merge 结果上。
- 新增 `sync_client_qt`（adapters）：QNetworkAccessManager 实现
  §4.1 对象读写 + 账户服务 REST；core 的 SyncEngineStd 不见网络。
- 新增 UI（gui/qmlui，后置）：账号设置页、设备列表/撤销、
  同步状态指示（最近成功时间/待上行数/冲突副本提示）。

### 6.5 路径与缓存

- `PathUtilsStd::cache_dir()` 下新增 `sync/` 子目录：游标、manifest
  链缓存、待上传暂存。**不进** mdict mdd 缓存目录（`mdd_<hash>` 语义
  不变，两套缓存互不感知）。
- 测试注意（实测教训，见 todo.md B-3）：任何以内容重写文件的操作
  都会换 mdd 缓存签名——同步代码与测试不得触碰 mdd 缓存目录。

## 7. 分阶段实施

每阶段独立可交付，门槛统一：`build-std` ctest 全绿 +
`scripts/coverage.sh` 阈值 PASS + Qt 树全绿（仓库既有门禁）。

| 阶段 | 内容 | 交付物 |
|---|---|---|
| S1 本地引擎 | DataStoreStd 加 id/hlc 与迁移；SyncEngineStd 变更导出/合并函数/HLC；游标持久化 | `core/std/sync_engine_std.*` + std-only 测试（无网络无加密，纯逻辑） |
| S2 E2EE 格式 | vendor monocypher（A3）；密钥派生、chunk/manifest 序列化与往返 | `core/std/sync_crypto_std.*` + KAT 向量（沿 `scripts/gen_mdict_crypto_vectors.py` 惯例新增生成脚本） |
| S3 传输客户端 | adapters 对象存储客户端（列举/读/写/回读校验）、退避重试、同步循环编排 | `adapters/qt/sync_client_qt.*` + fake 后端测试 |
| S4 账号与设备 | 账户服务客户端、登录/配对/撤销/改口令；Qt 设置页 | 账户服务（独立部署物，A4）+ UI |
| S5 打磨 | 墓碑 GC、流量 padding（可选）、冲突副本 UI、快照导出接入 | 收尾与文档更新 |

S1/S2 无任何外部服务依赖，可先行落地；S3 起依赖 A1/A4。

## 8. 测试策略

### 8.1 合并函数：表驱动冲突矩阵

每集合 × 每字段 × {本地新/远端新/相等/墓碑} 的期望结果表，
std-only assert 风格（仓库惯例）。重点断言：

- **全序收敛**：同一对冲突状态无论从哪台设备算，胜者一致
  （hlc 相等时 device_id 决胜的用例必须覆盖）。
- 不丢数据：败方 `notes` 文本进冲突副本；`tags` 并集不重不漏。

### 8.2 协议：多设备模拟（内存 fake 后端）

- fake 后端实现 §4.1 布局（内存 map + 可注入故障：丢包/乱序/
  head 并发冲突）。
- **收敛性性质测试**：2–3 台虚拟设备随机产生操作（增删改、离线
  N 轮再联网），断言最终所有设备状态一致且含全部非墓碑操作效果；
  同一操作序列重复执行结果不变（幂等）。
- 时钟用例：回拨、跨设备时钟差、同毫秒 counter 溢出。
- head 竞争：两设备同 push，断言回读校验后重放收敛。

### 8.3 E2EE：KAT 与破坏性测试

- 已知答案向量（monocypher 官方向量 + 自生成脚本，S2 交付）。
- 往返：任意字节 body（含空、超长、非法 UTF-8）。
- 篡改：改密文/tag/AAD 任一字节必失败；换 record_id 重放必失败
  （AAD 绑定）。
- 轮换：改口令后旧 wrapped_key 失效、数据可读；数据轮换后旧
  account_key 读不了新 chunk。

### 8.4 传输与集成

- fake HTTP 后端测退避/重试/断点；Qt 层沿 `QTRY_*` 惯例。
- 真后端冒烟清单：追加到 `docs/gui-smoke-checklist.md` 惯例
  （两台真机 + MinIO 一只：注册→生词→另机拉到→离线双改→冲突
  副本可见→撤销设备→改口令）。

## 9. 开放问题（实现前须决）

1. **账户服务形态**（A4）：自建极简服务 vs 接入第三方 OIDC。
   自建的运维成本最低路径可能是「对象存储 + 一个静态站点的
   token 签发函数」，待 S3 前定。
2. **monocypher 采购政策**（A3）：vendor 单文件到 `third_party/`
   是否符合仓库依赖惯例（现依赖 zlib 一类系统库）。
3. **history 是否默认入同步**：搜索历史是隐私敏感数据，默认开
   还是默认关（倾向默认关，首开时明示）。
4. **服务器条件写**：若确认目标后端支持 If-Match，§4.3 的回读
   校验可简化；不作为依赖。
5. **快照文件与云端 chunk 同格式**的版本演进规则（version 字段
   升级语义）。
