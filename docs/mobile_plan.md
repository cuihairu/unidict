> Language: **English** | [简体中文](./zh/mobile_plan.md)

# Unidict Mobile Version Selection and Batch Planning

> 2026-09-29 first batch (selection design + feasibility verification). Local feasibility
> testing was completed by this batch; test environment: Linux x86_64 host, NDK
> r27.0.12077973, SDK build-tools 36 / platform android-36, emulator AVD test30
> (android-30 google_apis x86_64), Qt 6.10.3 (gcc_64 desktop kit only), aqt 3.3.0,
> OpenJDK 21.
> Prerequisite report: docs/qmlui_mobile_adaptation_report.md (responsive refactoring
> inside qmlui, **not** a phone app).

## 0. Background and Problem

The user pointed out "a phone version is also needed", but todo.md has no mobile entries. Current state inventory:

- `docs/qmlui_mobile_adaptation_report.md` records **mobile-friendly refactoring inside qmlui** -- responsive layout (`ResponsiveLayout.qml`), touch targets, `MobileUtils` platform detection and the document-picker intent bridge, AndroidManifest/Info.plist templates. It makes qmlui look good on small screens, but has never produced an installable phone app, and offers no selection conclusion.
- Since 2026-09, `core/` has maintained a **std-only, Qt-free** state (AGENTS.md architecture constraint); the two static libraries `unidict_std_core` + `unidict_index_std` cover the full lookup chain (dictionary loading/parsing of mdx·mdd·stardict·dsl·csv·json·epub, index, fulltext, aggregation, vocabulary DataStore, text normalization, pronunciation pure-logic layer) -- exactly the asset that can go on a phone.
- The platform direction already has a settled position (2026-09-28): Android / iOS / HarmonyOS three-platform **native shells** + `core/` shared library; desktop Qt (adapters/qt, gui, qmlui) is only one of the adaptation layers.

Precedent projects' tech-stack decisions on this machine (reference for selection):

| Project | Mobile decision | Key record |
|---------|-----------------|------------|
| **cockpit** | **Flutter** (2026-09-22, user-specified) | Pure REST client with no shared C++ assets, connects directly to the existing `/api/*`; M1-M6 delivered in full (112 tests, CI produces APK, R8/signing closed out). **Applicable premise: the shell is everything; there is no native core to reuse.** |
| **chirp** | Flutter → **Android/iOS dual-native migration** (started 2026-09-28, Flutter app and CI leg removed 2026-09-29) | Original decision text: "Android/iOS/HarmonyOS switch from the Flutter unified stack to per-platform native implementations. Path: first nativize the pure protocol core unrelated to transport, platform by platform (independently testable, byte-level comparable), then fill in each platform's shell layer"; gates: Android `make test` 84/84, iOS `swift test` 81/81. **Applicable premise: there is a shared core to carry, and the cross-platform UI stack has proven to be a burden.** |

unidict is isomorphic to chirp (heavy shared core, three-platform target) and heterogeneous to cockpit (core is the product itself; it cannot be rewritten inside a shell). The precedents point clearly.

## 1. Selection Comparison

### Option A: Qt for Android reusing qmlui

Package the existing QML UI onto Android (qmlui already has an Android branch: `find_package(Qt6 ... AndroidExtras)`, `mobile/android/AndroidManifest.xml`).

| Dimension | Measured/facts |
|-----------|----------------|
| Build chain | **Not feasible on this machine**: Qt 6.10.3 has only `gcc_64` installed; the `aqt list-qt linux android` index tops out at **6.7.3** (desktop is already at 6.10.3). Going with A means either downgrading the entire Qt to ≤6.7 (qmlui declares dependencies on TextToSpeech / QuickDialogs2 / QuickControls2, so cross-3-major-version API compatibility risk lands entirely on UI regression), or relying on a commercially licensed channel for a newer Android kit. **Note**: the aqt index only reflects public download channels; if A is taken seriously, first re-check whether the official download.qt.io installer has 6.10 Android open-source packages. |
| UI reuse | Nominally reuses qmlui, but mobile UX (bottom navigation, SAF import flow, touch query page) still has to be redone; the complexity of the two desktop main UIs (Main/MainDesktop; the MainModern dead tree was already deleted) gets carried along. |
| Platform integration | SAF/TTS/share reception/desktop widgets all need Java/JNI-side code written through Qt's platform bridge -- Qt does not save platform work, it only adds a layer of translation. |
| Size | Full Qt Quick + QuickControls2 + TextToSpeech + QuickDialogs2 runtime; APK starts at tens of MB. |
| Three-platform route | **Solves only the Android end**. iOS needs a separate Qt iOS kit + macOS build machine; HarmonyOS has no official open-source Qt backend (the claim in docs/qmlui_mobile_adaptation_report.md that "Qt for HarmonyOS: officially supported by Huawei" does not match public facts). Directly conflicts with the settled three-platform native-shell direction. |

### Option B: Kotlin native shell + NDK-compiled std-only core + thin JNI bindings (recommended)

New Android project (Kotlin + Jetpack Compose), `unidict_std_core` ships as a static library, `adapters/android/` holds the C++ JNI glue (core stays free of platform headers).

| Dimension | Measured/facts (verified in this batch) |
|-----------|------------------------------------------|
| Build chain | **Already working on this machine**: NDK r27 + CMake toolchain file, full compilation of both arm64-v8a and x86_64 ABIs, producing `libunidict_std_core.a` / `libunidict_index_std.a`; the std test suite executables also all link successfully under the Android target (zlib uses the system `-lz`, `USE_ZLIB` semantics unchanged). |
| Runtime | Emulator-verified: on an android-30 x86_64 image, a smoke program drives the full lookup chain (JSON dictionary loading → exact/prefix/fulltext search) + vocabulary (word lists/tags/notes/persistence round-trip/CSV export) + path environment fallback, printing `ANDROID-SMOKE-OK`. std::filesystem / exceptions / STL work correctly under bionic (minSdk 24). |
| Platform integration | The right path: SAF = `ActivityResultContracts.OpenDocument`, TTS = `android.speech.tts.TextToSpeech`, clipboard/share/widget via native APIs directly, no translation layer needed. |
| Size | core static library is MB-scale (no UI runtime), small net APK increment. |
| Three-platform route | One approach reused three times: Android = Kotlin+JNI, iOS = Swift (core static library + C interface), HarmonyOS = ArkTS+NAPI. chirp's comparison methodology (name-by-name protocol vector comparison across two native packages) is copied directly. |
| Cost | UI written from scratch (it had to be written anyway -- mobile lookup UX has no reuse value with the three desktop main UIs); the JNI binding layer must be maintained (thin glue, pure data in/out). |

### Option C: Both options in parallel

No. Double build/test/maintenance cost, no comparison benefit; both precedent projects converged on a single stack.

### Conclusion

**Choose Option B**. Consistent with three things: the settled platform direction of 2026-09-28 (three-platform native shells + core shared library), the chirp precedent (a cross-platform UI stack has proven to be a burden in scenarios carrying a shared core), and local measurement (the NDK chain passed the same day; the Qt Android chain broke on the spot).

## 2. Mobile Feature Surface (Explicit Boundaries)

**Going on the phone (by batch)**:

1. **Lookup** -- dictionary loading/start-stop/deletion, exact/prefix/fuzzy/wildcard search, multi-dictionary aggregated display, fulltext search (reusing `fulltext_index_std` / search_grouped).
2. **Dictionary file management (SAF)** -- `OpenDocument` import of .mdx/.mdd/.stardict/.dsl/.csv/.json/.epub → copy into the app's private directory; dictionary list (word count/enabled state); deletion. The mobile equivalent of the desktop UNIDICT_DICTS environment variable = persisted import list.
3. **Vocabulary** -- word lists/group tags/notes/search history/CSV export (`DataStoreStd` binding surface, files live with the app data directory).
4. **Pronunciation TTS** -- system `TextToSpeech` reads entries aloud (engine selection/speech rate), pronunciation button on the entry page.

**Not going on the phone (desktop-only, explicitly excluded)**:

- Plugin development and plugin management (`plugin_manager`)
- Batch conversion tools
- Screen word capture / global hotkeys / clipboard monitoring (desktop semantics, no mobile equivalent)
- AI services and translation engines (`ai_service`)
- Sync service (re-evaluated in a later batch; boundary left open for now)
- Pronunciation practice / GOP scoring (depends on onnxruntime + model asset download chain, large; not in the first phase; TTS readout already covers the "hear the pronunciation" need)

## 3. Batch Breakdown

Each batch has an independent gate, independent commit, independent acceptance (aligned with chirp's per-platform cadence):

| Batch | Content | Acceptance gate |
|-------|---------|-----------------|
| **M0 (this batch)** | Selection document + feasibility verification (NDK dual-ABI compilation passes, emulator runtime smoke passes, Qt Android chain breakage recorded) | Document enters docs/ + todo.md section |
| **M1** | Android project skeleton: Gradle + Compose empty shell, `adapters/android/` JNI binding layer (dict/lookup/store three domains), core .so packaging, smoke page (load a sample dictionary and look up a word) | `gradle assembleDebug` produces a package; install on the emulator and the smoke page can look up words |
| **M2** | Lookup chain: SAF import/dictionary management page + query page (exact/prefix/fuzzy/fulltext) + entry rendering (plain text in the first phase; `html_renderer_std` output sanitized then handed to WebView is deferred for later evaluation) | Import a real .mdx/.json dictionary and query it; dictionary start/stop takes effect |
| **M3** | Vocabulary + history + notes: full DataStoreStd binding surface on the shell, word list page/tags/CSV export (SAF CreateDocument) | Vocabulary CRUD + persistence round-trip |
| **M4** | TTS pronunciation: entry page pronunciation button, engine/speech-rate settings | Verify readout once on the emulator and once on a real device |
| **M5** | Polish and packaging: dark theme, error states, first-launch onboarding (SAF permission semantics), release signing/R8/size (pit list refers to cockpit M3 record) | Release packaging machine runs |
| **M7 (delivered 2026-10-07)** | Quick lookup/share surface: Share word reception + word-selection menu (SEND/PROCESS_TEXT → query page prefilled with auto-aggregated lookup) + desktop static shortcut (long-press icon goes straight to the query page with cursor in place); singleTask + onNewIntent receives words at runtime without stacking instances | Three-entry logcat token `M7-INTENT-OK` + dump verifies prefill/focus/keyboard + normal startup regression with zero tokens |
| **M8 (delivered 2026-10-11)** | Floating lookup window: FloatingLookupService foreground service (specialUse type + persistent notification), draggable floating ball that expands a mini lookup panel on tap (aggregated best-group first definition + readout/vocab/open main window/collapse); SYSTEM_ALERT_WINDOW settings page guides authorization; shares the process-level singleton DictRepository (UnidictApp) with the main UI | logcat token `M8-FLOAT-OK` (ball/panel/lookup/speak/vocab/open_main/collapse/toggle full chain) + dumpsys mFrame actual tap + drag off-screen clamp-back regression; TTS synthesis itself depends on a device engine, so the emulator has no engine and only verifies button wiring |
| **M8-B (delivered 2026-10-11)** | Desktop resizable widget: QuickLookupWidgetProvider (RemoteViews: title + most recent word), the query page explicitly broadcasts a refresh after each lookup (updatePeriodMillis=0); tap widget_root → MainActivity ACTION_QUICK_LOOKUP (same entry as the M7 shortcut); min 110×40dp / minResize 80×40dp, resizeMode=H\|V | Emulator-verified: placement token `M8-WIDGET-OK updated ids=1`, tap `M7-INTENT-OK quick_lookup`, after looking up hello the desktop text changes to hello in real time (refresh chain closed), long-press shows resize handles, drag to widen and release with BACK, dumpsys rechecks resizeMode=3 |
| (later) | iOS shell (Swift + core static library + C interface), HarmonyOS shell (ArkTS + NAPI) -- each gets its own section, reusing the M2+ binding surface design and comparison methodology | Planned separately |

## 4. JNI Binding Surface Design Conventions (effective from M1)

- **Location**: `adapters/android/` (C++ JNI glue). `core/` continues to have zero platform headers -- JNI headers appear only in the glue (AGENTS.md constraint not broken).
- **Domain split**: `dict_jni.cpp` (loading/start-stop/deletion/metadata), `lookup_jni.cpp` (exact/prefix/fuzzy/fulltext/aggregation), `store_jni.cpp` (vocabulary/history/notes).
- **Data shapes**: UTF-8 `std::string` ↔ `jstring`; structs flattened into Kotlin `data class` fields, avoiding frequent `jobject` round-trips; lists converted to `Array`/`List` in one shot.
- **Lifecycle**: C++-managed objects are held as `jlong` handles with explicit `close()` release; the shell layer starts with single-threaded calls (core has no internal locking assumption); if multithreading is needed later, add it in the glue layer.
- **Behavior spec**: the std test suite (112 ctest) is the core behavior spec; each JNI domain gets a minimal smoke assertion (M0's android_smoke.cpp is the template); do not duplicate the test matrix.

## 5. This Batch's Feasibility Verification Record (M0 measurements)

1. **NDK cross-compilation**: `cmake -B <dir> -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI={arm64-v8a,x86_64} -DANDROID_PLATFORM=android-24` + std-only config (all four `UNIDICT_BUILD_*` off) → both ABIs compile in full, static libraries and all std test executables link successfully.
2. **Emulator runtime smoke**: AVD test30 (android-30 google_apis x86_64) started headless; `/tmp/asmoke/android_smoke.cpp` (NDK clang++ directly linking the static library + `-lz`) pushed via `adb push` to `/data/local/tmp` and executed, covering the lookup chain + vocabulary persistence + path fallback, printing `ANDROID-SMOKE-OK`.
3. **Qt for Android chain**: local Qt 6.10.3 has only the desktop kit; the `aqt list-qt linux android` public index tops out at 6.7.3 (desktop 6.10.3) -- the real cost of Option A on this machine: downgrade Qt by three major versions or rely on a commercial channel. Downgrade compatibility was not tested (not doing A, no investment).

Measurement environment note (reproduction tip): this machine has two SDK roots (`~/android-sdk` and `~/.local/android-sdk`); the emulator's search order for `ANDROID_SDK_ROOT`/`ANDROID_HOME` can resolve an AVD's `image.sysdir.1` to an empty root -- explicitly pointing the way with `-sysdir <SDK>/system-images/android-30/google_apis/x86_64` works around it.
