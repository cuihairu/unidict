# Source Architecture Layers

> Language: **English** | [简体中文](/zh/architecture)

This page records the runtime layers after reading source. A layer is a responsibility, not a directory.

## Layer Map

| # | Layer | Responsibility | Representative implementation |
|---|---|---|---|
| L1 | Application shell | Start one product face, initialize services, own command parsing, window lifecycle, and event-loop teardown. | `qmlui/main.cpp:75`, `gui/main.cpp:1604`, `cli-std/main.cpp:304` |
| L2 | Presentation and service binding | Publish UI-facing objects, adapt Qt types to std types, and assemble shell-only signals such as lookup to speech. | `qmlui/main.cpp:181`, `qmlui/main.cpp:229`, `qmlui/lookup_adapter.cpp:36` |
| L3 | Domain services | Own multi-dictionary aggregation, user learning data, import/import failure policy, and deterministic sync state. | `core/std/dictionary_manager_std.h:19`, `core/std/data_store_std.h:63`, `core/std/sync_engine_std.h:103` |
| L4 | Knowledge codecs | Read dictionaries and companion resources from external formats into domain objects. | `core/std/parser_registry_std.cpp:15`, `core/std/mdd_resource_std.cpp:986` |
| L5 | Search and entry rendering | Normalize keys, index words and definitions, rank results, sanitize and rewrite entry HTML. | `core/std/index_engine_std.h:39`, `core/std/fulltext_index_std.h:15`, `core/std/html_renderer_std.h:21` |
| L6 | Intelligence and sensing | Provide optional ONNX pronunciation scoring, local/online speech, clipboard/selection capture, hotkeys, and desktop shell support. | `adapters/pron/onnx_pron_scorer.h:22`, `qmlui/voice_adapter.cpp:166`, `qmlui/platform_adapter.h:1` |
| L7 | Sync communication | Move sealed commands over HTTP, host the relay contract, discover LAN peers, and expose relay status to the shell. | `adapters/qt/sync_http_transport_qt.cpp:58`, `server/sync_relay/cpp/relay_routes.h:1`, `server/sync_relay/cpp/lan_host_std.cpp:1` |
| L8 | State infrastructure | Persist user data, dictionary/sync state, indexes, caches, settings, keys, and relay data in their appropriate stores. | `core/std/data_store_std.cpp:30`, `core/std/dictionary_manager_std.cpp:971`, `adapters/qt/sync_manager_qt.cpp:52` |

## Service Inventory

| Service/module | Layer | State store | Call surface | Configuration | Lifecycle |
|---|---|---|---|---|---|
| QML shell | L1 | None directly | QML views call context objects; `main` owns the engine and context object lifetime. | Bundled dictionary paths and `UNIDICT_DICTS`; `qmlui/main.cpp:133` | Starts after `QApplication`; context properties live as stack objects until process teardown; `qmlui/main.cpp:181` |
| Widgets shell | L1 | `dictionary_state.json` | `MainWindow` calls `dictMgr()` and `DataStoreQt`; user dialogs mutate both; `gui/main.cpp:67`, `gui/main.cpp:74` | `UNIDICT_STATE_DIR`, `UNIDICT_DICTS`, `UNIDICT_DICT_DIR`; `gui/main.cpp:74`, `gui/main.cpp:152` | Loads state before `MainWindow`; explicit save after dictionary mutations; `gui/main.cpp:154`, `gui/main.cpp:247` |
| CLI | L1 | Reads/writes user store and cache paths passed by commands | Direct C++ calls into `DictionaryManagerStd`, `DataStoreStd`, exports, and optional pronunciation adapter; `cli-std/main.cpp:304` | argv plus `UNIDICT_DICTS`, `UNIDICT_MDICT_PASSWORD`; `cli-std/main.cpp:350`, `cli-std/main.cpp:336` | Parses argv, constructs managers inside `main`, returns after one command |
| Lookup facade | L2 | Owns import counters/queue and navigation stacks in memory | QML invocations -> `DictionaryManagerStd`, `HtmlRendererStd`, `MddResourceParser`, and history write through `DataStoreQt`; `qmlui/lookup_adapter.cpp:36`, `qmlui/lookup_adapter.cpp:721` | `UNIDICT_DICTS`, MDict password in memory, normalization profile from QSettings; `qmlui/lookup_adapter.cpp:171`, `qmlui/main.cpp:185` | Lives for the app; import queue is canceled/reset through timer steps; `qmlui/lookup_adapter.cpp:226` |
| Dictionary aggregator | L3 | `DictionaryStd` objects, failure records, and index caches in memory | Facade/GUI/CLI call lookup, import, resource, metadata, and state APIs; `core/std/dictionary_manager_std.h:19` | `DictionaryManagerStd::save_state/load_state`, text-norm profile; `core/std/dictionary_manager_std.cpp:971` | Lives with owning shell; rebuild/clear on add/remove/reset; `core/std/dictionary_manager_std.cpp:144` |
| Dictionary instance | L3/L4 | Holds parser, words, resource parsers, and source paths | Aggregator loads via registry, forwards lookups and normalization; `core/std/dictionary_std.cpp:15` | Text-norm options; parser-specific password flows through manager; `core/std/dictionary_std.cpp:87` | Owns parser/resources until removed/cleared |
| User state store | L3/L8 | `data/unidict.json` by default; explicit path supported | Qt singleton, JNI, and CLI call add/remove/query/export/import; `adapters/qt/data_store_qt.cpp:13`, `adapters/android/jni/store_jni.cpp:31` | `DataStoreQt::setStoragePath`, Android store path, CLI export/import paths; `adapters/qt/data_store_qt.cpp:17` | Lazy loads on first access; every mutation saves immediately; `core/std/data_store_std.cpp:426` |
| Lexical index | L5 | In-memory maps/trie; optional TSV-like index file | Manager mutates and queries it; CLI can save/load it; `core/std/index_engine_std.cpp:381`, `core/std/index_engine_std.cpp:397` | Normalization profile and index file path; `core/std/index_engine_std.h:78` | Incremental on add/remove; explicit rebuild after bulk import or load; `core/std/index_engine_std.h:44` |
| Full-text index | L5 | In-memory postings; `UDFT1/2/3` files | Manager lazy-builds/warms, exports stats, and saves/loads through facade or adapter; `core/std/dictionary_manager_std.cpp:711`, `adapters/qt/fulltext_manager_qt.cpp:39` | Content signature plus tokenizer version; CLI save/load/upgrade options; `core/std/fulltext_index_std.h:19`, `cli-std/main.cpp:377` | Lazy first query, warm after import, reset when dictionary set changes; `qmlui/lookup_adapter.cpp:210` |
| Entry renderer/resource resolver | L5 | Resource key map in memory and extracted files under `CacheLocation` | Lookup facade and GUI render/sanitize/rewrite before display; `qmlui/lookup_adapter.cpp:347`, `gui/main.cpp:186` | Cache location and MDD source path; `qmlui/lookup_adapter.cpp:352` | Renderer is stateless; resource parsers live with dictionaries; cache survives process scope |
| Pronunciation scoring | L6 | Model/vocab files and in-memory inference tensors | GUI/CLI load configured assets and call scorer; core still computes GOP; `gui/main.cpp:55`, `adapters/pron/onnx_pron_scorer.cpp:1` | `UNIDICT_BUILD_PRON`, model/vocab paths, fetch model command; `cmake/BuildOptions.cmake:68`, `cli-std/main.cpp:337` | Build-time optional; session/model loaded on demand; process ends after CLI scoring |
| Speech/lookup sensing | L6 | QML properties, TTS/player state, clipboard cache | Shell connects lookup hits to speech; QML calls voice/platform adapters; `qmlui/main.cpp:190`, `qmlui/MainDesktop.qml:527` | QSettings keys under `ui`, `selection`, `quicklookup`; `qmlui/MainDesktop.qml:598` | Objects live with shell; monitors stop/start by user switch; `qmlui/clipboard_monitor.cpp:14` |
| Client sync engine | L7/L3 | `engine_state.json`; QSettings for metadata | `SyncManagerQt::syncNow` pushes/pulls through sealed transport; enqueue currently occurs in tests/core APIs, while backup materializes the store snapshot; `adapters/qt/sync_manager_qt.cpp:214`, `core/std/sync_engine_std.cpp:454` | `sync/enabled`, `sync/gid`, `sync/relayUrl`, `sync/hostedUrl`, `sync/transportForm`; `adapters/qt/sync_manager_qt.cpp:34` | State restored in constructor; disabled by default; explicit enable/sync/save; `adapters/qt/sync_manager_qt.cpp:81` |
| Relay protocol service | L7 | D1 (`groups`, `ops`, `snapshots`), C++ line file, or process memory | Client HTTP/WebSocket-free JSON; LAN host reuses `RelayRoutes`; `server/sync_relay/PROTOCOL.md:25`, `server/sync_relay/cpp/lan_host_std.h:37` | Bind host/port, data dir, Worker `DB` binding and limits; `server/sync_relay/cpp/main.cpp:23`, `server/sync_relay/worker/src/worker.js:15` | CLI relay runs until stopped; LAN host is explicit start/stop; Worker isolates per request with lazy schema; `server/sync_relay/worker/src/worker.js:114` |

## Configuration Surfaces

- **Build topology**: `UNIDICT_BUILD_STD_CORE`, Qt core/adapter/apps/tests, CLI, crash, and pronunciation options are declared in `cmake/BuildOptions.cmake:74`; root wiring is `CMakeLists.txt:85`.
- **Runtime paths**: `UNIDICT_DATA_DIR` and `UNIDICT_CACHE_DIR` redirect std path defaults (`core/std/path_utils_std.cpp:24`). `UNIDICT_DICTS` uses platform path separators in both shells (`qmlui/lookup_adapter.cpp:21`, `gui/main.cpp:163`). `UNIDICT_STATE_DIR` relocates Widgets dictionary state (`gui/main.cpp:74`).
- **UI settings**: `SettingsQt` stores INI keys through `QSettings` (`adapters/qt/settings_qt.cpp:1`). QML reads and writes keys such as `ui/theme`, `ui/lang`, `textnorm/profile`, `selection/enabled`, and `quicklookup/enabled` (`qmlui/MainDesktop.qml:41`, `qmlui/MainDesktop.qml:224`, `qmlui/MainDesktop.qml:615`).
- **Protocol schema**: relay request/response schemas and limits are in `server/sync_relay/PROTOCOL.md:25`; D1 table schema is mirrored in `server/sync_relay/worker/schema.sql:1` and Worker code (`server/sync_relay/worker/src/worker.js:32`).

## Calling Channels

1. **Direct in-process C++ calls** connect CLI and shell code to domain services. There is no IPC for lookup, dictionary import, rendering, or scoring.
2. **Qt signals and context properties** connect QML to C++ services and decouple lookup hits from speech (`qmlui/main.cpp:190`, `qmlui/main.cpp:229`).
3. **JNI handles** expose the same std manager/store to Android (`android/app/src/main/kotlin/dev/unidict/mobile/UnidictCore.kt:63`, `adapters/android/jni/dict_jni.cpp:1`).
4. **JSON over HTTP** is reserved for sync relay clients, self-hosted relays, and LAN direct transport (`adapters/qt/sync_http_transport_qt.cpp:58`, `server/sync_relay/cpp/relay_routes.h:1`).
5. **UDP discovery** carries metadata only; data still follows the HTTP relay contract (`server/sync_relay/PROTOCOL.md:147`).
6. **External process** is used only by the optional CLI model fetcher through curl configuration files (`cli-std/main.cpp:63`).

## Why These Layers Exist

1. **L1 is separate because each product has a different lifecycle contract.** CLI parses argv and exits (`cli-std/main.cpp:304`), while QML installs context properties before loading the main window (`qmlui/main.cpp:229`, `qmlui/main.cpp:270`) and Widgets maintains persistent window geometry/settings (`gui/main.cpp:294`).
2. **L2 exists at the Qt boundary, not as redundant wrapping.** `LookupAdapter` owns QML-facing import progress, stamps, and navigation state that core does not know about (`qmlui/lookup_adapter.h:18`, `qmlui/lookup_adapter.h:173`); `DataStoreQt` exposes a Qt-typed singleton over `DataStoreStd` (`adapters/qt/data_store_qt.cpp:13`).
3. **L3 owns cross-book and cross-device policy.** Dictionary enablement, priority, tags, quarantine, and grouped fallback are centralized in `DictionaryManagerStd` (`core/std/dictionary_manager_std.h:19`, `core/std/dictionary_manager_std.h:117`); deterministic replay belongs to `SyncEngineStd` (`core/std/sync_engine_std.h:125`).
4. **L4 is isolated because formats have distinct failure and companion-file rules.** Registry creation is separated from unsupported extension versus parse failure (`core/std/dictionary_std.cpp:19`), while MDX loads companion `.mdd` files without rejecting the dictionary if resources fail (`core/std/dictionary_std.cpp:56`).
5. **L5 is isolated because query cost and cache invalidation differ from domain ownership.** Lexical keys have a normalization version (`core/std/text_norm_std.h:86`), full-text postings have a tokenizer version and signature (`core/std/fulltext_index_std.h:19`), and render sanitization has its own std API (`core/std/html_renderer_std.h:21`).
6. **L6 is optional and platform-bounded.** ONNX is not linked unless `UNIDICT_BUILD_PRON` is on (`cmake/BuildOptions.cmake:68`, `adapters/pron/CMakeLists.txt:68`); clipboard/selection behavior depends on Qt platform support (`qmlui/platform_adapter.h:1`, `qmlui/clipboard_monitor.cpp:14`).
7. **L7 exists only where bytes cross a process or device.** The client wraps transport in encryption before HTTP (`core/std/sync_sealed_transport_std.h:24`, `adapters/qt/sync_http_transport_qt.cpp:214`); relay hosts validate/order ciphertext and never interpret it (`server/sync_relay/PROTOCOL.md:7`, `core/std/sync_relay_state_std.cpp:21`).
8. **L8 is separate because stores have different durability and compatibility contracts.** `unidict.json` is immediate per-mutation JSON (`core/std/data_store_std.cpp:426`), lexical index is a simple line file (`core/std/index_engine_std.cpp:384`), full-text index is versioned binary (`core/std/fulltext_index_std.cpp:332`), sync state is offline-continuation JSON (`core/std/sync_engine_std.cpp:629`), and relay data uses D1 or a line-format file (`server/sync_relay/worker/schema.sql:1`, `core/std/sync_relay_state_std.cpp:45`).

## UI, Copy, and Protocol Locations

- **Runtime copy** is inline bilingual QML text, with English/Chinese choices co-located beside each control (`qmlui/MainDesktop.qml:1387`, `qmlui/MainDesktop.qml:2500`).
- **Reusable visual language** is centralized in theme tokens and the icon path table (`qmlui/theme_tokens.h:41`, `qmlui/components/Icon.qml:27`).
- **Product documentation copy** lives in VitePress source pages, mirrored at `docs/site/*.md` and `docs/site/zh/*.md`; page registration is in `docs/site/.vitepress/config.mts:13`.
- **Protocol copy** has one authoritative file, `server/sync_relay/PROTOCOL.md:1`; the Worker, C++ relay, LAN host, client transport, and tests implement that contract.
