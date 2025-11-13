# UniDict Core - Quick Reference Guide

## Quick Stats

| Metric | Value |
|--------|-------|
| Total Core Files | 26 (9 Qt + 17 Std) |
| Total Core LOC | ~6,000 LOC |
| Std Core LOC | ~2,000 LOC |
| Qt Wrappers LOC | ~4,000 LOC |
| Test Files | 26 |
| Supported Dictionary Formats | 5 (JSON, StarDict, DSL, CSV, MDict*) |
| Search Types | 5 (Exact, Prefix, Fuzzy, Wildcard, Regex) |
| Completeness | 95% (MDict partial) |

## Key Files at a Glance

### Core Interfaces (Qt-based)
- `/core/unidict_core.h` - Main DictionaryManager singleton
- `/core/index_engine.h` - Index-backed search wrapper
- `/core/data_store.h` - History and vocabulary persistence
- `/core/lookup_service.h` - Search service wrapper

### Std Implementations
- `/core/std/dictionary_manager_std.h` - Multi-format orchestration
- `/core/std/index_engine_std.h` - Trie + HashMap hybrid
- `/core/std/data_store_std.h` - JSON persistence
- `/core/std/*_parser_std.h` - Format-specific parsers

## Common Usage Patterns

### Load a Dictionary
```cpp
#include "dictionary_manager_std.h"

UnidictCoreStd::DictionaryManagerStd mgr;
mgr.add_dictionary("/path/to/dict.ifo");  // StarDict
mgr.add_dictionary("/path/to/dict.mdx");  // MDict
mgr.add_dictionary("/path/to/dict.json"); // JSON
mgr.add_dictionary("/path/to/dict.dsl");  // DSL
mgr.add_dictionary("/path/to/dict.csv");  // CSV/TSV

// Build index for fast searching
mgr.build_index();
```

### Perform Searches
```cpp
// Exact match
auto results = mgr.exact_search("hello");

// Prefix search
auto prefixed = mgr.prefix_search("hel", 10);

// Fuzzy search (Levenshtein distance)
auto fuzzy = mgr.fuzzy_search("hello", 10);

// Wildcard search
auto wildcard = mgr.wildcard_search("h*lo", 10);

// Regex search
auto regex = mgr.regex_search("h.*o", 10);

// Simple word lookup
std::string def = mgr.search_word("hello");
```

### Manage Data Store
```cpp
#include "data_store_std.h"

UnidictCoreStd::DataStoreStd store;
store.set_storage_path("./data/unidict.json");

// Search history
store.add_search_history("hello");
auto history = store.get_search_history(100);
store.clear_history();

// Vocabulary
UnidictCoreStd::VocabItemStd item{"hello", "greeting"};
store.add_vocabulary_item(item);
auto vocab = store.get_vocabulary();
store.export_vocabulary_csv("./export.csv");

// Persist
store.save();
store.load();
```

### Cache Management
```cpp
#include "path_utils_std.h"

namespace pu = UnidictCoreStd::PathUtilsStd;

std::string data_dir = pu::data_dir();    // ./data or UNIDICT_DATA_DIR
std::string cache_dir = pu::cache_dir();  // ./data/cache or UNIDICT_CACHE_DIR

pu::ensure_dir(data_dir);
uint64_t size = pu::cache_size_bytes();
pu::prune_cache_bytes(100 * 1024 * 1024);    // Keep under 100MB
pu::prune_cache_older_than_days(30);         // Remove files > 30 days old
```

### Index Persistence
```cpp
mgr.build_index();
mgr.save_index("./data/index.txt");

// Later...
UnidictCoreStd::DictionaryManagerStd mgr2;
mgr2.load_index("./data/index.txt");
auto results = mgr2.prefix_search("hel", 10);  // Instant!
```

## Architecture Overview

### Dual-Layer Design
```
┌─────────────────────────────────────┐
│     Qt-based Interface Layer        │
│  (DictionaryManager, IndexEngine)   │
└────────────┬────────────────────────┘
             │ (Qt Adapters)
             v
┌─────────────────────────────────────┐
│     Std-only Core Layer (C++17)     │
│  (DictionaryManagerStd, etc.)       │
└─────────────────────────────────────┘
```

### Parser Dispatch
```
DictionaryManagerStd::add_dictionary(path)
  |
  +-> Auto-detect by extension
  |   - .json -> JsonParserStd
  |   - .ifo  -> StarDictParserStd
  |   - .mdx  -> MdictParserStd
  |   - .dsl  -> DslParserStd
  |   - .csv/.tsv/.txt -> CsvParserStd
  |
  +-> Load dictionary
  |
  +-> Add words to IndexEngineStd
  |
  +-> Return success/failure
```

## Format Support Matrix

| Format | Extension | Status | Notes |
|--------|-----------|--------|-------|
| JSON | .json | ✅ Complete | Custom format |
| StarDict | .ifo/.idx/.dict | ✅ Complete | No compression |
| DSL | .dsl | ✅ Complete | ABBYY Lingvo |
| CSV | .csv/.tsv/.txt | ✅ Complete | Auto-detects separator |
| MDict | .mdx | ⚠️ Partial | SIMPLEKV only |

## Search Algorithm Complexity

| Search Type | Complexity | Data Structure | Notes |
|-------------|-----------|-----------------|-------|
| Exact | O(log n) | HashMap | Normalized lookup |
| Prefix | O(k + m) | Trie | k=prefix len, m=results |
| Fuzzy | O(n * m) | HashMap | n=word len, m=dict size |
| Wildcard | O(n * d) | HashMap | n=word len, d=dict size |
| Regex | O(n * d) | HashMap | n=word len, d=dict size |

## Component Dependencies

```
DictionaryManagerStd
  ├── IndexEngineStd (required)
  ├── JsonParserStd (for .json)
  ├── StarDictParserStd (for .ifo)
  ├── MdictParserStd (for .mdx)
  ├── DslParserStd (for .dsl)
  └── CsvParserStd (for .csv/.tsv)

IndexEngineStd
  ├── Trie (internal)
  └── HashMap (internal)

DataStoreStd
  ├── JSON parser (tolerant, built-in)
  └── <filesystem> (C++17)

All Parsers
  └── ZLIB (for decompression)
```

## Build & Link

### CMake
```cmake
# Link to std-only core
target_link_libraries(myapp PRIVATE unidict_std_core)

# Or link to Qt-wrapped core
if(UNIDICT_BUILD_QT_CORE)
  target_link_libraries(myapp PRIVATE unidict_core)
endif()

# Both need:
find_package(ZLIB REQUIRED)
```

### C++ Standard
- Requires C++17 minimum
- Uses `<filesystem>`, `<unordered_map>`, `<regex>`
- No Qt required for std layer

## Environment Variables

```bash
# Override default data directory (default: ./data)
export UNIDICT_DATA_DIR=/custom/path

# Override default cache directory (default: <UNIDICT_DATA_DIR>/cache)
export UNIDICT_CACHE_DIR=/custom/cache/path
```

## Performance Tips

1. **Index Building**: Call `build_index()` once after loading dictionaries
2. **Index Persistence**: Save and load index to avoid rebuilding
3. **Search Methods**:
   - Use exact_search for known words
   - Use prefix_search for typeahead
   - Limit fuzzy_search results to avoid slowdowns
4. **Large Dictionaries**: Consider loading index from cache
5. **Multiple Dictionaries**: Build unified index once

## Testing

Run all tests:
```bash
cd build
ctest
```

Run specific test:
```bash
./build/tests/test_index_engine_std
./build/tests/test_stardict_std
./build/tests/test_data_store_std
```

## Troubleshooting

### Dictionary not loading
- Check file exists and is readable
- Verify format matches extension
- Check UNIDICT_DATA_DIR permissions

### Search returns empty
- Call `build_index()` after adding dictionaries
- Verify dictionary loaded successfully with `loaded_dictionaries()`
- Check word case sensitivity (searches are normalized)

### Memory issues with large dictionaries
- Use `prune_cache_bytes()` to limit cache
- Load index from cache instead of rebuilding
- Consider indexed search over literal parsing

## Known Limitations

1. **MDict**: Only SIMPLEKV format fully supported
2. **StarDict**: No .dict.dz (gzip) compression support
3. **Encoding**: Assumes UTF-8 throughout
4. **Memory**: No memory-mapped file support in std layer
5. **Internationalization**: Limited to ASCII/UTF-8

## Recommended Next Steps

1. Complete MDict format support (KIDX, KBIX, KEYB)
2. Add StarDict compression support (.dict.dz)
3. Add memory-mapped file support
4. Add more dictionary formats (EPUB, Kobo, Kindle)
5. Performance optimization and benchmarking

