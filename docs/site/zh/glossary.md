# 源码架构术语表

> 语言: [English](/glossary) | **简体中文**

术语按源码中真实存在的职责归纳，不按目录名。`file:line` 指向定义或执行位置。

| 术语 | 英文 | 定义 | 出处 |
|---|---|---|---|
| 应用壳 | Application shell | 拥有启动、输入、产品布局和用户可见生命周期的进程外壳，覆盖 CLI、Widgets 与 QML。 | `qmlui/main.cpp:75` |
| 上下文绑定 | Context binding | QML 启动时把 C++ 服务对象以 `lookup`、`vocab` 等固定名发布给声明式视图。 | `qmlui/main.cpp:229` |
| 查词门面 | Lookup facade | QML 可调用的适配器，持有一台词典管理器，负责 Qt 值转换并把目录导入拆成定时器步进。 | `qmlui/lookup_adapter.cpp:36` |
| 词典聚合器 | Dictionary aggregator | 保存词典与失败记录，应用启用态、优先级、分组标签，并协调聚合查询的领域服务。 | `core/std/dictionary_manager_std.h:19` |
| 词典实例 | Dictionary instance | 一本已加载词典：解析器、词表、源路径、资源解析器和该书的启用/优先级/标签状态。 | `core/std/dictionary_std.h:29` |
| 解析器注册表 | Parser registry | 扩展名到工厂的映射，创建 JSON、StarDict、MDX、DSL、CSV/TSV、纯文本或 EPUB 解析器。 | `core/std/parser_registry_std.cpp:15` |
| 词形索引 | Lexical index | Trie、精确词表、词典反向表、乱序桶和扫描词数组，支撑前缀、模糊、通配、正则、乱序与子串查询。 | `core/std/index_engine_std.h:96` |
| 全文倒排索引 | Full-text inverted index | 释义文本倒排表、TF/IDF 和可持久化的 `UDFT1/2/3`；分词器版本参与缓存签名。 | `core/std/fulltext_index_std.h:15` |
| 查询键规范化 | Query-key normalization | 把输入折叠成可比较查词键的有损档位，同时保留原文用于展示。 | `core/std/text_norm_std.h:42` |
| 聚合查询管线 | Aggregated lookup pipeline | 精确、词形还原、前缀、释义匹配、模糊五层降级，并按词典分组返回。 | `core/std/dictionary_manager_std.h:117` |
| 词条渲染器 | Entry renderer | 词条进入 UI 前执行白名单清洗、纯文本提取、命中标注和富文本渲染的 std 实现。 | `core/std/html_renderer_std.h:21` |
| 资源解析器 | Resource parser | MDX 伴生资源编解码器，把规范键映射到解压块，并按需写入缓存文件。 | `core/std/mdd_resource_std.cpp:986` |
| 用户状态存储 | User state store | 保存查词历史、生词本、笔记、颜色、技能和发音记录的单一 `unidict.json`；写入即时落盘。 | `core/std/data_store_std.h:63` |
| 词典状态 | Dictionary state | 源路径、启用态、优先级、标签和隔离失败的 JSON 投影；qmlui 由环境驱动，GUI 会持久化。 | `core/std/dictionary_manager_std.cpp:971` |
| 同步物化状态 | Sync materialized state | 指令回放得到的词、笔记、标签、历史、偏好和词典清单，也是快照负载。 | `core/std/sync_engine_std.h:80` |
| 同步指令 | Sync command | 带设备级 `op_id` 的类型/值负载；本地入队立即生效并进入待发队列，中继重放幂等。 | `core/std/sync_engine_std.cpp:454` |
| 密封传输 | Sealed transport | 客户端加密包装器，把中继可见密文转成标准传输调用。 | `core/std/sync_sealed_transport_std.h:24` |
| 中继协议 | Relay protocol | 组、顺序指令、位点、幂等追加和快照的 HTTP v1 契约。 | `server/sync_relay/PROTOCOL.md:1` |
| 中继状态 | Relay state | 宿主侧组 binlog 和快照；按部署分别存于 D1 表、行式文件或进程内存。 | `server/sync_relay/worker/schema.sql:1` |
| 局域网宿主 | LAN host | 内嵌中继，提供同一 HTTP 路由并应答 UDP 发现，但不见负载明文。 | `server/sync_relay/cpp/lan_host_std.h:23` |
| 推理适配器 | Inference adapter | 可选 onnxruntime 边界，把 PCM 和词表转成 CTC/GOP 发音评分；评分逻辑仍在核心。 | `adapters/pron/onnx_pron_scorer.h:22` |
| 语音适配器 | Speech adapter | QML 可调用的 TTS/网络/播放门面，选择本地或在线发音并暴露可用性与状态。 | `qmlui/voice_adapter.cpp:166` |
| 平台采集 | Platform capture | 剪贴板轮询、选区读取、热键、开机启动和托盘支持，把桌面事件转成查词或壳动作。 | `qmlui/platform_adapter.h:1` |
| Qt 桥 | Qt bridge | 只做 Qt 类型和平台原语到 std-core 调用转换的薄包装，避免 Qt 进入 `core/std`。 | `adapters/qt/CMakeLists.txt:1` |
| 构建能力矩阵 | Build capability matrix | 让 std core、CLI、Qt 应用、发音推理、崩溃采集和测试可独立裁剪构建的 CMake 开关。 | `cmake/BuildOptions.cmake:74` |

