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

基线 → 现状（`build-cov`，gcovr 8.6 / gcc 15.2；分母已剔除"单独成行的
右花括号"，见下文说明：6073 → 5591 行）：

| 指标 | 基线 | 现状 | 目标 |
|------|------|------|------|
| lines | 96.6% (5868/6073) | **100.0%** (5591/5591) | 100% ✅ |
| functions | 99.3% (579/583) | **100.0%** (584/584) | 100% ✅ |
| branches | 61.7% (6087/9870) | 64.4% (6124/9514) | 记录，见下 |

基线时未覆盖行共 205 行，按文件分布（缺口行数）：

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

- [x] **P0-1 `FullTextIndexStd::clear()` 只清了 4 个成员**。漏掉 `terms_sorted_`
      （存的是指向 `postings_` 的裸 `PostingEntry*`，头文件注释自称
      "invalidated by clear()"——恰恰没失效）、`ngram3_index_`/`ngram2_index_`/
      `char_index_`/`prefix_index_`、`signature_`、`version_`、`last_error_`。
      后果：`clear()` 后 `version()`/`stats()` 仍报旧 UDFT 版本，四个辅助
      索引残留陈旧词项，是埋着的地雷。**修**：全量复位 + 断言复位后
      `version()==0`、搜索返回空、`stats()` 全零的回归测试。
- [x] **P0-2 `HtmlRendererStd::set_max_text_length()` 是静默空操作**。
      `max_text_length_` 声明了 100KB 默认值、注释写"100KB default"，
      但 `html_renderer_std.cpp` 从头到尾**没读过它**——一个宣称的安全
      上限从未生效，超大/恶意 MDX 词条可以在渲染器里无上限膨胀。
      同族死成员 `max_nesting_depth_`（32）同样声明未用。
      **修**：`render()` 出口按字节截断（避免切碎 UTF-8 序列），超限
      追加省略标记；`max_nesting_depth_` 接入 tokenizer 深度护栏。
- [x] **P0-3 `core/std/memory_optimizer_std.h` 是彻底的死代码**。611 行，
      **不在任何 CMake target 里**（从未被编译）、全仓库零 `#include`、
      零文档提及；且用到 `std::optional`/`std::ostringstream`/
      `std::setprecision` 却没 include `<optional>`/`<sstream>`/`<iomanip>`
      ——**原样放进任何 target 都会编译失败**。来源是早期 `chore:` 提交
      里的遗留草稿。**修**：删除。一个从不编译、从不include、
      编译不过的模板文件对"100% 覆盖"是结构性障碍。
      （如后续要复活，先补 include 再单独提交。）
- [x] **P0-4 两个"假装有测试"的空壳用例**。
      `tests/aggregate_lookup_std_test.cpp` 的 `test_relevance_calculation()`
      与 `test_deduplication()` **函数体只有注释、零断言**（"needs
      DictionaryManagerStd, skip for now"）。名字在、覆盖不在——比没有
      测试更危险。**修**：补真实断言，否则删掉空壳。

### P1 — 补测到 100% lines（按缺口行数从多到少推进）

- [x] P1-1 `epub_parser_std`：`decode_entities` 全套（`&lt;`/`&gt;`/`&quot;`/
      `&apos;`/`&nbsp;`/`&#NN;`/`&#xHH;`/`code>=128` 透传/畸形 hex 拒收/
      缺分号/`;` 超 10 字节）、无 `&` 快路径、`opf_dir` 为空（OPF 在 zip 根）、
      `./` 前缀剥离、`<h2 id="x">` 带属性标题、无 `<dc:title>` 时回退
      `"EPUB Dictionary"`、重复 headword 覆盖、缺 `</head>`、
      `find_similar(0)` 早退。
- [x] P1-2 `mdd_resource_std`：`save_metadata`/`load_metadata`（此前**全仓库
      零调用**，且要在测试里真跑一遍落盘/回读/脏文件）、`get_cache_info`
      边界、prune 系列边界。
- [x] P1-3 `html_renderer_std`：P0-2 新增路径 + 残余 sanitizer 分支。
- [x] P1-4 `pron_vocab_std`：严苛解析的全部拒收路径（重复 id、稀疏空洞、
      非法 blank、坏转义、代理对、UTF-8 截断）。
- [x] P1-5 `zip_reader_std`：恶意 zip 防御分支——`size < 22`、
      EOCD 注释长度不自洽、**zip64 拒收**、CDE 签名不符、CDE 越界、
      LFH 越界、条目数据 OOB、截断 deflate 流、零长条目、
      `kMaxEntryBytes` 上限。
- [x] P1-6 `aggregate_lookup_std`：补 P0-4 的空壳；`similarity_threshold`
      从"只断言默认字段值"变成真当行为阈值用。
- [x] P1-7 `cross_reference_std` / `mdict_parser_std` / `dsl_parser_std` 残余分支。
- [x] P1-8 `index_engine_std`：`exact_match`/`all_words`/`clear` 三个
      **零调用**公开方法 + `load_index`/`save_index` 失败路径、
      `add_word("")`、`remove_word` 未命中、`clear_dictionary` 未知词典、
      空引擎 `prefix_search`、`max_results <= 0`。
- [x] P1-9 `dictionary_manager_std::fuzzy_search`（5 个检索包装里唯一没测的）。
- [x] P1-10 `data_store_std` / `stardict_parser_std` / `json_parser_std` /
      `text_norm_std`（含 `kFoldKeyVersion` 断言——它进索引签名，是索引失效
      的唯一开关）/ `pcm_util_std.h` / `ctc_logits_std.h` /
      `pron_wave_std` / `espeak_arpabet_std` / `ctc_gop_std` 残余行。

### P0 补记 — 补测过程中又挖出的真实缺陷

补测不是"把行数凑满"，过程中撞出三个此前没人发现的真问题：

- [x] **P0-5 `.mdd` v1/v2 头解析偏移错误，真实 `.mdd` 一律加载失败**
      （`fix(mdd)` 提交）。`parse_v1_header`/`parse_v2_header` 都从
      `buf[0]` 读 `header_len`，但 `parse_header` 读过 magic 后 rewind 了，
      `buf[0..2]` 就是 magic——等于把 magic 的头几字节当成头长度：
      V1 算出 0x1b2345 ≈ 455MB，V2 算出 0x1b23 = 6947。紧接着
      `fseek(file_, header_len, SEEK_SET)` 跳到 EOF 之外，解析必然失败。
      证据：声明 `header_len=8` 的 v2 文件被报成 `header_len=6947`。
      修：字段改从 `buf+3` 取；并重写两个测试的 fixture——它们此前是
      "把头撑到 6947 字节、让索引表正好落在假偏移上"，把 bug 固化成了断言。
- [x] **P0-6 `MddResourceCache::save_metadata`/`load_metadata` 是幽灵 API**
      ——头文件声明了，`.cpp` 里从来没有定义，任何调用方链接期就报
      undefined reference。已删声明并写明去向。
- [x] **P0-7 DSL 缩进续行被 `trim(line)` 抹掉**（`test(core)` 提交）。
      解析循环先无条件 `line = trim(line)`，再判 `!isspace(line[0])`
      区分"新词头"与"续行"——判据恒真，缩进续行全部被当成新词头。
      实测：`hello` / `  used when meeting someone` / `  and also...`
      会被切成两个词条，第二条把第三条吃成释义。ECDict/Youdao 导出的
      `.dsl` 大量使用缩进续行。修：在 trim 之前抓住 `indented` 标志。

### 记录但未修（写清判断，别让后人重复踩）

- **`is_compressed` 恒为 false**（`mdd_resource_std.cpp`）：三个块解析器
  都只写 `is_compressed = false`，头文件也默认 false，于是 `get_resource`
  的解压分支永不可达。MDD v2 的资源值在文件里是 zlib 压缩的，
  `parse_multi_block` 只 inflate"索引块"，条目 offset 仍指向压缩字节——
  `get_resource` 会把压缩字节当资源返回。修它要先确认各版本"每块是否
  压缩"标志怎么读（当前代码根本没读该字段），属真实文件兼容性工作，
  已在代码注释里指向 docs/roadmap.md 的 MDict 条目。
- **`calculate_relevance` 的 examples/pronunciation 加分是死代码**：
  `AggregatedEntry.examples`/`.pronunciation` 没有任何解析器路径填充，
  两个 `+=` 永远不执行；另外"精确命中 + priority 0"时基础分
  0.5+0.3+0.2 已经等于 1.0，释义质量分档被 clamp 吃掉，只有 fuzzy
  路径（分数是 `sim*0.7 + relevance*0.3`）才看得出差别。属打分设计
  问题而非缺陷，需要产品侧决定这些字段要不要真的接上。
- **`sort_by_relevance` 的 priority 兜底不可达**：`DictionaryAggregator`
  构造 `EntrySource` 时一律写死 `priority = 0`。带真实优先级的是
  `AggregatedLookupBuilder` 那条独立路径，它不经过这个函数。

### 不可达代码的处理方式（本轮形成的规矩）

补到最后剩下的几行都不是"忘了测"，而是**证明不可达**。处理分三类，
每类都在代码里留了理由，不靠"覆盖率好看"糊过去：

1. **删掉**——私有/内部、零调用方、且与别的实现重复的死函数：
   `MddResourceParser::read_string`、`HtmlRendererStd::normalize_url`、
   `CrossReferenceManager::extract_protocol`，以及
   `MddResourceCache::save_metadata`/`load_metadata`（幽灵 API）。
2. **GCOVR_EXCL 标注 + 写明为什么不可达**——防御性护栏，删掉会削弱
   安全/健壮性：`zip_reader` 的 `inflateInit2` 失败、`mdd` 的
   `ftell`/`fseek` 失败与 `is_compressed` 解压分支、`html_renderer` 里被
   `sanitize_attribute` 抢先执行的 `<a>`/`<img>` URL 复检、
   `mdict_decryptor` 双层 switch 的内层 `default`、`dsl` 的 `parse_header`
   尾部分支、`pron_vocab` 的 `id < 0` 与循环终止兜底等。
3. **测试钉住现状**——行为本身可疑但不属于本轮要改的：relevance 分数
   饱和、`prune_by_age` 用严格大于导致年龄 0 的条目剪不掉、
   `get_cached_path` 查的是元数据表而非拼路径、`MdictEncryptionType`
   越界值报"自定义"而非 "UNKNOWN"。

### 明确不做（记录判断，避免以后重复讨论）

- **branches 100% 不设为目标**。`core/std/` 9514 个分支里绝大部分是
  `||`/`&&` 短路、三目、循环条件的**一侧**可达性，gcovr 逐个凑满的边际
  收益极低且会写出大量"为覆盖率而覆盖率"的断言。定档：**lines/functions
  100%（脚本阈值强制），branches 只做趋势跟踪**（61.7% → 64.4%）。
- **`HtmlRenderOptions` 的 7 个死配置字段**（`allow_css`/`allow_tables`/
  `allow_media`/`extract_text`/`base_url`/`dictionary_id`/`link_resolver`）
  与 `RenderedHtml::has_math`/`resources`：这些是**声明了但没实现**的
  能力，属于路线图级功能缺口，不是覆盖率缺口。接线它们要先有产品需求，
  本轮不硬塞进覆盖率冲刺。`max_text_length_`/`max_nesting_depth_` 除外——
  它们是**安全护栏**，性质是缺陷（见 P0-2），必须修。
- ~~**Qt 层覆盖率**：本轮只对 `core/`（std-only 纯逻辑）设 100% 阈值。~~
  **2026-09-26 推翻**：当时把 Qt 层整体排除在阈值外只是权宜。实测下来
  Qt 层的**桥接/业务逻辑**（learning_manager / fulltext_manager /
  lookup_adapter / sync_service / legacy core）完全不碰音频设备与窗口，
  是可以单测的纯逻辑——排除在阈值外等于放任 3000+ 行无验证代码。改为
  按下面的「Qt 层缺口」分档处理，只有真·平台代码才排除。

---

## Qt 层缺口（2026-09-26 盘点）

### 基线

`scripts/coverage.sh --qt`（Qt 全量插桩构建，offscreen 跑全部 ctest）：

| 指标 | 数值 |
|------|------|
| lines | **68.8%** (6957/10107) |
| functions | **65.0%** (792/1218) |
| 未覆盖行 | **3150** |

注：`core/std/` 已在上轮做到 100%，这里 10107 行的分母里它占 5591 行；
真正的缺口全在 `core/`（legacy Qt 核心）、`adapters/qt/`、`qmlui/`、
`gui/`、`cli/`。

### 明确排除（真·平台/UI 代码，列出来是为了让排除项可审计）

共 968 行。这些不是"忘了测"，是**测了会假绿**或**没有可断言的行为**：

| 文件 | 行 | 排除理由 |
|------|----|---------|
| `gui/main.cpp` | 751 | QWidget 接线（信号槽/布局/菜单），无可断言的纯逻辑 |
| `gui/audio_recorder.cpp`(+`.h`) | 59 | Qt Multimedia 设备采集；仓库纪律明令"测试里不出现任何音频设备假设"，CI offscreen 会假绿 |
| `gui/pcm_playback.cpp` | 49 | 同上（QMediaPlayer 播放） |
| `gui/waveform_widget.cpp` | 25 | `paintEvent` 绘制 |
| `qmlui/main.cpp` | 43 | QML 应用引导（QQmlApplicationEngine 注册） |
| `qmlui/global_hotkeys.cpp` | 32 | Windows RegisterHotKey 专属；非 Windows 是 stub（已有 test_global_hotkeys 覆盖 stub 语义） |
| `qmlui/startup_launcher.cpp` | 9 | Windows HKCU Run 注册表专属（已有 test_startup_launcher） |

### P0 — 大块零覆盖的业务桥接（本轮主线）

这几个是**应用真正依赖**的逻辑，出问题会静默劣化，且全是纯逻辑/文件 IO，
完全可单测：

- [ ] **Q-1 `qmlui/learning_manager.cpp`（412 行，0%）**——学习数据全部走它：
      查词记录、答题统计、掌握度、标签/笔记、日/周/进度统计、复习调度
      （艾宾浩斯）、成就、导入导出。24 个 Q_INVOKABLE 零测试。
- [ ] **Q-2 `adapters/qt/sync_service_qt.cpp`（357 行，0%）**——文件级同步
      MVP：扫描/比对/合并/冲突预览。冲突合并逻辑出错会静默丢用户数据。
- [ ] **Q-3 `adapters/qt/fulltext_manager_qt.cpp`（320 行，0%）**——全文索引
      落盘/加载/版本协商（UDFT1/2/3）、签名校验、索引升级、源差异导出。
      11 个 Q_INVOKABLE 零测试，索引缓存失效判断全靠它。
- [ ] **Q-4 `qmlui/lookup_adapter.cpp`（383 行，24%）**——查词主路径，76 个
      Q_INVOKABLE。GUI/QML 的所有查询都过这里。

### P1 — legacy Qt 核心与薄适配器

- [ ] **Q-5 `core/unidict_core.cpp`（97 行缺口）**——应用真正链接的 Qt 库
      （`DictionaryManager` 单例等），86%。
- [ ] **Q-6 `core/` 其余 legacy 解析器**：`mdict_parser`(71)、
      `stardict_parser`(23)、`epub_parser`(19)、`lookup_service`(19)、
      `index_engine`(14)、`plugin_manager`(12)、`path_utils`(7)、
      `data_store`(7)、`json_parser`(4)、`unidict_core.h`(14)。
- [ ] **Q-7 薄适配器**（都是 std↔Qt 的 QString 桥接，逻辑少但一个没测）：
      `json_parser_qt`(32)、`plugin_manager_qt`(30)、`mdict_parser_qt`(27)、
      `stardict_parser_qt`(26)、`ai_service_qt`(39)、`settings_qt.h`(20)、
      `index_engine_qt`(12)、`path_utils_qt`(9)、`clipboard_qt`(8)、
      `data_store_qt`(10)、各 `*_qt.h`。
- [ ] **Q-8 `qmlui/clipboard_monitor.cpp`（60 行，4%）**——剪贴板监听，
      `QClipboard` 在 offscreen 下可测。
- [ ] **Q-9 `cli/main.cpp`（55 行，0%）**——Qt 版 CLI 参数解析。
- [ ] **Q-10 `gui/pronunciation_panel.cpp`（146 行，0%）**——发音练习面板
      里的非设备逻辑（TTS 文本准备/对比流程状态机/评分展示格式化）。

### 交付前检查清单

- [x] `build-std`（std-only）`ctest` 全绿
- [x] `build`（Qt 全量，`QT_QPA_PLATFORM=offscreen`）`ctest` 全绿
- [x] `build-pron`（`UNIDICT_BUILD_PRON=ON`）pron 相关 `ctest` 全绿
- [x] `scripts/coverage.sh` 达标（core/ lines 100%）
- [ ] `scripts/coverage.sh --qt` 达标（Qt 层 lines 100%，排除项按上表）
