# Source Architecture Glossary

> Language: **English** | [简体中文](/zh/glossary)

This glossary names the responsibilities that exist in source, not the directory names. Line references point to the defining or executing source.

| Term | English | Definition | Source |
|---|---|---|---|
| 应用壳 | Application shell | The process that owns startup, input, product layout, and user-visible lifecycle for one interface (CLI, Widgets, or QML). | `qmlui/main.cpp:75` |
| 上下文绑定 | Context binding | The QML startup step that publishes C++ service objects to declarative views under fixed names such as `lookup` and `vocab`. | `qmlui/main.cpp:229` |
| 查词门面 | Lookup facade | The QML-facing adapter that owns one dictionary manager, turns Qt values into core calls, and stages import work as timer steps. | `qmlui/lookup_adapter.cpp:36` |
| 词典聚合器 | Dictionary aggregator | The domain service that stores loaded dictionaries and failures, applies enablement, priority, tags, and coordinates aggregate query. | `core/std/dictionary_manager_std.h:19` |
| 词典实例 | Dictionary instance | One loaded book: its parser, words, source paths, resource parsers, and per-book enabled/priority/tag state. | `core/std/dictionary_std.h:29` |
| 解析器注册表 | Parser registry | The extension-to-factory map that creates a parser for JSON, StarDict, MDX, DSL, CSV/TSV, text, or EPUB. | `core/std/parser_registry_std.cpp:15` |
| 词形索引 | Lexical index | Trie, exact word map, dictionary reverse map, anagram buckets, and scan words used for prefix, fuzzy, wildcard, regex, anagram, and substring queries. | `core/std/index_engine_std.h:96` |
| 全文倒排索引 | Full-text inverted index | Definition-text postings with TF/IDF and persisted `UDFT1/2/3` formats; its tokenizer version participates in cache signatures. | `core/std/fulltext_index_std.h:15` |
| 查询键规范化 | Query-key normalization | The lossy folding profile that maps user input to comparable dictionary keys while retaining original display text. | `core/std/text_norm_std.h:42` |
| 聚合查询管线 | Aggregated lookup pipeline | The five-level fallback from exact lookup through lemma, prefix, definition match, and fuzzy results, grouped by dictionary. | `core/std/dictionary_manager_std.h:117` |
| 词条渲染器 | Entry renderer | The std-side sanitizer, text extractor, highlighter, and rich-HTML renderer used before dictionary HTML reaches a UI control. | `core/std/html_renderer_std.h:21` |
| 资源解析器 | Resource parser | The MDX companion-resource codec that maps normalized keys to decompressed blocks and materializes selected resources in cache files. | `core/std/mdd_resource_std.cpp:986` |
| 用户状态存储 | User state store | The single `unidict.json` document for search history, vocabulary, notes, colors, skills, and pronunciation records; writes are immediate. | `core/std/data_store_std.h:63` |
| 词典状态 | Dictionary state | The JSON projection of source paths, enablement, priority, tags, and quarantine failures; qmlui is environment-driven while GUI persists it. | `core/std/dictionary_manager_std.cpp:971` |
| 同步物化状态 | Sync materialized state | The deterministic words, notes, tags, history, preferences, and dictionary list reproduced from op-log replay and used as the snapshot payload. | `core/std/sync_engine_std.h:80` |
| 同步指令 | Sync command | A type/value payload with device-scoped `op_id`; local enqueue applies it immediately and enqueues it for idempotent relay. | `core/std/sync_engine_std.cpp:454` |
| 密封传输 | Sealed transport | The client-side encryption wrapper that turns relay-visible ciphertext into a standard transport call. | `core/std/sync_sealed_transport_std.h:24` |
| 中继协议 | Relay protocol | Version-one HTTP contract for groups, ordered ops, cursors, idempotent append, and snapshots. | `server/sync_relay/PROTOCOL.md:1` |
| 中继状态 | Relay state | The host-side group binlog and snapshot, held in D1 tables, a line-format file, or process memory depending on deployment. | `server/sync_relay/worker/schema.sql:1` |
| 局域网宿主 | LAN host | The embedded relay that serves the same HTTP routes and answers UDP discovery without exposing payload content. | `server/sync_relay/cpp/lan_host_std.h:23` |
| 推理适配器 | Inference adapter | The optional onnxruntime boundary that converts PCM and vocabulary into CTC/GOP pronunciation scores while core owns scoring logic. | `adapters/pron/onnx_pron_scorer.h:22` |
| 语音适配器 | Speech adapter | The QML-facing TTS/network/player facade that resolves local or online pronunciation and reports availability and status. | `qmlui/voice_adapter.cpp:166` |
| 平台采集 | Platform capture | Clipboard polling, focused-selection readers, hotkeys, launcher, and tray support that turn desktop events into lookup requests or shell actions. | `qmlui/platform_adapter.h:1` |
| Qt 桥 | Qt bridge | A thin wrapper that converts Qt types and platform primitives to std-core calls without placing Qt in `core/std`. | `adapters/qt/CMakeLists.txt:1` |
| 构建能力矩阵 | Build capability matrix | CMake switches that make std core, CLI, Qt apps, pronunciation inference, crash capture, and tests independently buildable. | `cmake/BuildOptions.cmake:74` |

