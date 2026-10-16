> Language: **English** | [简体中文](./gui-smoke-checklist.md)

# Windows GUI Smoke Verification Checklist

> Run through this checklist after each new Windows artifact (CI artifact, not a Release).
> All green means the hard goal of "a runnable Windows version" still holds; if any item fails, first note the artifact's commit (it's in `BUILD-INFO.txt`), then reproduce and locate.

> **Current-state update (verified 2026-10-14): since 419d58b (2026-10-10, TD-119) the CI Windows artifact has switched to `unidict-qml-windows` (main program `unidict_qml.exe`, QML shell; the zip also contains README/LICENSE and windeployqt DLLs), and daily-build delivers `unidict-windows-x64.zip` / setup.exe (same main program + cli/relay + built-in dictionaries, auto-loaded on first run). The interaction checklist below describes the gui (QWidget) shell's interface (dictionary management dialog / status bar / toolbar / sidebar) — gui is still built with the apps target (pronunciation-practice feature shell), and this checklist continues to serve as that shell's Windows manual regression sheet; the commands in the "Getting the artifact" section and the zip contents are written for the old artifact (`unidict-gui-windows` / `unidict_gui.exe`) and no longer apply (the local self-build verification path is unchanged). The QML main exit's manual smoke surface: USER_GUIDE + daily-build release notes (no independent checklist yet).**

## Getting the artifact

```bash
# 找最近的成功 run，下载 unidict-gui-windows artifact
gh run list --workflow ci.yml --limit 5
gh run download <run-id> -n unidict-gui-windows -D ./win-smoke
cd win-smoke && unzip ../unidict-gui-windows.zip   # artifact 本身是 zip
```

zip contents: `unidict_gui.exe` + the Qt DLLs collected by windeployqt (including `platforms/qwindows.dll`) + vcpkg's zlib1g.dll + `BUILD-INFO.txt` (build time and commit). Don't disturb the directory structure; DLLs load from their neighbors.

## Startup and Window

- [ ] Double-clicking `unidict_gui.exe` brings up the main window directly (SmartScreen blocks unsigned exes: "More info → Run anyway", expected, not a packaging problem)
- [ ] Window title Unidict; no DLL error popup (if it reports a missing Qt6Xxx.dll, windeployqt didn't bring everything; go back and check the CI Package step)
- [ ] Dragging/resizing the window works; position and size are restored after closing and reopening (window geometry memory)
- [ ] The toolbar theme button cycles three states: follow system → light → dark, remembered after restart

## Dictionaries and Query (core path)

- [ ] On first startup the status bar shows "0 dictionaries · Ready"; click "Dictionary Management…" to add a JSON dictionary (`examples/dict.json` works) or a StarDict directory
- [ ] Typing an entry and pressing Enter → results appear in the definition area, with the source dictionary name shown greyed out
- [ ] Typing a **nonexistent word** → a "Similar entries" list + a "Full-text hits (the word appears in definitions)" block appear; clicking an entry in the full-text hits re-queries it
- [ ] Typing a word prefix → a completion list pops up (Unfiltered popup); Enter/click selects and queries
- [ ] HTML in definitions (MDict dictionaries) renders normally, with no script-execution window popping up

## Word Capture and Sidebar

- [ ] After turning on the toolbar "Clipboard lookup" switch, copying a word in another app → the Unidict window pops up and auto-queries; after turning the switch off it no longer triggers (switch state remembered after restart)
- [ ] After turning on the toolbar "Global hotkey" switch, pressing Ctrl+Alt+U in another app → the Unidict window pops up with the input box focused; after turning the switch off the hotkey stops working (switch state remembered after restart; works on all three desktop platforms — Linux needs an X11 session (registration fails explicitly on Wayland sessions), macOS goes through Carbon)
- [ ] Queried words appear in the "History" sidebar; double-click backfills; right-click can pin/delete
- [ ] "☆ Favorite current word" enters the "Favorites" sidebar; right-click removes

## Dictionary Management Dialog

- [ ] Add files (.ifo/.mdx/.json filters) and add directories (recursive scan)
- [ ] After enabling/disabling a dictionary, query results change immediately; move up/down changes priority
- [ ] "Set group tags…" tags dictionaries (comma-separated, empty clears); the list row ends show "Group: xx/yy"
- [ ] Remove a dictionary; after closing the dialog the status bar count refreshes

## Group Filtering (profile)

- [ ] Tag two dictionaries with different groups (e.g. en / zh), the main window toolbar dropdown shows these two groups; after selecting "en", queries and completions return only results from the en group's dictionaries
- [ ] Switching back to "All groups" restores full results; after switching, press Enter directly to re-query the current input box
- [ ] Select a group and restart Unidict, the group selection is remembered; if all dictionaries in that group are removed, it falls back to "All groups" after restart

## Known Non-Verified Items (not coverable by local Linux offscreen smoke)

- The actual light/dark "follow system" linkage (needs a real desktop session)
- Real triggering of the global hotkey (needs another app in the foreground receiving keystrokes; the checkbox above is walked manually on the Windows artifact)
- Multi-monitor geometry restoration edge cases (Qt saveGeometry handles it; in abnormal cases deleting the config under `%APPDATA%/Unidict` resets it)

## Historical Pitfalls (problems that have occurred; watch for them on recheck)

- Windows Git Bash has no zip command; CI packaging uses PowerShell `Compress-Archive` (once failed with `zip: exit 127`)
- Qt 6.10's offscreen platform plugin silently crashes on Windows, so Qt tests on Windows runners go through the default qwindows main path
- The vcpkg zlib DLL must be copied into dist; without it, loading .dict.dz/.mdx compressed blocks errors (decompression failure ≠ corrupted dictionary)
