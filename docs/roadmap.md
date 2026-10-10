# Unidict Development Roadmap

This document outlines the detailed development plan for Unidict. It is organized by modules and features.

> Disposition note (2026-10-10, closes TD-143): items that conflict with the product
> positioning are disposed per the ban list in todo.md §A and struck through below
> ("dropped per §A"). Delivered items are checked with their delivery record.

##  MVP (Minimum Viable Product) Priorities
1.  **Core Lookup Functionality** - Support for 2-3 major dictionary formats (e.g., MDict, StarDict).
2.  **Basic UI** - A clean, fast interface for search and display.
3.  **Vocabulary Book** - Basic functions for saving and reviewing words.
4.  **Cross-Platform Support** - Initial support for Windows, macOS, and Linux.
5.  **Offline First** - Core features must work without an internet connection.

---

## Core Architecture
- [x] Cross-platform framework selection and setup (C++/Qt)
- [x] Database schema design (JSON-based DataStore MVP)
- [x] Sync engine design (accounts, incremental sync, E2EE)
      (design doc: docs/design/sync-engine.md — S3-compatible blob backend +
      thin account service, state-based pull-merge-push with HLC, field-level
      merge per collection, monocypher E2EE key hierarchy; implementation
      phased S1–S5, not started)
- [x] Plugin system architecture (extension -> parser factory registry)
- [x] Entry rendering pipeline (HTML/CSS subset, link handling, asset resolving)
      (MDX-only first pass: HtmlRendererStd whitelist sanitize; entry://bword://
      links → #w: in-page anchors; img/audio relative src → res:///<key>?dict=<id>
      served from sibling .mdd via QTextBrowser::loadResource; other formats
      remain plain text)
- [x] Text normalization strategy (Unicode/case/diacritics folding, configurable)
      (strategy doc: docs/design/text-normalization.md — core folding landed
      as text_norm_std v2 (table-driven, no ICU, Options switches, fold-key
      version for cache invalidation); this round: RFC 3629 out-of-range
      decoder reject + astral-plane test matrix; UI-facing profile config
      deferred until demand)

## Dictionary Support & Management
- [x] **Multi-format Dictionary Support**
  - [x] StarDict (.ifo/.idx/.dict/.dict.dz)
  - [x] MDict (.mdx/.mdd) (sibling .mdd auto-detected and attached; resources
        served to the rendering pipeline; real-world compatibility ongoing)
  - [x] DSL
  - [x] EPUB (minimal: zip container + OPF + heading-based entry extraction; real-world layout compatibility ongoing)
        (Qt 面 EpubParser + std 面 EpubParserStd 双实现，DictionaryManager
        两面均接线；真 deflate epub（mimetype stored 首条、子目录 href、
        实体/嵌套标签变体）经 CLI 双面端到端查词验收，std 面
        manager 级回归见 test_dictionary_manager_std)
  - [x] Custom JSON format
  - [x] CSV/TSV/plain-text (simple custom formats)
- [ ] **Dictionary Management**
  - [ ] Online dictionary store browser/downloader (deferred — P-10, user-driven only)
  - [x] Local dictionary import (paths/env var + scan-dir)
  - [x] Local dictionary export/packaging (core/std/dictionary_export_std +
        cli-std `--export-dict <name> <out.json>`: packs a loaded dictionary
        into the project custom JSON format; round-trip verified —
        json_parser_std gained escape-aware string extraction + decode and
        string-aware object scanning to read back entries with quotes,
        backslashes, newlines, tabs and raw braces intact)
  - [x] Dictionary priority settings (UI + persisted ordering)
  - [x] Dictionary grouping/profiles (e.g., EN-EN vs EN-ZH)
  - [x] Corrupted dictionary detection (load-failure diagnostics + quarantine:
        parse failures quarantined until explicit retry, missing files
        self-heal; GUI 词典管理 ⚠ 行 + 重试/移除, CLI --list [FAILED])

## Lookup Features
- [x] **Basic Search**
  - [x] Exact match
  - [x] Fuzzy search
  - [x] Full-text search (inverted index + TF/IDF + persistence)
  - [x] Wildcard search
  - [x] Regex search
- [x] **Quick Access**
  - [x] In-text lookup (mouseselect/hotkey) — carried by the clipboard listener
        and the global hotkey (mouse-hover lookup dropped per §A)
  - [x] Clipboard listener (Qt Widgets GUI toolbar toggle; ClipboardMonitor
        polling + filtering, auto-raises window and fills the query)
  - [x] Global hotkey (Windows: RegisterHotKey + WM_HOTKEY, Ctrl+Alt+U raises
        the window; Linux/macOS remain stubs — revisit on demand)
  - ~~Mouse hover lookup~~ (dropped per §A — Quick Lookup is carried by
        clipboard/hotkey)
- ~~**Visual Lookup**~~ (dropped per §A — non-core, a different product shape)
  - ~~OCR from screenshot/camera~~
  - ~~PDF word extraction~~

## Smart Features
- [ ] **AI Integration**
  - [ ] LLM integration (provider auth, streaming, caching, prompts)
        (deferred — needs external AI service auth/infra; the P-9
        command-bridge provider covers local/CLI use without it)
  - [x] AI-powered translation (via external command bridge)
  - [x] AI-powered grammar check & polish (via external command bridge)
  - [ ] AI-powered contextual sentence generation
- [x] **Voice Features**
  - [x] TTS pronunciation (Qt TextToSpeech; voice selection/presets)
  - ~~Voice search~~ (dropped per §A — TTS output face exists; voice-input
        queries not doing)
  - [x] Pronunciation practice & scoring — delivered M1–M9 (local ONNX scoring,
        offline-first; recording infra, read-after loop, ARPAbet+GOP scoring
        kernel, onnxruntime adapter + CLI --pron-score, variant tolerance,
        confusion localization, position-aware variants, notebook tagging,
        practice history; plan and per-milestone records in
        [docs/pronunciation-plan.md](pronunciation-plan.md))

## Learning Features
- [x] **Vocabulary Management**
  - [x] Vocabulary book (basic CRUD + export CSV)
  - [x] Vocabulary book tagging/grouping (per-word tags persisted in the data
        store; GUI 收藏面板分组过滤下拉 + 右键设置标签)
  - [x] Search history
  - ~~Learning progress tracking (analytics, streaks, goals)~~ (dropped per §A —
        gamification: streaks/goals not doing; review scheduling is covered by
        the forgetting-curve item below)
  - [x] Forgetting curve algorithm (basic scheduled reviews)
- [x] **Memory System**
  - [x] Anki-style flashcard review (basic)
  - ~~Customizable review schedules (user-configurable algorithms)~~ (dropped
        per §A — conflicts with "simple and predictable" positioning)
  - ~~Learning statistics and visualizations~~ (dropped per §A)
  - ~~Achievement/gamification system~~ (dropped per §A)
- [ ] **Note-Taking System**
  - [x] Add notes to dictionary entries (per-word notes persisted in the data
        store, upsert via toolbar 笔记 button; empty text removes the note;
        shown at the end of the entry view)
  - [x] Markdown support (GitHub-dialect via QTextDocument::setMarkdown,
        plain-text escaped fallback)
  - [x] Export notes (PDF/HTML) (std exporter core/std/notes_export_std →
        standalone HTML with escaped word/text, newline→<br>, UTC timestamp;
        Qt PDF via DataStoreQt::exportNotesPdf (QPdfWriter+QTextDocument);
        desktop sidebar 「导出笔记」 button, suffix-routed .pdf/.html;
        cli-std --export-notes)
  - [x] Search within notes (DataStoreStd::search_notes — case-insensitive
        substring over note text, empty query = all; lookup_adapter
        searchNotes → desktop vocab tab note-search field, mobile vocab
        filter also matches note content)

## Translation Features
- ~~**Online Translation Engines**~~ (dropped per §A — translation goes through
  the AI external-command bridge, already delivered)
  - ~~Google Translate~~
  - ~~DeepL~~
  - ~~Custom API integration~~
- ~~**Document Translation**~~ (dropped per §A — PDF/Word extraction dropped)
  - ~~PDF/Word translation (preserving format)~~
  - ~~Bilingual side-by-side view~~

## User Interface
- [ ] **Main Interface**
  - [x] Modern UI/UX design (QML)
  - [x] Light/Dark themes (basic)
  - [ ] Custom theme/color support (deferred — design-system decision:
        accent presets must each pass the WCAG contrast machine
        regression (theme_tokens_contrast_test); palette selection is
        a product/design call, needs user direction)
  - [x] Font and layout customization (definition-area font via QFontDialog,
        persisted in QSettings; lists/toolbar follow the system theme)
- [ ] **Interaction**
  - [x] Fast, responsive search
  - [x] Keyboard shortcut mastery (in-app Shortcut set — Ctrl+K focus
        search, Ctrl+1/2/3 tabs, Enter/Esc, ←/→ entry nav; visible
        cheatsheet in the settings drawer 快捷键 section; global quick-
        lookup hotkey via P-5 with honest per-platform status)
  - ~~Gesture support (mobile)~~ (dropped per §A — mobile gesture lookup
        not doing)

## Platform Specifics
- [ ] **Desktop (Windows, macOS, Linux)**
  - [x] System tray / Menu bar integration (QSystemTrayIcon: close-to-tray
        hide + tray menu 显示主窗/开关/退出, double-click restore, first-hide
        balloon hint; falls back to plain close when no tray is available)
  - [x] Startup on login (Windows: HKCU Run registry key + tray menu toggle;
        Linux/macOS remain stubs — revisit on demand, same as global hotkey)
  - [ ] Native notifications (deferred — no concrete trigger scenario
        exists yet; needs product direction first)
- [ ] **Mobile (Android, iOS)**
  - [ ] Floating lookup widget (deferred — Android overlay work; needs
        a device/emulator verification cycle, adb tooling unavailable
        in the current workspace)
  - [x] Share menu integration (Android shell M7: SEND/PROCESS_TEXT share and
        text-selection menu prefill the lookup page; launcher long-press
        shortcuts jump straight to lookup)
  - [ ] Homescreen widgets (static quick shortcuts delivered in M7; resizable
        widgets not) (deferred — Android shell work; needs a device/emulator
        verification cycle, adb tooling unavailable in the current workspace)

## Advanced Features
- [ ] **Data Sync**
  - [ ] Multi-device sync (cloud) (deferred — P-10, user-driven only)
  - [x] Incremental sync algorithm (op-log relay MVP: B5 C++ relay with op_id
        ordering/idempotency, offset pull, snapshots; cloud S1–S5 design
        stays deferred per §A)
  - [x] Conflict preview & merge (file-based sync MVP)
- [x] **Plugin System**
  - ~~Third-party plugin support (dynamic loading + API/ABI versioning)~~
        (dropped per §A — no third-party runtime; developer dictionaries go
        through the std parser-factory registry)
  - ~~JavaScript/QML plugin engine~~ (dropped per §A)
  - ~~Plugin store/repository~~ (dropped per §A)
  - [x] Developer API (parser factory registration; built-ins)
- [ ] **Import/Export**
  - ~~Anki deck export (apkg/AnkiConnect)~~ (dropped per §A)
  - [x] CSV export
  - [x] Full data backup and restore (B7 self-rescue delivered:
        password-encrypted backup file (PBKDF2 + XChaCha20-Poly1305,
        sync_backup_std) covering words/notes/tags/history/prefs/installed
        dicts; restore merges missing entries via settings drawer
        同步备份 tab, audited by ui_click_audit S18; pron practice records
        stay device-local by design)

## Privacy & Security
- ~~**Data Protection**~~ (dropped per §A — the explicit no-logging stance is
  enough; no fake security promises)
  - ~~Local data encryption~~
  - ~~Privacy mode~~
  - ~~Secure data wipe~~

---
## Phased Rollout Plan
- **v1.1**: Introduce OCR and voice features. (voice/pronunciation done; OCR dropped per §A)
- **v1.2**: Integrate AI translation and writing assistance. (AI bridge MVP done)
- **v1.3**: Refine sync engine and launch plugin system. (sync refinement deferred — P-10; plugin runtime dropped per §A)
- **v2.0**: Introduce community features and advanced learning analytics. (dropped per §A)
