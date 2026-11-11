> Language: [English](../../research/search-index-benchmark.md) | **简体中文**

# 搜索索引对标：词典客户端的导入时建索引

> 调研日期 2026-11-03。用途：「导入字典后搜索不能用」的根因实证 +
> unidict 导入→索引→搜索管线的最佳实践对标。
> 来源：GoldenDict-ng 官方文档/架构页与公开 commit 历史
> （github.com/xiaoyifang/goldendict-ng）、mdict-utils / mdict4j 的 MDX
> 格式文档、StarDict 格式规范、欧路产品行为报道（GoldenDict-ng issue
> #1711 用户对比）。

## 一、unidict 根因（真实词典文件实测）

复现口径：`add_dictionary()` 返回后立即查询，**不做**任何显式
`build_index()`（修前的导入契约把 trie 构建留给了每个调用方）。

实测环境：24 源文件 core 构建，词典取自 `dictionaries/` 真实文件——
wikdict-en-zh StarDict（25,397 词）、CC-CEDICT JSON（121,401 词）、
ECDICT StarDict（191 MB，约 200 万词）。

| 查询路径 | 导入后立即查（修前） | `build_index()` 之后 |
|---|---|---|
| `exact_search("hello")` | 1 命中 ✔ | 1 命中 ✔ |
| `prefix_search("hel")` | **0 命中 ✘**（trie 空的） | 5 命中 |
| `prefix_search("你好")`（CC-CEDICT） | **0 命中 ✘** | 1 命中 |
| `fuzzy_search` / `wildcard_search` | 可用 ✔（add 时 `word_index_` 已填充） | 可用 ✔ |
| 首次 `full_text_search` | **4.2 s**（2.5 万词）/ **3.6 s**（CC-CEDICT）/ ECDICT：实测 **> 35 分钟仍未完成首次搜索，终止实测** | 同样成本（惰建一次，此后走缓存） |

根因（a51d1a3 的 file:line）：

1. **导入不建前缀 trie**——`core/std/index_engine_std.cpp` 的
   `add_word()` 只填 `word_index_`/`dict_`，`trie_` 留空，`build_index()`
   是独立的手动步骤。哪个调用方忘了，前缀层就静默死掉。实锤的遗忘方：
   `gui/main.cpp:175` `loadDefaultDictionaryLocations()`——重启时若全部
   词典都还原自持久化状态文件、没有新增（`added` 为假），`build_index()`
   永不执行。`search_grouped`/`suggest_corrections` 有惰性自愈，但公开
   的 `prefix_search`（dictionary_manager_std.cpp:556）、`search_all` 层 1
   （:333）以及两套 GUI 的输入联想（gui/main.cpp:686/711/1239、
   qmlui/lookup_adapter.cpp:84/806/837）都**没有**——症状：导入或重启后
   搜索框查不到 / 联想为空。
2. **全文索引完全惰性**——每次 `add_dictionary` 都 `ft_index_.reset()`
   （dictionary_manager_std.cpp:48），首次全文查询当场付全量倒排构建
   （实测数据见上表）。ECDICT 量级词典即「搜索卡死几分钟」。

修法（本次改动）：

- `add_word()` 增量维护 trie（导入即索引）；`remove_word()` 同步摘除
  （不留鬼影候选）。
- `add_dictionary` 将前缀索引置为干净；所有公开查询面（`prefix_search`、
  `search_all` 层 1）经 `ensure_prefix_index_built()` 自愈兜底（仅
  `load_index` 整表替换会置脏）。
- 新增 `warm_fulltext_index()`（幂等）：两套 GUI 的导入收尾
  （`gui/main.cpp` 单文件/目录导入处理、`qmlui/lookup_adapter.cpp`
  `finishImport()`）在导入完成时建好释义倒排——进度框关闭后所有查询
  路径即查即得。CLI 保持惰建（单次调用的进程不该付构建成本）。

## 二、成熟客户端怎么做

| 维度 | GoldenDict-ng | 欧路词典 | MDX 生态（mdict-utils / mdict4j） | StarDict 格式 | unidict（修后） |
|---|---|---|---|---|---|
| 导入时词头索引 | 按格式解析进内存索引；每词典一份索引文件（`IdxHeader`/`Chunks`/`BtreeIndex`，xiaoyifang.github.io/goldendict-ng/architecture） | 导入时把第三方词典**转换成自有内部格式**并建索引（「30G 词典即开即用」，GoldenDict-ng issue #1711 用户对比） | MDX 文件**本身就是索引**：有序 key block + key index（块级二分）；读取器装载时展开 key 列表（`readmdict.py` `_read_key_block`） | `.idx` 出厂即有序 → 二分查找，无需导入时建索引 | `add_word()` 内增量建 trie + 哈希索引——`add_dictionary` 返回即可查 |
| 全文索引 | **后台线程池**（`FtsIndexing` 经 `QtConcurrent`），程序启动/加词后立即开始，状态栏显示正在建索引的词典；按词典开关，超大词典可关 | 不暴露；查词优先的产品取态 | 无 FTS——生态把 MDX 视为纯词头检索；全文属工具链范畴 | 格式内没有；GoldenDict 在外部建 | 导入收尾同步预热建（`warm_fulltext_index`），分词构建多线程并行（`build_from_documents`） |
| FTS 持久化 | 每词典落盘一份持久索引（Xapian 库，`*_FTS_x` 目录）；**增量续建**——「全文索引是增量的，不会重新开始」（维护者 #1711）；已知膨胀投诉：60G 词典 → >100G 索引 | 不适用（内部格式） | 不适用 | 不适用 | UDFT1/2/3 落盘缓存带词典集签名（`fulltext_signature()`，TV/NV 版本盐）——匹配即载、失配即重建 |
| 前缀搜索 | Xapian term 迭代器做通配/前缀；CJK 走 n-gram 分词器 | 即时（内部索引格式） | 词头有序块 → 前缀 = 块区间扫描 | 有序 .idx → 前缀二分区间 | trie 下行；fold_key 归一档位（宽容/谨慎/精确） |
| 模糊搜索 | 词形还原/同义词 + Xapian 模糊 | 内部实现 | 无 | 无 | `word_index_` 上编辑距离 ≤ 2（O(N) 扫描——2M 词已不轻，见 §四） |
| 索引过期失配 | 签名/ID 校验（按词典文件列表 `needToRebuildIndex`）；「Schedule for reindex」 | 不透明 | 签名即文件本体 | 文件集即索引 | UDFT 签名失配 → 自动重建；`kTokenizerVersion`/`kFoldKeyVersion` 盐保证行为变更即失效 |

## 三、采纳结论

1. **导入完成 = 可搜索**——欧路的「导入即自有索引格式」与 GoldenDict-ng
   的「索引立即开始」都把建索引当作摄取的一部分，而非第二步手动动作。
   unidict 现在词级索引同步建（成本低：trie/哈希），与其解析器现状一致
   （MDX/StarDict 本来也只是「装载出厂索引」）。
2. **全文是独立的重索引，有自己的生命周期**——GoldenDict-ng 刻意放后台
   跑并按词典持久化；没有谁在导入对话框里内联建 FTS。unidict 的
   `warm_fulltext_index()` 是它的诚实同步版；惰建路径保留给启动装载
   （CLI、状态还原），启动速度不受影响。
3. **持久化 + 正确性签名**是共识机制（GoldenDict 的词典文件集 id；
   unidict UDFT 签名掺分词/折叠版本）——任何行为变更都必须失效缓存。
4. **按格式认清现实**：MDX 与 StarDict 文件内已带有序词头索引；对它们
   「导入时建索引」只指跨词典合并层（trie/哈希），不是重新索引正文。

## 四、已知后续（本次刻意不做）

- 导入收尾的 FTS 预热是同步的：多 GB 词典会让导入收尾阻塞整个构建时长。
  GoldenDict-ng 的答案是后台 worker + 进度 + 按词典开关；等该量级成为
  真实负载再移植（manager 尚不支持并发建索引+变更——见 TECH_DEBT/todo
  的 `ensure_postings` 条目）。
- `fuzzy_search` 是全表编辑距离扫描；ECDICT 量级的标准下一步是 BK-tree
  或 n-gram 候选过滤。
- ECDICT 量级首载实测：191 MB 导入 + 惰性 FTS 构建实测 **> 35 分钟仍未
  完成首次搜索**；UDFT
  索引持久化加载是缓解手段且已存在，但 GUI 启动时尚未自动预取
  （CLI 已有 `--fulltext-index-load`）。
