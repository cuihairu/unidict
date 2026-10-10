# Unidict 当前架构审计

> Phase 1 · 现状基线 · 审计日 2026-10-04 · 基线提交 fae5002
> 方法：四域只读代理审计（core / 接入层 / QML UI / 测试文档 CI）+ 承重事实人工抽验。
> 本文档只陈述现状事实，不含改动建议（处置方向见 TECH_DEBT.md）。

## 0. 一句话现状

能力已收敛到 **core/std（纯 C++17）单侧**：数据存储、全文索引、聚合、渲染、发音逻辑均为 std 单实现，且有 `cli-std`（零 Qt）作为纯 std 生产链（deb/rpm 分发）。**产品 UI 主链已全量骑在 std 核心上**（2026-10-10 更新）：qmlui 与 gui 的字典访问都走 `DictionaryManagerStd` + std 解析器（gui 于同日切 std，9ccf0bf），legacy core/ 的 `DictionaryManager` 单例与四套 Qt 解析器仅剩测试引用（TECH_DEBT TD-101/105 生产风险解除，连码退役待批）。壳层面 QML（唯一主出口）与 gui（发音练习特性壳）双壳并存是当前最大的架构事实。

## 1. 分层总览

```
┌─────────────────────────────────────────────────────────────┐
│ 壳层   qmlui（Qt Quick，MainDesktop.qml 为活入口）           │
│        gui（QWidget 单窗口桌面，发音评分整链在此）           │
│        cli-std（无 Qt 主力 CLI）  cli（Qt 遗留诊断 CLI）     │
│        android（Kotlin+Compose 原生壳 + JNI 桩）             │
├─────────────────────────────────────────────────────────────┤
│ 接入层 adapters/qt（12 桥，24 文件）                        │
│        adapters/android/jni（21 个 JNI 导出）                │
│        adapters/pron（ONNX 评分推理壳，UNIDICT_BUILD_PRON 门控）│
├─────────────────────────────────────────────────────────────┤
│ 核心-新 core/std/（68 文件，零 Qt）                          │
│        解析器/索引/存储/渲染/聚合/发音/工具                  │
├─────────────────────────────────────────────────────────────┤
│ 核心-旧 core/（14 cpp，全部 Qt 类型）                        │
│        DictionaryManager 单例 + 四套 Qt 解析器（仅测试引用） │
└─────────────────────────────────────────────────────────────┘
```

QML 入口事实（qmlui/main.cpp:159-161）：桌面默认加载 `qrc:/MainDesktop.qml`；
`qrc:/Main.qml` 仅在 Android/iOS 宏下编译（已被原生壳取代，成死路径）。
`MainModern.qml` 与 `qmlui/modern/` 死树已于 2026-10-08 删除（原 TECH_DEBT TD-111）。
resources.qrc 只编 8 个文件；活面 qmlui 只剩这 8 个 QML（Main/MainDesktop/components×3/
mobile/common×3）。

## 2. 构建目标与链接拓扑

core/CMakeLists.txt 两个 std 目标：

- `unidict_index_std` — index_engine_std + text_norm_std（小而纯）
- `unidict_std_core` — 其余全部 std（USE_ZLIB 编译宏连 zlib）

adapters/qt/CMakeLists.txt：`unidict_core_qt` = core/ legacy 全部 + 传递链接
`unidict_index_qt / unidict_utils_qt / unidict_plugins_qt / unidict_data_qt / unidict_std_core`
—— **legacy 目标叠在 std 核心之上**，两层都参与链接。

生产链：

| 产物 | 链接 | 性质 |
|---|---|---|
| qmlui（unidict_qml） | unidict_core_qt | Qt 链路，桌面活入口（唯一主出口） |
| gui（unidict_gui） | unidict_core_qt | Qt 链路，QWidget 发音练习专用壳（录音/跟读/评分仅此有） |
| cli-std（unidict_cli_std） | unidict_std_core + unidict_index_std(+pron) | 纯 std，唯一 CLI，deb/rpm 打包 |
| android（unidict_jni） | unidict_std_core + unidict_index_std | std-only 口径硬置，AGP 与命令行 NDK 双路径 |

## 3. 双实现并存总表

| 能力 | legacy core/ | core/std | 生产链路实际使用 |
|---|---|---|---|
| StarDict/MDict/JSON/EPUB 解析 | 四套完整 Qt 实现（QDataStream/QFile） | 四套完整 std 实现 | **std 四套**（qmlui/gui 经 DictionaryManagerStd；legacy 工厂仅测试引用） |
| DSL/CSV/TSV/plain | — | std（dsl/csv 解析器） | std 面（cli-std）；UI 主链 factory 未注册 |
| *ParserQt 桥（3 个） | — | 薄包装 std 内核 | **仅测试**（tests/qt_adapters_test.cpp 独苗），生产壳零引用 |
| IndexEngine | Qt 实现 | IndexEngineStd（prefix/fuzzy/wildcard/regex + 索引维护） | qmlui 未直连；std 侧仅供 std 链 |
| DataStore | 48 行门面 | DataStoreStd 真实现（641 行） | 经 data_store_qt 收敛到 std 单实现 |
| 全文检索 | 组合 std 引擎（unidict_core.cpp:842） | FullTextIndexStd | std 引擎（经 std manager） |
| MDD 资源 | — | MddResourceParser | std（std manager has_resource/resource_* 直达；gui res:// 回调同源） |
| path_utils / text_norm | Qt 实现 / — | path_utils_std / text_norm_std v2 表驱动 | path_utils_qt 包 std；text_norm 全 std |

要点：**真·双实现分叉面（解析器 4 对、双 manager）已收敛为 std 单侧生产**（2026-10-10 起
qmlui/gui 生产链全 std；legacy 面仅测试引用）。核心正门面：分组查询/失败隔离 std 侧齐备
（`DictionaryManagerStd::search_grouped`、failed_dictionaries/retry/forget）；历史/词本/笔记
单真源在 DataStoreStd（std manager 不持历史，双跳门面转发）。

## 4. 查词主链路（qmlui 桌面，欧路重排后）

```
openWord → lookup_adapter.aggregateLookup(word, {maxTotalResults:20, sanitizeHtml, rewriteCrossRefs})
  → **DictionaryManagerStd::search_grouped**（std 单口径，见下节聚合注记）
    词头精确 → 前缀 → 释义包含 三层降级，层内按词典分组、同词头折叠去重
  → EntryResultsPane：内容 Tab「词典 / 例句 / 词组 / 近义联想 / 全文检索」
                        + 每词典一个可折叠分组卡（同词典释义聚组、细线分隔、无硬边框）
```

- 例句 Tab = 释义含目标词的条目（**无独立例句库**）；词组 = 查询词开头复合词
  （`relatedLookup(word,"phrases")`）；近义联想 = 前缀+模糊候选词表
  （`relatedLookup(word,"related")`，无反义词数据源）；全文检索 = `fullTextLookup`。
- **聚合口径 = DictionaryManagerStd::search_grouped 单口径（UI 主链 + CLI 各检索面）**：
  五层降级（精确 > 词形还原 > 前缀 > 释义 > 模糊）+ 组内词头去重。原独立聚合器
  DictionaryAggregator（TD-104 双套）于 2026-10-10 经消费面扫描退役（生产零消费，
  双套分歧风险随之根除；P-3 3.3 已把降级语义移植进 manager）。

## 5. 存储与数据文件

| 数据 | 载体 | 位置 | 备注 |
|---|---|---|---|
| 搜索历史（limit 100）、生词本 CRUD+标签+笔记、置顶 | DataStoreStd（JSON） | 标准数据目录 | 唯一真实现，双跳门面（legacy facade → DataStoreQt → DataStoreStd） |
| 学习统计/复习历史 | learning_stats.json | AppDataLocation（learning_manager.cpp:418） | **与生词本双存储**（见 TD-113） |
| 同步清单 | JSON manifest（owner*/added/removed/changedSources*） | 用户自选本地路径 | 文件级同步，无账号无云端 |
| 全文索引 | UDFT 磁盘格式（fulltext_index.md 规格） | 索引目录 | 惰性构建（perf 记录约 1.3s） |
| 设置 | QSettings IniFormat | 标准配置 | ui/* 7 键、pron/* 3 键、voice/* 4 键；键名由 QML 调用方自由定义 |
| 词典健康 | 隔离/quarantine | — | 解析失败隔离至显式重试；缺失文件自愈 |

无本地加密（roadmap 未实现项）；另有 mdict 加密词典支持（`[encrypted]` 检测 + 密码设置）。

## 6. 渲染管线

html_renderer_std：白名单净化 + 链接改写（entry:// bword:// → `#w:` 页内锚点；
img/audio 相对 src → `res:///<key>?dict=<id>`，由 QTextBrowser::loadResource 从 sibling .mdd
取资源）。gui 侧 ResultBrowser（QTextBrowser）同一口径。
`HtmlRenderOptions` 8 字段中仅 `resolve_links` 被 render() 读取，其余 7 字段声明未读
（死配置 API，见 TD-115）。cross_reference_std 提供词条交叉引用。

## 7. 发音体系（三层）

1. **本地 TTS**：QTextToSpeech；voice/preset/rate/pitch/volume + 自动播放延迟（Drawer「语音」tab）。
2. **在线源**：`PronunciationSourceStd` 抽象 → `FreeDictionarySource`（dictionaryapi.dev，免密钥）。
   三态（本地/在线/自动）、口音挑选 US→UK→AU→Unknown（URL 文件名段与 text 标注双推断）；
   隐私口径：request_url 仅外发查询词本身，UI 有明示文案。
3. **评分（可选，门控）**：`adapters/pron` PronScorerOnnx（Pimpl 隔离 onnxruntime）；
   模型 635MB fp16 + vocab.json **不入库**，运行时给路径。M1–M9 全部实装：
   录音（16k/mono/16bit）→ 示范 → 跟读 → 波形 → 回放 → CTC 强制对齐 + GOP 逐音素评分、
   混淆定位、变体容忍记分、词尾 g→ŋ、生词本联动（词分<0.6 自动打「发音不稳」标签）、
   相对化练习历史。`UNIDICT_BUILD_PRON` 默认 OFF；M10 模型自举下载（cli-std 写 curl config，
   断点续传 + 防假模型落地），依赖外部 `curl`。

发音整链路（录音/波形/评分）在 gui（QWidget）侧接入；qmlui 侧仅有设置面。

## 8. 移动端

- Android：Kotlin+Compose 原生壳 + JNI（dict/lookup/store 三域 21 个导出）→ std core。
  M0–M3-C 已交付（docs/mobile_plan.md：导入/查词/生词本/历史/笔记）；M4 TTS、M5 打磨出包未做。
  daily-build.yml 有 Gradle+AGP+JNI 的 release apk job。
- QML 移动路径（Main.qml + qmlui/mobile/）已被原生壳取代——死路径，仍编 qrc。
- iOS / HarmonyOS：未开始。

## 9. 同步 / AI / Server 面

- **sync_service_qt**：本地文件级合并 MVP（history 有序并集、vocab 本地赢、remote-only 追加；
  previewDiff/applyPreview/applySelection/exportSelection/importSelection 可选择性合并）。
- **ai_service_qt**：外部命令桥（env `UNIDICT_AI_CMD`；`translate` / `grammarCheck` 两项；
  无 streaming、无 provider 概念）。
- **同步中转 relay**（2026-10-04 交付）：`server/sync_relay/`——PROTOCOL.md v1 契约 +
  dev（Python stdlib）/ Worker（Cloudflare D1）双参考实现，契约测试 dev 16 用例
  （ctest `sync_relay_protocol`）+ worker 14 用例（node:sqlite D1 shim）；C++ 版
  `unidict-relay` 与客户端同步引擎归 B2/B5，未开始。
- **server_plan.md**（2026-10-02）：字典库服务 = 元数据目录 + 匿名文件分发 + 多设备安装协调
  ——设计稿，"审核通过前不动实现"；其中同步中转面 B1 已如上交付。
- **design/sync-engine.md**：S3 兼容 blob + 账号 + E2EE（HLC + monocypher）云级设计，S1–S5 未实现。
- 查词核心路径不含任何网络依赖（core 零网络栈）——**local-first 基线已立**。

## 10. 与 roadmap.md 的映射差异（抽样）

| roadmap 条目 | 审计事实 |
|---|---|
| 发音 M1–M3（roadmap:109 一带而过） | 实际 M1–M9 全部实装，滚动实现已超过文案 |
| "Anki-style flashcard review (basic)" [x] | 活路径无复习 UI（复习只在死 Main.qml / 未发布 modern 组件） |
| "Achievement/gamification" [ ] | learning_manager 已实现（成就/激励语/每日目标）但为死码（TD-113） |
| "Markdown support" 出现两次（一勾一空） | 陈旧重复条目 |
| "Conflict preview & merge (file-based MVP)" [x] | 属实（sync_service_qt） |

---

附录：审计依据（本会话四份只读域报告 + 抽验命令）；本文档未修改任何实现文件。