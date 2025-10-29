# Unidict Core Architecture Test

This demonstrates the core dictionary parsing architecture for Unidict.

## Components Implemented

### 1. Base Interface (`unidict_core.h`)
- `DictionaryParser` - Abstract base class for all format parsers
- `DictionaryEntry` - Standardized data structure for dictionary entries  
- `DictionaryManager` - Singleton manager for multiple dictionaries

### 2. Format Parsers
- `StarDictParser` - Handles StarDict (.ifo/.idx/.dict) format
- `MdictParser` - Handles MDict (.mdx/.mdd) format

### 3. Search Engine
- `IndexEngine` - Fast lookup using Trie data structure
- Support for exact match, prefix search, fuzzy search, wildcard search
- Memory-mapped file access for performance

## Key Features

### Multi-Format Support
```cpp
// Automatically detects format and loads appropriate parser
DictionaryManager::instance().addDictionary("dictionary.mdx");
DictionaryManager::instance().addDictionary("stardict.ifo");
```

### Fast Search
```cpp
// Prefix search with Trie
QStringList results = indexEngine.prefixSearch("inter", 10);

// Fuzzy search with edit distance
QStringList fuzzy = indexEngine.fuzzySearch("internation", 5);

// Wildcard search  
QStringList wildcard = indexEngine.wildcardSearch("inter*al", 10);
```

### Extensible Architecture
- Plugin system ready for new formats
- Clean separation between parsing and search logic
- Qt-based for cross-platform compatibility

## Build Requirements
- Qt6 Core
- CMake 3.16+
- C++20 compiler
- zlib (for MDict decompression)

## Next Steps
1. Add more dictionary formats (DSL, EPUB)
2. Implement user vocabulary management
3. Add AI integration for smart features
4. Create QML-based user interface