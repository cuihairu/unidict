# UniDict Core Architecture Analysis

## Executive Summary
The UniDict core is a sophisticated dictionary framework that exists in two parallel implementations:
1. **Qt-based interfaces** in `/core/*.h` and `/core/*.cpp` - Full-featured Qt wrappers
2. **Qt-free std implementations** in `/core/std/` - Pure C++17 standard library implementations

The architecture follows a **dual-path design pattern** where the Qt layer gradually delegates to std-only backends, enabling Qt-independence while maintaining backward compatibility.

---

## Directory Structure

```
core/
├── types.h                          # Core type aliases for std-only modules
├── unidict_core.h/cpp               # Main DictionaryManager interface (Qt-based)
├── index_engine.h/cpp               # Qt wrapper for prefix/fuzzy/wildcard search
├── data_store.h/cpp                 # Qt wrapper for history/vocabulary (JSON-backed)
├── lookup_service.h/cpp             # Search service layer
├── path_utils.h/cpp                 # Path and cache management
├── plugin_manager.h/cpp             # Dynamic parser factory registration
├── json_parser.h/cpp                # Qt-based JSON dictionary parser
├── stardict_parser.h/cpp            # Qt-based StarDict parser (ifo/idx/dict)
├── mdict_parser.h/cpp               # Qt-based MDict parser (mdx/mdd)
│
└── std/                             # Pure C++17 standard library implementations
    ├── index_engine_std.h/cpp       # Trie-based indexing (prefix/fuzzy/wildcard/regex)
    ├── data_store_std.h/cpp         # JSON persistence for history/vocabulary
    ├── dictionary_manager_std.h/cpp # Dictionary loading and search orchestration
    ├── path_utils_std.h/cpp         # Path utilities and cache pruning
    ├── json_parser_std.h/cpp        # JSON dictionary loader (102 LOC)
    ├── stardict_parser_std.h/cpp    # StarDict loader (190 LOC)
    ├── mdict_parser_std.h/cpp       # MDict loader (567 LOC - most complete)
    ├── dsl_parser_std.h/cpp         # DSL/Lingvo parser (291 LOC)
    └── csv_parser_std.h/cpp         # CSV/TSV parser (163 LOC)
```

---

## Component Analysis

### 1. Core Types Module
**File**: `types.h`  
**Status**: COMPLETE  
**Purpose**: Provides Qt-free type aliases for std-only modules

**Key Features**:
- `String = std::string` (UTF-8 by convention)
- Template aliases: `Vector<T>`, `UnorderedMap<K,V>`, `UnorderedSet<T>`
- Minimal header - designed not to pull Qt

**Dependencies**: None (std C++ only)

---

### 2. Index Engine (Dual Implementation)

#### Qt-based (Compatibility Wrapper)
**File**: `index_engine.h/cpp`  
**Status**: COMPLETE  
**Size**: 1,304 LOC (header)

**Interface**:
```cpp
class IndexEngine {
    void addWord(const QString& word, const QString& dictionaryId);
    void removeWord(const QString& word, const QString& dictionaryId);
    void clearDictionary(const QString& dictionaryId);
    
    QStringList exactMatch(const QString& word) const;
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    QStringList regexSearch(const QString& pattern, int maxResults = 10) const;
    
    QStringList getAllWords() const;
    int getWordCount() const;
    QStringList getDictionariesForWord(const QString& word) const;
    
    void buildIndex();
    void saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);
};
```

**Implementation Strategy**:
- Delegates to `UnidictAdaptersQt::IndexEngineQt` via Qt adapter
- Acts as a compatibility wrapper preserving old Qt-facing API

---

#### Std-only Implementation
**File**: `std/index_engine_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 240 LOC

**Architecture**:
- **Trie structure** for prefix matching
- **HashMap** for word normalization and lookup
- **Per-dictionary tracking** of which words belong to which dictionaries

**Key Data Structures**:
```cpp
struct IndexEntry {
    std::string word;
    std::string normalized_word;
    std::vector<std::string> dictionary_ids;  // Track multiple dictionaries
    int frequency = 0;
};

class TrieNode {
    std::unordered_map<char, std::unique_ptr<TrieNode>> children;
    std::unordered_set<std::string> words;  // Full words at this node
};

class IndexEngineStd {
    std::unique_ptr<TrieNode> trie_;
    std::unordered_map<std::string, IndexEntry> word_index_;
    std::unordered_map<std::string, std::unordered_set<std::string>> dict_;
    bool built_ = false;
};
```

**Search Algorithms**:
- **Exact match**: O(log n) hash table lookup with normalization
- **Prefix search**: O(k) trie traversal where k = prefix length
- **Fuzzy search**: Levenshtein distance (edit distance)
- **Wildcard search**: Pattern matching with `*` and `?`
- **Regex search**: Full regex via `<regex>` library

**Persistence**:
- `save_index()`: Simple line-based text format
- `load_index()`: Reconstructs trie from saved index

**Completeness**: ✅ Fully implemented with comprehensive search support

---

### 3. Data Store (Dual Implementation)

#### Qt-based
**File**: `data_store.h/cpp`  
**Status**: COMPLETE  
**Size**: 1,028 LOC (header)

**Features**:
- Search history tracking (limited to N entries)
- Vocabulary management (word + definition)
- CSV export of vocabulary
- Persistent JSON storage

**Interface**:
```cpp
class DataStore {
    void setStoragePath(const QString& filePath);
    
    void addSearchHistory(const QString& word);
    QStringList getSearchHistory(int limit = 100) const;
    void clearHistory();
    
    void addVocabularyItem(const DictionaryEntry& entry);
    QList<DictionaryEntry> getVocabulary() const;
    void clearVocabulary();
    bool exportVocabularyCSV(const QString& filePath) const;
    
    bool load();
    bool save() const;
};
```

---

#### Std-only Implementation
**File**: `std/data_store_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 223 LOC

**Features**:
- Simple JSON persistence without Qt
- Minimal custom JSON parser (tolerant)
- Same API as Qt version but returns `std::string` and `std::vector`

**Tolerant JSON Parser**:
- Handles malformed JSON gracefully
- Section-based parsing: finds "history" and "vocab" sections
- Unquotes strings properly
- Handles escape sequences

**Storage Format**:
```json
{
    "history": ["word1", "word2", ...],
    "vocab": [
        {"word": "...", "definition": "..."},
        ...
    ]
}
```

**Completeness**: ✅ Fully implemented

---

### 4. Dictionary Parsers (Std Implementations)

#### JSON Parser Std
**File**: `std/json_parser_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 102 LOC

**Supported Format**:
```json
{
    "name": "My Dictionary",
    "description": "Optional",
    "entries": [
        {"word": "hello", "definition": "..."},
        ...
    ]
}
```

**Interface**:
```cpp
bool load_dictionary(const std::string& file_path);
std::string lookup(const std::string& word) const;
std::vector<std::string> find_similar(const std::string& word, int max_results) const;
std::vector<std::string> all_words() const;
```

**Completeness**: ✅ Fully implemented

---

#### StarDict Parser Std
**File**: `std/stardict_parser_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 190 LOC

**Supported Format**: `.ifo` / `.idx` / `.dict` files (no compression support yet)

**Features**:
- Parses `.ifo` header files
- Reads binary `.idx` index with word positions
- Accesses `.dict` file for definitions
- Handles both 32-bit and 64-bit offset modes

**Header Structure**:
```cpp
struct StarDictHeaderStd {
    std::string version;
    std::string book_name;
    int word_count = 0;
    int index_file_size = 0;
    int idx_offset_bits = 32;  // 32 or 64
    std::string description;
};
```

**Index Format** (binary):
- Null-terminated word string
- Big-endian 32/64-bit offset into .dict file
- Big-endian 32-bit definition size

**Completeness**: ✅ Fully implemented (uncompressed files)

---

#### MDict Parser Std
**File**: `std/mdict_parser_std.h/cpp`  
**Status**: PARTIALLY COMPLETE  
**Size**: 567 LOC (largest parser)

**Supported Formats**: 
1. **SIMPLEKV** - Simple key-value format (fully implemented)
2. **KIDX/RDEF** - Key index + record definition (partially implemented)
3. **KBIX/KBIX2** - Key block index (stub)
4. **KEYB/RDEF** - Key block + record definition (stub)

**Features**:
- Handles UTF-16 headers with BOM detection
- Zlib decompression for KIDX/RDEF records
- Big-endian binary parsing utilities
- Multiple heuristic parsing attempts

**Format Examples**:

**SIMPLEKV**:
```
Magic: "SIMPLEKV"
Count: u32 (BE)
Entries: repeated {
    WordLen: u16 (BE)
    Word: bytes
    DefLen: u32 (BE)
    Definition: bytes
}
```

**Implementation Status by Format**:
- SIMPLEKV: ✅ Full support
- KIDX/RDEF: ⚠️ Partial support (decompression exists, record parsing incomplete)
- KBIX variants: 🚫 Stubs only
- KEYB/RDEF: 🚫 Stubs only

**Completeness**: ⚠️ 40-50% (SIMPLEKV works, others stubbed)

---

#### DSL Parser Std
**File**: `std/dsl_parser_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 291 LOC

**Format**: ABBYY Lingvo DSL text-based format

**Features**:
- Header parsing (lines starting with `#`)
- BOM handling (UTF-8 BOM removal)
- Entry parsing (headword + definition pairs)
- Markup cleanup (removes DSL formatting)

**File Structure**:
```
#NAME=Dictionary Name
#DESCRIPTION=...
#ENCODING=UTF-8
#SOURCEENTRIES=en
#TARGETENTRIES=en

headword
definition text
multiple lines allowed
    indented lines too

next_headword
its definition
```

**Entry Parsing**:
- Detects entry boundaries (non-indented lines become headwords)
- Accumulates definition lines (indented or continuation)
- Cleans markup (removes DSL-specific formatting)

**Completeness**: ✅ Fully implemented

---

#### CSV Parser Std
**File**: `std/csv_parser_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 163 LOC

**Features**:
- Auto-detects separator (tab, comma, semicolon)
- Handles quoted fields
- Skips comment lines (starting with `#` or `;`)
- Sets name from filename

**Format**:
```
word1<sep>definition1
word2<sep>definition2
# comment line
word3<sep>definition3
```

**Separator Detection Priority**: Tab > Comma > Semicolon

**Completeness**: ✅ Fully implemented

---

### 5. Dictionary Manager (Dual Implementation)

#### Qt-based
**File**: `unidict_core.h/cpp`  
**Status**: COMPLETE  
**Size**: 2,637 LOC (header)

**Features**:
- Loads multiple dictionaries from files
- Auto-detects format by extension
- Delegates to parser plugins
- Maintains global index
- Provides search across all loaded dictionaries

**Interface**:
```cpp
class DictionaryManager {
    bool addDictionary(const QString& filePath);
    bool removeDictionary(const QString& dictionaryId);
    QStringList getLoadedDictionaries() const;
    
    DictionaryEntry searchWord(const QString& word) const;
    QStringList searchSimilar(const QString& word, int maxResults = 10) const;
    std::vector<DictionaryEntry> searchAll(const QString& word) const;
    
    void buildIndex();
    QStringList prefixSearch(const QString& prefix, int maxResults = 10) const;
    QStringList fuzzySearch(const QString& word, int maxResults = 10) const;
    QStringList wildcardSearch(const QString& pattern, int maxResults = 10) const;
    QStringList regexSearch(const QString& pattern, int maxResults = 10) const;
    
    bool saveIndex(const QString& filePath) const;
    bool loadIndex(const QString& filePath);
};
```

---

#### Std-only Implementation
**File**: `std/dictionary_manager_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 116 LOC (header), 116 LOC (impl)

**Architecture**:
```cpp
class DictionaryManagerStd {
    struct Holder {
        std::shared_ptr<JsonParserStd> json;
        std::shared_ptr<StarDictParserStd> stardict;
        std::shared_ptr<MdictParserStd> mdict;
        std::shared_ptr<DslParserStd> dsl;
        std::shared_ptr<CsvParserStd> csv;
        std::string name;
        std::vector<std::string> words;
        std::string lookup(const std::string& w) const;
    };
    
    std::vector<Holder> dicts_;
    IndexEngineStd index_;
};
```

**Strategy**:
- One-of union pattern: each `Holder` has only one non-null parser
- Lazy union pattern: lookup() dispatches to active parser
- Maintains word lists for quick index building
- Full indexed search support via `IndexEngineStd`

**Features**:
- Auto-detect by extension (.json, .ifo, .mdx, .dsl, .csv/.tsv/.txt)
- Dictionary metadata (name, word count, description)
- Single-word search and multi-dictionary search
- All index-backed searches (exact, prefix, fuzzy, wildcard, regex)
- Index persistence

**Completeness**: ✅ Fully implemented

---

### 6. Lookup Service
**File**: `lookup_service.h/cpp`  
**Status**: COMPLETE (lightweight wrapper)  
**Size**: 693 LOC (header)

**Interface**:
```cpp
class LookupService {
    QString lookupDefinition(const QString& word, bool allowSuggest = true, int suggestMax = 10) const;
    QStringList suggestPrefix(const QString& prefix, int maxResults = 10) const;
    QStringList suggestFuzzy(const QString& word, int maxResults = 10) const;
    QStringList searchWildcard(const QString& pattern, int maxResults = 10) const;
};
```

**Purpose**: Encapsulates lookup strategy and fallback suggestions

**Completeness**: ✅ Fully implemented

---

### 7. Path Utilities (Dual Implementation)

#### Qt-based
**File**: `path_utils.h/cpp`  
**Size**: 1,018 LOC (header)

**Features**:
- `dataDir()` - Returns data directory (default: ./data, env: UNIDICT_DATA_DIR)
- `cacheDir()` - Returns cache directory (default: <dataDir>/cache)
- `ensureDir()` - Create directory recursively
- `clearCache()` - Remove all cache files
- `cacheSizeBytes()` - Calculate total cache size
- `pruneCacheBytes()` - Keep cache under size limit
- `pruneCacheOlderThanDays()` - Remove old files by age

---

#### Std-only Implementation
**File**: `std/path_utils_std.h/cpp`  
**Status**: COMPLETE  
**Size**: 100 LOC

**Same interface as Qt version**:
```cpp
namespace PathUtilsStd {
    std::string data_dir();
    std::string cache_dir();
    bool ensure_dir(const std::string& dir_path);
    bool clear_cache();
    std::uint64_t cache_size_bytes();
    bool prune_cache_bytes(std::uint64_t max_bytes);
    bool prune_cache_older_than_days(int days);
}
```

**Implementation**:
- Uses `<filesystem>` (C++17) for path operations
- Environment variable overrides work identically
- Recursive directory creation with proper error handling

**Completeness**: ✅ Fully implemented

---

### 8. Plugin Manager
**File**: `plugin_manager.h/cpp`  
**Status**: COMPLETE  
**Size**: 782 LOC (header)

**Purpose**: Dynamic factory registration for parsers

**Interface**:
```cpp
class PluginManager {
    using FactoryFn = std::function<std::unique_ptr<DictionaryParser>()>;
    
    void registerFactory(const QStringList& extensions, FactoryFn factory);
    std::vector<FactoryFn> factoriesForExtension(const QString& ext) const;
    std::vector<std::unique_ptr<DictionaryParser>> createCandidatesForFile(const QString& filePath) const;
    void ensureBuiltinsRegistered();
    QMap<QString, int> extensionStats() const;
};
```

**Key Features**:
- Multiple parsers per extension (tries them in order)
- Built-in registration for JSON, StarDict, MDict parsers
- File extension matching (case-insensitive)
- Statistics on registered extensions

**Completeness**: ✅ Fully implemented

---

## Completeness Summary

### Fully Implemented (✅)
1. **IndexEngineStd** - Trie-based searching with 5 search types
2. **DataStoreStd** - JSON persistence for history/vocabulary
3. **DictionaryManagerStd** - Multi-format orchestration
4. **JsonParserStd** - JSON dictionary format
5. **StarDictParserStd** - StarDict (uncompressed) format
6. **DslParserStd** - DSL/Lingvo text format
7. **CsvParserStd** - CSV/TSV format
8. **PathUtilsStd** - File and cache management
9. **Qt Wrapper Layer** - IndexEngine, DataStore, DictionaryManager (Qt-based)
10. **PluginManager** - Dynamic factory pattern
11. **LookupService** - Search service wrapper

### Partially Implemented (⚠️)
1. **MdictParserStd** - Only SIMPLEKV format works; KIDX/RDEF/KBIX/KEYB are stubs

### Not Implemented (🚫)
- Compressed dictionary support in StarDict (.dict.dz)
- Advanced MDict format variants (KBIX, KEYB with encryption)
- GUI-specific components (in adapters/)

---

## Architecture Patterns

### 1. Dual-Layer Architecture
```
Qt Layer (interfaces)
    ↓
Qt Adapters (bridges to std)
    ↓
Std Core (pure C++17)
```

### 2. One-of Union Pattern (DictionaryManagerStd)
Each loaded dictionary is represented as a `Holder` containing exactly one non-null parser:
```cpp
Holder {
    json | stardict | mdict | dsl | csv
    std::string lookup(word) { dispatch to active parser }
}
```

### 3. Trie + HashMap Hybrid (IndexEngineStd)
- **Trie** for prefix search (O(k) where k = prefix length)
- **HashMap** for normalization and dictionary tracking
- **Dual indices**: word_index_ (normalized → entry) and dict_ (dict_id → words)

### 4. Tolerant Parser Pattern
Data store and parsers handle malformed input gracefully:
- JSON parser: section-based, not strict JSON
- CSV: auto-detects separator
- DSL: handles BOM and mixed indentation

---

## Dependencies

### External Libraries
- **ZLIB** - Decompression in StarDict and MDict parsers
- **Qt6::Core** - Only in Qt layer, not in std layer
- **Standard C++ Library** - std layer uses C++17 features:
  - `<filesystem>` for path operations
  - `<unordered_map>`, `<unordered_set>` for indexing
  - `<regex>` for regex search
  - `<zlib.h>` for compression

### Build Configuration
- `UNIDICT_BUILD_QT_CORE` - Controls Qt-based core library build
- Std core always built as `unidict_std_core`
- Qt core depends on adapters: `unidict_index_qt`, `unidict_data_qt`, `unidict_plugins_qt`

---

## Test Coverage

Comprehensive test suite covering:

**Std Implementations** (23 test files):
- `index_engine_std_test.cpp` - Trie and search algorithms
- `data_store_std_test.cpp` - JSON persistence
- `dictionary_manager_std_test.cpp` - Multi-format loading
- `json_parser_std_test.cpp` - JSON parsing
- `stardict_std_test.cpp` - StarDict format
- `dsl_parser_std_test.cpp` - DSL format
- `cache_prune_std_test.cpp` - Cache management
- `index_persistence_std_test.cpp` - Index I/O
- **MDict variants** (8 files):
  - `mdict_simplekv_std_test.cpp` ✅
  - `mdict_kidx_std_test.cpp`
  - `mdict_kbix_std_test.cpp`
  - `mdict_keyb_std_test.cpp`
  - `mdict_utf16_header_std_test.cpp`
  - `mdict_zlib_std_test.cpp`
  - `mdict_realheur_std_test.cpp`
  - `mdict_mdxkr_std_test.cpp`

**Qt Implementations**:
- `index_engine_test.cpp` - Qt wrapper testing
- `data_store_test.cpp` - Qt wrapper testing
- `lookup_service_test.cpp` - Service layer

---

## Known Limitations

1. **MDict Format Coverage**: Only SIMPLEKV format fully works; other variants have placeholder implementations
2. **StarDict Compression**: No .dict.dz (gzip) support in std implementation
3. **MDict Encryption**: No encryption/decryption support
4. **Large Dictionary Performance**: No memory-mapped file support in std implementation (Qt version has optional mmap)
5. **No Internationalization**: Assumes UTF-8 encoding throughout

---

## Architecture Strengths

1. **Qt Independence**: Core logic in std layer can be used without Qt
2. **Incremental Parser Support**: New formats can be added as new Parser implementations
3. **Flexible Search**: Multiple search algorithms (exact, prefix, fuzzy, wildcard, regex)
4. **Persistence**: Both data store and index support save/load
5. **Backward Compatibility**: Qt layer provides same API as before
6. **Plugin Architecture**: Dynamic parser registration allows runtime extensibility
7. **Robust Parsing**: Tolerant parsers handle malformed input gracefully
8. **Comprehensive Testing**: 23 test files covering major functionality

---

## Recommendations for Completion

1. **Complete MDict Support**:
   - Implement KIDX/RDEF record decompression fully
   - Add KBIX block index support
   - Add KEYB alternative format support

2. **Add StarDict Compression**:
   - Implement .dict.dz decompression using zlib
   - Cache decompressed data appropriately

3. **Performance Optimization**:
   - Add memory-mapped file support to std parser implementations
   - Implement LRU cache for decompressed dictionary blocks

4. **Extended Format Support**:
   - Add EPUB dictionary support
   - Add Kobo dictionary support
   - Add Kindle dictionary support

5. **Testing**:
   - Add fuzzing tests for parsers
   - Add performance benchmarks
   - Add integration tests across multiple dictionaries
