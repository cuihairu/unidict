# 大词典性能压测记录

> 2026-09-24，前缀补全与全文索引在 10 万词级词典上的回归打磨。
> 结论先行：**StarDict/Mdict 的 lowerBound 二分在大词典下完全无压力；
> 全文索引惰性构建一次性 1.3s 可接受；真正的坑是 JsonParser 原先走
> 基类线性 prefixSearch，无命中也要扫全表（10 万词 13ms/次），已修。**

## 为什么压测

前缀补全（QCompleter 输入即查）与全文回落落地时，测试数据都是几个词的
迷你词典，`prefixSearch` 线性还是二分在数据上不可见。真实词典（英汉类
30-50 万词）才是这条路径的考场，先把 10 万词级的数据跑出来再说。

## 压测方法

一次性 Qt Console 程序，链 `unidict_core_qt`（GUI 同款链路），固定种子
造两部 10 万词词典，程序化生成不入库（仓库铁律：不 commit 词典资产，
压测数据也一样）：

- **JSON**：`aaa00000` 起 2 万 + `zzz00000` 起 2 万（制造密集前缀区段）
  + 6 万随机 5-9 字母词；释义统一句式内嵌共享关键词 `kw<N%50>`
  （每个 kw 恰好出现在 2000 条释义里，全文命中数可预期）。
- **StarDict**：同词表按字节序写三件套（.ifo/.idx/.dict，无压缩），
  仿 `tests/dictionary_manager_prefix_test.cpp` 的 writeStarDict。

测三段：parser 加载、prefixSearch 单次均值（200 次）、manager 层全文
惰性构建与热查询。外层 `/usr/bin/time -v` 量 RSS。

## 数据（修复前，gcc15 -O2，本机 Linux）

| 指标 | 结果 |
| --- | --- |
| JsonParser 加载 10 万词 | 787 ms |
| StarDictParser 加载 10 万词 | 171 ms |
| JSON prefixSearch，命中前缀 'aaa'（2 万命中，凑满 20 即停） | <0.05 ms |
| **JSON prefixSearch，无命中 'zzqq'（线性扫全表）** | **13.1 ms/次** |
| StarDict prefixSearch（lowerBound 二分，含无命中） | 全部 <0.05 ms |
| 全文索引首次构建（20 万条目，含 StarDict 逐词解码） | 1301 ms（一次性惰性） |
| 全文热查询 'kw3' | 1.4 ms |
| 全文热查询宽词 'definition' | 58.2 ms |
| 峰值 RSS（20 万条目全量 in-memory） | ~457 MB |

## 发现与修复

**坑：JsonParser 无小写有序索引**。`m_entries` 是 QMap 但键为原词形
（大小写敏感序），前缀查询要大小写不敏感，只能走基类线性扫 `m_words`，
且无命中场景必须扫满全表。GUI 的 textChanged 每敲一键同步调一次：
10 万词 13ms 还算无感，真实 50 万词词典就到 60ms+，快速打字可感知卡顿。

修法与 StarDictParser 对称（`core/json_parser.{h,cpp}`）：

- 加 `QMap<QString, QString> m_lowerWords`（lower(word) → original word），
  loadDictionary 时随词条同步填充，同小写键后写覆盖（canonical 取最后
  词形，与 StarDict 的 m_canonicalWords 语义一致）。
- `prefixSearch` override：`lowerBound(p)` 落前缀区段起点，`!startsWith`
  即 break，O(log N + 命中数)。

修复后 JSON 全部前缀场景 <0.05 ms，与 StarDict 二分同级。

## 行为变化与守护测试

- **输出顺序**：JSON 补全从「词表插入序」变为「小写键字母序」——对
  GUI 补全弹窗是改善（字母序展示）；同小写不同词形（如 Hello/hello）
  从可能各出一条变为合并出一条（manager 层本就有小写去重，最终结果
  语义不变）。
- 守护测试（`test_dictionary_manager_prefix`，只增不减）：
  - `json_parser_lowerbound_path`：钉住二分路径语义（字母序、大小写
    不敏感、canonical 后写覆盖、截断、无命中）。
  - `json_parser_large_dict_prefix_fast`：5 万词 JSON 无命中前缀查询
    <5ms。阈值放得极宽——线性退回约 6.5ms 会撞线，二分离阈值三个
    数量级，CI 抖动不会误伤，只拦「退回线性扫全表」级别的回归。

## 遗留观察（已知、暂不动）

- 全文首次构建 1.3s 是惰性一次性成本，发生在第一次全文回落查询时
  （GUI 表现为精确未命中后的首次回落稍慢）。若将来体感不佳，方向是
  GUI 侧后台线程预热，不是改索引本身。
- 宽词（命中集合大）热查询 58ms：TF-IDF 要遍历命中集合打分，属于
  算法固有成本，仍在无感区间。
- 20 万条目全量驻留 ~457MB：索引与词条副本双份在内存。等真实用户
  报内存问题再做分页/瘦身，先记录基线。
