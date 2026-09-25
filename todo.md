# Unidict Implementation Todo

> **归档说明（2026-09）**：本文是早期实现清单的历史快照，全部条目已完成。
> 现行路线图以 [docs/roadmap.md](docs/roadmap.md) 与
> [docs/pro_dictionary_gap.md](docs/pro_dictionary_gap.md) 为准。
>
> **2026-09-25 覆盖缺口盘点**：下方新增「覆盖率缺口」章节，记录 std-only
> 核心库的实测覆盖率基线与按优先级排序的补测/修复清单。

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
  - [x] Implement MDict format parser (std-only, Qt adapter integrated)
    - 已实现：无加密 + zlib 的多种块布局原型（KIDX/RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 等）与启发式解析；提供 `MdictParserStd` 并由 `MdictParserQt` 适配用于应用端
    - 待办：加密变体支持、更多真实文件兼容性回归与边界用例覆盖
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
- [x] Add vocabulary management UI (history/vocab list)
- [x] Integrate with core services

### Phase 4: Advanced Features
- [x] Add fuzzy search
- [x] Implement full-text search (MVP: substring over definitions via std-only manager)
- [x] Upgrade full-text search to an inverted index (tokenizer + TF/IDF，含UDFT1/2/3持久化与签名)
- [x] Add cross-platform sync (SyncServiceQt wired into QML + adapters/qt/sync_service_qt.*)
- [x] Integrate AI features (AiServiceQt exposed to QML for translate/grammar tools)

## Technical Requirements
- **Performance**: Memory mapping + binary search for ms-level response
- **Cross-platform**: Qt6 framework for unified experience
- **Extensibility**: Plugin architecture for community contributions
- **AI Integration**: LLM interfaces for smart translation and grammar check

---

## 覆盖率缺口（2026-09-25 盘点）

### 测量方式

仓库此前**没有任何覆盖率工具链**，"覆盖率"只是口号，无法验证。本次补上：

- `cmake/BuildOptions.cmake` 新增 `UNIDICT_ENABLE_COVERAGE=ON`
  （等价 `--coverage -O0 -g`，仅 gcc/clang，MSVC 下静默跳过）。
- `scripts/coverage.sh`：配置 coverage 构建 → 跑全部 std ctest →
  gcovr 出 `lines/branches/functions` 汇总与逐文件缺口 + 阈值判定。

```bash
scripts/coverage.sh                 # 阈值校验（默认 lines 100）
scripts/coverage.sh --threshold 95  # 临时放宽
```

基线（`build-cov`，core/std/ 6073 行，gcovr 15.x / gcc 15.2）：

| 指标 | 基线 | 目标 |
|------|------|------|
| lines | **96.6%** (5868/6073) | 100% |
| functions | **99.3%** (579/583) | 100% |
| branches | **61.7%** (6087/9870) | 记录，见下 |

未覆盖行共 205 行，按文件分布（缺口行数）：

| 文件 | 缺口 | 性质 |
|------|------|------|
| `epub_parser_std.cpp` | 53 | 最差：整条 `decode_entities` 数字实体分支未测 |
| `mdd_resource_std.cpp` | 27 | 缓存/元数据路径 |
| `html_renderer_std.cpp` | 25 | 含**未实现的** `max_text_length_` 上限 |
| `pron_vocab_std.cpp` | 21 | HF vocab.json 严苛解析的拒收路径 |
| `zip_reader_std.cpp` | 17 | **恶意 zip 防御分支**（zip64/EOCD/LFH 越界） |
| `aggregate_lookup_std.cpp` | 9 | 两个测试函数是**空壳**（见 P1-6） |
| `cross_reference_std.cpp` | 9 | |
| `mdict_parser_std.cpp` | 8 | |
| `dsl_parser_std.cpp` | 6 | 仅 1 个测试 target |
| `index_engine_std.cpp` | 5 | `exact_match`/`all_words`/`clear` 零调用 |
| `data_store_std.cpp` | 3 | |
| `dictionary_manager_std.cpp` | 3 | `fuzzy_search` 零调用 |
| `text_norm_std.cpp` | 3 | `kFoldKeyVersion` 常量未断言 |
| `pcm_util_std.h` | 3 | |
| `ctc_gop_std.cpp` / `stardict_parser_std.cpp` | 2 each | |
| `json_parser_std.cpp` / `pron_wave_std.cpp` / `espeak_arpabet_std.cpp` / `ctc_logits_std.h` / `html_renderer_std.h` | 1 each | |

### P0 — 盘点中暴露的真实缺陷（不是"没测"，是"写错了"）

- [ ] **P0-1 `FullTextIndexStd::clear()` 只清了 4 个成员**。漏掉 `terms_sorted_`
      （存的是指向 `postings_` 的裸 `PostingEntry*`，头文件注释自称
      "invalidated by clear()"——恰恰没失效）、`ngram3_index_`/`ngram2_index_`/
      `char_index_`/`prefix_index_`、`signature_`、`version_`、`last_error_`。
      后果：`clear()` 后 `version()`/`stats()` 仍报旧 UDFT 版本，四个辅助
      索引残留陈旧词项，是埋着的地雷。**修**：全量复位 + 断言复位后
      `version()==0`、搜索返回空、`stats()` 全零的回归测试。
- [ ] **P0-2 `HtmlRendererStd::set_max_text_length()` 是静默空操作**。
      `max_text_length_` 声明了 100KB 默认值、注释写"100KB default"，
      但 `html_renderer_std.cpp` 从头到尾**没读过它**——一个宣称的安全
      上限从未生效，超大/恶意 MDX 词条可以在渲染器里无上限膨胀。
      同族死成员 `max_nesting_depth_`（32）同样声明未用。
      **修**：`render()` 出口按字节截断（避免切碎 UTF-8 序列），超限
      追加省略标记；`max_nesting_depth_` 接入 tokenizer 深度护栏。
- [ ] **P0-3 `core/std/memory_optimizer_std.h` 是彻底的死代码**。611 行，
      **不在任何 CMake target 里**（从未被编译）、全仓库零 `#include`、
      零文档提及；且用到 `std::optional`/`std::ostringstream`/
      `std::setprecision` 却没 include `<optional>`/`<sstream>`/`<iomanip>`
      ——**原样放进任何 target 都会编译失败**。来源是早期 `chore:` 提交
      里的遗留草稿。**修**：删除。一个从不编译、从不include、
      编译不过的模板文件对"100% 覆盖"是结构性障碍。
      （如后续要复活，先补 include 再单独提交。）
- [ ] **P0-4 两个"假装有测试"的空壳用例**。
      `tests/aggregate_lookup_std_test.cpp` 的 `test_relevance_calculation()`
      与 `test_deduplication()` **函数体只有注释、零断言**（"needs
      DictionaryManagerStd, skip for now"）。名字在、覆盖不在——比没有
      测试更危险。**修**：补真实断言，否则删掉空壳。

### P1 — 补测到 100% lines（按缺口行数从多到少推进）

- [ ] P1-1 `epub_parser_std`：`decode_entities` 全套（`&lt;`/`&gt;`/`&quot;`/
      `&apos;`/`&nbsp;`/`&#NN;`/`&#xHH;`/`code>=128` 透传/畸形 hex 拒收/
      缺分号/`;` 超 10 字节）、无 `&` 快路径、`opf_dir` 为空（OPF 在 zip 根）、
      `./` 前缀剥离、`<h2 id="x">` 带属性标题、无 `<dc:title>` 时回退
      `"EPUB Dictionary"`、重复 headword 覆盖、缺 `</head>`、
      `find_similar(0)` 早退。
- [ ] P1-2 `mdd_resource_std`：`save_metadata`/`load_metadata`（此前**全仓库
      零调用**，且要在测试里真跑一遍落盘/回读/脏文件）、`get_cache_info`
      边界、prune 系列边界。
- [ ] P1-3 `html_renderer_std`：P0-2 新增路径 + 残余 sanitizer 分支。
- [ ] P1-4 `pron_vocab_std`：严苛解析的全部拒收路径（重复 id、稀疏空洞、
      非法 blank、坏转义、代理对、UTF-8 截断）。
- [ ] P1-5 `zip_reader_std`：恶意 zip 防御分支——`size < 22`、
      EOCD 注释长度不自洽、**zip64 拒收**、CDE 签名不符、CDE 越界、
      LFH 越界、条目数据 OOB、截断 deflate 流、零长条目、
      `kMaxEntryBytes` 上限。
- [ ] P1-6 `aggregate_lookup_std`：补 P0-4 的空壳；`similarity_threshold`
      从"只断言默认字段值"变成真当行为阈值用。
- [ ] P1-7 `cross_reference_std` / `mdict_parser_std` / `dsl_parser_std` 残余分支。
- [ ] P1-8 `index_engine_std`：`exact_match`/`all_words`/`clear` 三个
      **零调用**公开方法 + `load_index`/`save_index` 失败路径、
      `add_word("")`、`remove_word` 未命中、`clear_dictionary` 未知词典、
      空引擎 `prefix_search`、`max_results <= 0`。
- [ ] P1-9 `dictionary_manager_std::fuzzy_search`（5 个检索包装里唯一没测的）。
- [ ] P1-10 `data_store_std` / `stardict_parser_std` / `json_parser_std` /
      `text_norm_std`（含 `kFoldKeyVersion` 断言——它进索引签名，是索引失效
      的唯一开关）/ `pcm_util_std.h` / `ctc_logits_std.h` /
      `pron_wave_std` / `espeak_arpabet_std` / `ctc_gop_std` 残余行。

### 明确不做（记录判断，避免以后重复讨论）

- **branches 100% 不设为目标**。`core/std/` 9870 个分支里绝大部分是
  `||`/`&&` 短路、三目、循环条件的**一侧**可达性，gcovr 逐个凑满的边际
  收益极低且会写出大量"为覆盖率而覆盖率"的断言。定档：**lines/functions
  100%（脚本阈值强制），branches 只做趋势跟踪**。
- **`HtmlRenderOptions` 的 7 个死配置字段**（`allow_css`/`allow_tables`/
  `allow_media`/`extract_text`/`base_url`/`dictionary_id`/`link_resolver`）
  与 `RenderedHtml::has_math`/`resources`：这些是**声明了但没实现**的
  能力，属于路线图级功能缺口，不是覆盖率缺口。接线它们要先有产品需求，
  本轮不硬塞进覆盖率冲刺。`max_text_length_`/`max_nesting_depth_` 除外——
  它们是**安全护栏**，性质是缺陷（见 P0-2），必须修。
- **Qt 层覆盖率**：本轮只对 `core/`（std-only 纯逻辑）设 100% 阈值。
  `gui/`/`qmlui/` 涉及音频设备与窗口，CI offscreen 环境会假绿，
  且仓库纪律明确要求"纯逻辑与平台代码物理分离"。Qt 桥接层沿用现有
  Qt Test 覆盖，不进阈值。

### 交付前检查清单

- [ ] `build-std`（std-only）`ctest` 全绿
- [ ] `build`（Qt 全量，`QT_QPA_PLATFORM=offscreen`）`ctest` 全绿
- [ ] `build-pron`（`UNIDICT_BUILD_PRON=ON`）pron 相关 `ctest` 全绿
- [ ] `scripts/coverage.sh` 达标（core/ lines 100%）
