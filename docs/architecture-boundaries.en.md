> Language: **English** | [简体中文](./architecture-boundaries.md)

# Unidict Architecture Boundaries

> Phase 2 · 2026-10-04 · Normative document: violating any of the following = architecture debt (register in TECH_DEBT, never break the boundary silently).
> Current-state reference: docs/CURRENT_ARCHITECTURE.md (2026-10-04 audit baseline, incrementally refreshed 2026-10-14).

## 1. Dependency Direction (the Only One)

**Shell/App → adapter layer → core/std. core never depends back.**

- `core/std/` includes no Qt (**zero exceptions**, verified: 97 files, zero hits). The `core/` root retains two Qt legacy leftovers: `path_utils.{h,cpp}` (QString path API) and `unidict_core.h` (dictionary DTO), compiled only into the Qt adapter target `unidict_core_qt`, never into the std build chain — the CI std-only job's "Qt-free core boundary scan" step pins these two files via a whitelist; any Qt include outside the whitelist fails the build.
- **core/std is the single source of knowledge**: all new domain logic goes into std. `core/` (legacy leftovers: path_utils/types/unidict_core thin layers) is output-only — a convergence target, no new legacy logic.
- The adapter layer stays thin: bridges only do type translation and platform interaction; domain logic must remain in std (historical lesson: the legacy DictionaryManager ~1200-line singleton was retired with TD-105, 2026-10-10 927f376 — that shape was never allowed by the boundary).
- Dual implementations are a transitional state, not a steady state: the `*ParserQt` bridges were deleted along with the entire legacy chain (TD-102 closed, 2026-10-10) — a bridge must never be treated as evidence that "wiring is done".

## 2. The Lookup Core Path Makes No Network Requests (local-first hard constraint)

- Lookup, aggregation, rendering, history, vocabulary notebook, and notes are **all fully offline**, including on first install.
- The network may only appear in explicit user actions at the shell layer (online pronunciation, AI, manual sync).
- **The server never enters the lookup core path** — looking up a word does not depend on a server or on login state.
- A violation is P0 architecture debt (register in TECH_DEBT and roll back).

## 3. AI Replaceability

- AI capabilities go through a provider abstraction (local / OpenAI-compatible / free / custom / System AI); the interface is defined at the shell adapter surface, core carries no AI.
- AI failure or absence degrades **seamlessly** (returns blank/skips rather than erroring out).
- No single-vendor lock-in; keys/endpoints are user-managed (environment variables or settings surface, never hardcoded).

## 4. Data Boundaries

| Class | Semantics | Constraints |
|---|---|---|
| Dictionary files | User assets | Read-only loading (path/env/scan); never enter the database, never committed to the repo, **content never synced** |
| User data | History/vocabulary notebook/tags/notes | Local persistence (JSON today), exportable (CSV); sync (if enabled) syncs only this class |
| Settings | Preferences | Two-way split: **Account** (cross-device sync surface) vs **Device** (local); key-name convergence (TD-122 track) |
| Learning state | Statistics/review history | Single source of truth (TD-113 closed: dual storage merged into DataStoreStd, 2026-10-10); four-skill field bits landed (set_vocabulary_skill) |

Encryption: synced data, if it goes through the server, follows the E2EE semantics of design/sync-engine.md; local data is not forcibly encrypted (the only stated lower bound: no plaintext logs).

## 5. Testing and Gates

- core/std tests are Qt-free (`<cassert>`+main or Catch2 in the future); Qt-side tests live only in adapter/shell tests.
- Gate criteria: build-std and build(Qt) ctest all green + `scripts/coverage.sh` lines 100% threshold (core scope).
- **New source code must not be committed without passing the gates** (precedent: online_pron was committed without running the gates, causing lines coverage to drop to 99.6% before being fixed, closed in fae5002).
- Tests only grow, never shrink; `GCOVR_EXCL` must carry a comment with the reason (structural unreachability counts; never force it to hit the number).

## 6. Evolution Directions (on record, no implementation)

These are registered technical-debt tracks — **the first four were closed in the 2026-10-10/11 batch** (TD-101~105, TD-115, TD-122, TD-119); they are recorded here to prevent regression, and no new code may reopen them:

- ~~**Dictionary vs DictionaryManager separation** (TD-101~105)~~: closed (927f376) — dictionary instances decoupled from the manager, single std surface.
- ~~**Rendering abstraction**: `HtmlRenderOptions` dead-field convergence (TD-115)~~: closed (seven dead fields removed); the explicit `DictionaryContent` abstraction was not built (dead fields are gone, no current demand pulling it, revisit under the §8 criteria).
- ~~**Settings convergence**: settings_qt key names aligned to domains + Account/Device split (TD-122)~~: closed (fifteen keys verified with zero drift).
- ~~**UI exit**: QML shell and QWidget shell serve the same core (dual-shell TD-119)~~: closed (419d58b) — QML is the sole desktop main exit, gui is the pronunciation-practice feature shell; exit selection is a shell-layer matter and must not split the core.
- **CLI man-style**: pure lookup and diagnostics (cli-std); learning management lives in the GUI; the CLI does not write history (existing discipline, README).
- **Mobile**: native shells + JNI/FFI reusing core/std (Android established; iOS/HarmonyOS isomorphic, not started). The QML mobile shell (Main.qml) is a deprecated path, not to be revived.

## 7. Platform Boundaries

- Platform-specific features (global hotkeys/autostart/tray): implemented on all three desktop platforms (F9/F10/F11); at the platform boundary, **stub/failure must be explicit, never pretend to work** (current state: Wayland sessions honestly fail hotkey registration, macOS/Linux without a tray environment degrade to close-window-exits). The three targets of the same capability must go through the same surface helper (hotkey `unidict_hotkey_platform`, autostart `StartupLauncher`), filled in together with the porting party — missing bridges only surface on macOS CI (undefined symbols).
- The Android shell sits alongside the desktop shells, both consuming core through a single adapter surface — no third domain logic appears.
- Dictionary assets are not distributed with the repo (including test fixtures, which use only generated/minimal samples).

## 8. Violation Is Debt (register-and-fix criteria)

If any line on this page is violated: open a TECH_DEBT entry + write down the violation location and the recovery track; fixes take priority over new features (§6 tracks).
