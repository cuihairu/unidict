<div align="center">
  <p><b>English</b> | <a href="README.zh.md">简体中文</a></p>
  <img src="docs/logo.svg" width="64" height="64" alt="Unidict logo"/>
  <h1>Unidict</h1>
  <p>
    <a href="https://github.com/cuihairu/unidict/actions/workflows/ci.yml"><img src="https://github.com/cuihairu/unidict/actions/workflows/ci.yml/badge.svg" alt="CI"/></a>
    <a href="https://codecov.io/gh/cuihairu/unidict"><img src="https://codecov.io/gh/cuihairu/unidict/graph/badge.svg" alt="Codecov coverage"/></a>
    <a href="https://github.com/cuihairu/unidict/releases/tag/nightly"><img src="https://img.shields.io/github/v/release/cuihairu/unidict?label=nightly&sort=date&logo=github&color=b11964" alt="nightly release"/></a>
    <a href="https://cuihairu.github.io/unidict/"><img src="https://img.shields.io/website?url=https%3A%2F%2Fcuihairu.github.io%2Funidict%2F&up_message=online&down_message=offline&label=docs&color=b11964" alt="Documentation site"/></a>
    <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20Android-3d4451" alt="Platform support"/>
  </p>
  <p>A personal dictionary — look up fast, understand fully, hear clearly, speak well, write right.</p>
  <p>Offline &amp; local-first · No ads · No forced login · AI and cloud services optional</p>
  <p><b>uni = universal</b>: more than English lookups — Chinese and other languages are equally goals (hence the "uni" in the name).</p>
  <p>Built on C++17: the core library has zero Qt dependencies (Qt is used only for desktop and the app shell).</p>
</div>

## What is this

Turn your StarDict / MDict / DSL / JSON / CSV dictionaries into **your own dictionary**:

- **Look up fast** — six search modes: exact, prefix, fuzzy, wildcard, regex, full-text + multi-dictionary aggregation with grouping and dedup
- **Understand fully** — definitions grouped and folded per dictionary, sanitized rendering, cross-references for examples/phrases/synonyms-antonyms
- **Hear clearly** — local offline TTS; optional online voices (US/UK/AU accents, off by default, only the query word is sent)
- **Speak well** — optional local pronunciation scoring (phoneme-level feedback; experimental, not installed by default)
- **Write right** — vocabulary notebook + tags + notes + pinned history, CSV export; backups are just files

Product principles and non-goals: [docs/product-principles.md](docs/product-principles.md);
architecture boundary rules: [docs/architecture-boundaries.md](docs/architecture-boundaries.md).

## Platform status

| Platform | Status |
|---|---|
| Windows / macOS / Linux desktop | nightly distribution (GUI + CLI) |
| Android | native shell delivered to M5 + M7 (dictionary import, lookup, notebook/history/notes, TTS, dark theme, APK packaging, share-into-lookup); release signing pending key |
| iOS / HarmonyOS | not started |

## Nightly builds

Every day at 05:17 (Beijing time) an automated build is published to the **rolling nightly Release** (fixed tag `nightly`, old artifacts replaced each run, anonymous download, no login needed):

- **Download: <https://github.com/cuihairu/unidict/releases/tag/nightly>** · mirror: [docs site download page](https://cuihairu.github.io/unidict/download)
- Packages include a `VERIFY.md` verification guide and per-platform `PLATFORM-NOTES.txt`

One-line install (auto-detects OS/arch, overwrite-install to upgrade, runs a smoke check after install):

```bash
# Linux / macOS (Apple Silicon)
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell
# Windows (PowerShell)
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

## UI preview

Desktop QML app (full light/dark eight-screen gallery in `docs/ui/` and on the [docs site](https://cuihairu.github.io/unidict/)):

| <img src="docs/ui/result-light.png?v=d2" width="400" alt="Lookup result · light"/><br><sub>Lookup result · aggregated definitions from multiple dictionaries, with read-aloud/favorite/copy</sub> | <img src="docs/ui/result-dark.png?v=d2" width="400" alt="Lookup result · dark"/><br><sub>Lookup result · dark</sub> |
| --- | --- |

> Screenshots reflect the current implementation ("implementation as prototype", D2 query command bar/header already landed); a UI redesign is in progress — result panel visual upgrade (D3) is queued, screenshots will be refreshed with that batch. The `?v=` query param only busts GitHub README image caching; the file paths themselves are unchanged.

## Quick start

```bash
# Look up real dictionaries out of the box: the repo ships CC-CEDICT zh-en (125k headwords, CC BY-SA 4.0)
UNIDICT_DICTS="dictionaries/ccedict-zh-en.json" build-std/Release/unidict_cli_std dictionary
UNIDICT_DICTS="dictionaries/ccedict-zh-en.json" build-std/Release/unidict_cli_std -m prefix -p dictiona

# Specify dictionaries and search modes (exact/prefix/fuzzy/wildcard/regex/fulltext;
# query word is a positional argument, fulltext also accepts --pattern)
unidict_cli_std -d dict.mdx -m prefix inter
unidict_cli_std -d dict.ifo -m fuzzy helo
unidict_cli_std -d dict.mdx -m fulltext --pattern "annual meeting"

# Dictionary management / encrypted MDict / index maintenance
unidict_cli_std --scan-dir ./dictionaries --list-dicts-verbose
unidict_cli_std -d encrypted.mdx --mdict-password <pw> hello
unidict_cli_std --index-save index.bin --index-load index.bin
```

See `unidict_cli_std --help` for all options. The CLI is positioned as a man-style pure lookup/diagnostic tool: vocabulary notebook, history, notes and other learning management live in the desktop GUI; the CLI offers no entry points and lookups are not written to history.

### More open-source dictionaries (one-command fetch)

Beyond the bundled CC-CEDICT, `scripts/fetch_sample_dicts.sh` downloads 5 open-source dictionaries locally (assets not committed): ECDICT en-zh ~3.4M entries (MIT), WikDict zh/en bidirectional (CC BY-SA 4.0, traditional-Chinese headwords), FreeDict eng-deu/eng-cmn (GPL-3.0). 6 dictionaries usable out of the box:

```bash
scripts/fetch_sample_dicts.sh                       # fetch all (~100MB)
UNIDICT_DICTS="$(scripts/fetch_sample_dicts.sh --print-env)" \
  build-std/Release/unidict_cli_std lobster         # joint lookup over bundled + fetched dictionaries
build-std/Release/unidict_cli_std --scan-dir dictionaries/downloaded -m prefix car
```

Attribution and licenses for each dictionary: `dictionaries/downloaded/ATTRIBUTION.md` (generated automatically on fetch).

Environment variables: `UNIDICT_DICTS` (dictionary list), `UNIDICT_DICT_DIR` (dictionary directory), `UNIDICT_DATA_DIR`/`UNIDICT_CACHE_DIR` (data and cache dirs), `UNIDICT_MDICT_PASSWORD` (default MDict password).

## Build & test

Requirements: CMake 3.16+, a C++17 compiler, zlib; Qt 6 (Core/Gui/Widgets; the QML app additionally needs Qml/Quick/QuickControls2/TextToSpeech).

```bash
# Full Qt build
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure

# std-only (no Qt, recommended for core development)
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

Pronunciation scoring (experimental, not built by default):

```bash
cmake -B build-pron -S . -DUNIDICT_BUILD_PRON=ON \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
# Models (~635MB) and vocab files are not in git, specify at runtime:
unidict_cli_std --pron-model model.onnx --pron-vocab vocab.json \
    --pron-phones "K AE T" --pron-score cat.wav
```

See [docs/pronunciation-plan.md](docs/pronunciation-plan.md).
Testing conventions: new tests in `core/` must not depend on Qt (`tests/test_<module>_std.cpp`, `<cassert>`+`main()`); Qt bridge-layer tests use Qt Test; tests are add-only; before committing, both build variants must pass ctest plus the coverage gate (`scripts/coverage.sh`).

## Repository layout

- `core/`: std-only core library (parsers, index engine, full-text search, storage, aggregation, cross-references, rendering) and legacy Qt core (being consolidated)
- `adapters/qt/`: Qt bridge (exposes the std core to Qt apps); `adapters/android/`: JNI glue; `adapters/pron/`: pronunciation-scoring inference shell
- `cli/`: legacy Qt diagnostic CLI; `cli-std/`: std-only primary CLI (deb/rpm)
- `gui/`: Qt Widgets desktop; `qmlui/`: QML desktop app
- `tests/`: dual-track Qt Test and std-only (cassert) tests
- `docs/`: documentation (see the map below)

## Documentation map

- Current-state baseline audit (2026-10-04): [CURRENT_ARCHITECTURE](docs/CURRENT_ARCHITECTURE.md) · [CURRENT_FEATURE_MATRIX](docs/CURRENT_FEATURE_MATRIX.md) · [TECH_DEBT](docs/TECH_DEBT.md)
- Product direction: [product-principles](docs/product-principles.md) · [architecture-boundaries](docs/architecture-boundaries.md) · [roadmap](docs/roadmap.md)
- Usage: [USER_GUIDE](docs/USER_GUIDE.md) · development: [dev](docs/dev.md) · design: [sync-engine](docs/design/sync-engine.md) · [text-normalization](docs/design/text-normalization.md)
- Online docs site: <https://cuihairu.github.io/unidict/>

## Privacy & data

- **Local-first**: lookups, notebook, history, notes all stay on your machine; the core lookup path makes no network requests.
- **The only outbound-by-default-optional traffic**: online pronunciation (off by default; when enabled only the query word is sent, clearly stated in the UI).
- **No ads, no forced login, no telemetry**; dictionary files are the user's own assets and no dictionary data is committed to the repo; AI and cloud services are both optional.

### Design notes on personal vocabulary sync (in design)

Cross-device sync of personal vocabulary (notebook/favorites/notes/preferences) uses **dynamic passphrase pairing + end-to-end encryption**:

- **No account needed**: one device generates a short-lived **dynamic passphrase**, the other enters it to join the sync group; all devices in the group have equal rights and may come and go freely, with the device list visible to all (each device can only edit its own remark).
- **End-to-end encryption**: the group-creating device generates the **group key** locally and never uploads it; new devices exchange the passphrase for the same group key via a PAKE protocol (the short code is not the key, preventing brute force and man-in-the-middle). All vocabulary command streams are encrypted before hitting the cloud — **the server only ever sees ciphertext and cannot read your vocabulary**.
- **Sync semantics**: all changes sync as commands (add/remove words, edit notes…); replaying them on multiple devices converges to the same state with no lost updates.
- **Cost and self-rescue**: there is no account recovery — **if you lose all devices, you lose the vocabulary**; use "export encrypted backup" to save the ciphertext locally or to cloud storage.
- **Off by default**: sync is disabled by default; enabling it clearly states the sync scope (which entries go to the cloud).

## Contributing

Repository discipline and workflow: [AGENTS.md](AGENTS.md) (build/test/commit conventions, Conventional Commits, quality gates).

## License

MIT, see [LICENSE](LICENSE).
