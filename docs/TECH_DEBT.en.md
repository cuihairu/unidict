> Language: **English** | [简体中文](./TECH_DEBT.md)

# Unidict Technical Debt Ledger

> Phase 1 · Current-state baseline · Audit date 2026-10-04 · Baseline commit fae5002
> Negative output of the current-state audit. States facts and handling direction suggestions
> only; does not implement.
> Positive asset list at the end (to prevent reporting only problems and causing collateral damage).

## Six-Question Quick Answer

> 2026-10-10 incremental update (the rest keeps audit-date wording; this section only records
> closure deltas): dual CLI retired (0f688c8, cli-std sole); dual desktop shells selected and
> closed (QML sole main exit, gui forwards as pronunciation practice feature shell); vocabulary
> API single-family (LearningManager retired, TD-124 checked); platform debts ③-⑦ all closed
> (TD-132 checked); benchmark CSV cleared (TD-134 checked); Android shell M0-M5+M7 all
> delivered (remaining release signing waits on keys); roadmap drift corrected (TD-143
> checked); charset_codec branch patrol closed (72%→82%, eb18b4e); TD-120 zero dead QML
> checked; TD-121 current-state re-verified (102 Q_INVOKABLE, empty stubs now real, Drawer
> landed in qmlui); TD-101/105 scope narrowed (main chain already on std, legacy manager only
> consumed by gui feature shell); same evening gui also switched to std (9ccf0bf), legacy
> manager production consumption surface zeroed; same-day incremental B chain retired
> (927f376): legacy chain (unidict_core.cpp + four Qt parser sets + plugin_manager +
> bridge layer + core_lookup_tests) all deleted, TD-101/102/105 checked; TD-117 tag renamed
> (`近反义词`→`近义联想`, both shells + live docs fully synced); TD-122 fifteen settings keys
> zero-drift checked; TD-112/113/114 current-state re-verification checked (Main.qml = mobile
> live entry, learning_manager surface already zeroed with P-7 batch four, roadmap review
> entry already reconciled). Blockers re-verification
> maintained: TD-131 residual (visual pixel regression requires manual comparison), TD-133
> (real model 635MB cannot enter the cabinet); record stops: TD-123 (clipboard two files'
> responsibilities still clear; merging is cross-shell coupling churn); user domain untouched:
> TD-142 (README product copy positioning), TD-144 (cloud-level design draft already has
> "not implemented" annotation).
>
> 2026-10-11 incremental update: TD-106 DataStore double-hop facade checked (core/data_store
> two files deleted, consumers directly connect DataStoreQt; incidentally zeroed the only
> remaining Qt header leak at core top level); TD-142 current-state re-verification checked
> (README opens with product positioning; audit-date judgment outdated); TD-144 closed
> (docs/design/ entire directory anti-misreading declaration); TD-154 current-state
> re-verification checked (pcm_util pure logic already sunk into core/std into the 100%
> gate, three-piece platform thin shell exclusion maintained); TD-121 three cuts closed
> (platform/voice/vocab three adapters split out, bridge has 43 Q_INVOKABLE remaining);
> TD-123 record stops (same name different responsibility, merging is cross-shell coupling
> churn).
>
> 2026-10-13 incremental update: TD-151 current-state re-verification checked (F10/F9
> three-desktop-platform hotkeys and boot autostart implemented, residual only Wayland
> session hotkeys transferred to pro_dictionary_gap P1); TD-131 count moved forward
> (audit 57→134, sandbox screenshots 16→32, residual "pixel regression requires manual
> comparison" maintained); gate counts build 147/123 → 160/139 (positive asset section
> synced and refreshed). Six-question ① "relay B1" is audit-date residue - B series B1-B7
> all delivered (todo.md B series section; feature matrix §9 relay row already refreshed
> to "live"), six-question body keeps audit-date wording, not rolled back (this section's
> convention).
>
> 2026-10-14 incremental update: TD-152 current-state re-verification checked - observation
> point "scoring model must not enter default install package" empirically proven on three
> sides: default build OFF (BuildOptions.cmake:68 option OFF, adapters/pron only compiled
> in when option is ON, Android shell FORCE OFF), model not in git (repo only has
> tests/fixtures/fake_ctc.onnx unit test fixture), daily build three-platform stage manifest
> has no model no ORT (workflows/packaging zero PRON references).
> Remaining open: TD-131 residual (pixel regression requires manual comparison, no
> automatic gate is a design trade-off), TD-133 (635MB real model cannot enter the cabinet,
> end-to-end automatic verification structurally blocked).

1. **What is currently implemented** → see CURRENT_FEATURE_MATRIX.md. Capability surface far
   exceeds "dictionary lookup": six-format dual-side parsing, six lookup modes,
   aggregation/dedup/relevance, HTML sanitized rendering, vocabulary book + tags + notes,
   history, TTS + online pronunciation + accent, pronunciation scoring M1-M9 (gated),
   file-level sync MVP, sync relay B1 (dual reference implementation), AI external command
   bridge, Windows desktop integration three-piece set, Android native shell M0-M3-C
   (M4 TTS, M5 packaging not done), pure std mainline CLI, CI three-OS + Android daily build.
2. **Which features are duplicated** → ① parsers 4 pairs of dual implementations
   (2026-10-10 all cleared - legacy four sets retired with incremental B chain retirement,
   std single approach); ② dual desktop shells (gui QWidget and qmlui both active, both have
   commits in recent months); ③ dual CLI (cli legacy 3 subcommands vs cli-std full feature);
   ④ learning data dual stores (DataStore vocabulary book vs learning_stats.json);
   ⑤ aggregation scoring two sets not shared; ⑥ vocabulary book API two families
   (lookup_adapter.vocabulary* vs learningManager getAllStats); ⑦ clipboard domain two files
   (responsibilities still clear).
3. **Which APIs are unstable** → ① legacy/std two managers' public surfaces asymmetric (see
   TD-103) - from 2026-10-10 legacy manager retired, this item dissolves with TD-105;
   ② lookup_adapter god-bridge three-cut teardown closed (TD-121, 2026-10-11: platform/voice/
   vocab three adapters, bridge has 43 Q_INVOKABLE remaining query/render surface);
   ③ relatedLookup key overloading carries two unrelated meanings (TD-117); ④ HtmlRenderOptions
   7/8 fields declared but unread (TD-115); ⑤ *ParserQt bridge retired with legacy chain
   (TD-102 checked); ⑥ settings_qt key names freely invented by QML callers (TD-122).
4. **Where is coupling** → ① UI main chain qmlui→lookup_adapter→DictionaryManagerStd (gui
   also switched to std, legacy manager already deleted, TD-101/105 checked); ② DataStore
   double-hop facade (2026-10-11 with TD-106 checked); ③ gui audio three-piece set not in
   coverage (2026-10-11 with TD-154 checked: pcm_util already sunk into std gate); ④ ONNX
   dependency locked in adapters/pron (Pimpl isolation good, positive).
5. **Where are test gaps** → ① QML automation only has sandbox screenshots + 57 real-click
   audits, pixel regression still manual (TD-131); ② platform debts 5 items unfixed (TD-132);
   ③ onnx_pron_scorer not in std gate, real model cannot enter the cabinet (TD-133);
   ④ benchmark data one-off residue (TD-134). Positive: core/std lines/functions 100% gate,
   theme contrast WCAG AA machine regression, charset_codec/ripemd128 indirectly covered
   without gaps.
6. **What deviates from positioning** → ① learning_manager gamified API (achievements/
   encouragement copy/daily goals) is dead code but exists and has storage (TD-113); ② 635MB
   scoring model vs lightweight positioning - already gated + self-service download,
   structurally controllable (TD-152); ③ cloud-level sync/server design drafts placed
   alongside local-first positioning documentation, easily misread as established direction
   (TD-144); ④ "near/antonym" name does not match reality (association/near-synonym, TD-117).

---

## A. Dual Implementation and Transition Debt (Highest Priority, Main Battlefield of Convergence Period)

- ~~**TD-101 Parsers four pairs of dual implementations, production uses legacy**~~
  (2026-10-10 closed, 927f376): legacy four Qt parser sets (stardict/mdict/json/epub)
  and the unidict_core.cpp factory together with their bridge layer (adapters/qt/*_parser_qt)
  all deleted. core/unidict_core.h only keeps the DictionaryEntry DTO (DataStore facade and
  adapters/qt share shape). Production chain std single approach: DictionaryManagerStd +
  core/std four std parser sets.
- ~~**TD-102 *ParserQt bridge is a test-only surface**~~ (2026-10-10 closed, 927f376):
  bridge layer (json_parser_qt / mdict_parser_qt / stardict_parser_qt /
  plugin_manager_qt) deleted together with TD-101; tests/qt_adapters_test.cpp three bridge
  slots retired (AI service/clipboard/settings entries kept). plugin_manager plugin registry
  production zero consumption surface confirmed - zero call sites across the chain.
- ~~**TD-103 Two managers' APIs asymmetric**~~ (2026-10-10 dissolved with legacy chain
  retirement, 927f376): legacy DictionaryManager entire surface deleted, std
  DictionaryManagerStd single approach; migration-period dual-maintenance tax ended.
- ~~**TD-104 Aggregation scoring two sets not shared**~~: already closed (2026-10-10, reviewed
  with dictionary core quality batch ②) - review found this item's premise has drifted: UI
  main chain aggregateLookup switched to DictionaryManagerStd::search_grouped as early as
  P-6 batch one bridge teardown (five-layer degradation, qmlui/lookup_adapter.cpp:1129
  onward); "legacy searchGrouped wrapper" description outdated; CLI uses manager's lookup
  surfaces and never uses the aggregator. Real residual = std DictionaryAggregator
  (aggregate_lookup_std ~1070 lines) production zero-consumption orphan; dual-set divergence
  risk only holds when someone wires it up again. Handling: per consumer-surface scan
  convention, chain-retire (source + dual test targets + cover test aggregate segments),
  aggregation single approach = manager.search_grouped permanently closed.
- ~~**TD-105 legacy DictionaryManager ~1200 lines singleton**~~ (2026-10-10 closed, 927f376):
  singleton and implementation (core/unidict_core.cpp) retired with legacy chain, gui switched
  to DictionaryManagerStd (9ccf0bf), tests/core_lookup_tests.cpp entire file retired
  (1006 lines Qt Test, seven real regression gaps first ported into std test surface before
  deletion).
- ~~**TD-106 DataStore double-hop facade**~~ (2026-10-11 closed): 48-line facade
  (core/data_store.h/.cpp) deleted, consumers (gui four files + qmlui/lookup_adapter +
  adapters/qt sync two files + three test sets) all directly connect to unidict_data_qt's
  `DataStoreQt` (facade's 26 methods are a superset on it, semantics zero change). Incidentally
  zeroed: the only remaining Qt header leak at core top level (that facade includes
  QString/QList/QVariant) disappeared; core top level now only has the DictionaryEntry DTO
  header + path_utils facade. unidict_core_qt target only has DTO header + path_utils two
  sources, DataStoreQt symbols transitively linked to consumers via static library.
  Six-question quick answer ④② dissolved.

## B. Dead Code and Positioning Drift ("Don't Add Features to Add Features" Cleanup List)

- ~~**TD-111 MainModern entire dead tree**~~: already closed (2026-10-08) - qmlui/MainModern.qml +
  qmlui/modern/ entire tree (19 files ~8.8K lines) deleted; before deletion re-verified
  zero references across the whole repo (qrc/CMake/dev tools/tests all no hits), live surface
  qmlui only has 8 QML files. Exit = UI redesign batch cleanup, no longer keeping dead tree
  candidates.
- ~~**TD-112 Main.qml dead path still compiled into qrc**~~ (2026-10-10 re-verification
  checked): "dead path" judgment does not hold - qmlui/main.cpp:223-228 loads by
  `Q_OS_ANDROID || Q_OS_IOS` branch, Main.qml is the mobile (Android shell M0-M5+M7) live
  entry, must be compiled into qrc. Original record "learningManager sole consumer" is also
  outdated: qmlui entire tree learningManager references already zeroed with P-7 batch four
  (grep zero hits).
- ~~**TD-113 learning_manager gamified API + dual storage**~~ (2026-10-10 re-verification
  checked): LearningManager class retired with P-7 batch four, gamified API and
  learning_stats.json storage disappeared together (grep zero hits across the repo, only
  one comment mention in theme_tokens.h already incidentally changed). Historical residual
  re-characterized: qmlui (std chain→DataStore, data_store_std.cpp:424) and gui
  (legacy manager→dictionary_state.json) **each write their own source, single path without
  dual-write**; cross-shell history having different sources is an inherent surface of
  dual-shell coexistence, std manager history gap already noted
  (dictionary_manager_std.h:70), follows shell convergence decision.
- ~~**TD-114 Review/forgetting curve no live path**~~ (2026-10-10 re-verification checked):
  roadmap review entry already reconciled and cleared with TD-143 (9ef59a2, "[x] does not
  match user reachability" already gone); review session UI no live surface; flashcard
  four-skill marking write entry live (Main.qml cycleSkill, P-7 review surface data
  reserved), data side ready when review surface is built later.
- ~~**TD-115 HtmlRenderOptions 7/8 fields declared but unread**~~: already closed
  (2026-10-04) - seven dead fields removed, only resolve_links kept; content allow surface
  belongs to sanitize whitelist, custom resolution goes through instance-level
  set_link_resolver.
- ~~**TD-116 aggregate examples/pronunciation scoring structural dead branch**~~: already
  closed (2026-10-04) - core side has no structured pronunciation/sentence source (QML card
  phonetic transcription goes through its own extractPhonetics), field + scoring arm +
  GCOVR_EXCL all removed together, header file keeps note explaining; consumer only unit
  tests, already synced and changed.
- ~~**TD-117 "near/antonym" name does not match reality**~~ (2026-10-10 closed): `related`
  key actual meaning = prefix + fuzzy candidate words (near-synonym/association), no antonym
  data source at all - qmlui EntryResultsPane and gui five-view two shells' labels uniformly
  changed to "near-synonym association", comments/audit assertions/README bilingual/
  USER_GUIDE/CURRENT_ARCHITECTURE/FEATURE_MATRIX/product-principles live docs all fully
  synced. Incidentally corrected ARCHITECTURE's original record error: full-text lookup goes
  through independent `fullTextLookup`, `relatedLookup("phrases")` only carries the phrases
  tab - "phrases key same-shape reuse carrying full-text lookup" original judgment does not
  hold, both keys are in the headword-association domain (key meaning comment pinned at
  lookup_adapter.h:169). gui-side market research document (dictionary-market-survey.md
  describing competitors) per archive convention not rolled back.
- ~~**TD-118 Dual CLI feature surfaces unequal**~~ (2026-10-10 closed, 0f688c8): cli(Qt) 3
  subcommands vs cli-std full feature (six formats/six modes/index ops/scoring/download) -
  cli(Qt) even target/test/packaging rows retired, cli-std is the sole CLI.
- ~~**TD-119 Dual desktop shells coexist**~~ (2026-10-10 closed, 419d58b): QML designated as
  the sole desktop main exit (product UI/automation/daily-build main program all in QML);
  gui through consumer-surface scan identified as pronunciation practice dedicated shell
  (recording/shadow reading/scoring/model download only exists here), kept as feature shell
  identity, ci.yml Windows artifact aligned to unidict_qml.
- ~~**TD-120 qmlui ~11K lines of QML with only ~2.1K on user path**~~ (2026-10-10 closed):
  "11K lines" bulk is the MainModern dead tree deleted by TD-111 (19 files ~8.8K lines) -
  current tree 5185 lines / 8 files all compiled into qrc and all have instantiation
  references (Main.qml mobile surface references mobile/common three pieces: MobileFileDialog
  4 places, DefinitionContent 2 places, ResponsiveLayout 1 place; MainDesktop.qml references
  components three pieces totaling 6 places) - **zero dead QML files**, scaffolding residue
  judgment does not hold. Residual scope: reachability within conditional branches
  (mobile-only area/dark theme arm) not line-by-line re-scanned, attribute-level granularity
  done on demand later.

## C. API Stability and Coupling

- ~~**TD-121 lookup_adapter 102 Q_INVOKABLE god-bridge**~~ (2026-10-11 three cuts closed):
  lookup/render/learn/platform all one class; rewriteResourceUrls/setRelevanceMode and other
  byte-level details exposed. QML-Qt responsibility boundary already blurred.
  (2026-10-10 current-state re-verification: original record's three outdated spots already
  corrected per reality - ① empty stubs setDictionaryPriority/setDictionaryEnabled already
  became real with P-7 batch one (start/stop only affects query path, changes emit stamp);
  ② qmlui Drawer "Dictionary" page already landed dictionary status UI with P-6 batch nine
  (original "only in gui dialog" not true); ③ backend already switched to
  DictionaryManagerStd member, history/vocabulary book still via DataStoreQt forwarder.
  Residual debt narrowed to "single class large surface" itself.)
  **Bridge teardown first cut landed (2026-10-11)**: platform collection cluster 26
  Q_INVOKABLE + 4 signals entirely migrated out to qmlui/platform_adapter.{h,cpp}
  (clipboard/hotkey/text-selection three collectors held and wired by it), context property
  name "platform"; MainDesktop/QuickLookupPane call sites and two dev audit fixtures
  synced and rebound, LookupAdapter narrowed to dictionary/pronunciation/learning surface.
  **Bridge teardown second cut landed (2026-10-11)**: voice surface 28 Q_INVOKABLE +
  pronOnlineStatus signal entirely migrated out to qmlui/voice_adapter.{h,cpp} (TTS/online
  pronunciation/presets/autoplay), context property name "voice"; lookup-hit auto-pronunciation
  changed to go through entryLookedUp signal (main.cpp assembles
  VoiceAdapter::speakAfterLookup), lookup and voice sides mutually unaware. Note
  QuickLookupPane's platform/voice instance properties must go through root-scope
  platformService/voiceService forwarding (child components with bare names will
  self-reference-shadow the context - same pitfall as clipService).
  **Bridge teardown third cut landed (2026-10-11)**: vocabulary/history/notes surface 17
  Q_INVOKABLE entirely migrated out to qmlui/vocab_store_adapter.{h,cpp} (DataStoreQt thin
  forwarding + stored rich-text downsampling), context property name "vocab". TD-121 three
  cuts all closed: LookupAdapter only has dictionary/render/import/navigation lookup surface,
  context property name "lookup" semantics restored.
- ~~**TD-122 settings_qt generic domain-less keys**~~ (2026-10-10 closed): entire repo 15
  settings keys (QML side 6 keys 13 call sites + C++ direct-connect 9 keys 10 files) all
  "domain/field" consistent format, zero drift zero spelling fork; wrapper layer is actually
  a 49-line thin surface. Key-catalog refactor is churn without observation demand, record
  stops.
- ~~**TD-123 Clipboard domain two files**~~ (2026-10-11 record stops): clipboard_monitor
  (qmlui, polling word-fetch engine, held by PlatformAdapter) and ClipboardQt (adapters/qt,
  46 lines setText/text read-write facade, context property "clip") - same name different
  responsibility, separately on collection engine and Qt facade sides; merging only stuffs
  monitor lifecycle into read-write facade (or vice versa), after TD-121 three cuts
  PlatformAdapter→ClipboardMonitor ownership already closed, no convergence benefit.
- ~~**TD-124 Vocabulary book API two families**~~ (2026-10-10 closed): after LearningManager
  retired with P-7 batch four, the getAllStats family disappeared (grep zero hits across the
  repo), vocabulary book API only has adapter.vocabulary* single family.

## D. Test Gaps

- **TD-131 QML automation tests thin** (2026-10-05 partially closed; 2026-10-13
  re-verification updated counts): unidict_ui_sandbox (UNIDICT_BUILD_UI_SANDBOX default OFF,
  CI qt job on) offscreen assembles real MainDesktop screenshots 16→32 + pixel assertions;
  unidict_ui_click_audit real-click assertions 57→134 items (press+release send to window,
  popup/page-switch settle, core dual evidence; follow-up batches ③④⑤ add hotkeys/tray/
  full-text rebuild three surfaces), already in ctest (among build 160). Residual: visual
  pixel regression still relies on manual screenshot comparison (BUG-010 dual-side comparison
  not finished).
- ~~**TD-132 CI platform debts ③-⑦ five items unfixed**~~ (2026-10-10 closed): ④⑤⑥⑦
  7afd4c0+3973dfe fixed (CI all-platform green empirically proven); ③ resource local path
  direct-splice file:// links (backslash entering link + drive letter missing leading /)
  8c9d914 launch point unified normalization fix.
- **TD-133 onnx_pron_scorer tests not in std gate**: only Qt build + PRON gate; real model
  635MB cannot enter the cabinet (fake CTC fixture only reaches unit surface), no end-to-end
  automatic verification.
- ~~**TD-134 benchmark CSV is one-off residue**~~ (2026-10-10 cleared, 38a1355): two CSVs
  shipped out + .gitignore (benchmark.sh rewrites each run, run on demand when needed).
- Positive brain line: core/std lines/functions 100% gate, branch 71.1% trend rising,
  theme_tokens_contrast WCAG AA machine regression, charset_codec/ripemd128 indirectly
  covered by adjacent tests without vacuum.

## E. Documentation Debt

- ~~**TD-141 Three analysis documents in root directory suspected outdated**~~ (2026-10-10
  cleared, 38a1355): three shipped out (including 12 retired module references, no inbound
  links); architecture_diagram.txt live document retained.
- ~~**TD-142 README is nightly distribution oriented**~~ (2026-10-11 current-state
  re-verification checked): audit-date judgment already outdated - README opens with product
  positioning (one-sentence positioning + What is this six-capability list +
  product-principles link), nightly/one-click install recedes to mid-section as normal usage
  surface information (install.sh/ps1 has CI syntax check job as backstop). No residual
  action.
- ~~**TD-143 roadmap drifts from current state**~~ (2026-10-10 closed): roadmap.md full-table
  reconciliation - §A forbidden-zone entries (Visual Lookup/voice search/hover word-fetch/
  gesture word-fetch/online translation/plugin runtime/Anki export/review algorithm
  customization/learning stats/gamification/local encryption three-piece set) each struck
  through with dropped per §A; pronunciation practice M1-M9 checked (old "stopped at M3"
  note replaced with concise delivery line); duplicate entries (vocabulary book two rows,
  Markdown two rows) deduplicated; Share menu integration checked (Android M7); In-text
  lookup checked (clipboard+hotkey carries it); incremental sync algorithm checked (B5
  relay); v1.x phased narrative synced.
- ~~**TD-144 Cloud-level design drafts placed alongside documentation surface**~~
  (2026-10-11 closed): risk surface adds directory-level guardrail - docs/design/README.md
  entire directory declares "design drafts ≠ established direction, no implementation,
  starting work requires explicit product decision", server_plan.md and sync-engine.md each
  already have banners (design drafts/implementation untouched before review). Misreading
  entry points (feature matrix "dictionary library service = design draft", roadmap
  "deferred-user-driven only") all carry gating annotations.
- ~~**TD-145 MOBILE_ADAPTATION_REPORT.md naming misleading**~~ (2026-10-10 closed, bd82ca3):
  renamed to docs/qmlui_mobile_adaptation_report.md (git mv, file header 2026-09-29 note
  retained), mobile_plan.md three live references synced; todo.md historical rows per archive
  convention not rolled back.

## F. Platform and Dependencies

- ~~**TD-151 Hotkeys/boot autostart Linux/macOS stub**~~ (2026-10-13 current-state
  re-verification checked, F9/F10 closed): F10 (e6f80ce) global hotkeys three desktop
  platforms implemented - Linux X11 XGrabKey (Wayland session registration honestly fails
  with explicit indication) + macOS Carbon RegisterEventHotKey (qmlui/gui/tests same-approach
  unidict_hotkey_platform helper); F9 (4cf3778) boot autostart Linux XDG autostart +
  macOS launchd. Original "residual stuck on external dependency" now only Wayland hotkeys
  remain (waiting on global shortcut protocol/portal approach, pro_dictionary_gap P1
  remaining gap listed separately).
- ~~**TD-152 635MB scoring model vs lightweight positioning**~~ (2026-10-14 current-state
  re-verification checked): already triple composite soft landing (UNIDICT_BUILD_PRON default
  OFF, model not in repo runtime gives path, M10 self-service download). Observation point
  "must not enter default install package" empirically proven on three sides: default build
  OFF (cmake/BuildOptions.cmake:68 option OFF; adapters/CMakeLists.txt only
  add_subdirectory(pron) when option is ON; adapters/android FORCE OFF); model not in git
  (repo only has tests/fixtures/fake_ctc.onnx unit test fixture, adapters/pron/CMakeLists.txt
  header note "model files do not enter git, runtime gives path"); daily build three-platform
  stage manifest only exe/Qt DLL/dictionary/relay (workflows and packaging/ zero PRON/onnx
  references, PRON off means windeployqt also does not carry ORT). No residual action.
- ~~**TD-153 M10 bootstrap download depends on external curl**~~ (2026-10-10 closed): cli-std
  download entry adds curl availability pre-check (`curl --version` probe, Windows uses
  `>nul`), on absence gives targeted guidance (install curl or manually download assets to
  model directory) and return 5 same-family exit codes - no more making users guess at
  truncated exit codes (cmd 9009&0xff colliding with real error codes). Resume/retry logic
  unchanged.
- ~~**TD-154 gui audio three-piece set not in coverage**~~ (2026-10-11 current-state
  re-verification checked): pcm_util pure logic already sunk into core/std/pcm_util_std.h on
  2026-09-25 (gui/pcm_util.h only a forwarding header), entered 100% std coverage gate
  (pcm_util_std_test + core_std_cover_test + pron_wave_std_test three-surface coverage) -
  "pure logic should continue to be maintained" already satisfied. recorder/playback/waveform
  platform thin shells keep QT_EXCLUDES exclusion (per-item comments give reasons), no
  residual action.

---

## Positive Assets (To Prevent Reporting Only Problems)

- **Gate discipline**: coverage.sh exit code is the only trustworthy source + build-std
  139/139 + build(Qt) 160/160 (2026-10-13 re-verification time point) + branch trend
  rising; "don't force numbers to hit targets" established stance (GCOVR_EXCL all have
  comment reasons).
- **core zero Qt holds**: core/std 68 files include scan has no Qt hits; cli-std/jni
  dual no-Qt production chains verified.
- **local-first baseline**: dictionary lookup core path has no network dependency; online
  pronunciation only sends the lookup word externally and the UI explicitly states it.
- **CI surface**: three OS × (std+Qt) + Android daily build + Windows installer silent
  verification + single-exit documentation deployment.
- **Privacy/security surface**: URL path escaping prevents path traversal, HTML whitelist
  sanitization, encrypted dictionary support, SHA-256 verification chain.

Handling direction overall note: B category follows "first decide data model/API survival,
then delete dead tree", non-parallel cleanup; A/C categories converge with Phase 3 Core
stability; D category platform debts handled in dedicated batches; E/F categories converge
with Phase 2 documentation.
