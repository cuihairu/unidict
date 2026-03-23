# Unidict

Open-source offline dictionary workbench built with C++ and Qt.

## Current Scope

This repository is being rebuilt into a usable dictionary MVP.

- Stable offline lookup is the current priority.
- `StarDict` is supported.
- `MDict` has MVP support for unencrypted `.mdx` dictionaries using zlib-compressed text records.
- GUI and CLI both support dictionary import and lookup.

## Current Features

- Import a `StarDict` or supported `MDict` dictionary by file or by scanning a folder
- Exact lookup with near-match suggestions
- Multi-dictionary loading
- Persistent dictionary workspace state, including order and enabled status
- Dictionary detail inspection in the GUI, including format, path, priority, and load metadata
- Dictionary filtering in the GUI by text and enabled state
- Dictionary tags in the GUI for organizing large local collections
- Search history with pin/unpin support, single-item removal, filtering, import/export, and persistence
- Workspace panel in the GUI with state-file visibility and manual save/reload actions
- Desktop GUI for dictionary management and lookup
- CLI mode for scripting and terminal use

## Directory Layout

- `core/`: dictionary parsers and lookup services
- `gui/`: Qt Widgets desktop client
- `cli/`: command line client

## Build

Requirements:

- CMake 3.16+
- Qt 6 with `Core`, `Gui`, and `Widgets`
- C++20 compiler
- zlib

Configure:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
```

Build:

```bash
cmake --build build
```

Test:

```bash
ctest --test-dir build --output-on-failure
```

## Usage

CLI:

```bash
unidict_cli --dict path/to/dictionary.ifo hello
unidict_cli --dict path/to/dictionary.mdx hello
unidict_cli --dict-dir path/to/dictionaries hello
unidict_cli --list
unidict_cli --history
unidict_cli --save-state path/to/workspace.json
unidict_cli --load-state path/to/workspace.json
unidict_cli --export-history path/to/history.json
unidict_cli --import-history path/to/history.json --replace-history
unidict_cli --clear-history
```

GUI:

- Launch `unidict_gui`
- Import a `.ifo` or supported `.mdx` file, or a folder that contains dictionaries
- Search from the main input box

You can also place dictionaries under a `dictionaries/` folder next to the executable, or set `UNIDICT_DICT_DIR`.
Loaded dictionaries, their order, and enabled state are restored automatically on next launch.

## License

MIT. See [LICENSE](LICENSE).
