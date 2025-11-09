# Unidict Implementation Todo

## Architecture Overview

### Core Dictionary Format Support Layer
```
DictionaryFormatManager
├── StarDictFormat (.dz, .dict, .idx)
├── MDictFormat (.mdx, .mdd) 
├── DSLFormat
├── EPUBFormat
└── CustomFormat (JSON/SQLite)
```

### Modular Architecture
```
Core Layer (C++)
├── DictionaryParser - 格式解析器接口
├── IndexEngine - 快速查找引擎  
├── DataStore - 本地数据存储
└── SearchEngine - 搜索算法实现

Service Layer
├── DictionaryManager - 词典管理
├── LookupService - 查词服务
├── SyncService - 跨平台同步
└── PluginManager - 插件系统

UI Layer (QML)
├── SearchInterface
├── ResultDisplay  
├── VocabularyBook
└── Settings
```

## Implementation Tasks

### Phase 1: Core Foundation
- [x] Create DictionaryParser base interface
- [x] Implement StarDict format parser
 - [ ] Implement MDict format parser (header解析已完成；zlib无加密的key/record block解析进行中)
- [x] Create IndexEngine for fast lookups
- [x] Design DataStore schema (JSON MVP)
- [x] Implement basic SearchEngine (via IndexEngine integration in DictionaryManager)

### Phase 2: Service Layer
- [x] Build DictionaryManager (load by file, expose prefix/fuzzy/wildcard)
- [x] Create LookupService (fallback+建议)
- [x] Implement basic PluginManager (内建 StarDict/MDict/JSON)
- [x] Add vocabulary book functionality (MVP via DataStore, CLI expose)

### Phase 3: UI Integration
- [x] Create QML search interface (MVP: input, suggestions, search)
- [x] Implement result display (QML TextArea)
- [ ] Add vocabulary management UI (history/vocab list)
- [ ] Integrate with core services

### Phase 4: Advanced Features
- [x] Add fuzzy search
- [x] Implement full-text search (MVP: substring over definitions via std-only manager)
- [ ] Upgrade full-text search to an inverted index (tokenizer + TF/IDF)
- [ ] Add cross-platform sync
- [ ] Integrate AI features

## Technical Requirements
- **Performance**: Memory mapping + binary search for ms-level response
- **Cross-platform**: Qt6 framework for unified experience
- **Extensibility**: Plugin architecture for community contributions
- **AI Integration**: LLM interfaces for smart translation and grammar check
