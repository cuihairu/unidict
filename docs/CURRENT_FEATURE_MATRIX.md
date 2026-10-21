> Language: **English** | [简体中文](./zh/CURRENT_FEATURE_MATRIX.md)

# Unidict Current Feature Matrix

> Phase 1 · Current-state baseline · Audit date 2026-10-04 · Baseline commit fae5002
> Partial revision 2026-10-12: theme (partial→live, three-state manual toggle), search history (+per-item delete), dictionary priority (+qmlui inline reorder), vocabulary notebook (+CLI CSV in/out), real-click audit count 57→121; patrol batch ③: definition font size (qmlui appearance tab four levels, ui/def_font_size), dictionary removal (qmlui dictionary tab ✕ session surface), audit count 121→124; batch ④: autostart Linux/macOS implemented (XDG/LaunchAgent + qmlui lookup tab switch), audit count 124→126; batch ⑤: global hotkeys implemented on three platforms F10 (Windows/macOS/Linux-X11, honest failure on Wayland), qmlui tray parity F11, full-text index rebuild button F12, audit count 126→134
> Partial revision 2026-10-13: §6 desktop integration three rows refreshed (tray row adds qmlui TrayController surface, autostart partial→live, hotkey partial→live on three platforms); §1 adds full-text index rebuild button row (F12); §6 P-5 row adds Windows quick_lookup hotkey focus-window selection reading (F13a, UIA-triggered, empty falls back to clipboard); §7 clipboard lookup (Android shell) "not built→not doing" (mobile_plan §2 explicitly excludes: desktop semantics, no mobile equivalent); §1 DSL partial→live (parser_registry_std registered); §9 relay partial→live (B1-B7 all delivered), AI row Live UI surface corrected (desktop qmlui, not mobile), plugins partial→live (built-in registry is the final form); §10 benchmarks partial→live (TD-134 closed), local encryption row roadmap note aligned with the struck-through abolition criteria; §11 pronunciation/review two notes aligned with TD-143 reconciliation then roadmap current state
> Partial revision 2026-10-16: §2 text folding row adds three-level profile and qmlui settings surface (48eff28)
> Status legend: **live** = reachable on the user path; **partial** = usable but with gaps / only on some surface; **dead** = no references or unreachable;
> **gated** = compile switch default OFF; **design draft** = documentation-level only; **not built**.

## 1. Dictionary Formats and Dictionary Management

| Feature | Status | Location | Notes |
|---|---|---|---|
| StarDict (.ifo/.idx/.dict/.dict.dz) | live | core/std/stardict_parser_std (single implementation; legacy retired) | UI main chain goes through the std surface (qmlui/gui both DictionaryManagerStd) |
| MDict .mdx / .mdd | live | core/std/mdict_parser_std + std/mdd_resource | sibling .mdd auto-attached, resources enter the rendering pipeline; v2 faithful reading (Encoding code table/encrypted key block) + UTF-16 header full Chinese titles (std surface) |
| StarDict resources (images/audio) | live | std/stardict_resource_std + manager convenience methods | .ifo res key (catalog/manifest) + loose media fallback; qmlui rendering fallback chain (mdd first) |
| MDict encrypted dictionaries | live | core/std/mdict_crypto + decryptor | GUI password setting ([encrypted] detection) |
| DSL | live | core/std/dsl_parser_std | parser_registry_std registers .dsl; qmlui/cli-std available on the same chain via DictionaryManagerStd |
| EPUB | live | single std surface (legacy surface retired 927f376) | true deflate end-to-end + EPUB2 text/html/dt-dd layout/href encoding normalization (std surface) |
| JSON custom | live | single std surface (legacy surface retired 927f376) | environment variable/scan directory import |
| CSV/TSV/plain | live | core/std (custom format family) | cli-std surface |
| Dictionary priority/grouping/enable/remove | live | Drawer "Dictionary" tab + gui toolbar grouping dropdown + qmlui inline P↑/P↓ (normalize-on-move) + ✕ remove (qmlui session surface: env-driven stateless file, reverts with UNIDICT_DICTS on restart) | Settings persistable (except qmlui removal) |
| Local dictionary export/packaging (JSON) | live | core/std/dictionary_export_std + cli-std --export-dict | Project-custom JSON format; round-trip fully faithful (json_parser escape decoding + string-aware scanning as companions) |
| Corrupted dictionary isolation/retry | live | GUI ⚠ row + CLI --list [FAILED] | Missing files self-heal |
| Full-text index status row + rebuild button | live | qmlui dictionary tab status row (build on demand / built N entry documents) + "Rebuild" button (dictIndexRebuildButton, available only when loadedDictionaries>0; F12) | reset drops the in-memory index, lazily rebuilt on the next full-text search; audit S35 three checks (lazy-build trigger/real clicks docs 13→0/status row returns to lazy-build state) |
| Online dictionary library browse/download | design draft | docs/server_plan.md | Metadata catalog + anonymous distribution |

## 2. Search

| Feature | Status | Location | Notes |
|---|---|---|---|
| exact / prefix / fuzzy / wildcard / regex / fulltext | live (fully implemented at core layer) | DictionaryManagerStd 6 modes; IndexEngineStd 4 modes | exact goes through the word list |
| UI exposure: mode dropdown | partial | SidebarPanel.qml:55-61, 5 states [auto/prefix/fuzzy/wildcard/regex] | Default "auto" = prefix first, fuzzy fallback when empty; **fulltext is not in the dropdown** (it is a content tab) |
| Full-text search UI | live | EntryResultsPane "Full-text search" tab | relatedLookup same-shape key reuse |
| Input suggestions/completion | live | gui QCompleter; qmlui search box suggestions | qmlui suggestions in three grouped sections (exact/prefix/entry, MainDesktop suggestSection computes section keys + ListView section headers; UI batch 1 2026-10-11) |
| Aggregation grouping/dedup/relevance | live | aggregateLookup = manager.search_grouped wrapper (five-level fallback + within-group dedup; TD-104 dual sets retired and eliminated by consumer-surface scan on 2026-10-10) | UI/CLI single surface (DictionaryManagerStd); qmlui dictionary tab multi-dictionary hits show chip overview (click to solo-focus a single dictionary, click again to cancel), grouping via GroupCard collapsible cards (UI batch 1 2026-10-11) |
| Text folding | live | core/std/text_norm_std v2 (table-driven, no ICU) | case/diacritics/punctuation/ligatures independent switches, fold-key versioning; three-level profile (lenient=all folded/cautious=keep accents/exact=raw bytes) qmlui appearance tab three radios (textnorm/profile, persisted before startup load, immediate re-injection on profile change), engine and json/stardict/mdict parser folding caches on the same surface (48eff28) |

## 3. Rendering and Content

| Feature | Status | Location | Notes |
|---|---|---|---|
| HTML sanitized rendering + resource resolution | live | html_renderer_std + QTextBrowser::loadResource | whitelist sanitize; res:// fetches from sibling .mdd |
| Cross references | live | cross_reference_std | synonym associations/phrases/related |
| Entry notes (Markdown) | live | gui toolbar notes button; qmlui vocabulary notebook notes | QTextDocument::setMarkdown |

## 4. Pronunciation

| Feature | Status | Location | Notes |
|---|---|---|---|
| TTS local pronunciation | live | qmlui Drawer "Voice" tab | voice/preset/rate/pitch/volume + autoplay delay |
| Online pronunciation source (three states + accent) | live | PronunciationSourceStd → FreeDictionarySource | dictionaryapi.dev; sends only the query word (explicit in UI) |
| Recording/waveform/playback | live | gui: audio_recorder / waveform_widget / pcm_playback | 16k/mono/16bit thin shell |
| Pronunciation scoring M1–M9 | gated | adapters/pron + core/std pronunciation family | UNIDICT_BUILD_PRON (default OFF); 635MB fp16 model not committed; M10 bootstrap download (cli-std + curl) |
| Accent selection (scoring reference field switch) | gated | gui accent selector + QSettings | M5 |

## 5. Learning and Personal Data

| Feature | Status | Location | Notes |
|---|---|---|---|
| Vocabulary notebook CRUD+tags+notes+CSV export | live | MainDesktop vocabulary notebook tab (lookup_adapter 22 Q_INVOKABLE → DataStore) | gui right-click menu; qmlui card "+Tag"/"Notes" dialog + tag filter chips; real-click audit 124 items (qmlui/dev/ui_click_audit.cpp); cli-std --import-vocab/--export-vocab same CSV surface (import/export batch 2026-10-12) |
| Search within notes | live | data_store_std search_notes → data_store_qt getNotesByText → lookup_adapter searchNotes | case-insensitive substring (non-ASCII participates as-is), empty query = all; desktop vocabulary notebook independent search box, mobile merged into vocabFilter |
| Notes export (Export notes PDF/HTML) | live | core/std/notes_export_std (HTML) + data_store_qt exportNotesPdf (QPdfWriter) → lookup_adapter → desktop "Export notes" button dispatches by suffix | HTML: escaping (&<>"') + newlines to <br> + UTC timestamp (0 left empty); PDF: Qt layout; cli-std --export-notes same surface |
| Search history + pinning + delete + clear | live | MainDesktop history tab | limit 100; same-word re-lookup hits increment hit_count (case-insensitive dedup accumulation, ×N capsule at line end, shown only when count>1; store JSON compact write omits ==1, read side falls back to 1, UI batch 1 2026-10-11); 📌 pinning block semantics (pinned block floats to top, UI batch 2026-10-12); ✕ per-item delete (core case-insensitive delete, 2026-10-12); whole-page clear |
| Learning statistics/progress/achievements/motivational quotes/daily goals | dead | learning_manager (consumed only by dead Main.qml) | learning_stats.json independent storage (TD-113) |
| Review (flashcard) | dead | only dead Main.qml (modern dead tree deleted, TD-111) | roadmap checkbox does not match reality |
| Forgetting-curve scheduling | dead | same as above | no review entry on the live path |
| Unstable-pronunciation tag integration (word score <0.6) | gated | gui favorites panel (M8) | only affects already-favorited words |

## 6. Desktop Integration

| Feature | Status | Location | Notes |
|---|---|---|---|
| System tray (close-to-tray + menu + restore) | live | gui main.cpp (QSystemTrayIcon) + qmlui TrayController (F11 parity: show main window/hide to tray on close/autostart/quit, double-click restore, first-hide bubble) | degrades to normal close when no tray (qmlui close-window-exits) |
| Autostart | live | gui tray menu + qmlui lookup tab switch (F9, shared StartupLauncher: Windows HKCU Run / Linux XDG autostart / macOS launchd, UNIDICT_AUTOSTART_DIR test seam) | |
| Global hotkey Ctrl+Alt+U | live | qmlui global_hotkeys three desktop platforms (F10: Windows RegisterHotKey / Linux X11 XGrabKey — honest failure on Wayland sessions / macOS Carbon) + Drawer "Shortcuts" tab | Linux needs X11 headers (UNIDICT_HAS_X11), Wayland registration fails explicitly |
| Clipboard lookup | live | qmlui clipboard_monitor (polling + filtering + auto word placement) | switch/interval/word length/exclusion patterns configurable |
| Selection lookup (X11 primary selection) | partial | qmlui selection_monitor (xcb pinned: switch hidden on non-xcb platforms, start no-op) | lookup on selection, hit dispatch same as clipboard (lookup window/main window); Win/mac Selection falls back to clipboard so not doing (would double-trigger), pro_dictionary_gap P1 wrap-up |
| P-5 floating lookup window | live | qmlui QuickLookupPane (clipboard lookup/quick_lookup hotkey → floating window pasted at cursor) | lookup/pronunciation/vocabulary notebook/open main window; collapses on focus loss; switch persisted (quicklookup/enabled); Windows hotkey reads focus-window selection first (F13a win_selection_reader, UIA-triggered, empty falls back to clipboard) |
| Theme | live | qmlui Theme three states (0 follow system/1 light/2 dark; gui same semantics same key ui/theme; settings "Appearance" tab themeModeRadio0/1/2 immediate flip + persisted) | qmlui follow system + manual override; gui QPalette dark |
| Definition font settings | live | gui QFontDialog (QSettings persisted); qmlui appearance tab four-level radio 12/14/16/18 (ui/def_font_size persisted, level change immediately rebuilds the current query surface) | |
| Shortcut Ctrl+1/2/3 switch tabs | live | MainDesktop.qml:1221 | |

## 7. Mobile (Android)

| Feature | Status | Location | Notes |
|---|---|---|---|
| Lookup (exact/prefix/fuzzy/searchAll) | live | JNI lookup_jni + Compose shell (M0–M3-C) | |
| Vocabulary notebook/history | live | JNI store_jni | |
| TTS pronunciation (M4) | live | UnidictTts.kt (system TextToSpeech) | 7761c04 on device, M4-TTS-OK token acceptance; entry page/vocabulary notebook card pronunciation |
| Polish and packaging (M5: dark/error states/first-launch guide/R8) | live | MainActivity.kt M5 first-launch guide + build.gradle.kts R8 | |
| Share word intake/selection menu/shortcut (M7) | live | MainActivity singleTask + SEND/PROCESS_TEXT | three entry points logcat token acceptance |
| Clipboard lookup (Android shell) | not doing | — | mobile_plan §2 explicitly excludes: clipboard monitoring = desktop semantics, no mobile equivalent; desktop clipboard lookup is live (qmlui) |
| Floating lookup window (Android M8-A) | live | FloatingLookupService (overlay ball + mini lookup panel) | M8-FLOAT-OK full-chain token acceptance; drag past screen edge clamps back |
| Desktop-resizable widget (Android M8-B) | live | QuickLookupWidgetProvider (last word + tap to jump to lookup, H\|V resizable) | emulator tested: placement/refresh token M8-WIDGET-OK, desktop updates live after lookup, resize handle drag settles |
| iOS / HarmonyOS | not built | | |
| QML mobile shell | dead | Main.qml + qmlui/mobile/ | superseded by native shell, still compiled into qrc |

## 8. CLI

| Feature | Status | Location | Notes |
|---|---|---|---|
| cli-std full functionality (six-format loading, six-mode lookup, --mdict-password, --list-dicts, --drop-dict, --scan-dir, --where/--all, index ops, full-text index ops, cache five-piece set, --list-plugins, --pron-* family, model bootstrap download) | live | cli-std/ (zero Qt, deb/rpm) | primary CLI |
| cli (Qt) diagnostic surface (--dict/--dict-dir/--list) | live (legacy) | cli/ | functionality severely unequal (TD-118) |

## 9. Sync / AI / Server / Plugins

| Feature | Status | Location | Notes |
|---|---|---|---|
| Local file-level sync (merge + preview + selective apply) | live | adapters/qt/sync_service_qt | no account, no cloud |
| Sync relay (protocol v1 + dev/Worker dual reference implementations + contract tests) | live | server/sync_relay/ + adapters/qt/sync_manager_qt | B1-B7 all delivered (todo.md B series): client SyncEngineStd connected (resumable + device surface), SPAKE2+/encrypted backup complete; sync off by default, relay sees ciphertext only |
| AI translation / grammar check | live | ai_service_qt (external command bridge, UNIDICT_AI_CMD) | desktop qmlui surface; no streaming/provider; mobile_plan §2 explicitly excludes AI on mobile |
| AI contextual sentence generation | live | ai_service_qt sentences subcommand + heuristic mock self-tagged [Mock sentences] | desktop entry card "✨ Sentence generation" light entry + read-only popup, ui_click_audit S20 |
| Dictionary library service (account/catalog/distribution/sync) | design draft | docs/server_plan.md + design/sync-engine.md | cloud-scale S1–S5 unimplemented |
| Plugins (parser factory registry) | live | cli-std --list-plugins | built-in registry is the final form; dynamic loading abolished per §A (legacy plugin_manager retired) |

## 10. Export / Security / Other

| Feature | Status | Location | Notes |
|---|---|---|---|
| Vocabulary notebook CSV export | live | MainDesktop + cli-std | |
| Notes export HTML / PDF | live | notes_export_std + DataStoreQt::exportNotesPdf → desktop "Export notes" button dispatches by suffix; cli-std --export-notes | roadmap Note-Taking delivered (c0408fa) |
| Full backup/restore (passphrase encrypted) | live | sync_backup_std → settings drawer "Sync backup" tab | B7 delivered; pron practice records stay local by design |
| Anki export (apkg/AnkiConnect) | not built | roadmap [ ] abolished (§A) | |
| Local encryption / privacy mode / secure erase | not built | roadmap struck through and abolished (§A) | the explicit no-logging criteria suffices; no fake security promises |
| Privacy criteria (online pronunciation sends only the query word) | live | online_pron_std header comment + explicit UI copy | |
| Performance benchmarks | live | benchmark.sh (repo root; 12 classes of CLI operations, 3–20 runs/class) | TD-134 closed (38a1355): CSV output + .gitignore, run on demand |
| Theme contrast machine regression | live | tests/theme_tokens_contrast_test | WCAG AA values locked |

## 11. Differences from roadmap.md (outside the selection)

- roadmap "Basic Search" all [x] matches core-layer facts; "full-text search" is a content tab in the UI rather than a mode, a difference the roadmap does not express.
- roadmap "review/forgetting curve [x]" is a dead path (see Section 5); after TD-143 (9ef59a2) reconciliation the roadmap added the explicit note "review UI stays reserved per TD-114 … not wired to any live screen", so the checkbox and the implementation surface are consistent; the remainder is only the product-level wording of "[x] but no live entry".
- roadmap "pronunciation" entry TD-143 (9ef59a2) has been rewritten as an M1–M9 delivered row; the old note "stopped at M3" is gone.
- roadmap "AI translation/grammar [x]" is true (external command bridge).
