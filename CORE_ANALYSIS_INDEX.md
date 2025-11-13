# UniDict Core Analysis - Complete Documentation Index

This directory contains comprehensive analysis of the UniDict core architecture. Use this index to navigate the documentation.

## Documents

### 1. **core_analysis.md** (20 KB)
The definitive detailed analysis of the core architecture.

**Contents**:
- Executive summary of dual-layer architecture
- Complete directory structure with descriptions
- Detailed analysis of 8 major components:
  - Core Types Module
  - Index Engine (Qt and Std)
  - Data Store (Qt and Std)
  - Dictionary Parsers (5 formats)
  - Dictionary Manager (Qt and Std)
  - Lookup Service
  - Path Utilities (Qt and Std)
  - Plugin Manager
- Completeness summary with status indicators
- Architecture patterns (6 major patterns)
- Dependencies and build configuration
- Test coverage summary
- Known limitations and recommendations

**Best for**: Understanding architectural decisions, component relationships, and implementation status.

### 2. **architecture_diagram.txt** (12 KB)
Visual ASCII diagrams and structured overview of the system.

**Contents**:
- Complete layered architecture diagram
- Component relationships visualization
- File format support matrix
- Search capabilities breakdown
- Build configuration overview
- Design patterns explanation
- Test coverage summary

**Best for**: Visual learners, quick understanding of relationships, seeing how components fit together.

### 3. **core_quick_reference.md** (7.6 KB)
Quick reference guide for developers.

**Contents**:
- Quick stats table
- Key files at a glance
- Common usage patterns with code examples:
  - Loading dictionaries
  - Performing searches
  - Managing data store
  - Cache management
  - Index persistence
- Architecture overview
- Format support matrix
- Search algorithm complexity
- Component dependencies
- Build & link instructions
- Environment variables
- Performance tips
- Testing instructions
- Troubleshooting guide
- Known limitations

**Best for**: Getting started quickly, code examples, troubleshooting, performance optimization.

---

## Navigation Guide

### I want to understand...

**The overall architecture**
→ Read: `architecture_diagram.txt` (diagrams section)
→ Then: `core_analysis.md` (Executive Summary + Architecture Patterns)

**How components relate to each other**
→ Read: `architecture_diagram.txt` (Component Relationships)
→ Then: `core_analysis.md` (Component Analysis sections)

**How to use the core API**
→ Read: `core_quick_reference.md` (Common Usage Patterns)
→ Then: `core_analysis.md` (relevant component sections)

**Implementation details of a specific parser**
→ Read: `core_analysis.md` (Section 4: Dictionary Parsers)
→ Then: Look at actual code in `/core/std/*_parser_std.h/cpp`

**What's complete vs incomplete**
→ Read: `core_analysis.md` (Completeness Summary)
→ Then: `architecture_diagram.txt` (File Format Support section)

**How to improve/extend the system**
→ Read: `core_analysis.md` (Known Limitations + Recommendations)
→ Then: Look at component patterns and tests

**How tests are organized**
→ Read: `architecture_diagram.txt` (Test Coverage Summary)
→ Then: `core_quick_reference.md` (Testing section)

**Performance considerations**
→ Read: `core_quick_reference.md` (Performance Tips + Search Algorithm Complexity)
→ Then: `core_analysis.md` (Architecture Patterns section)

---

## Key Statistics

- **Total Core Files**: 26 (9 Qt-based + 17 Std)
- **Total Core LOC**: ~6,000 lines of code
- **Std Core LOC**: ~2,000 lines
- **Qt Wrapper LOC**: ~4,000 lines
- **Test Files**: 26 test files
- **Supported Formats**: 5 (JSON, StarDict, DSL, CSV, MDict)
- **Search Types**: 5 (Exact, Prefix, Fuzzy, Wildcard, Regex)
- **Overall Completeness**: 95% (MDict partial at 40-50%)

---

## Architecture at a Glance

```
┌─────────────────────────────────────┐
│     Qt-based Wrappers               │
│  (DictionaryManager, IndexEngine)   │
└────────────┬────────────────────────┘
             │ (Qt Adapters)
             ↓
┌─────────────────────────────────────┐
│  Std-only Core (C++17)              │
│  - Index Engine (Trie + HashMap)    │
│  - 5 Dictionary Format Parsers      │
│  - Data Store (JSON persistence)    │
│  - Path Utils & Cache Management    │
└─────────────────────────────────────┘
```

**Key Design Principle**: Dual-layer architecture enables Qt-independence while maintaining backward compatibility.

---

## File Locations

### Main Headers
```
/core/unidict_core.h                 # Main DictionaryManager
/core/index_engine.h                 # Index interface
/core/data_store.h                   # Data persistence
/core/plugin_manager.h               # Parser factory
/core/lookup_service.h               # Search service
```

### Std Implementations
```
/core/std/dictionary_manager_std.h   # Core orchestrator
/core/std/index_engine_std.h         # Trie-based search
/core/std/data_store_std.h           # JSON persistence
/core/std/*_parser_std.h             # Format parsers
/core/std/path_utils_std.h           # Utilities
```

### Tests
```
/tests/index_engine_std_test.cpp      # Search algorithms
/tests/data_store_std_test.cpp        # Persistence
/tests/dictionary_manager_std_test.cpp # Integration
/tests/*_parser_std_test.cpp          # Format-specific
```

---

## Component Status

### Fully Implemented (✅)
- IndexEngineStd - Trie-based searching
- DataStoreStd - JSON persistence
- DictionaryManagerStd - Multi-format orchestration
- JsonParserStd - JSON format
- StarDictParserStd - StarDict format
- DslParserStd - DSL/Lingvo format
- CsvParserStd - CSV/TSV format
- PathUtilsStd - File management
- Qt wrapper layer - Backward compatibility
- PluginManager - Dynamic registration
- LookupService - Search service

### Partially Implemented (⚠️)
- MdictParserStd - Only SIMPLEKV format works (40-50% complete)

### Not Implemented (🚫)
- StarDict compression (.dict.dz)
- MDict encryption support
- Advanced MDict formats (KBIX, KEYB)

---

## Key Design Patterns

1. **Dual-Layer Architecture** - Qt layer delegates to std layer
2. **One-of Union Pattern** - DictionaryManagerStd::Holder
3. **Trie + HashMap Hybrid** - Optimal search performance
4. **Tolerant Parser Pattern** - Handles malformed input
5. **Plugin Architecture** - Dynamic parser registration
6. **Singleton Pattern** - DictionaryManager & DataStore

---

## Dependencies

### External
- ZLIB - Compression/decompression
- Qt6::Core - Only in Qt layer
- C++17 standard library - Filesystem, containers, regex

### Internal
```
DictionaryManagerStd → IndexEngineStd + 5 Parsers
Qt wrappers → Qt Adapters → Std Core
```

---

## Next Steps

### To Understand a Component
1. Read the relevant section in `core_analysis.md`
2. Check `architecture_diagram.txt` for relationships
3. Look at the header file (`.h`)
4. Review test files in `/tests/`
5. Examine implementation (`.cpp`)

### To Add a New Format
1. Create `/core/std/mynewformat_parser_std.h/cpp`
2. Implement required interface
3. Update `DictionaryManagerStd::add_dictionary()`
4. Add test file in `/tests/`
5. Document in this analysis

### To Improve Performance
1. Review `core_quick_reference.md` (Performance Tips)
2. Check algorithm complexity in diagrams
3. Consider index persistence
4. Profile hot paths
5. Look at cache implementation

---

## Quick Reference Commands

### Understand Index Search
→ `core_analysis.md` Section 2 "Index Engine"
→ `architecture_diagram.txt` "Search Capabilities"
→ `/core/std/index_engine_std.cpp` (240 LOC)

### Understand Data Persistence
→ `core_analysis.md` Section 3 "Data Store"
→ `/core/std/data_store_std.cpp` (223 LOC)

### Understand Format Parsers
→ `core_analysis.md` Section 4 "Dictionary Parsers"
→ `/core/std/*_parser_std.h/cpp` (varies)

### Understand Dictionary Loading
→ `core_analysis.md` Section 5 "Dictionary Manager"
→ `/core/std/dictionary_manager_std.cpp` (116 LOC)

---

## Frequently Asked Questions

**Q: Why dual-layer architecture?**
A: Enables Qt-independence while maintaining backward compatibility.

**Q: Why Trie + HashMap for indexing?**
A: Trie is O(k) for prefix search, HashMap is O(log n) for exact match.

**Q: Why tolerant JSON parser?**
A: Handles real-world data gracefully, not strict for better UX.

**Q: Can I use std layer without Qt?**
A: Yes! Link against `unidict_std_core` only.

**Q: How to add a new dictionary format?**
A: Create a new parser implementing the parser interface.

**Q: How to improve MDict support?**
A: Implement KIDX/RDEF record decompression and KBIX block index support.

---

## Document Versions

- **core_analysis.md**: 1.0 (2025-11-05)
- **architecture_diagram.txt**: 1.0 (2025-11-05)
- **core_quick_reference.md**: 1.0 (2025-11-05)
- **CORE_ANALYSIS_INDEX.md**: 1.0 (2025-11-05)

Last Updated: 2025-11-05

---

## Additional Resources

### In Repository
- `/core/CMakeLists.txt` - Build configuration
- `/tests/CMakeLists.txt` - Test configuration
- `/ARCHITECTURE.md` - Original architecture notes
- `/README.md` - Project overview

### External References
- [C++17 Standard Library Reference](https://en.cppreference.com/w/cpp/17)
- [StarDict Format Documentation](https://github.com/huzheng001/stardict-3/blob/master/docs/StarDict-dict-format.txt)
- [MDict Format Documentation](https://github.com/readmdict/readmdict)

---

**For more details, start with `core_analysis.md` for comprehensive information, or `core_quick_reference.md` for quick answers.**
