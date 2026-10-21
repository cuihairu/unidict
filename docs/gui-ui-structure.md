> Language: **English** | [简体中文](./zh/gui-ui-structure.md)

# GUI Interface Structure Description (Qt Widgets Redesign)

> Redesigned around a dictionary tool's core job of interaction: **one screen, one focus** — the search box is the protagonist, everything else plays a supporting role.
> Built entirely from off-the-shelf Qt Widgets; no custom-drawn controls, no theme system, no over-engineering.

> **2026-10-13 increment** (the body keeps the redesign-landing-period criteria and is not rewritten; subsequent deliveries are recorded here): gui delivered three more surfaces — ① system tray (F11): hide to tray on close (QSettings `ui/closeToTray`, default on), tray menu show main window/hide to tray on close/autostart/quit, double-click restore, first-hide bubble; sessions without a tray degrade to close-window-exits. ② Toolbar "Global hotkey" switch (F10): default Ctrl+Alt+U, implemented on three platforms (Windows RegisterHotKey / macOS Carbon / Linux XGrabKey; registration fails explicitly on Wayland sessions), unsupported platforms are greyed out with the reason stated. ③ Tray menu "Autostart" switch (F9): Windows registry Run key / Linux XDG autostart / macOS launchd, shared with qmlui via `StartupLauncher`. The current complete criteria are in the code and docs/USER_GUIDE.md.

> **Current-state update (verified 2026-10-14): the desktop exit was revised by TD-119 (2026-10-10, 419d58b) — QML (unidict_qml) is the sole desktop main exit, and gui is the pronunciation-practice feature shell. This document is the structural finalization from the gui redesign period (2026-09-23): the interaction structure and control selection still apply to the gui shell, but the `DictionaryManager::searchWord` / `searchAll` chain references in the body are legacy-chain wording — that chain was retired with TD-105, and gui now goes through DictionaryManagerStd on the same source as qmlui (gui/main.cpp "all go through DictionaryManagerStd, same source as the qmlui main chain"); the "three-platform Qt 6.8.3" in §5 is old criteria (CI now installs 6.10.3). The current criteria are in docs/CURRENT_ARCHITECTURE.md and architecture-boundaries.md §6.**

## 1. Layout Overview

```
┌────────────────────────────────────────────────────────────┐
│ [Unidict]                                        [◐ 主题]  │  ← 顶部工具栏
├────────────────────────────────────────────────────────────┤
│            ┌──────────────────────────────┐                │
│            │   输入单词或词组…           │               │  ← 搜索框（全宽）
│            └──────────────────────────────┘                │     QCompleter 补全下拉
├──────────────────────────────┬─────────────────────────────┤
│                              │  [历史 | 收藏]               │
│                              │  ─────────────────          │
│   释义主区（QTextBrowser）    │   hello        10:32        │
│                              │   world        09:15        │
│   hello                      │   …                         │
│   int. 你好；喂               │                             │
│   ── Mini ──                 │  [☆ 收藏当前词]              │
│                              │                             │
├──────────────────────────────┴─────────────────────────────┤
│ 2 部词典 · 就绪                                   [词典管理…]│  ← 状态栏
└────────────────────────────────────────────────────────────┘
```

- **Top toolbar**: app name + light/dark toggle button (`QToolBar` + `QAction`).
- **Search box**: `QLineEdit` full-width at the top, `QCompleter` (`PopupCompletion`) attached to the word list, typing brings up the entry dropdown; Enter or clicking a completion item triggers the query.
- **Definition main area**: `QTextBrowser` (scrollable, clickable links, copyable); multi-dictionary hits render continuously grouped by dictionary, with the source marked by separator lines; the empty state shows guidance copy.
- **Right sidebar**: `QTabWidget` with two tabs — "History" (recent queries, with pinning/failures greyed out) and "Favorites" (user-favorited words); double-clicking any entry backfills the search. A "Favorite current word" button at the bottom.
- **Status bar**: `QStatusBar` shows the loaded dictionary count and query state; a permanently present "Dictionary Management…" button on the right pops up the management dialog (the existing sidebar logic converged into a `QDialog`: list + enable/disable + move up/move down + add directory).

## 2. Controls and Off-the-Shelf Components List (all off-the-shelf, zero custom drawing)

| Area | Control | Notes |
| --- | --- | --- |
| Completion | `QCompleter` + `QStringListModel` | Data source = each dictionary's word list (incrementally loaded into the model, case-insensitive `MatchContains`) |
| Definition | `QTextBrowser` | Renders the HTML definitions the rendering engine already produces (existing html_renderer) |
| History/Favorites | `QTabWidget` + `QListWidget` | Entry double-click backfills; favorites stored in the existing DataStore vocabulary notebook |
| Theme | `QApplication::styleHints()->colorScheme()` + manual toggle | Qt 6.5+ native light/dark awareness; manual toggle uses a `QPalette` dark palette, no QSS theme table needed |
| Dictionary management | `QDialog` + `QListWidget` + `QPushButton` | Logic moved from the existing gui sidebar code, converged into a dialog |

## 3. Key Interaction Flows

1. **Input → completion**: `textEdited` → QCompleter prefix/contains matching against the word list → dropdown. No heavy queries on the input path (completion only queries the in-memory word list, not the dictionary library).
2. **Enter/select → query**: goes through `DictionaryManager::searchWord` (the chain at the time), results render into the main area; the history tab refreshes at the same time.
3. **Multi-dictionary results**: `searchAll` aggregation (this interface was completed in that round), rendered grouped by dictionary, each group header marked with the dictionary name and priority.
4. **Favorites**: the "☆" at the top right of the main area or the sidebar button favorites the current word; double-clicking in the favorites tab queries directly.
5. **Light/dark**: follow system by default; clicking the toolbar button toggles between auto/light/dark, remembered to QSettings.

## 4. Explicitly Not Doing (anti-over-engineering)

- No custom-drawn controls, custom animations, or skin systems;
- No MDI/multi-tab query pages (single-page scrolling suffices for dictionary scenarios);
- No fuzzy pinyin matching for completion (add to M3 if demand appears later);
- When the word-list model exceeds 500k words, no full loading — switch to `QAbstractItemModel` lazy loading by prefix (at current dictionary scale, `QStringListModel` directly).

## 5. Landing Order

1. Rewrite `gui/main.cpp` into the above structure (migrate history/dictionary management logic from the existing code);
2. Light/dark three-state toggle + QSettings memory;
3. `UNIDICT_DICT_DIR` / `UNIDICT_DICTS` load paths stay unchanged;
4. Build verification locally (Linux/Qt 6.10) and in CI (three platforms Qt 6.8.3).
