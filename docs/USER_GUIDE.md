> Language: **English** | [简体中文](./zh/USER_GUIDE.md)

<div align="center">
  <img src="../docs/logo.svg" width="120" alt="Unidict logo"/>
</div>

# Unidict User Guide

Unidict packs the StarDict / MDict / DSL / JSON / EPUB / CSV dictionaries you have into one offline lookup tool: six search modes, multi-dictionary aggregation with grouping, local TTS readout, vocabulary and notes. The core lookup chain makes no network requests; the only default outbound traffic is online pronunciation (off by default; when enabled it sends only the queried word).

## Quick Start

### Command line (unidict_cli_std)

```bash
# 基本查词
unidict_cli_std -d /path/to/dict.mdx hello

# 前缀搜索
unidict_cli_std --mode prefix inter

# 模糊搜索
unidict_cli_std --mode fuzzy compuetr

# 环境变量词典列表（Windows 用 ';' 分隔）
export UNIDICT_DICTS="/path/to/dict1.mdx:/path/to/dict2.ifo"
unidict_cli_std hello
```

### Desktop GUI (qmlui)

```bash
export UNIDICT_DICTS="/path/to/dict.mdx"
build/qmlui/unidict_qml
```

Type a word in the sidebar and press Enter to look it up. The results page is folded and grouped by dictionary; the content tabs switch between Example Sentences / Collocations / Synonym Associations / Fulltext Search; the two sidebar tabs "History" and "Vocabulary" manage learning data. Vocabulary, history, and notes are concentrated in the desktop GUI (only vocabulary has a CLI CSV import/export entry, see the README), and lookups are not written into history either.

### Supported Dictionary Formats

| Format | Extensions | Description | Example |
|------|----------|------|------|
| MDict | .mdx, .mdd | Encrypted dictionaries need a password (see Encrypted Dictionaries below) | `longman.mdx` |
| StarDict | .ifo, .idx, .dict | Open format, supports compression | `stardict.ifo` |
| DSL | .dsl | ABBYY Lingvo dictionary format | `lingvo.dsl` |
| EPUB | .epub | Dictionary-type EPUB | `dict.epub` |
| JSON | .json | Custom format | `mydict.json` |
| CSV/TSV | .csv, .tsv | Custom format | `mydict.csv` |

## Search Modes

### Exact match (exact)

```bash
unidict_cli_std --mode exact computer
```

Only headwords that match exactly count as hits, going through a direct word-table lookup. When you are sure of the spelling, this is the fastest.

### Prefix search (prefix)

```bash
unidict_cli_std --mode prefix inter
```

Matches headwords that start with your input; good for completion. The desktop "Auto" mode goes prefix first, then falls back to fuzzy if there are no results.

### Fuzzy search (fuzzy)

```bash
unidict_cli_std --mode fuzzy compuetr
```

Returns close headwords by edit distance, tolerating typos.

### Wildcard (wildcard)

```bash
unidict_cli_std --mode wildcard --pattern "te*t?"
```

`*` matches any character sequence, `?` matches a single character, `[abc]` matches a character set, `[a-z]` matches a character range.

### Regex (regex)

```bash
unidict_cli_std --mode regex --pattern "^inter.*et$"
```

### Fulltext search (fulltext)

```bash
unidict_cli_std --mode fulltext --pattern "machine learning"
```

Searches inside definition text; results are sorted by relevance. The index can be persisted to disk, and subsequent queries get noticeably faster (see Index and Cache below).

## Dictionary Management

```bash
# 查看已加载词典（含词条数）
unidict_cli_std --list-dicts-verbose

# 递归扫描目录并加载
unidict_cli_std --scan-dir /path/to/dictionaries

# 按名字移除已加载词典
unidict_cli_std --drop-dict "Longman"

# 查一个词都在哪些词典里
unidict_cli_std --where hello

# 精确命中时展示全部释义
unidict_cli_std --all hello

# 同时加载多个词典
unidict_cli_std -d dict1.mdx -d dict2.ifo -d dict3.json
```

## Encrypted Dictionaries

An MDX marked as encrypted is parsed on a best-effort basis; variants such as SimpleXOR need a password. There are two ways to give it:

```bash
# 环境变量（对整个会话生效）
export UNIDICT_MDICT_PASSWORD="your-password"

# 或命令行参数（仅本次运行）
unidict_cli_std --mdict-password "your-password" secret_word
```

On the desktop, enter the password in the sidebar and Apply & Reload (a hint row appears in the sidebar when an encrypted dictionary is detected). Compatibility note: `UNIDICT_PASSWORD` is also accepted as an MDict password and may be removed later.

## Index and Cache

```bash
# 词条索引落盘与复用
unidict_cli_std --index-save my_index.db
unidict_cli_std --index-load my_index.db
unidict_cli_std --index-count

# 全文索引落盘、统计、校验
unidict_cli_std --fulltext-index-save ft_index.db
unidict_cli_std --fulltext-index-load ft_index.db
unidict_cli_std --ft-index-stats ft_index.db
unidict_cli_std --ft-index-verify ft_index.db

# 缓存
unidict_cli_std --cache-size
unidict_cli_std --cache-dir
unidict_cli_std --cache-prune-mb 500
unidict_cli_std --cache-prune-days 30
unidict_cli_std --clear-cache
```

For large dictionaries, build the index before querying; fulltext search benefits most from a persisted index.

## Desktop Interface (qmlui)

- **Sidebar**: search box + mode dropdown (Auto/Prefix/Fuzzy/Wildcard/Regex) + live suggestion list; the "History" and "Vocabulary" tabs. History rows can be 📌 pinned (the pinned block floats to the top), deleted one by one with ✕, or the whole page cleared; on vocabulary cards you can add tags, write notes, filter by tag, and export CSV; the notes search box on the Vocabulary page filters by note content (on mobile, notes are directly searchable in the vocabulary filter box); the "Export Notes" button exports all entry notes into a standalone HTML (a .pdf suffix exports PDF).
- **Entry card**: light actions along the top are Read / Favorite / Copy / Note / Make Sentence (✨ Make Sentence goes through the AI external command bridge `sentences` subcommand; when unconfigured it outputs a self-labeled `[Mock sentences]` offline fallback); definitions are folded and grouped by dictionary.
- **Content tabs**: Example Sentences / Collocations / Synonym Associations / Fulltext Search; data is fetched only when opened.
- **Tool drawer** (the "Settings" button in the header): Lookup (clipboard toggle, polling interval, word length range; word-selection lookup toggle -- on X11 selecting is looking up, not offered on other platforms; launch on boot -- Windows registry Run key / Linux XDG autostart / macOS LaunchAgent), Voice (pronunciation source three states, accent, volume), Dictionaries (priority P↑/P↓ reordering, normalized across the column; ✕ removes a single one -- session-scoped, comes back on restart from the UNIDICT_DICTS sources; fulltext index status row + "Rebuild" button -- discards the in-memory index, rebuilt on demand at the next fulltext search), Hotkeys, Sync, Appearance (light-dark three states: follow system / light / dark, choice remembered to disk; definition font size in four steps 12/14/16/18 applied instantly).
- **Floating lookup window**: summoned by clipboard lookup or the quick_lookup hotkey, shown next to the cursor, collapses on focus loss; inside you can read aloud, add to vocabulary, or open in the main window. On Windows the hotkey first reads the focused window's selection (UIA, trigger-based) and falls back to the clipboard when there is no selection.
- **System tray**: icon always present; menu items are Show Main Window / Hide to tray on close (checkbox remembered, on by default) / Launch on boot / Quit; double-click the icon to restore the main window, with a balloon hint on the first hide. By default closing the window hides to the tray (can be turned off in the tool drawer); environments without a tray (some Linux sessions) automatically degrade to quit-on-close.
- **Keyboard shortcuts**: Enter searches, Esc clears the search box, Ctrl+1/2/3 switches Results/History/Vocabulary; the lookup window global hotkey works on three desktop platforms (Windows RegisterHotKey / Linux X11 sessions XGrabKey -- Wayland sessions fail with an explicit message at registration / macOS Carbon), defaults to Alt+Q, changeable on the "Hotkeys" page of the tool drawer.

Online pronunciation is off by default; once enabled it sends only the queried word itself to dictionaryapi.dev, with explicit wording in the UI.

Pronunciation scoring (shadowing, per-phoneme feedback) is a separate experimental feature: not built by default, the scoring UI lives in the gui (QWidget) desktop, and the ~635MB model must be downloaded separately -- see [pronunciation-plan](zh/pronunciation-plan.md).

## Cross-Platform Installation

A rolling nightly Release is built automatically every day at 05:17 (Beijing time) (fixed tag `nightly`, anonymous download): <https://github.com/cuihairu/unidict/releases/tag/nightly>

```bash
# Linux / macOS 一键安装（识别 OS/架构，覆盖安装即升级）
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell
# Windows
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

Linux also has deb/rpm packages for cli-std. The Android native shell has delivered dictionary import, lookup and vocabulary/history/notes, TTS readout, dark theme, APK packaging, the three share entries (Share/selection menu/long-press shortcut), the floating lookup window and the desktop widget (delivered up to M8-B); iOS / HarmonyOS have not started.

## Troubleshooting

**Dictionary fails to load**: confirm the path and that the format is supported, then check the verbose list:

```bash
unidict_cli_std -d your_dict.mdx --list-dicts-verbose
```

If it still fails, use `--scan-dir` to check whether the file is recognized. Corrupt dictionaries are quarantined and marked `[FAILED]` in the list; fix the file and retry to recover.

**No search results**: check the spelling first, then switch modes -- use `--mode fuzzy` when unsure of the spelling, `--mode prefix` when you only know the beginning, `--mode fulltext` to search definition content.

**MDict will not open**: encrypted dictionaries need a password (see Encrypted Dictionaries above). To inspect the file structure:

```bash
unidict_cli_std --mdx-debug encrypted.mdx
```

**High memory usage**: `--cache-prune-mb` caps the cache, `--index-load` reuses an index to reduce resident structures; the more dictionaries loaded at once, the higher the usage -- trim `UNIDICT_DICTS` as needed.

## Further Reading

- [Architecture current state](zh/CURRENT_ARCHITECTURE.md) / [Feature matrix](zh/CURRENT_FEATURE_MATRIX.md): the real current implementation status
- [Development environment](dev.md) and the root [ARCHITECTURE](../ARCHITECTURE.md): build and layering notes
- [Contributing guide](../CONTRIBUTING.md)
- [roadmap](roadmap.md): planning entries (some entries have drifted from the current state; defer to the feature matrix)
- Documentation site: <https://cuihairu.github.io/unidict/>

## Getting Help

```bash
unidict_cli_std --help
```

Bug reports go to [GitHub Issues](https://github.com/cuihairu/unidict/issues).
