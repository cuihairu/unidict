> Language: **English** | [简体中文](./server_plan.md)

# Unidict Server Plan: Dictionary Library Service (Multi-Device Dictionary Management/Distribution)

> 2026-10-02 first batch (planning document, **no implementation moves before review approval**). Aligned with the
> document shape of mobile_plan.md: positioning and boundaries → the Chinese dictionary format
> surface → deployment forms and client integration →
> M-level milestones. The privacy position follows the established security defaults: **data egress is off by default, explicitly enabled,
> with the scope spelled out when enabled**.

## 1. Server Positioning: Dictionary Library Service

Not a cloud sync service, not an account center. In one sentence: **a dictionary warehouse installed in your own home + a multi-device install coordinator**.

### 1.1 Three Core Capabilities

| Capability | Content | Anonymous/account |
|------|------|-----------|
| **Dictionary metadata catalog** | Name, format (mdx/stardict/dsl/json...), entry count, language (zh/en/...), source (user-supplied/publicly licensed), file size and digest | Anonymous browsing |
| **Dictionary file distribution** | Download dictionary files by catalog entry (including .mdd resource companions); desktop/phone and every other end pull and load them through **the device's own existing parsing chain** (the same code path as local import today; the server does not parse dictionaries) | Anonymous download |
| **Multi-device install coordination** | Managed in one place (management page openable on any end), each end pulls the "should-install list": added → install, updated → upgrade, removed → uninstall | **Account required**: install state is private data that follows the device identity |

### 1.2 Account and Device Management Boundary

- **Usable anonymously (no account)**: catalog browsing, catalog search, dictionary file download, viewing service status. For the self-hosted single-user case even this layer is optional (direct local read on localhost).
- **Account required**: device registration (device name/platform/last online), "my install list" (which device installed which dictionary at which version), cross-device install/uninstall directives, multiple users sharing one warehouse.
- **Account model kept simple**: single administrator + optional read-only passphrase to start (99% of self-hosting is a personal warehouse); multi-user/invite systems do not enter M2, to be discussed as demand appears. Device credentials use long random tokens (issued at device registration), no OAuth suite.
- **Out of bounds (explicitly not doing)**: cloud sync of lookup history/vocabulary, statistics reporting, server-side dictionary content identification/deduplication (digest comparison is computed on the client).

### 1.3 Privacy Boundary (Established Security Defaults)

| Data | Default | Notes |
|------|------|------|
| Dictionary metadata/files (public material) | Anonymously fetchable | Dictionaries the user put into the warehouse themselves are treated as public material |
| Vocabulary, lookup history | **Not uploaded to the cloud** | Synced only when explicitly enabled; when enabled the UI states plainly "will send: entry list, notes, tags" (scope itemized), stopping the moment it is turned off |
| Device information | Minimal set | Only device name + platform + install list; no device fingerprinting/location collection |
| Server logs | Off by default | Access logging can be optionally enabled, with retention days stated plainly |

The sync channel (vocabulary etc.) is **independent of the dictionary distribution API**; its switch is listed separately in the client settings and is not tied to the master "connect to server" switch.

## 2. Chinese Dictionary Format Surface (User Called It Out: Must Be Able to Load Things Like Xinhua Dictionary)

### 2.1 Support Matrix

The mainstream interchange format for Chinese reference works (Xinhua Dictionary, Contemporary Chinese Dictionary, Classical Chinese Dictionary, Oxford English-Chinese Bilingual...) is MDict; the matrix is graded by "degree of support":

| Format/reference work | Status | Notes |
|-------------|------|------|
| **MDict (.mdx/.mdd)** -- the mainstream format for Chinese dictionaries like Xinhua Dictionary/Contemporary Chinese | Supported | core has a dedicated parser (`mdict_parser_std`), covering multiple block layouts, SimpleXOR encryption (password via parameter or `UNIDICT_MDICT_PASSWORD`), `.mdd` resource companions (images/audio), GB18030-encoded dictionaries (`charset_codec_std` + `gb18030_table`). **Loading a Xinhua Dictionary mdx = works today** |
| StarDict (.ifo/.idx/.dict[.dz]) Chinese-English/Chinese dictionaries | Supported | `stardict_parser_std`, with corrupt-file defenses |
| DSL (AbaDict etc., a common Chinese dictionary distribution) | Supported | `dsl_parser_std` |
| JSON / CSV / TSV / plain-text self-made wordbooks | Supported | Chinese entries go through as UTF-8 |
| EPUB dictionaries | Supported | `epub_parser_std` |
| Definition layout (dictionary-embedded HTML: bold/line breaks/images/audio) | Supported | `html_renderer_std`; images and audio inside `.mdd` are fetched through the resource chain |
| Fulltext search of Chinese content | Supported | Inverted index + TF/IDF; tokenization granularity is CJK per-character + combinations (see gap ⑤) |

### 2.2 Chinese-Specific Surface: Status and Gaps

| Surface | Status | Gap/road |
|----|------|-----------|
| ① **Pinyin lookup** (look up characters/words by pinyin: xíngháng finds "行") | Missing. Search has only exact/prefix/fuzzy/wildcard/regex/fulltext (Latin-letter oriented) | **High priority**. Two-layer approach: (a) metadata search over pinyin fields that the dictionary itself carries; (b) a built-in pinyin library (pinyin ↔ Chinese character polyphone mapping) doing query-side normalization -- "search by pinyin" as a new search mode `--mode pinyin`, results = characters/words whose readings include that pinyin. Change surface: add `pinyin_std` to `core/std` (mapping table + normalization), hang the new mode on `lookup_service`, entry points on all three ends CLI/GUI/Android |
| ② **Phonetic annotation display** (pinyin annotation xíng / ㄅㄆㄇ bopomofo) | Missing. Whatever the dictionary author put in the definition text is displayed as-is (most mdx carry pinyin text) | Medium priority. Pure display layer: annotating Chinese words in query results needs ①'s pinyin library + HTML ruby tag rendering (`html_renderer` currently has no ruby expansion). If the dictionary carries its own pinyin field, it costs nothing |
| ③ **Radical/stroke-count lookup** (look up characters by radical) | Missing | Medium-low priority. Needs a radical/stroke table (Unicode radical-strokes block + common character table), as a filter surface for catalog browsing (dictionary-lookup scenario), not a search mode |
| ④ **Chinese character stroke order** | Missing | Low priority. Stroke order is dynamic demonstration data (stroke sequences), with no ready-made standard to build in; depends on external data source licensing, evaluated separately, not scheduled at M level (list as a roadmap to-do first) |
| ⑤ Chinese word segmentation (fulltext search granularity) | Partial: naive n-gram of CJK per-character + two-character combinations | Medium priority. Fulltext search works for Chinese but recall noise is on the high side; bring in lightweight segmentation (dictionary-driven/two-character splitting) to improve, evaluated in the same batch as ① |
| ⑥ Traditional/Simplified conversion | Missing (`text_norm` does full-width/punctuation/Latin normalization, not traditional/simplified) | Medium priority. Traditional dictionaries queried in Simplified (and vice versa) is a hard need; an OpenCC-style table (static data) enters core |
| ⑦ Polyphones/context | Depends on the dictionary content itself | Do no engine-layer semantics; rely on reference work content |

**Relationship between the server and the Chinese surface**: pinyin/radicals/traditional-simplified are **client-side core search capabilities**,
decoupled from the server -- the server only delivers dictionary files to each end. The server plan only promises
**catalog metadata carrying "language / has-pinyin-index" tags** (for each end to filter on); strengthening the body of this lives on the
`core/` roadmap (see the §4 M4 dependency item).

## 3. Deployment Forms and Client Integration

### 3.1 Server Implementation Form (Self-Hosting First)

- **Single binary**: `unidict_server` -- reuses the `core/std` static library (no Qt, same chain as
  the CLI), HTTP static service + JSON API + local data directory
  (`UNIDICT_SERVER_DATA`, default `~/.unidict/server`: `catalog.json` +
  `files/`). No database dependency (the catalog is a single JSON/SQLite file, chosen at M1;
  default leaning toward single-file SQLite = zero dependency and migratable).
- **Docker one-liner**: `docker run -p 8787:8787 -v ./unidict-data:/data
  ghcr.io/cuihairu/unidict-server` (image build in CI, multi-arch
  amd64/arm64).
- **Reverse proxy/HTTPS**: no built-in TLS (self-hosters mostly have caddy/nginx),
  the docs give a one-liner example.
- Port defaults to 8787 (away from common ports), bind defaults to 127.0.0.1 (**listens on the
  local machine only by default**; exposure outward is an explicit action -- same position as the
  privacy default), opened explicitly with `--host 0.0.0.0` or config.

### 3.2 Client Integration Surface

| End | Entry | Action |
|----|------|------|
| Desktop qmlui | Dictionary management page + settings page | "Server" card: enter address → pull catalog anonymously → list display → tap install (through the existing `LookupAdapter` import path); device registration (account capability) explicitly enabled in settings |
| Android | DictManagerScreen | Same as above: remote catalog tab + one-tap install into the private directory (reusing the local loading chain after SAF import); server address scannable/pastable |
| CLI | `unidict_cli_std --server <url>` subcommand | Catalog list/download/local install (scripted deployment scenarios) |
| Desktop GUI (Qt Widgets demo) | Not connecting | Demo program, not chasing the new surface |

On every end, **the install action completes locally** (download → local parse and load); the server delivers no executable
content and takes no part in interpretation -- the attack surface collapses to "catalog and file integrity" (download verified
with SHA-256; `core` already has a sha256 implementation).

## 4. Milestone Schedule (M Level)

| Milestone | Content | Dependencies | Change surface |
|--------|------|------|--------|
| **S-M1 minimal usable server** (1 batch) | `unidict_server` single binary: catalog API (GET /api/catalog), file distribution (GET /files/<id>, SHA-256 verification field), local catalog scan into store (generate metadata for dictionaries the user drops into `files/`); Docker image + README integration section | None (core/std fully ready) | New `server/` directory (single binary + HTTP routing, selection below), CMake option `UNIDICT_BUILD_SERVER`, CI adds a server job, docs `docs/server_plan.md` → implementation record |
| **S-M2 desktop/Android integration** (1-2 batches) | "Remote catalog → tap install" on the desktop dictionary management page + Android DictManagerScreen; anonymous chain fully working; device registration and install list (first batch of account capability: device token + install state API) | S-M1 | qmlui dictionary management panel, Android MainActivity DictManager section, `LookupAdapter` remote-loading wrapper, core untouched |
| **S-M3 install coordination** (1 batch) | "Manage in one place, each end pulls": any end changes the install list → other ends pull the diff on next open and install/uninstall; list change notification (client polling to start, no push channel) | S-M2 | server list API, both ends' "sync install state" UI, conflict semantics (the list is authoritative + locally unique dictionaries preserved) |
| **S-M4 Chinese search surface reinforcement** (parallel with the S line, independent batch) | ①⑤⑥: `pinyin_std` pinyin search mode + Chinese-segmentation fulltext improvement + traditional/simplified conversion (OpenCC-style static table); catalog metadata carries the pinyin index tag | No dependency on S-M1/M2 (core is self-contained); referenced by the roadmap Chinese entries | New modules + tests in `core/std`, search entry points on three ends, catalog metadata schema v2 |
| **S-M5 enhancements** (deferred, on demand) | ② phonetic annotation display (ruby), ③ radical/stroke filtering, the vocabulary sync channel (explicit privacy switch), pronunciation asset server-side cached distribution (transparent proxy for online pronunciation = a privacy-protecting form, related to pronunciation source expansion) | S-M4① (annotation depends on the pinyin library) | Each an independent batch |

Technology selection (decided before S-M1, planning stage gives the lean): the HTTP layer leans toward a **single-file embedded
HTTP library** (mongoose/civetweb class, C++ single-binary friendly) rather than pulling in a framework dependency;
JSON in/out with a lightweight library (core currently has no server-side JSON need; the parser in json_parser
faces only dictionary formats). The lean becomes a final decision in the S-M1 kickoff batch and is recorded.

### 4.1 Explicitly Not Doing (Out of Scope for This Plan)

- Cloud lookup/definition hosting (dictionaries are parsed locally on the client; the server touches no queries)
- Store-style listing/review/payment
- Mobile background persistent sync (pulls happen when the user opens the app)
- Third-party login/SSO beyond the account system

## 5. Acceptance Criteria (Filled In Per Milestone)

- S-M1: local `unidict_server` serving → curl the catalog JSON → download an mdx → client loads locally and lookup hits;
  the Docker container passes the same chain
- S-M2: desktop/Android tap-installs a dictionary from the server → both ends can look it up; the anonymous (no
  token) chain fully works
- S-M3: end A adds, end B pulls and installs automatically; end A removes, end B uninstalls automatically; locally
  unique dictionaries are not mistakenly deleted by the list
- S-M4: `--mode pinyin xíngháng` hits the "行" polyphone; a Traditional dictionary queried in Simplified hits;
  fulltext Chinese recall compared before and after

## 5.5 Architecture Revision: Unified Zero-Account (2026-10-04, designed together with §6)

No server code has been written yet; this section is a design revision (not a code refactor): **the account system is cancelled as a whole** -- the original §1.2 "device registration / my install list requires an account" is instead carried by **sync-group device identity** (join via a dynamic passphrase), and the install list is folded into the sync-group instruction stream (InstallOp/RemoveOp etc., the same pipeline as the word-library instructions). After the revision the server has only two faces:

1. **Dictionary library face** (§1 original positioning unchanged): catalog/search/download, anonymous;
2. **Sync relay face** (§6): ciphertext instruction send/receive + sequencing, three deployable forms self-hostable. **Naming unified as "relay", not "server"** -- its responsibility is only instruction send/receive + sequencing, not being a server; implementation is dual-track: **C++ relay (`unidict-relay`, for self-hosting/LAN, same stack as the client, single binary)** + a Worker version (for hosting), same protocol.

No account center, no login/registration, no recovery flows; the account/anonymous boundary clauses of §1.2 are superseded by this section. The dictionary library face's "multi-device install coordination" is rewritten in sync-group terms (should-install list = shared state instructions within the group).

## 6. Personal Word Library Sync (added 2026-10-04, WeChat Keyboard mode)

> Modeled on WeChat Keyboard's personal word library sync: dynamic-passphrase pairing + collection sync + incremental push/pull, **zero accounts**. Position: this is **sync** (multi-device real-time consistency, entry-merge semantics), not "backup".

- **Sync surface**: vocabulary/favorite entries (core), lookup history, entry notes, custom dictionary configuration and preferences (learning-record class).
- **Instruction stream model (upgraded 2026-10-04, binlog mode)**: all changes are **instructions** (AddEntry/RemoveEntry/UpdateNote/UpdatePref...); what syncs is the instruction stream, not state snapshots -- **replaying yields consistency; conflicts do not exist**: instructions carry op_id (device id + local sequence) for idempotent dedup, the server sequences and dispatches (single binlog source, global sequence number); clients pull incrementally by **sync position** and replay (on returning from offline, resume from the position); a deletion is just one instruction (no tombstone semantics needed); periodic snapshots + instruction compaction (preventing unbounded log growth, binlog purge likewise); instructions replay deterministically, concurrent updates to the same field take effect in server order (total order, no silent lost updates).
- **Pairing method (corrected 2026-10-04, same as WeChat Keyboard)**: **no account needed** -- one end generates a **dynamic passphrase** (short-lived pairing code), the other end enters it to join the sync group; the dynamic passphrase is the sync group's shared secret and can directly derive the encryption key (end-to-end, the server sees only ciphertext), a zero-account system.
- **Version number and sync status display (added 2026-10-04)**: the sync group has a **monotonic version number** (= the instruction stream position, sequenced by the server); each device's sync UI shows: ① the group's latest version number; ② this device's synced position; ③ the status verdict ("up to date" / "behind by N instructions") + last sync time -- at a glance the user knows whether this device is synced to the latest; when syncing completes and the positions align, it is "up to date".
- **Device mutual visibility and permissions (added 2026-10-04)**: every device can see **all devices in its sync group** -- device list (name/platform/last sync time/remark, self marked). **Equal permissions within the group, no administrator**: any device freely joins/leaves (dynamic passphrase to join, voluntary exit); each device **can only edit its own remark** (cannot edit others').
- **Keys and encryption (end-to-end)**: ① the founding device locally generates the **group key** (high entropy, never uploaded) and derives a short-lived **dynamic passphrase**; ② a new device joins via a **PAKE protocol** (SPAKE2/OPAQUE class) trading the short code for a strong key -- the short code itself is not used as the key (anti-brute-force), and PAKE also defends against man-in-the-middle and code collision; ③ the whole instruction stream is symmetrically encrypted (XChaCha20-Poly1305/AES-GCM class) before upload, and **the server sees only ciphertext** (ciphertext binlog, sequencing and forwarding only); ④ the group key never leaves the device; leaving/removal = destroy the key locally, optionally rotate the group key (new key encrypted and delivered to online devices, or reset the group). **The cost, stated plainly: all devices lost = all data lost** (no account recovery), the self-rescue hatch = export an encrypted backup to local/cloud storage.
- **Deployment forms (three built in, user's choice, decided 2026-10-04)**: ① **official hosted relay** -- Cloudflare Worker+KV/D1 free tier (receive ciphertext/sequence/forward, zero ops zero cost; under E2E the hoster sees only ciphertext, no privacy liability, storage quota-capped against abuse); ② **bring-your-own relay** -- supports pointing at the user's own WebDAV/cloud storage/R2 (Syncthing mode, zero dependency); ③ **LAN direct transfer** -- same-network devices discover each other and sync directly (optional bonus). Pick one of three in the client settings; sync capability is completely identical across the three forms (instruction stream + group key untouched). **The relay service itself is open source** (distributed with the repository), users can self-host -- an open-source project carries no hosting obligation, and official hosting is merely an option.
- **Privacy hard constraints (off by default)**: data egress/sync features are **off by default, explicitly enabled in config, with the sync scope stated when enabled** (which entries go to the cloud); end-to-end encryption (the server sees only ciphertext, keys never leave the device).
- **Schedule**: follows the server_plan M-level milestones (no dependency on the account face, can move earlier); the mobile_plan device side reuses the same collection-sync client.

## 7. Sync Service Implementation Batches (refactor plan, established 2026-10-04)

> Implement per the §5.5/§6 positions; features before tests, each batch an acceptably incremental commit pushed; the **sync feature off by default** red line holds throughout.

- **B1 relay-face core**: instruction-stream relay -- receive instructions (op_id idempotent dedup)/sequence/incremental pull by position; **C++ `unidict-relay`** (single binary, self-hosting/LAN landing, same repo same stack as core) + Cloudflare Worker version (hosted form), two implementations of one protocol, tests mutually recognized. → **Landed 2026-10-04**: `server/sync_relay/` (PROTOCOL.md v1 contract + dev/ Python reference implementation + worker/ D1 hosted form + README); contract compliance tests dev 16 cases (ctest `sync_relay_protocol`) + worker 14 cases (node:sqlite D1 shim); the C++ `unidict-relay` is deferred to B5 in the same batch as the three forms.
- **B2 client sync engine**: instruction generation (device id + local sequence)/incremental push-pull/offline position resume/deterministic replay/snapshot and instruction compaction (preventing unbounded log growth). → **Landed 2026-10-05**: `core/std/sync_engine_std.h/.cpp` (core, no Qt) -- `SyncTransportStd` narrow transport interface (HTTP/WebDAV/LAN bindings go to B5); instruction generation op_id=`<device_id>:<local sequence>` idempotency key, enqueue takes effect locally immediately + outbox; push 256 instructions/batch + ack dedup (offline retry resume), pull per PROTOCOL §2.8 (snapshot gap skip → incremental paging); deterministic replay takes the **full canonical order** (words/tags lexicographic, history by (ts,word)), instruction semantics commutative and idempotent -- two devices with the same instruction set are byte-identical in state; snapshot-threshold compaction + save/load_state checkpoint resume. Tests `test_sync_engine_std` 13 groups (in-memory relay fault injection), three gates build-std/build/coverage lines 100% all green
- **B3 pairing and keys**: dynamic passphrase generation (short-lived) + PAKE (SPAKE2 class) key exchange, group key management (destroy on exit/rotate/reset group), instruction symmetric encrypt/decrypt (XChaCha20-Poly1305 class) -- keys never leave the device. → **Landed 2026-10-07 B3-a**: `core/std/crypto_std.h/.cpp` + `core/std/sync_crypto_std.h/.cpp` (core with no Qt and no third-party dependencies) -- HMAC/HKDF/Poly1305/XChaCha20-Poly1305 self-implemented, pinned to the vectors of RFC 8439 §2.8.2/§2.5.2, draft-irtf-cfrg-xchacha-03 §A.3.1, RFC 5869/4231; group key ring (rotate/destroy on exit/reset group, sealed version header + AAD binding); dynamic pairing code (Crockford 8 characters 40bit, TTL + single consumption); `import_key` as the key-injection surface. **B3-b landed (same day)**: `core/std/spake2_std.h/.cpp` -- SPAKE2+ (RFC 9383 augmented PAKE, P256-SHA256-HKDF-SHA256-HMAC-SHA256) pure C++17 self-implementation (P-256 field arithmetic + Jacobian point arithmetic); pairing code derives w0/w1 via PBKDF (identity binding, the short code does not directly derive keys -- the anti-downgrade position unchanged); the handshake's K_shared travels to the group key via the key-exchange envelope (XChaCha20-Poly1305), injected with `import_key`, keys never leave the device; RFC 9383 appendix C full-chain vectors pinned byte-for-byte. Three gates build-std 127 / build 148 / coverage lines 100% (core/ 8741 lines) all green.
- **B4 device face**: sync-group device list (name/platform/last sync/remark/self marked), equal permissions with free join/leave, each device edits only its own remark.
- **B5 three deployment forms**: bring-your-own relay (`unidict-relay` self-hosted / WebDAV/cloud storage/R2), LAN direct transfer (same-network discovery), official hosting (Worker) deployment scripts -- three forms pick one of three in client settings, sync behavior completely identical. **Relay one-line installer** (`install.sh`: OS/arch detection, anonymous pull of release artifacts, idempotent reinstall = upgrade, fail-fast with errors, optional `--systemd` service registration; following the chirp/luna install.sh spec; a Windows install.ps1 to the same spec). → **Landed 2026-10-07 B5-a/B5-b (C++ bring-your-own relay)**: `core/std/sync_relay_state_std` (contract semantics layer, structural API with zero JSON dependency) + `server/sync_relay/cpp/` (single-binary HTTP shell, Qt-free, POSIX/Winsock); the dev contract compliance suite of 16 cases pointed straight at the C++ implementation via `UNIDICT_RELAY_EXTERNAL_BASE` (ctest `sync_relay_cpp_protocol`) -- three implementations, one set of assertions; three gates build-std 129 / build 150 / coverage lines 100% (core/ 8989 lines) all green.→ **Landed 2026-10-07 B5-c (relay into release artifacts + one-line install)**: the daily-build three-platform zip stage collects `unidict-relay`, Linux deb/rpm via `install(TARGETS unidict-relay RUNTIME DESTINATION bin)`, the Windows installer takes in the whole stage tree + a verification manifest pinning unidict-relay.exe; PLATFORM-NOTES/BUILD_INFO/VERIFY.md/Release artifact lists synchronized in four places (liveness probe `GET /api/sync/relay/ping`); install.sh/install.ps1 landed (2e6b8c3 distribution chain + b964e60 installer scripts; NOMINMAX fixes the Windows leg 7a0f917). Interface minimally corrected per the decided position: `--version` verification → liveness probe `GET /api/sync/relay/ping`, `-bind HOST:PORT` split into `--host/--port`, artifacts aligned with the nightly platform zips (missing armv7/macos-x64 legs means an error), the unauthenticated fake `--secret` withdrawn, default listen back on loopback. LAN direct transfer/hosted client bindings go to the rest of B5.
- **B6 install list folded in**: dictionary multi-device install coordination switches to the instruction stream (InstallOp/RemoveOp, should-install list = shared state within the group), replacing the §1.2 account position.
- **B7 UI and self-rescue hatch**: sync settings page (dynamic passphrase/device list/form selection/scope stated), the **off-by-default explicit-enable** switch, export encrypted backup (against all devices lost).

Schedule: start after the in-flight client batches (P-4/P-5 etc.) wrap up, in B1→B7 order; B1/B2 are the foundation and go first.
