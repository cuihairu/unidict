# Unidict 📖

**A universal, open-source, and AI-powered dictionary and language workbench.**

Unidict is not just another dictionary app. It's an ambitious project to build the ultimate tool for language learners, linguists, and knowledge workers. It combines the power of traditional offline dictionaries with modern AI assistance and a flexible, open architecture.

## ✨ Core Philosophy

Our development is guided by four key principles:

1.  **Performance is Paramount**: Dictionary lookups, especially in massive local files, must be instantaneous. The application is built with C++ and Qt to deliver native speed.
2.  **Truly Cross-Platform**: Unidict is designed from the ground up to work seamlessly across **Windows, macOS, Linux, Android, and iOS**, with a single, unified codebase.
3.  **Open and Extensible**: The project is open-source (MIT License). A powerful plugin system will allow the community to extend its capabilities, from supporting new dictionary formats to creating unique learning tools.
4.  **AI-Powered**: Unidict integrates modern Large Language Models (LLMs) to go beyond simple definitions, offering intelligent translation, grammar correction, contextual example sentences, and more.

## 🚀 Key Features

- **Broad Dictionary Support**: Works with your existing dictionary files, including popular formats like **MDict (.mdx/.mdd)** and **StarDict** (including `.dict.dz`).
- **Powerful Search**: Offers everything from simple lookups to full-text, wildcard, and regex searches across all your dictionaries.
- **Integrated Learning Tools**: Features a built-in vocabulary book with an Anki-style spaced repetition system (SRS) to help you remember what you learn.
- **AI Assistant**: Provides smart translation, writing assistance, and contextual understanding that static dictionaries can't offer.
- **Cross-Platform Sync**: Seamlessly syncs your vocabulary, notes, and settings across all your devices.
- **Terminal Mode**: Look up words directly from your command-line interface for maximum speed and efficiency.

## 🛠️ Tech Stack

- **Core (std-only)**: C++20 + STL + zlib (no Qt in `core/`)
- **Adapters (Qt)**: thin bridges in `adapters/qt/` expose core to Qt apps
- **UI**: Qt Widgets + QML/Quick
- **Build**: CMake, GitHub Actions

## 🧪 Try It (CLI)

You can run the CLI after building. Point it at your dictionary files or use an env var (the GUI also reads this env var at startup):

```
# load dictionaries via flags
unidict_cli -d /path/to/dict.mdx -d /path/to/stardict.ifo hello

# or via environment variable (use ':' or ';' as separator)
UNIDICT_DICTS="/path/a.mdx:/path/b.ifo" unidict_cli --mode prefix inter
```

Supported modes: `exact` (default), `prefix`, `fuzzy`, `wildcard`, `regex`, `fulltext` (inverted index + TF/IDF; supports persistence and version negotiation).

Note: The MDict parser is being migrated to a real, Qt‑free implementation. It already supports multiple block‑based prototypes (simulating MDX KeyBlock/RecordBlock layouts, unencrypted + zlib) and will prefer real layouts when detected. StarDict std parser supports .ifo/.idx/.dict (and `.dict.dz`). For quick testing, a JSON format is supported too:

```
unidict_cli -d examples/dict.json hello
```

Extra CLI utilities:

```
# List loaded dictionaries
unidict_cli -d examples/dict.json --list-dicts
unidict_cli -d examples/dict.json --list-dicts-verbose

# Show recent history (last 20 by default)
unidict_cli --history 50

# Save exact-match result to vocabulary and display it
unidict_cli -d examples/dict.json --save hello
unidict_cli --show-vocab

# Scan a folder for dictionaries (recursively)
unidict_cli --scan-dir /path/to/dicts --list-dicts

# Index persistence (speed up next run on large dicts)
unidict_cli -d /path/to/xxx.ifo --index-save my.index
unidict_cli --index-load my.index --mode prefix inter

# Cache maintenance / Export
unidict_cli --clear-cache
unidict_cli --export-vocab vocab.csv
unidict_cli --cache-prune-mb 500
unidict_cli --cache-prune-days 30
unidict_cli --cache-size
unidict_cli --cache-dir
unidict_cli --data-dir

# Index utilities
unidict_cli --dump-words 50
unidict_cli --drop-dict "Some Dictionary Name" --list-dicts
unidict_cli --list-plugins
unidict_cli --index-count
```

Std-only CLI (no Qt required):

```
# Build the std CLI
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF -DUNIDICT_BUILD_STD_CLI=ON
cmake --build build-std -j

# Run (same flags as Qt CLI)
UNIDICT_DICTS="examples/dict.json" ./build-std/cli-std/unidict_cli_std --mode prefix he
./build-std/cli-std/unidict_cli_std --list-dicts-verbose
./build-std/cli-std/unidict_cli_std --cache-size
./build-std/cli-std/unidict_cli_std --cache-dir
./build-std/cli-std/unidict_cli_std --data-dir
./build-std/cli-std/unidict_cli_std --mdx-debug /path/to/file.mdx   # inspect header/containers/zlib headers

# Full-text index persistence & compatibility
# Save signed UDFT3 full-text index (compressed postings + signature bound to current dictionaries)
./build-std/cli-std/unidict_cli_std --mode fulltext greet --fulltext-index-save ft.index
# Load with compatibility mode (strict|auto|loose; default auto)
./build-std/cli-std/unidict_cli_std --mode fulltext greet --fulltext-index-load ft.index --ft-index-compat auto
# Upgrade legacy UDFT1 (no signature) to UDFT3 (compressed with signature)
./build-std/cli-std/unidict_cli_std --ft-index-upgrade-in old.index --ft-index-upgrade-out new.index -d examples/dict.json
# Batch upgrade directory (recursive); only upgrades legacy v1 files; supports dry-run, ext filter, force overwrite, and separate out-dir (mirror structure)
./build-std/cli-std/unidict_cli_std --ft-index-upgrade-dir ./my-indexes \
  --ft-index-upgrade-suffix .v2 --ft-index-dry-run \
  --ft-index-filter-ext .index,.idx --ft-index-force \
  --ft-index-out-dir ./converted-indexes \
  -d examples/dict.json
```

## 🖼️ Try It (QML UI)

A minimal Qt Quick UI is available. It reads dictionaries from the `UNIDICT_DICTS` env var at startup:

```
UNIDICT_DICTS="examples/dict.json" ./build/qmlui/unidict_qml
```

Features:
- Type to see prefix suggestions, press Enter or click Search to view definition
- Save to vocabulary via the button
- Shows loaded dictionaries at the top

## 🧩 Plugin Architecture (MVP)

Parsers are registered via a lightweight plugin manager that maps file extensions to parser factories. Built-ins include StarDict (`ifo/idx/dict/dz`), MDict (`mdx/mdd` - skeleton), and JSON (`json`).

Architecture note: the registration happens in the Qt adapter layer and delegates to std-only implementations, keeping `core/` free of Qt.

Build flags (default ON):

```
-DUNIDICT_BUILD_ADAPTER_QT=ON      # Qt adapters
-DUNIDICT_BUILD_QT_APPS=ON         # GUI/QML/CLI
-DUNIDICT_BUILD_QT_CORE=ON         # legacy Qt core target (compat)
-DUNIDICT_BUILD_QT_TESTS=ON        # Qt-based tests
```

Std-only build & tests (no Qt required):

```
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF \
  -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF \
  -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

## 🗺️ Development Plan

The detailed development plan, including the feature roadmap and MVP goals, can be found in [docs/roadmap.md](docs/roadmap.md).

## 🤝 Contributing

This is a community-driven project, and contributions are highly welcome! Whether you're a C++/Qt developer, a UI/UX designer, a linguist, or just an enthusiastic user, there are many ways to help.

Please check back soon for a `CONTRIBUTING.md` file with detailed guidelines.

## 📄 License

Unidict is licensed under the [MIT License](LICENSE).
