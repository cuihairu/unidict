# Repository Guidelines

## Project Structure & Module Organization
- `core/`: pure C++20 library (no Qt). Parsers, index engine, data store, services.
- `adapters/qt/`: Qt bridge wrappers (e.g., `IndexEngineQt`) for UI/apps.
- `cli/`: command-line app using adapters to call the core.
- `gui/`: QWidget demo; `qmlui/`: Qt Quick UI.
- `tests/`: unit tests (Qt Test today and std-only tests via CTest).
- `examples/`, `docs/`, `ARCHITECTURE.md`; build artifacts under `build/`.

## Build, Test, and Development Commands
- Configure and build:
  ```bash
  cmake -B build -S . [-DCMAKE_PREFIX_PATH=/path/to/Qt]
  cmake --build build -j
  ```
- Run tests:
  ```bash
  ctest --test-dir build --output-on-failure
  ctest --test-dir build -R test_index_engine
  ```
- Std-only build/tests (no Qt):
  ```bash
  cmake -B build-std -S . \
    -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
    -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
  cmake --build build-std -j
  ctest --test-dir build-std -R _std --output-on-failure
  ```
- Run apps:
  ```bash
  UNIDICT_DICTS="examples/dict.json" build/cli/unidict_cli hello
  UNIDICT_DICTS="examples/dict.json" build/qmlui/unidict_qml
  build/gui/unidict_gui
  ```

## Coding Style & Naming Conventions
- Core: 4-space indent, UTF-8, braces on same line. Use STL types only (`std::string` UTF-8, `std::vector`, `std::unordered_map/set`). Do not include Qt in `core/`.
- UI/adapters: Qt is allowed in `cli/`, `gui/`, `qmlui/`. Bridge types (e.g., `QString to_qt(const std::string&)`, `std::string from_qt(const QString&)`).
- Naming: `PascalCase` classes, `lowerCamelCase` methods/vars, `snake_case` files.
- Keep headers lean; avoid globals; use existing singletons sparingly (`DataStore`).

## Testing Guidelines
- Current: Qt Test (`QTEST_MAIN`) in `tests/` as `<unit>_test.cpp`.
- Direction: new tests for `core/` should not use Qt (e.g., Catch2 or GTest). Keep `ctest` passing locally and in CI.

## Commit & Pull Request Guidelines
- Conventional Commits: `feat:`, `fix:`, `docs:`, `test:`, `chore:`, `refactor:` (e.g., `feat(core): add stardict index`).
- PRs: clear description, link issues, call out behavior changes; include CLI examples and screenshots/gifs for UI changes; update README/ARCHITECTURE when needed; CI must be green.

## Architecture Notes (Core Without Qt)
- Core must build without Qt; only the apps/adapters may depend on Qt. Prefer STL, zlib, and small helpers in `core/`.
- Move Qt-only utilities (JSON, regex, file dialogs) to adapters. If a feature needs Qt, add a thin adapter instead of pulling Qt into `core/`.
- Do not commit dictionary assets; use `UNIDICT_DICTS` to load local paths.
