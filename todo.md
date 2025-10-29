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
- [ ] Create DictionaryParser base interface
- [ ] Implement StarDict format parser
- [ ] Implement MDict format parser
- [ ] Create IndexEngine for fast lookups
- [ ] Design DataStore schema
- [ ] Implement basic SearchEngine

### Phase 2: Service Layer
- [ ] Build DictionaryManager
- [ ] Create LookupService
- [ ] Implement basic PluginManager
- [ ] Add vocabulary book functionality

### Phase 3: UI Integration
- [ ] Create QML search interface
- [ ] Implement result display
- [ ] Add vocabulary management UI
- [ ] Integrate with core services

### Phase 4: Advanced Features
- [ ] Add fuzzy search
- [ ] Implement full-text search
- [ ] Add cross-platform sync
- [ ] Integrate AI features

## Technical Requirements
- **Performance**: Memory mapping + binary search for ms-level response
- **Cross-platform**: Qt6 framework for unified experience
- **Extensibility**: Plugin architecture for community contributions
- **AI Integration**: LLM interfaces for smart translation and grammar check