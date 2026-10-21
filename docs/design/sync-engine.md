> Language: **English** | [简体中文](../zh/design/sync-engine.md)

# Sync engine design (cloud-grade: accounts, incremental sync, end-to-end encryption)

> Status: design draft (2026-09-27). This document is a design freeze
> only and **contains no implementation**; implementation proceeds in
> phases per §7, with each phase landing as an independent commit.
> Related: [roadmap](../roadmap.md) "Core Architecture -> Sync engine
> design" and "Advanced Features -> Data Sync"; the file-sync MVP is in
> `adapters/qt/sync_service_qt` (kept as the no-cloud fallback, see
> §6.4).

## 0. Scope and non-interactive assumptions (annotated this round)

Per the established convention, missing information is assumed on the
spot and annotated in place. The assumptions below affect the design but
are replaceable: swapping one out touches only its own subsection, never
the overall skeleton.

- **A1 backend**: assume an **S3-compatible object store** (self-hosted
  MinIO or any cloud vendor) plus one extremely thin account service
  (authentication and device registration only; it never touches business
  data). Rationale: stateless object storage has the lowest operations
  cost; the protocol depends only on "store and fetch immutable objects
  by key + listing", avoiding a hand-built stateful sync server.
- **A2 network stack**: the core (under the no-Qt constraint) does not
  touch the network; the HTTP client lives in adapters (Qt
  `QNetworkAccessManager`). The sync **protocol and merge logic** are
  pure std, testable core code, and transport is only a thin shell around
  them.
- **A3 cryptography library**: assume **monocypher** is vendored (BSD-2,
  single-file scale, providing argon2id / XChaCha20-Poly1305 / Ed25519 /
  X25519) into `third_party/`. Vendoring third-party source is a
  repository-policy decision; it is listed among the §9 open questions.
- **A4 account form**: assume a self-built minimal account service
  (opaque user id + short-lived tokens); whether to switch to a
  third-party OIDC provider is covered in §9. The protocol depends only
  on "every user has an authentication-protected namespace", not on any
  specific authentication mechanism.
- **A5 sync scope**: only **user data** is synced -- vocabulary, notes,
  pronunciation practice records, search history, dictionary group
  profiles, and a core subset of settings. **Dictionary assets
  (.mdx/.mdd etc.) are not synced**: they are large and carry
  copyright/distribution issues; each device imports them on its own.

## 1. Goals and non-goals

**Goals**

1. Offline-first: everything works as usual with no network; once
   online, state converges automatically, with no manual intervention.
2. Multi-device: at least 3 devices in parallel use over the long term,
   eventually consistent.
3. Incremental: day-to-day sync traffic is proportional to the change
   volume of the round, not to the total size of the library.
4. E2EE: the server (including a compromised server) sees only
   ciphertext and unreadable metadata.
5. Controllable conflicts: every data class has deterministic merge
   semantics that are explainable and testable; never silently drop
   data.
6. Self-hostable: the backend pair (object storage + account service)
   can be deployed independently.

**Non-goals**

- Real-time collaboration / multi-user shared word libraries (to be
  revisited for v2+; the key hierarchy reserves a slot for inter-device
  authorization).
- Dictionary file distribution / online word libraries (a separate
  roadmap item that shares only the account with this project).
- Server-side search / server-side computation (naturally mutually
  exclusive with E2EE).
- Syncing every detail of the dictionary UI configuration (only the core
  settings subset listed in §2).

## 2. Data model: entities, identity, and versions

### 2.1 Collections

| collection | backing existing store | record granularity | merge semantics |
|---|---|---|---|
| `vocab` | `DataStoreStd::VocabItemStd` | one per word | field-level (§4.5) |
| `notes` | `DataStoreStd::NoteItemStd` | one per word | LWW (whole record) |
| `pron` | `DataStoreStd::PronRecordStd` | one per word | field-level (§4.5) |
| `history` | `DataStoreStd` search history | one standalone snapshot record | ordered union (§4.5) |
| `profiles` | `AggregateLookupStd::DictionaryProfile` | one per profile | LWW (whole record) |
| `settings` | core subset of Qt settings (dictionary order / group toggles) | a single KV | LWW |

The dictionary index (`IndexEngineStd`), the full-text index, and the mdd
cache are **derived data**: they are not synced and each device rebuilds
them (§6.2).

### 2.2 Record envelope (plaintext, before encryption)

```text
SyncRecord {
  id          : UUIDv7          // 稳定主键，创建时生成，不复用
  collection  : enum            // §2.1
  hlc         : u64             // 混合逻辑时钟（§4.4），最后修改时间
  deleted     : bool            // 墓碑标记（§4.6）
  body        : bytes           // 集合特定的明文载荷（JSON）
}
```

- Example `body`: for `vocab` it is `{word, definition, added_at,
  tags[]}`, mapping one-to-one onto the existing `VocabItemStd`; **do
  not invent new business fields**.
- The existing store uses "word (case-insensitive)" as its primary key
  and has no stable id. Migration rule (§6.1): backfill UUIDv7 for
  existing data in one pass and persist it; at **first multi-device
  pairing**, dedupe and merge by word key (reusing the file MVP's
  preview semantics); from then on the id is the authoritative key.

### 2.3 Devices

```text
Device {
  device_id  : 16B 随机数（展示为指纹）
  name       : 用户可见名（"台式机"、"手机"）
  created_at : 首次注册时间
  last_seen  : 最近一次推送的 hlc（账户服务记录，仅用于设备列表展示）
  revoked    : bool
}
```

## 3. Account and device model

### 3.1 Accounts

- Registration: email/username + **passphrase**. The server stores the
  `argon2id` salt and an authentication verifier (used for login,
  **not** the data key -- see §5.1).
- Sessions: logging in exchanges for a short-lived access token
  (15 min) + a refresh token (30 days, revocable). A token authorizes
  access only to the `users/<uid>/` namespace.

### 3.2 Device registration and revocation

- Registration: after a new device logs in with the account, it
  generates a `device_id`, registers with the account service, and pulls
  down the **wrapped form of the account master key** locally (§5.2).
- Revocation: the account service marks the device `revoked`; the
  revoked device's tokens and refresh token are invalidated immediately.
  Object-storage prefix authorization rides on a short-lived storage
  token issued by the account service (the account service checks device
  status every time it issues a token). Historical data already synced
  onto the revoked device remains readable -- revocation guards against
  **future** writes, it does not erase history; erasing history goes
  through key rotation (§5.4).

### 3.3 Local/anonymous mode

The app is usable without registering an account: export an encrypted
snapshot file (reusing the file MVP's entry point); the snapshot format
is identical to the cloud chunk (§5.3). When an account is registered
later, that file can be imported as-is.

## 4. Storage layout and the incremental sync protocol

### 4.1 Backend object layout (all ciphertext, see §5.3)

```text
users/<uid>/
  wrapped_key                  # wrapped account key（§5.2），登录后可读
  head                         # {seq, manifest_hash}——当前清单指针
  manifests/<seq 8 位零填充>    # 清单快照（不可变，新序号新对象）
  chunks/<sha256>              # 内容寻址记录块（不可变，天然去重）
```

- Objects inside the namespace are readable and writable by token
  holders; `head` is the only object that is concurrently overwritten.

### 4.2 Manifests

```text
Manifest {                    // 整体作为单个 chunk 加密存储
  seq                : u64        // 单调递增
  prev_manifest_hash : 32B        // 上一清单哈希，构成链（§5.5 回滚检测）
  records            : [ { id, collection, hlc, deleted, chunk_ref } ]
  device_id          : 16B        // 产生本清单的设备
}
```

`records` carries envelope metadata only; `chunk_ref` points at the
encrypted payload.

### 4.3 The sync loop (pull-merge-push; state-based, not operation-based)

Choice: **state-based** (each record uploaded whole) rather than an
op-log. Rationale: state-based is naturally idempotent, needs no local
outbound queue, and the merge logic after days offline is exactly the
same as after one hour online; the cost (no fine-grained operation
history) is negligible at this data scale (vocabulary level, each record
< a few KB).

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

**head concurrency control**: no server support for conditional writes is
required. Of the three push steps, the first two write immutable objects
(naturally idempotent, retry-safe); the third overwrites `head` and then
**verifies by reading back**: if the seq read back is smaller than the
one just written (lost to a concurrent device), start over from step 1
(the new manifest necessarily already contains the manifest object just
uploaded; after merge, re-push and it converges). If the server supports
If-Match (a noted option under A1), the third step degrades into a true
CAS, saving one read-back.

**Cursor**: persist locally `cursor = 已完整合并的 manifest seq`. The
cursor only accelerates (skipping known manifests); correctness does not
depend on it -- delete the cursor and replay the merge function from
head over the full history, and the result is unchanged (the merge
function is idempotent over duplicate input, §8.2).

### 4.4 Clock: the hybrid logical clock (HLC)

- `hlc = (physical_ms << 16) | counter`; physical milliseconds come from
  the local wall clock, the counter increments within the same
  millisecond, and the clock jumps forward on receiving a larger hlc.
- Comparison order: larger hlc is newer; on equal hlc, compare
  `device_id` (a total order, guaranteeing every device computes **the
  same winner** for the same pair of records -- the precondition for
  convergence).
- Device clock rollback/drift affects only the "perception of time", not
  correctness (the total order is guaranteed by hlc+device_id). The
  wall-clock skew bound only affects how semantically sane LWW is; when
  the clock gap between devices exceeds a threshold, the UI prompts for
  time correction.

### 4.5 The merge function (per collection, pure)

The inputs are the local and remote states under the same `id` (or the
same key for `history`); the output is the merged result. **All of them
are core/std pure functions**, tested table-driven (§8.1).

| collection | field | rule |
|---|---|---|
| `vocab` | `definition` | LWW (larger hlc+device_id wins) |
| | `added_at` | take the earlier one (min), preserving the "first recorded" semantics |
| | `tags` | union (deduped case-insensitively) |
| `notes` | `text` | LWW whole record; the loser's text is kept once in a "conflict copy" field (viewable and restorable in the UI, preventing silent loss of drafts) |
| `pron` | `last_score`/`last_at` | LWW |
| | `best_score` | max |
| | `attempts` | summed (accumulates when both sides practiced) |
| `history` | - | ordered union: sort by hlc and dedupe, truncated at a cap of 200 (current queries fetch 100 per call) |
| `profiles` / `settings` | - | LWW whole record |

Tombstones take part in the same comparison: `deleted` records compete
with live records by their hlc, and the newer wins (delete-then-edit
ordering questions are adjudicated by the hlc total order, not guessed
from wall time).

### 4.6 Deletion and tombstone GC

- Delete = set `deleted=true` locally, advance the hlc, and sync as
  usual.
- Tombstones may be physically purged locally after **90 days** of
  retention; tombstone entries in manifests are batch-cleaned by the
  device with the smallest seq when GC triggers (initiated locally, no
  server intelligence required), which pushes a new manifest. Once
  purged, the record's chunk object is no longer referenced by any
  manifest, and object-storage lifecycle rules reclaim it
  asynchronously (A1: a 90-day bucket rule, aligned with the tombstone
  window).

## 5. E2EE key hierarchy and threat model

### 5.1 Key hierarchy

```text
passphrase ──argon2id(salt≈每账号随机, 服务器存)──▶ KEK
随机 256bit ─────────────────────────────────────▶ account_key（账号主密钥）
account_key ──HKDF-SHA256(info="unidict/collection/<id>")──▶ collection_key
collection_key + 随机 24B nonce ──XChaCha20-Poly1305──▶ chunk 密文
account_key ──HKDF(info="unidict/manifest")──▶ manifest_key
account_key ──X25519──▶ 设备密钥对（v2 共享/授权预留，本轮不用）
```

- **All data is encrypted under keys derived from account_key, decoupled
  from the login passphrase**: changing the passphrase = rewriting one
  object, `wrapped_key`, on the server; already-uploaded chunks need
  **no re-encryption**.
- `wrapped_key = XChaCha20-Poly1305(KEK, account_key)`. After login it
  is pulled down and unwrapped; when changing the passphrase, the same
  account_key is re-wrapped under the new KEK.

### 5.2 Device provisioning flow

1. The new device logs in (passphrase -> KEK).
2. GET `wrapped_key` -> unwrap it to obtain account_key.
3. Verification: successfully decrypting the manifest that head points
   to with account_key counts as a valid key (the AEAD authentication
   tag performs this verification; no extra magic is needed).
4. Register the device (§3.2); store account_key in platform secure
   storage (Linux libsecret / macOS Keychain / Windows DPAPI; a Qt layer
   responsibility -- the core does not touch it).

### 5.3 Ciphertext object format

```text
chunk = version(1B) || nonce(24B) || AEAD 密文 || tag(16B, monocypher 布局)
AAD   = record_id || collection || hlc || deleted   （信封元数据绑定）
```

- Metadata is bound into the AAD: a server tampering with the id/hlc in
  the manifest causes decryption to fail (integrity), and the manifest
  the server **sees** is itself ciphertext (confidentiality).
- File names and object keys other than `head`/`wrapped_key` carry no
  plaintext information: chunks use content hashes, manifests use seq
  (sequence numbers are not sensitive), and collection membership stays
  inside the ciphertext.
- The encrypted payload (body) is collection-specific JSON, UTF-8, with
  field names identical to DataStoreStd's existing persistence format;
  the serializer is shared.

### 5.4 Key rotation and post-revocation data protection

- Changing the passphrase: rewrite only `wrapped_key` (§5.1).
- Suspected device compromise: after revocation (§3.2), perform a **data
  rotation** -- re-encrypt all chunks with a new account_key and push a
  new manifest (small data volume, one pass); the old account_key is
  discarded. A revoked device, even if it kept the old key, can no longer
  read data added or modified after the rotation (its unmodified old
  data still sits in the old ciphertext -- this residue is accepted, and
  the UI states plainly that "revocation does not erase
  already-distributed history").

### 5.5 Threat model

| Threat | Defense | Residual risk (accepted and noted) |
|---|---|---|
| Server / operator snooping | E2EE: the server sees only ciphertext chunks, ciphertext manifests, and unreadable object names | It can see object counts/sizes and timing (metadata traffic analysis; optional padding, §7 S5) |
| Full server compromise | The same as above + the manifest chain (monotonic seq + prev_hash) makes **tampering** detectable | The server can **roll back** (serving an old head to a new device); stateful devices reject the downgrade via the chain; a brand-new device accepting an "old snapshot" is the residual risk |
| Device theft | Platform secure storage locks account_key; revocation blocks future writes | Physically extracting memory/disk in an unlocked state is outside the threat model |
| Weak passphrase | argon2id (m=64MiB, t=3, p=4) + passphrase-strength UI | A weak passphrase can still be brute-forced offline: the server stores the salt, not the passphrase, but wrapped_key is exposed to brute force; the UI enforces minimum entropy |
| Forgotten passphrase | No recovery backdoor (the price of E2EE) | Data is unrecoverable, stated plainly at registration; an optional paper recovery code (= the passphrase itself, merely a reminder to keep it safe) |
| Man-in-the-middle | TLS + object ETag / content-hash verification | Trust establishment for the account service on first login depends on the CA system (not rebuilt) |
| Malicious peer device | Every device holds the full account_key; no fine-grained isolation | Holds in a family-trust setting; untrusted peers are a v2 authorization-system problem |
| Replaying merge results | Merge-function idempotency + hlc total order | None |

Explicitly out of scope: server-side dedup by ciphertext hash (already
achieved by content addressing), zero-knowledge account recovery, and
post-quantum (the algorithm slot is kept in the version field for a
smooth swap).

## 6. Interfaces with the existing core modules

### 6.1 DataStoreStd (`core/std/data_store_std.h`)

- **Migration**: add `id` (UUIDv7) and `hlc` fields to
  `VocabItemStd`/`NoteItemStd`/`PronRecordStd`; on first load of an old
  file, backfill the defaults and `save()` once (done inside the
  existing `ensure_loaded` convention; the additive JSON fields are
  backward compatible -- every old field is retained). `history` needs
  no id (one snapshot record for the whole library).
- **New interfaces (design sketch, not this round's implementation)**:

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

- DataStoreStd's **query API is unchanged** (`get_vocabulary` etc.):
  sync is a capability beneath the storage layer; the UI is unaware.
- Case-insensitive word-key semantics stay inside DataStoreStd (the id
  is the foreign key); first-pairing dedup goes through the existing
  preview merge semantics (word key).

### 6.2 IndexEngineStd (`core/std/index_engine_std.h`)

**Does not participate in sync.** The index is derived data built from
dictionary files and rebuilt locally on each device; after sync completes
(user-data changes like vocabulary edits do not affect the dictionary
index -- the two are decoupled; only on `settings/profiles` changes),
notify the upper layer to rebuild or invalidate the aggregated query
cache on demand.

### 6.3 AggregateLookupStd (profiles)

`DictionaryProfile` is serialized as a `profiles` collection record
(`profile_id` is the record id), with LWW merge semantics; when a
profile referenced by `enabled_profiles` is deleted remotely, treat it
as a dangling reference: keep it locally but mark it disabled (never
silently delete groups).

### 6.4 adapters and the file-sync MVP

- `sync_service_qt` (file MVP) is **retained**: the degradation path
  (export with no account / no network) and "encrypted snapshot export"
  (§3.3) share its UI entry point; its merge semantics are already
  covered by §4.5 (history ordered union; local-wins for vocab is a
  special case of LWW). Long term, wire its preview/apply UI onto
  SyncEngineStd's merge results.
- Add `sync_client_qt` (adapters): a QNetworkAccessManager implementation
  of the §4.1 object read/write + account service REST; the core's
  SyncEngineStd never sees the network.
- Add UI (gui/qmlui, later): account settings page, device
  list/revocation, and a sync status indicator (last successful time /
  pending upload count / conflict-copy hints).

### 6.5 Paths and caching

- Add a `sync/` subdirectory under `PathUtilsStd::cache_dir()`: cursor,
  manifest chain cache, and pending-upload staging. It must **not** enter
  the mdict mdd cache directory (the `mdd_<hash>` semantics are
  unchanged, and the two caches are unaware of each other).
- Testing note (a lesson from real measurement, see todo.md B-3): any
  operation that rewrites a file's content changes the mdd cache
  signature -- sync code and tests must not touch the mdd cache
  directory.

## 7. Phased implementation

Every phase is independently deliverable, under a uniform gate:
`build-std` ctest all green + `scripts/coverage.sh` threshold PASS + the
Qt tree fully green (the repository's existing gates).

| Phase | Content | Deliverables |
|---|---|---|
| S1 local engine | id/hlc fields and migration in DataStoreStd; SyncEngineStd change export / merge function / HLC; cursor persistence | `core/std/sync_engine_std.*` + std-only tests (no network, no encryption, pure logic) |
| S2 E2EE format | vendor monocypher (A3); key derivation, chunk/manifest serialization and round trips | `core/std/sync_crypto_std.*` + KAT vectors (add a generator script following the `scripts/gen_mdict_crypto_vectors.py` convention) |
| S3 transport client | adapters object-storage client (list/read/write/read-back verification), backoff retry, sync-loop orchestration | `adapters/qt/sync_client_qt.*` + fake-backend tests |
| S4 accounts and devices | account service client; login / pairing / revocation / passphrase change; Qt settings page | account service (a separately deployable artifact, A4) + UI |
| S5 polish | tombstone GC, traffic padding (optional), conflict-copy UI, snapshot-export hookup | wrap-up and documentation updates |

S1/S2 have no external service dependency and can land first; S3 onward
depends on A1/A4.

## 8. Test strategy

### 8.1 Merge function: a table-driven conflict matrix

An expected-result table over every collection x every field x {local
newer / remote newer / equal / tombstone}, in std-only assert style
(repository convention). Key assertions:

- **Total-order convergence**: for the same pair of conflicting states,
  the winner is identical no matter which device computes it (cases
  where device_id breaks an hlc tie must be covered).
- No data loss: the loser's `notes` text goes into the conflict copy;
  the `tags` union is neither duplicated nor missing anything.

### 8.2 Protocol: multi-device simulation (in-memory fake backend)

- The fake backend implements the §4.1 layout (an in-memory map +
  injectable faults: packet loss / reordering / head concurrency
  conflicts).
- **Convergence property tests**: 2-3 virtual devices generate random
  operations (add/delete/edit; go offline for N rounds and reconnect);
  assert that all devices eventually converge to identical state
  containing the effects of every non-tombstone operation; re-running
  the same operation sequence yields unchanged results (idempotency).
- Clock cases: rollback, cross-device clock skew, same-millisecond
  counter overflow.
- head contention: two devices push at the same time; assert convergence
  after the read-back verification replay.

### 8.3 E2EE: KAT and destructive tests

- Known-answer vectors (monocypher official vectors + a self-generated
  script, delivered in S2).
- Round trips: arbitrary byte bodies (including empty, oversized, and
  invalid UTF-8).
- Tampering: flipping any single byte of the ciphertext/tag/AAD must
  fail; replaying with a swapped record_id must fail (AAD binding).
- Rotation: after a passphrase change, the old wrapped_key is invalid yet
  the data remains readable; after a data rotation, the old account_key
  can no longer read the new chunks.

### 8.4 Transport and integration

- A fake HTTP backend exercises backoff/retry/interruption; the Qt layer
  follows the `QTRY_*` convention.
- Real-backend smoke checklist: append to the
  `docs/gui-smoke-checklist.md` convention (two real devices + one MinIO:
  register -> add vocabulary -> pull on the other device -> offline edits
  on both -> conflict copy visible -> revoke a device -> change
  passphrase).

## 9. Open questions (to be decided before implementation)

1. **Account service form** (A4): a self-built minimal service vs
   integrating a third-party OIDC provider. The lowest-operations-cost
   path for self-hosting might be "object storage + a token-issuing
   function on a static site", to be decided before S3.
2. **monocypher procurement policy** (A3): whether vendoring a single
   file into `third_party/` fits the repository's dependency
   conventions (current dependencies are system libraries like zlib).
3. **Whether history is synced by default**: search history is
   privacy-sensitive data; on by default or off by default (leaning
   off, and stated plainly when first enabled).
4. **Server conditional writes**: if the target backend is confirmed to
   support If-Match, the §4.3 read-back verification can be simplified;
   not treated as a dependency.
5. **Version evolution rules for the shared snapshot-file/cloud-chunk
   format** (the semantics of version field upgrades).
