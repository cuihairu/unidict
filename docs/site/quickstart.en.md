> Language: **English** | [简体中文](./quickstart.md)

# Quick Start

Three steps: **install** ([download](/download) page one-click script or installer, two minutes) → **smoke verify** → look up your own words. For development and customization, go through [building from source](#build-from-source).

## Install

One-click install commands, installer direct links, and per-platform steps for every platform are collected on the [download](/download) page. The shortest path (Linux/macOS):

```bash
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

The desktop QML app runs on double-click, and the bundled sample dictionary loads automatically; once the CLI is installed, verify it with the smoke line below.

## Smoke Verification

::: code-group

```bash [Linux / macOS]
UNIDICT_DICTS=dict.json unidict_cli_std hello
# 预期输出:
# hello: A greeting or expression of goodwill.
```

```powershell [Windows]
set UNIDICT_DICTS=dict.json && unidict_cli_std.exe hello
```

:::

Install locations and per-platform differences are on the [download](/download) page.

## Build from Source

Requires CMake 3.16+, a C++17 compiler, and zlib. For core development the std-only build is recommended (no Qt dependency, compiles fast):

```bash
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

The full Qt build (desktop QML app, Widgets demo, Qt tests) requires Qt 6.10+:

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Next Steps

- Use your own dictionaries: [Dictionary Management and Import Formats](/dictionaries)
- Look up words in every way: [Looking Up Words](/search)
- Keep the words you have looked up: [Vocabulary Book](/vocabulary)
