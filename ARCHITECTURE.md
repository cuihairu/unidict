<div align="center">
  <img src="docs/logo.svg" width="120" alt="Unidict logo"/>
</div>

# Unidict 架构

## 总体设计：std-only 核心 + Qt 适配器

```
┌────────────────────────────────────────────────┐
│                应用层（Qt 允许）                  │
│   qmlui/（QML 桌面）  gui/（Widgets）  cli/      │
├────────────────────────────────────────────────┤
│                适配器层 adapters/qt/             │
│   IndexEngineQt / DataStoreQt / SyncServiceQt   │
│   AiServiceQt / FulltextManagerQt / ...         │
├────────────────────────────────────────────────┤
│                核心层 core/（无 Qt）              │
│   解析器：stardict / mdict(+解密) / dsl /        │
│           json / csv（core/std/）               │
│   索引：index_engine_std（Trie+前缀/模糊/通配/正则）│
│   全文：fulltext_index_std（倒排+TF/IDF+UDFT 持久化）│
│   服务：dictionary_manager_std（聚合查询/启用状态）│
│         aggregate_lookup_std / cross_reference_std│
│   渲染：html_renderer_std；资源：mdd_resource_std │
│   存储：data_store_std（JSON/CSV，历史/生词本）    │
└────────────────────────────────────────────────┘
```

要点：

- **核心不 include 任何 Qt 头**，只用 STL + zlib。需要 Qt 的能力（QSettings、QClipboard、QTextToSpeech、JSON 持久化 UI 状态等）在 `adapters/qt/` 做薄桥接。
- **`core/` 顶层遗留的 `unidict_core.*` 等 Qt 接口是兼容 wrapper**：`IndexEngine` 委托 `IndexEngineQt`，`DictionaryManager` 是 Qt 应用仍依赖的旧门面；新代码应面向 `core/std/` 的 `*_std` 类型。
- **历史教训**：改动接口时头文件与实现必须同步——`index_engine.cpp` 曾因只改头文件不实现而长期编译失败。

## 关键机制

### MDict 解析（core/std/mdict_parser_std.cpp）
- 无加密 + zlib 的多种块布局：KIDX/RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 及启发式解析
- SimpleXOR 解密由 `mdict_decryptor_std` 提供，密码来自参数或 `UNIDICT_MDICT_PASSWORD`
- 边界容错：越界偏移忽略、空块+垃圾尾部安全回退（有对应回归测试）

### 全文检索（core/std/fulltext_index_std.cpp）
- ASCII 分词 + postings + TF/IDF 评分；3-gram 辅助子串候选
- UDFT1/2/3 三代持久化格式，加载时版本协商；按词典内容签名防串档
- 并行构建（有并行回归测试）

### 聚合查询（core/std/aggregate_lookup_std.cpp + dictionary_manager_std）
- 多词典按优先级聚合，尊重启用状态；`--all` 输出按词典分组

### 交叉引用（core/std/cross_reference_std.cpp）
- `@@@LINK=` 跳转与历史栈，std 侧维护，Qt 侧展示

## 构建形态

| 形态 | 开关 | 产物 |
|------|------|------|
| std-only | `UNIDICT_BUILD_QT_* = OFF` | `unidict_std_core`、`unidict_index_std`、`unidict_cli_std`、`_std` 测试 |
| Qt 全量 | 默认 ON | `unidict_core_qt`、适配器、qmlui/gui/cli、Qt 测试 |

Qt 组件按需查找：基础组件（Core/Gui/Widgets/Test）总是需要；TextToSpeech 仅 Qt 应用/测试需要；Quick 系列仅 QML 应用需要。

## 下一步方向

以 [docs/pro_dictionary_gap.md](docs/pro_dictionary_gap.md) 的 P0 清单为准：
StarDict `ifo` 关键字段解释（`sametypesequence`/`charset`）、字符归一化（大小写/重音/全半角折叠）、真实词典兼容性回归集。
