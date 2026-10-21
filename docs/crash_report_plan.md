> Language: **English** | [简体中文](./zh/crash_report_plan.md)

# Unidict Crash Collection Profile (Crashpad)

> 2026-10-08 first batch. Selection finalized (user decision): direct integration of Google **Crashpad**; Breakpad does not enter the product, serving only as a historical reference and host-side reconstruction tool (dump_syms / minidump_stackwalk).

## I. Goals and Boundaries

**Goal**: when the desktop app crashes, an independent handler process produces a minidump, first written to disk locally; the developer can then reconstruct a call stack with function names offline using symbol tables, as diagnostic evidence for user-reported bugs.

**Boundaries**:

| Surface | This batch's scope |
| --- | --- |
| Integration surface | Only the `qmlui` desktop shell (unidict_qml) — the official desktop delivery surface; gui (demo shell) and cli/cli-std (dev tools) are not integrated, core dumps suffice for them |
| Android | **Not integrated**. Crashpad does not officially support Android; Android-shell native crashes go through the system tombstone, Java exceptions go through the system ANU/crash error dialog. If native collection is needed later, evaluate it separately (breakpad or a crashpad-android port), not in this batch |
| std-only builds | Unaffected. `UNIDICT_BUILD_CRASH` hangs under `UNIDICT_BUILD_QT_QMLUI`; std builds (QMLUI=OFF) never touch crashpad |
| Data exfiltration | **Off by default**. An empty upload URL = write to disk only, no upload; if it is ever enabled it is a new external endpoint requiring explicit user authorization + privacy statement (see Section VII) |

## II. Selection and Version

- **Crashpad** (backtrace-labs maintained fork @ `7b9686b`) + **mini_chromium** (@ `9cdc2a7`) + **linux-syscall-support** (@ `e1e7b0a`) — three upstream commits pinned inside the vendored wrapper; the CMake wrapper itself ([TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake), Apache-2.0) is **vendored into the repo** at `cmake/crashpad/` (13 files), adapted to the host compiler (C++17 / `-Werror` removed / forced pre-include of `cstdint`); the root CMakeLists attaches `UNIDICT_BUILD_CRASH` via `add_subdirectory(cmake/crashpad EXCLUDE_FROM_ALL)` — no FetchContent, not in the default full build.
- Why this wrapper: it builds the mini_chromium/lss dependencies and the `crashpad_handler` executable target, its own CI covers Linux/macOS/Windows; verified locally with a spike (compiles with cmake 4.2 + gcc 15.2).
- **Why Crashpad instead of Breakpad** (historical reference):

| | Crashpad | Breakpad |
| --- | --- | --- |
| handler location | Independent process; a main-process crash does not affect dump writing | In-process; it can itself be damaged at the moment of the crash |
| Crash database | Built in (state machine new/pending/completed, settings) | None, managed by hand |
| Maintenance status | Actively maintained by Chromium | Maintenance mode (2009, 32-bit era) |
| minidump format | Standard minidump + extensions | Standard minidump |

The two dump formats are compatible, so the reconstruction toolchain uses Breakpad's `dump_syms` + `minidump_stackwalk` directly (mature and user-specified); they run only on the host/CI, **never enter the product or the main CMake**.

## III. Process Model and Initialization Point

```
unidict_qml (主进程)
   │  main() 早期：crash::init(crashDir, appDir)
   │     └─ CrashpadClient::StartHandler → fork+exec crashpad_handler
   ├──────────────► crashpad_handler (独立子进程)
   │                  常驻监听 socket；主进程 SIGSEGV/SIGILL/abort 时
   │                  由内核信号 → 客户端 stub 通过 ptrace 让 handler
   │                  跨进程读取寄存器/内存 → 写 minidump 进崩溃库
   └─ 正常业务继续跑，与 handler 生命周期解耦（restartable=true）
```

- **Initialization point**: `qmlui/main.cpp`, called immediately after `QGuiApplication` construction and application property setup — initialize as early as possible to cover all subsequent code; the cost is that crashes during the `QGuiApplication` construction segment itself are not covered (accepted, extremely small magnitude).
- Android build compile isolation: `#ifdef UNIDICT_ENABLE_CRASH` wraps the integration code + the CMake option is forced OFF under ANDROID.
- Failure semantics: if the handler is missing or fails to start, init returns false and the app **runs as usual** (collection is an enhancement, not a dependency), with a qWarning explaining the reason.

## IV. Dump Storage Directory

- Root directory: `QStandardPaths::AppDataLocation + /crash`. The current organization name is a placeholder (`YourCompany`); the actual paths are:
  - Linux: `~/.local/share/YourCompany/Unidict/crash/`
  - Windows: `%APPDATA%\YourCompany\Unidict\crash\`
  - macOS: `~/Library/Application Support/YourCompany/Unidict/crash/`
  - Changing the organization name to the real one would migrate existing QSettings data (settings/sync bindings); not done as a side effect in this batch — the dump path follows the current state.
- Crashpad crash database layout (managed by crashpad itself, do not write by hand):

```
crash/
├── settings.dat          # 库元数据
└── crashpad/
    ├── new/              # 正在写入
    ├── pending/          # 待处理（无上传时最终归宿之一）
    └── completed/        # 已完成
```

- Retention policy: no manual cleanup in the first batch, the crashpad database manages itself; if disk sensitivity matters later, add count/age-based cleanup (noted in todo, no product UI).

## V. Packaging the Independent Handler Process

- **Build time**: the `crashpad_handler` target is provided by the wrapper; `unidict_qml` POST_BUILD copies it to `$<TARGET_FILE_DIR:unidict_qml>`, same directory as the main program.
- **Install time**: `install(TARGETS crashpad_handler DESTINATION bin)`, following the existing install/CPack tree (deb/rpm/NSIS/DMG same rules).
- **Runtime**: handler path = `QCoreApplication::applicationDirPath() + /crashpad_handler` (passed explicitly to StartHandler, no implicit lookup); if missing, init reports false and states the reason.
- Source-build users: crashpad is built into the default build (`UNIDICT_BUILD_CRASH` defaults ON), no extra switch needed; environments that don't want to compile crashpad use `-DUNIDICT_BUILD_CRASH=OFF`.

## VI. Symbol Table Management

**Compilation**: integration symbols = append `-g` to the `unidict_qml` target (when the crash option is on; the global build type is untouched). crashpad's own code needs no symbols (stack reconstruction cares about unidict frames).

**Separation** (distribution criteria: main program stripped, symbol files archived and not shipped in distribution packages):

- Linux (`scripts/symbols_split.sh`):
  ```bash
  objcopy --only-keep-debug unidict_qml unidict_qml.debug   # 1. 抽符号
  strip --strip-debug unidict_qml                            # 2. 主程序去符号
  objcopy --add-gnu-debuglink=unidict_qml.debug unidict_qml  # 3. 挂回链
  ```
- Windows: MSVC's `.pdb` is natively separated, obtained with a RelWithDebInfo build; distribution ships only the exe, the pdb is archived.
- macOS: `dsymutil unidict_qml` produces a `.dSYM` for archiving.

**Stack reconstruction** (host tools; `scripts/build_stackwalk_tools.sh` compiles from google/breakpad source into `build-tools/`, gitignored, not in the main CMake):

```bash
dump_syms unidict_qml > unidict_qml.sym                    # ELF+DWARF → breakpad 符号
# 按 MODULE 行的 uuid 摆放：symbols/<name>/<uuid>/<name>.sym
minidump_stackwalk <dump>.dmp symbols/ > stack.txt          # → 带函数名/文件行号的调用栈
```

- Version correspondence: the minidump records each module's debug uuid, and stackwalk matches symbol files by it automatically; **a dump must be paired with symbols of the corresponding version**. Distribution flow convention: run symbols_split at packaging time, archive the symbol files together with the version number (CI artifact or local archive), and pick symbols by the user's version when handling a bug report.

## VII. Upload Placeholder (off by default)

- `CrashpadClient::StartHandler`'s url parameter is passed an **empty string** → the handler only writes to disk, never uploads. The current code is in this state, with no network egress at all.
- Current code state: the url is a **hardcoded empty string** at the crash_reporter.cpp call site (with a privacy-criteria comment), and the `crash::init` signature is `(handlerPath, dbDir, annotations)` with no uploadUrl parameter slot. If it is ever enabled (add the parameter slot first, then fill in the endpoint), the prerequisites (all required):
  1. The user **explicitly authorizes** it on the settings page (default OFF, aligned with the "turning online is an explicit action" criteria for online pronunciation/AI);
  2. The endpoint and ownership are decided (a new external service, outside server_plan, requiring separate review);
  3. The privacy statement spells out what is sent — a minidump contains process memory fragments and **may include user dictionary content and vocabulary notebook data**; this is the point the data-egress bar must clear.

## VIII. Integration with Existing Logging/Monitoring

- The project has no central logging framework; the existing logging channel is Qt's default `qInfo`/`qWarning` (stderr). Integration means hooking into this channel:
- **Initialization reporting**: init success → `qInfo("崩溃采集已启用: %s", dbPath)`; failure → qWarning with the reason (handler missing/startup failure).
- **Historical crash detection**: each startup scans the crash database (the new/pending/completed directories); when dumps left over from the previous run are found, a `qWarning` states the count and paths — users can attach them when reporting bugs, and developers reconstruct per Section VI. A UI surface (settings page showing "N crash reports detected") may be discussed later; this batch does the logging surface only.
- **Diagnostic switch**: the `UNIDICT_CRASH_TEST=nullderef` environment variable triggers a null-pointer dereference sample after init (for acceptance/diagnosis only, permanent code, commented as such).

## IX. Acceptance Criteria

1. At configuration time crashpad enters the build, and `build/Release/` (or the installed bin/) carries `crashpad_handler`.
2. Starting unidict_qml with `UNIDICT_CRASH_TEST=nullderef` → the process crashes, a minidump appears in the crash database (reported by the next process's startup log).
3. `dump_syms` + `minidump_stackwalk` reconstructs a call stack **containing unidict function names** (hard validation of symbolication effectiveness).
4. All three gates green; the build-std build does not touch crashpad; coverage criteria unchanged (adapters/crash is not counted in the core/ scope).

## X. Risks and Mitigations

| Risk | Mitigation |
| --- | --- |
| gcc 15 / cmake 4 compatibility with a 2022-era wrapper | Local spike passed (crashpad_handler executable produced); CI compiler versions are older (gcc 13 / MSVC), so the risk surface is smaller |
| Intermittent googlesource (lss) network | Verified reachable locally; FetchContent cache means no re-pull; CI network is independent |
| Increased CI duration (crashpad first build takes a few minutes) | One-time cost, acceptable; if windows-latest has compile problems, the fallback is to lower the default to OFF and follow up on fixing the wrapper |
| crashpad_handler resident process | Single-digit MB of memory, decoupled from the main process |
