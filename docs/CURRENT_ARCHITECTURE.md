> Language: **English** | [简体中文](./zh/CURRENT_ARCHITECTURE.md)

# Unidict Current Architecture Audit

> Phase 1 · Current-state baseline · Audit date 2026-10-04 · Baseline commit fae5002
> Method: read-only agent audit of four domains (core / adapter layer / QML UI / test docs CI) + manual spot-checks of load-bearing facts.
> This document states current facts only, with no change proposals (disposition directions are in TECH_DEBT.md).

> **2026-10-13 incremental update** (the body keeps the audit-date scope and is not rewritten; per the TECH_DEBT document convention, closure differences are recorded here):
> - **§8 Mobile**: Android has advanced to **M8-B** — M4 TTS, M5 polish and packaging, M7 share three entry points (SEND/PROCESS_TEXT/long-press shortcut), M8 floating lookup window (foreground service floating ball + mini lookup panel), and M8-B desktop-resizable widget are all delivered (emulator acceptance tokens on record; authoritative scope is docs/mobile_plan.md); iOS/HarmonyOS still not started.
> - **§9 Sync surface**: the body's "B2/B5 not started" is outdated — the B5 C++ relay (op_id ordering/idempotency/offset pulling/snapshot) and the client engine are delivered: sync_device_std / sync_engine_std / sync_relay_state_std / sync_sealed_transport_std / sync_backup_std (PBKDF2 + XChaCha20-Poly1305 encrypted backup/restore) + LAN host/manager Qt surface; tests/ currently has 10 sync test files (the corresponding roadmap Advanced Features row is checked).
> - **Platform integration surface** (not in the audit-date body; F9–F12 delivered): autostart (Windows registry / Linux XDG / macOS launchd, single `StartupLauncher` surface + test seam); global hotkeys on three platforms (Windows RegisterHotKey / macOS Carbon / Linux XGrabKey, honest failure on Wayland sessions; qmlui/gui/tests three targets share the same-surface helper); system tray (qmlui `TrayController` + gui tray menu, degrading to close-window-exits when no tray is available); full-text index rebuild (`DictionaryManagerStd::reset_fulltext_index()` + settings page button). New on the build side: X11 QUIET probe gating (`UNIDICT_HAS_X11`) and top-level `enable_language(OBJCXX)` (directory-scope pitfall, see the build-form section of ARCHITECTURE.md).
> - **§5 Settings keys**: the audit-date "ui/* 7 keys, pron/* 3 keys, voice/* 4 keys" is incomplete — newly added ui/def_font_size, ui/close_to_tray, quicklookup/hotkey, quicklookup/enabled, sync/enabled, sync/gid, etc.; the TD-122 key-name convergence track remains in effect.

## 0. One-Sentence Current State

Capabilities have converged to a single side, **core/std (pure C++17)**: data storage, full-text index, aggregation, rendering, and pronunciation logic are all single std implementations, with `cli-std` (zero Qt) as the pure-std production chain (deb/rpm distribution). **The product UI main chain now rides entirely on the std core, and the legacy dictionary chain has been retired along with its code** (updated 2026-10-10): both qmlui and gui dictionary access go through `DictionaryManagerStd` + std parsers (gui switched to std in 9ccf0bf; the legacy `DictionaryManager` singleton, four sets of Qt parsers, plugin_manager, and the bridge layer were deleted the same day in 927f376, closing TD-101/102/105). At the shell layer, the coexistence of QML (the sole main exit) and gui (the pronunciation-practice feature shell) is currently the largest architectural fact.

## 1. Layered Overview

```
┌─────────────────────────────────────────────────────────────┐
│ 壳层   qmlui（Qt Quick，MainDesktop.qml 为活入口）           │
│        gui（QWidget 单窗口桌面，发音评分整链在此）           │
│        cli-std（无 Qt 主力 CLI）                             │
│        android（Kotlin+Compose 原生壳 + JNI 桩）             │
├─────────────────────────────────────────────────────────────┤
│ 接入层 adapters/qt（薄桥：DataStore/fulltext/utils/settings）│
│        adapters/android/jni（21 个 JNI 导出）                │
│        adapters/pron（ONNX 评分推理壳，UNIDICT_BUILD_PRON 门控）│
├─────────────────────────────────────────────────────────────┤
│ 核心 core/std/（纯 C++17，零 Qt）                            │
│        解析器/索引/存储/渲染/聚合/发音/工具                  │
│        （core/ 顶层仅剩 unidict_core.h 的 DictionaryEntry    │
│          DTO + path_utils 门面，无 legacy 实现）             │
└─────────────────────────────────────────────────────────────┘
```

QML entry facts (qmlui/main.cpp:159-161): the desktop loads `qrc:/MainDesktop.qml` by default; `qrc:/Main.qml` is compiled only under Android/iOS macros (already superseded by the native shell, now a dead path). `MainModern.qml` and the `qmlui/modern/` dead tree were deleted on 2026-10-08 (formerly TECH_DEBT TD-111). resources.qrc compiles only 8 files; the live qmlui surface is just these 8 QML files (Main/MainDesktop/components×3/mobile/common×3).

## 2. Build Targets and Link Topology

core/CMakeLists.txt has two std targets:

- `unidict_index_std` — index_engine_std + text_norm_std (small and pure)
- `unidict_std_core` — all remaining std (USE_ZLIB compile macro links zlib)

adapters/qt/CMakeLists.txt: `unidict_core_qt` = path_utils facade + dictionary entry DTO header (all that remains after the legacy dictionary chain retired; the DataStore double-hop facade was split in TD-106, and consumers connect directly to unidict_data_qt's DataStoreQt; the `unidict_plugins_qt` target was deleted) + transitive linking of `unidict_utils_qt / unidict_data_qt / unidict_std_core`.

Production chain:

| Artifact | Links | Nature |
|---|---|---|
| qmlui (unidict_qml) | unidict_core_qt + unidict_fulltext_qt | Qt chain, desktop live entry (sole main exit) |
| gui (unidict_gui) | unidict_core_qt | Qt chain, QWidget pronunciation-practice dedicated shell (recording/repeat-after/scoring exist only here) |
| cli-std (unidict_cli_std) | unidict_std_core + unidict_index_std(+pron) | Pure std, the only CLI, deb/rpm packaging |
| android (unidict_jni) | unidict_std_core + unidict_index_std | std-only surface hard-set, AGP and command-line NDK dual paths |

## 3. Dual-Implementation Coexistence Master Table

| Capability | legacy core/ (retired 927f376) | core/std | Actual production-chain usage |
|---|---|---|---|
| StarDict/MDict/JSON/EPUB parsing | ~~four complete Qt implementations~~ (deleted) | four complete std implementations | **std four sets** (qmlui/gui via DictionaryManagerStd) |
| DSL/CSV/TSV/plain | — | std (dsl/csv parsers) | std across the board (UI main chain likewise via DictionaryManagerStd→ParserRegistryStd; .dsl registration see parser_registry_std.cpp:19) |
| *ParserQt bridge | ~~thin wrapper over the std kernel~~ (deleted) | — | None (the bridge slot in tests/qt_adapters_test.cpp retired along with it) |
| IndexEngine | ~~Qt implementation~~ (deleted) | IndexEngineStd (prefix/fuzzy/wildcard/regex + index maintenance) | std side (via std manager) |
| DataStore | facade (267 lines) | DataStoreStd real implementation (977 lines) | Converged through data_store_qt to the single std implementation |
| Full-text search | ~~composed std engine~~ (deleted) | FullTextIndexStd | std engine (via std manager) |
| MDD resources | — | MddResourceParser | std (std manager has_resource/resource_* direct access; gui res:// callback same source) |
| path_utils / text_norm | Qt implementation / — | path_utils_std / text_norm_std v2 table-driven | path_utils_qt wraps std; text_norm fully std |

Key point: **the true dual-implementation fork surfaces (4 parser pairs, dual managers) were retired along with the legacy chain** (2026-10-10, 927f376). Production chain single-std surface: `DictionaryManagerStd` is the only manager, the std four parser sets are the only implementations; the core's positive facade: grouped queries/failure isolation are complete on the std side (`DictionaryManagerStd::search_grouped`, failed_dictionaries/retry/forget); history/vocabulary notebook/notes have a single source of truth in DataStoreStd (the std manager holds no history; after TD-106 consumers connect directly to DataStoreQt).

## 4. Lookup Main Chain (qmlui desktop, after the Eudic rearrangement)

> TD-121 bridge split (closed in three cuts on 2026-10-11): the original lookup_adapter god-bridge was split by surface into three adapters — platform (clipboard/selection/hotkey collection), voice (TTS/online pronunciation/autoplay), vocab (vocabulary/history/notes storage forwarding) — each registered under the same name as a QML context property; under the "lookup" name only the dictionary query/rendering/import/navigation surface remains. Auto-pronunciation on lookup hit is wired by main.cpp from the LookupAdapter::entryLookedUp signal to VoiceAdapter.

```
openWord → lookup_adapter.aggregateLookup(word, {maxTotalResults:20, sanitizeHtml, rewriteCrossRefs})
  → **DictionaryManagerStd::search_grouped**（std 单口径，见下节聚合注记）
    词头精确 → 前缀 → 释义包含 三层降级，层内按词典分组、同词头折叠去重
  → EntryResultsPane：内容 Tab「词典 / 例句 / 词组 / 近义联想 / 全文检索」
                        + 每词典一个可折叠分组卡（同词典释义聚组、细线分隔、无硬边框）
```

- The Examples tab = entries whose definition contains the target word (**no independent example-sentence library**); Phrases = compound words starting with the query word (`relatedLookup(word,"phrases")`); Related = prefix+fuzzy candidate word list (`relatedLookup(word,"related")`, no antonym data source); Full-text search = `fullTextLookup`.
- **Aggregation surface = DictionaryManagerStd::search_grouped single surface (UI main chain + each CLI search surface)**: five-level fallback (exact > lemmatization > prefix > definition > fuzzy) + within-group headword dedup. The original standalone aggregator DictionaryAggregator (TD-104 dual sets) was retired on 2026-10-10 after a consumer-surface scan (zero production consumption, the dual-set divergence risk thereby eliminated; P-3 3.3 has ported the fallback semantics into the manager).

## 5. Storage and Data Files

| Data | Carrier | Location | Notes |
|---|---|---|---|
| Search history (limit 100), vocabulary notebook CRUD+tags+notes, pinning | DataStoreStd (JSON) | Standard data directory | The only real implementation (consumers connect directly to DataStoreQt → DataStoreStd) |
| Learning statistics/review history | No independent file | Vocabulary notebook entry level (DataStoreStd set_vocabulary_skill, four-skill field bits) | learning_stats.json retired with learning_manager (P-7 dual storage merged, 2026-10-10); the review UI remains reserved and unwired (TD-114) |
| Sync manifest | JSON manifest (owner*/added/removed/changedSources*) | User-chosen local path | File-level sync, no account, no cloud |
| Full-text index | UDFT on-disk format (fulltext_index.md spec) | Index directory | Lazily built (perf record ~1.3s) |
| Settings | QSettings IniFormat | Standard config | ui/* 7 keys, pron/* 3 keys, voice/* 4 keys; key names freely defined by QML callers |
| Dictionary health | quarantine | — | Parse failures isolated for explicit retry; missing files self-heal |

No local encryption (roadmap unimplemented item); mdict encrypted dictionary support exists separately (`[encrypted]` detection + password setting).

## 6. Rendering Pipeline

html_renderer_std: whitelist sanitization + link rewriting (entry:// bword:// → `#w:` in-page anchors; img/audio relative src → `res:///<key>?dict=<id>`, with QTextBrowser::loadResource fetching resources from the sibling .mdd). The gui-side ResultBrowser (QTextBrowser) follows the same surface. `HtmlRenderOptions` has converged to the single field `resolve_links` (TD-115 closed: seven dead fields removed on 2026-10-04). cross_reference_std provides entry cross-references.

## 7. Pronunciation System (Three Layers)

1. **Local TTS**: QTextToSpeech; voice/preset/rate/pitch/volume + autoplay delay (Drawer "Voice" tab).
2. **Online sources**: `PronunciationSourceStd` abstraction → `FreeDictionarySource` (dictionaryapi.dev, keyless). Three states (local/online/auto), accent selection US→UK→AU→Unknown (dual inference from URL filename segments and text annotations); privacy criteria: request_url sends only the query word itself, with explicit UI copy.
3. **Scoring (optional, gated)**: `adapters/pron` PronScorerOnnx (Pimpl isolates onnxruntime); the 635MB fp16 model + vocab.json are **not committed to the repo**, paths given at runtime. M1–M9 all implemented: recording (16k/mono/16bit) → reference → repeat-after → waveform → playback → CTC forced alignment + GOP per-phoneme scoring, confusion localization, variant-tolerant scoring, word-final g→ŋ, vocabulary notebook integration (word score <0.6 auto-tags "unstable pronunciation"), relativized practice history. `UNIDICT_BUILD_PRON` defaults OFF; M10 model bootstrap download (cli-std writes curl config, resumable + anti-fake-model landing), depends on external `curl`.

The whole pronunciation chain (recording/waveform/scoring) is integrated on the gui (QWidget) side; the qmlui side has only the settings surface.

## 8. Mobile

- Android: Kotlin+Compose native shell + JNI (dict/lookup/store three domains, 21 exports) → std core. M0–M3-C delivered (docs/mobile_plan.md: import/lookup/vocabulary notebook/history/notes); M4 TTS and M5 polish/packaging not done. daily-build.yml has a Gradle+AGP+JNI release apk job.
- The QML mobile path (Main.qml + qmlui/mobile/) has been superseded by the native shell — a dead path, still compiled into qrc.
- iOS / HarmonyOS: not started.

## 9. Sync / AI / Server Surface

- **sync_service_qt**: local file-level merge MVP (history ordered union, vocab local-wins, remote-only append; previewDiff/applyPreview/applySelection/exportSelection/importSelection for selective merging).
- **ai_service_qt**: external command bridge (env `UNIDICT_AI_CMD`; `translate` / `grammarCheck` two items; no streaming, no provider concept).
- **Sync relay** (delivered 2026-10-04): `server/sync_relay/` — PROTOCOL.md v1 contract + dev (Python stdlib) / Worker (Cloudflare D1) dual reference implementations, contract tests dev 16 cases (ctest `sync_relay_protocol`) + worker 14 cases (node:sqlite D1 shim); the C++ version `unidict-relay` and the client sync engine belong to B2/B5, not started.
- **server_plan.md** (2026-10-02): dictionary library service = metadata catalog + anonymous file distribution + multi-device install coordination — a design draft, "no implementation until review passes"; of it, the sync relay surface B1 is delivered as above.
- **design/sync-engine.md**: S3-compatible blob + account + E2EE (HLC + monocypher) cloud-scale design, S1–S5 unimplemented.
- The lookup core path contains no network dependency (core has zero network stack) — **the local-first baseline is established**.

## 10. Mapping Differences with roadmap.md (sample)

| roadmap entry | Audit fact |
|---|---|
| Pronunciation M1–M9 | roadmap:106 already says delivered M1–M9 (aligned in 9ef59a2, difference eliminated) |
| "Anki-style flashcard review (basic)" [x] | No review UI on the live path (review exists only in the dead Main.qml / unpublished modern components) |
| "Achievement/gamification" [ ] | learning_manager retired and deleted with P-7 (achievements/gamification are a §A forbidden zone, TD-113 closed) |
| "Markdown support" (currently only :139 one [x]) | Duplicate entry removed |
| "Conflict preview & merge (file-based MVP)" [x] | True (sync_service_qt) |

---

Appendix: audit basis (four read-only domain reports from this session + spot-check commands); this document modified no implementation files.
