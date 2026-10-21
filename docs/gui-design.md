> Language: **English** | [简体中文](./zh/gui-design.md)

# GUI Technology Selection Finalized: Qt Widgets

> Status: finalized (2026-09-23). First hard goal: **produce a version that runs on Windows**.
> This document records the selection conclusion, rationale, review conditions, and landing path; the tech stack is not to be re-litigated.

> **Current-state update (verified 2026-10-14): the desktop exit landscape was revised by TD-119 (2026-10-10, 419d58b) — QML (unidict_qml) is the sole desktop main exit, and the Qt Widgets shell (gui/unidict_gui) is retained as the pronunciation-practice feature shell (recording/repeat-after/scoring/model download exist only there). The Widgets-vs-Tauri comparison rationale in this document remains the technical basis for the gui shell's continued existence, but "the GUI framework is set to Qt Widgets" no longer describes the desktop main exit; the current criteria are in docs/CURRENT_ARCHITECTURE.md and architecture-boundaries.md §6.**

## 1. Conclusion

- The GUI framework is set to **Qt 6 (Qt Widgets)**, dynamically linked (LGPLv3 compliant).
- The first delivery milestone is a **Windows runnable package** (exe + Qt DLL packaging), built and delivered as an artifact via the CI Windows job; feature completeness comes after that, and no new large feature starts before it.
- The search engine stays a **std-only standalone static library** (`unidict_std_core` / `unidict_index_std`), with Qt only as the shell. The engine depends on no UI framework and can be reused directly by other projects (CLI, server, other GUIs).

## 2. Qt Widgets vs Tauri: Comparison and Finalization Rationale

| Dimension | Qt 6 Widgets (selected) | Tauri 2 |
| --- | --- | --- |
| Rendering/memory | Native controls + raster drawing, no WebView stack; small memory footprint for an empty window | One WebView2 process per window; a resident dictionary tool amplifies the memory difference |
| Large-list performance | `QListView` + custom model is natively virtualized, smooth scrolling at hundred-thousand-entry scale | Virtual scrolling must be built by hand (large DOM lists are a perennial front-end hard problem) |
| Instant completion | `QCompleter` plugs directly into the entry prefix index, ready to use | The completion component and debounce pipeline must be built from scratch |
| Stability | Two decades of industrial-grade API, 6.x LTS long-term support; stable API semantics | Young framework (1.x→2.x already had breaking changes); WebView2 version drifts with system updates, rendering behavior uncontrollable |
| Language stack | C++ throughout, zero bridging cost with the existing core/adapters | Rust shell + C++ engine, must maintain a Rust↔C++ FFI bridge, dual-language build chain |
| Offline-scenario fit | No network dependency, no WebView2 runtime needed either | Depends on the system WebView2 (older Win10 needs a separate runtime install) |
| Beautification ceiling | QSS is enough for desktop-tool-grade looks; complex rich-text rendering is slightly weaker | Strong HTML/CSS expressiveness (dictionary definition typography is the one standout item) |

**Finalization rationale**: a dictionary app's core interactions are "high-frequency input → instant completion → fast scrolling of large lists → stable residency", and all four press on Qt Widgets' strengths; Tauri's only clear advantage (the visual ceiling of HTML-typeset definitions) is already covered by the HTML subset `QTextBrowser` supports, and if something stronger is ever truly needed it can be solved by embedding a rendering layer inside Widgets — not a reason to switch stacks.

## 3. Tauri Review Conditions (revisit if any of the following occurs; otherwise the topic is closed)

1. The product form shifts toward "definition pages need heavy Web decoration" (complex CSS animations, Web-ecosystem typography components become a hard requirement).
2. Mobile (iOS/Android) priority overtakes desktop, and Tauri Mobile's maturity risk is accepted.
3. The team later hires a dedicated Rust engineer, and FFI bridge maintenance is no longer an extra burden.

## 4. License Scheme (LGPLv3, dynamic linking)

- Modules used: Qt Core / Gui / Widgets / Test (+ the Quick/Qml and TTS left over from qmlui), all LGPLv3 licensed.
- **Dynamic linking is compliant**: Windows releases use `windeployqt` to package the needed Qt DLLs together with the exe, so users don't need Qt installed; this is LGPL's standard release form.
- **Static linking would require a commercial license** — we don't statically link, avoiding this cost.
- Compliance obligations (still required under dynamic linking, landed in the release package's `LICENSE-THIRD-PARTY.txt`):
  1. Declare that Qt is used and its version;
  2. Attach the LGPLv3 license text;
  3. Explain that users can use a modified Qt by relinking/replacing the Qt DLLs (preserve the possibility of replacing the Qt library — dynamic linking itself already satisfies the main requirement, an explanatory note suffices).

## 5. Architecture Finalization: Engine as a Library, Qt Only as the Shell

Dependency direction is one-way and acyclic, consistent with `herald`'s library-ization approach:

```
cli / gui / qmlui            ← 可执行壳（允许 Qt）
        ↓
adapters/qt                  ← 薄桥接层（QString↔std::string、Qt 容器适配）
        ↓
core/std  (unidict_std_core + unidict_index_std)   ← 引擎静态库，零 Qt、零 UI
        ↓
zlib（唯一第三方依赖）
```

- The engine (parsing, indexing, search, normalization, full-text) lives entirely in `core/std`, with independent CMake targets; any new UI/project links `unidict_std_core` directly, without going through Qt.
- The legacy Qt interface layer under `core/` (`unidict_core_qt`) is a transitional artifact; shell calls are gradually migrated to the `adapters/qt` → `core/std` chain; the end goal: shells never include legacy core directly.
- **CMake switches**: `UNIDICT_BUILD_QT_GUI` (Widgets shell) and `UNIDICT_BUILD_QT_QMLUI` (Quick shell) are separated, both following `UNIDICT_BUILD_QT_APPS` by default. This way a Qt environment with only the Widgets module (or a CI that only needs to verify the engine) can build the GUI without installing Quick/Qml.

## 6. Qt6 Dependency Acquisition: Two Paths, Whichever Works First

| Path | Current state | Use |
| --- | --- | --- |
| **aqtinstall** (`install-qt-action`) | **Verified working**: CI installs 6.10.3 (Quick/QuickControls2 come with the base package; modules lists only the addon `qtspeech`, whose CMake config does `find_dependency(Qt6Multimedia)`). 6.10.3 also avoids the macOS new-SDK AGL link issue (QTBUG-137687, see the ci.yml Install Qt step note) | CI, first choice for Windows packaging machines |
| vcpkg `qtbase` | Backup: `vcpkg install qtbase --triplet x64-windows` source build takes a long time (tens of minutes), and mixing it with aqt official binaries easily causes zlib/CRT conflicts | Only a fallback when the aqt mirror is unreachable |
| Local Linux dev machine | Qt 6.10.3 (`/home/cui/Qt/6.10.3/gcc_64`) includes Widgets/Quick/Qml/TTS/Multimedia (verified 2026-10-14: Qt6Qml/Qt6Quick/Qt6TextToSpeech cmake configs complete; the old "does not include Quick/Qml/TTS" criteria is outdated) | Local builds of gui + qmlui + std-only engine verification |

Conclusion: **dependencies uniformly use aqtinstall official binaries** (CI already works); Windows local developers use the [Qt online installer](https://www.qt.io/download-qt-installer) or aqt to install the same version; vcpkg only handles zlib; no vcpkg qtbase mixing.

## 7. Windows Output Path

- **No cross-compilation**: the MSVC toolchain + Qt cross-compilation combination has high maintenance cost and poor reproducibility; MinGW cross-compiled binaries are incompatible with official Qt prebuilt libraries (MSVC ABI).
- **Use the CI Windows job**: `windows-latest` + aqt Qt 6.10.3 + MSVC + Ninja build, artifact main exit = `unidict_qml.exe` (since 419d58b/TD-119; the gui shell is still built with the apps target, not entering the artifact), `windeployqt` collects DLLs, the produced zip is delivered via `upload-artifact`.
- The artifact is development-artifact distribution, not going through GitHub Release / tags (release actions remain on the forbidden list).

## 8. Milestones

- **M1 minimal runnable window** (done): dictionary directory loading + search box instant query + results list + definition panel, builds pass locally (Linux/Qt 6.10) and in CI (three platforms Qt 6.6.3).
- **M2 search experience** (done 2026-09-24): QCompleter instant entry completion (query-as-you-type `prefixSearch` prefix-index binary search, on-demand filling of the Unfiltered popup, no full word-list loading), full-text search result fusion (`DictionaryManager::fullTextSearch` composes the std inverted index, fallback display on exact miss, anchor click re-lookup), dictionary multi-select and priority (dictionary management dialog enable/disable + move up/move down).
- **M3 engineering** (done 2026-09-24): settings persistence (theme/clipboard lookup switch remembered by QSettings), clipboard lookup entry (reuses ClipboardMonitor polling filter, popup backfills the query after word capture), error prompts and empty states (definition panel placeholder + dictionary load failure QMessageBox).
- **M4 Windows packaging** (done): windeployqt collects DLLs + vcpkg zlib into a zip, CI `upload-artifact` produces `unidict-gui-windows-*` (daily-windows stably success). Verification steps after getting the artifact are in `docs/gui-smoke-checklist.md`.
