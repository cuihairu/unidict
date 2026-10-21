> Language: **English** | [简体中文](./zh/pro_dictionary_gap.md)

# Key Capabilities Unidict Still Lacks as a "Professional Dictionary"

This document, from the angle of "being able to replace GoldenDict / Eudic / macOS Dictionary in daily use and serve heavy language learners", inventories the capabilities Unidict already has and the ones still clearly missing or incomplete, and gives priority recommendations (P0 highest).

> Scope: gap analysis is done only against the repository's current implementation (core/std + Qt adapters + qmlui/cli); questions like "whether the dictionary content itself contains example sentences/audio" are out of scope for this list, but the app needs to be able to carry and present such content.

> **2026-10-13 refresh (patrol recheck)**: most of the P0/P1 items listed in the first version have been delivered (per-item fates annotated below).
>
> **2026-10-16 refresh (patrol recheck)**: the text-norm UI profile has been delivered (48eff28, three-tier settings page + consumption across core/engine/parsers); the non-X11 word-selection entry was actually a stale item -- F13a/F13b focused-window selection reading has been delivered and is consumed by the lookup window. The current real remaining gaps converge to: Wayland hotkeys (external protocol threshold), real dictionary compatibility matrix (asset/licensing threshold); the other open items (cloud sync/store/plugins/themes/native notifications) are all at the user threshold or already decided as dropped in §A, and are no longer listed as autonomously pushable.

## Already Present (Enough to Count as "Usable")

- Offline dictionary loading: `MDict(.mdx)`, `StarDict(.ifo/.idx/.dict/.dict.dz)`, `DSL(.dsl)`, `JSON/CSV/TSV`
- Multiple lookup modes: `exact/prefix/fuzzy/wildcard/regex`, plus fulltext search (inverted index + TF/IDF + persistence/version negotiation)
- Rich text rendering chain: `html_renderer_std` (HTML/CSS subset + safe sanitization) + resource parsing/caching/URL rewriting (MDict `.mdd` → `mdd_resource_std`; StarDict resources → `stardict_resource_std`) + text-node-level hit highlighting (`highlightHtml`)
- Cross references: `@@@LINK=` jumps and history stack (`cross_reference_std`, maintained on the std side / displayed on the Qt side)
- StarDict `ifo` key fields: `sametypesequence`/`charset`/`description` parsing + `charset_codec_std` codec with missing/mislabeled fallback (`charset_salvaged`)
- Vocabulary and history: local persistence, CSV export, basic review/learning pages (QML)
- Multi-dictionary workflow: priority-based aggregation (`search_grouped` five-layer degradation + in-group headword dedup), enable/disable, priority P↑/P↓, grouping, failure isolation + retry/removal
- Quick lookup: clipboard monitoring, word-selection lookup (X11 select-to-lookup), floating lookup window, global hotkeys on three desktop platforms (Windows RegisterHotKey / macOS Carbon / Linux XGrabKey -- Wayland sessions fail honestly at registration)
- System tray (F11) and launch-on-boot (F9, Windows registry / Linux XDG / macOS launchd)
- Basic sync: `SyncServiceQt` (local JSON file two-way merge + preview/selective import/export) + encrypted backup restore (`sync_backup_std`, PBKDF2 + XChaCha20-Poly1305)
- AI and voice: `AiServiceQt` (external command bridge + mock fallback), Qt `TextToSpeech`; gui-side pronunciation scoring experimental feature (recording/scoring/model bootstrap, not built by default)

## Missing/Incomplete (Key "Professional Dictionary" Gaps, 2026-10-13 Recheck)

### P0: Entry Presentation and Resources (Determines the Experience Ceiling) -- All Three First-Version Items Delivered
- ~~Incomplete rich text/HTML rendering~~ → delivered (`html_renderer_std` + `sanitizeHtml`/`presentEntry` pipeline)
- ~~Missing resource file (image/audio) chain~~ → delivered (`mdd_resource_std` + `stardict_resource_std` + `rewriteResourceUrls`/`loadDictionaryResourceData`)
- ~~Cross references/links not clickable~~ → delivered (`cross_reference_std` + `navigateToWord`/`goBack` history stack)

### P0: Dictionary Compatibility and Encoding (Determines Whether "Your Existing Dictionary Library Can Be Loaded")
- ~~StarDict `ifo` key fields not fully supported~~ → delivered (full parsing of `sametypesequence`/`charset`/`description` + `charset_codec_std` fallback)
- **Real dictionary compatibility matrix** (still missing): the repository stores no dictionary assets (settled position); compatibility is covered with synthetic fixtures; a real dictionary matrix requires CI asset injection and has not been systematized
- ~~Character normalization missing a UI configuration entry~~ → delivered (2026-10-16, 48eff28): `text_norm_std` three-tier profile (lenient/cautious/exact) settings page radio + `textnorm/profile` persisted to disk + profile applied before startup loading; on the core side the engine re-load and the json/stardict/mdict parser fold caches switch tiers in sync, and the query/dedup/suggestion chain consumes the same-tier Options throughout

### P1: Result Organization and Multi-Dictionary Workflow (Determines "Findable and Usable") -- All Delivered
- ~~Aggregated display of multi-dictionary results~~ → delivered (`search_grouped` five-layer degradation + in-group headword dedup, single spec for UI/CLI)
- ~~Insufficient dictionary management~~ → delivered (enable/disable, priority, grouping, failure isolation + retry)
- ~~Search highlighting and snippet preview~~ → delivered (`highlight_term` text-node-level annotation + fulltext/example-sentence snippets)

### P1: Quick Lookup (The "Second Entry" for Power Users) -- Main Chain Delivered, Only the Protocol Threshold Remains
- ~~Clipboard monitoring/global hotkeys/floating window~~ → delivered (clipboard monitoring + floating lookup window + hotkeys on three desktop platforms F10)
- ~~System tray/menu bar~~ → delivered (F11, same shape in qmlui/gui; when the tray is unavailable it degrades to exiting directly)
- ~~Selected-text lookup on non-X11 platforms~~ → delivered (F13a Windows UIA / F13b macOS AX, dispatched via `PlatformAdapter::readFocusedSelection`, consumed by the lookup window; the hotkey-triggered cross-process selection-grab branch has been decided as not doing -- clipboard lookup carries the same intent, see the landing comment in platform_adapter.cpp). 2026-10-16 recheck: this item was previously mislabeled "still missing" due to stale documentation
- **Wayland global hotkeys** (still missing): Wayland sessions have no XGrabKey path and currently fail honestly; waiting on a global hotkey protocol/portal solution (external protocol threshold)

### P1: Text Normalization UI (The Remaining Surface of the First-Version P0 Normalization) -- All Delivered
- ~~text-norm UI profile configuration~~ → delivered (2026-10-16, decided on the user's behalf -- PATROL1016 authorized position: core was ready + the capability was invisible to users under a 100% gate, which counts as "autonomously decidable"; three-tier radio + persistence + query chain consumption, 48eff28)

### P2: Sync, Privacy, and Portability
- ~~Complete backup/restore~~ → delivered (`sync_backup_std`: encrypted backup covers entries/notes/tags/history/installed dictionaries, restore merges by missing items)
- **Cloud sync/multi-device conflicts**: P-10 server surface (red line: only when the user explicitly pushes it; never enters the lookup core path)

### P2: Language Learning Depth (From "Can Look Up" to "Can Learn") -- §A Decided-Dropped/Do-Not-Touch Items
- SRS algorithm and learning progress UI: vocabulary memorization/learning progress is explicitly not done per §A ("simple and predictable" positioning)
- Anki/generic format export: §A decided-dropped (see the strikethrough in the roadmap Import/Export section)
- ~~Pronunciation practice/evaluation~~ → experimental feature delivered (gui-side recording/per-phoneme feedback/model bootstrap, not built by default)

### P3: Plugin Ecosystem and Online Capabilities -- Decided-Dropped or User Threshold
- Third-party plugin dynamic loading: §A decided-dropped (goes through the std parser factory registry)
- Online dictionaries/dictionary store: P-10 user threshold

## Recommended Next Steps (Sorted by Value/Difficulty Ratio, 2026-10-16 Recheck)

1. **P0: real dictionary compatibility matrix**: CI asset injection + per-format regression (unlock condition: dictionary assets and licensing sources, user threshold)
2. **Wayland global hotkeys**: wait for a global hotkey protocol/portal solution (external protocol threshold)
3. The other open items (cloud sync/store/plugins/themes/native notifications) are all at the user threshold or already decided-dropped in §A; not listed for autonomous advancement

> 2026-10-16 delivered by decision: P1 text-norm UI profile (originally ranked #1, 48eff28); the non-X11 word-selection entry was stale documentation (F13a/F13b already delivered), corrected.
