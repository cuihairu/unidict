# 源码分层架构

> 语言: [English](/architecture) | **简体中文**

本页记录通读源码后的运行时分层。层表示职责，不表示目录名。

## 分层总览

| # | 层 | 职责 | 代表实现 |
|---|---|---|---|
| L1 | 应用壳 | 启动一个产品面，初始化服务，拥有命令解析、窗口生命周期和事件循环退出。 | `qmlui/main.cpp:75`、`gui/main.cpp:1262`、`cli-std/main.cpp:304` |
| L2 | 表现与服务绑定 | 发布 UI 可调用对象，做 Qt/std 类型适配，并装配查词命中到语音等壳内信号。 | `qmlui/main.cpp:181`、`qmlui/main.cpp:229`、`qmlui/lookup_adapter.cpp:36` |
| L3 | 领域服务 | 拥有多词典聚合、用户学习数据、导入与隔离策略、确定性同步状态。 | `core/std/dictionary_manager_std.h:19`、`core/std/data_store_std.h:63`、`core/std/sync_engine_std.h:103` |
| L4 | 知识编解码 | 把外部词典格式与伴生资源读入领域对象。 | `core/std/parser_registry_std.cpp:15`、`core/std/mdd_resource_std.cpp:986` |
| L5 | 检索与词条呈现 | 规范化查词键，建立词形/全文索引，排序结果，清洗并重写词条 HTML。 | `core/std/index_engine_std.h:39`、`core/std/fulltext_index_std.h:15`、`core/std/html_renderer_std.h:21` |
| L6 | 智能与感知 | 提供可选 ONNX 发音评分、本地/在线语音、剪贴板/选区采集、热键和桌面壳支持。 | `adapters/pron/onnx_pron_scorer.h:22`、`qmlui/voice_adapter.cpp:166`、`qmlui/platform_adapter.h:1` |
| L7 | 同步通信 | 通过 HTTP 传输密封指令，宿主中继协议，发现局域网设备，并向壳暴露状态。 | `adapters/qt/sync_http_transport_qt.cpp:58`、`server/sync_relay/cpp/relay_routes.h:1`、`server/sync_relay/cpp/lan_host_std.cpp:1` |
| L8 | 状态基础设施 | 按 durability 与兼容性要求持久化用户数据、词典/同步状态、索引、缓存、设置、密钥和中继数据。 | `core/std/data_store_std.cpp:30`、`core/std/dictionary_state_std.cpp:184`、`adapters/qt/sync_manager_qt.cpp:52` |

## 模块/服务清单

| 服务/模块 | 层 | 状态存储 | 调用面 | 配置输入 | 生命周期 |
|---|---|---|---|---|---|
| QML 壳 | L1 | 不直接持有业务状态 | QML 视图调用上下文对象；`main` 拥有引擎与上下文对象生命周期。 | 随包词典候选和 `UNIDICT_DICTS`；`qmlui/main.cpp:133` | `QApplication` 后启动；上下文属性是栈对象，随进程销毁；`qmlui/main.cpp:181` |
| Widgets 壳 | L1 | `dictionary_state.json` | `MainWindow` 调 `dictMgr()` 和 `DataStoreQt`；对话框同时改词典与用户数据；`gui/main.cpp:67`、`gui/main.cpp:74` | `UNIDICT_STATE_DIR`、`UNIDICT_DICTS`、`UNIDICT_DICT_DIR`；`gui/main.cpp:74`、`gui/main.cpp:152` | 主窗前恢复状态；词典变更后显式保存；`gui/main.cpp:154`、`gui/main.cpp:247` |
| CLI | L1 | 按命令读写用户存储与缓存路径 | 直接调用 `DictionaryManagerStd`、`DataStoreStd`、导出器和可选发音适配器；`cli-std/main.cpp:304` | argv、`UNIDICT_DICTS`、`UNIDICT_MDICT_PASSWORD`；`cli-std/main.cpp:350`、`cli-std/main.cpp:336` | 解析 argv，在 `main` 内构造管理器，单命令后返回 |
| 查词门面 | L2 | 内存中的导入计数/队列和导航栈 | QML 调用 -> `DictionaryManagerStd`、`HtmlRendererStd`、`MddResourceParser`，历史经 `DataStoreQt`；`qmlui/lookup_adapter.cpp:36`、`qmlui/lookup_adapter.cpp:721` | `UNIDICT_DICTS`、内存中的 MDict 密码、QSettings 归一化档位；`qmlui/lookup_adapter.cpp:171`、`qmlui/main.cpp:185` | 随应用存活；导入队列经定时器步进，可取消复位；`qmlui/lookup_adapter.cpp:226` |
| 词典聚合器 | L3 | 内存中的 `DictionaryStd`、失败记录和索引缓存 | 门面/GUI/CLI 调查询、导入、资源、元数据和状态 API；`core/std/dictionary_manager_std.h:19` | `save_state/load_state` 与 text-norm 档位；`core/std/dictionary_state_std.cpp:184` | 随所属壳存活；增删/重置时重建或清空；`core/std/dictionary_manager_std.cpp:22` |
| 词典实例 | L3/L4 | 解析器、词表、资源解析器、源路径 | 聚合器经注册表装载，并转发查询与规范化；`core/std/dictionary_std.cpp:15` | text-norm 选项；MDict 密码经管理器传入；`core/std/dictionary_std.cpp:87` | 持有解析器/资源直到移除或清空 |
| 用户状态存储 | L3/L8 | 默认 `data/unidict.json`，可显式指定路径 | Qt 单例、JNI、CLI 调增删查/导出导入；`adapters/qt/data_store_qt.cpp:13`、`adapters/android/jni/store_jni.cpp:31` | `DataStoreQt::setStoragePath`、Android store path、CLI 导出导入路径；`adapters/qt/data_store_qt.cpp:17` | 首次访问懒加载；每次变更立即保存；`core/std/data_store_std.cpp:426` |
| 词形索引 | L5 | 内存映射/Trie；可选简单行式索引文件 | 聚合器变更和查询；CLI 可保存/加载；`core/std/index_engine_std.cpp:381`、`core/std/index_engine_std.cpp:397` | 归一化档位、索引文件路径；`core/std/index_engine_std.h:78` | 增删增量维护；批量导入或加载后显式重建；`core/std/index_engine_std.h:44` |
| 全文索引 | L5 | 内存倒排；`UDFT1/2/3` 文件 | 聚合器懒构建/预热/导出统计；门面或适配器保存加载；`core/std/dictionary_manager_std.cpp:711`、`adapters/qt/fulltext_manager_qt.cpp:39` | 内容签名加分词器版本；CLI 保存/加载/升级选项；`core/std/fulltext_index_std.h:19`、`cli-std/main.cpp:377` | 首查懒建，导入后预热，词典集变化时重置；`qmlui/lookup_adapter.cpp:210` |
| 词条渲染/资源解析 | L5 | 内存资源键表；解出文件位于 `CacheLocation` | 查词门面和 GUI 在展示前清洗/重写/提取资源；`qmlui/lookup_adapter.cpp:347`、`gui/main.cpp:186` | 缓存目录、MDD 源路径；`qmlui/lookup_adapter.cpp:352` | 渲染器无状态；资源解析器随词典存活；缓存跨请求 |
| 发音评分 | L6 | 模型/词表文件和内存推理张量 | GUI/CLI 加载资产后调用 scorer；GOP 逻辑仍在核心；`gui/main.cpp:55`、`adapters/pron/onnx_pron_scorer.cpp:1` | `UNIDICT_BUILD_PRON`、模型/词表路径、拉取模型命令；`cmake/BuildOptions.cmake:68`、`cli-std/main.cpp:337` | 构建期可选；会话/模型按需加载；CLI 评分后结束 |
| 语音/取词感知 | L6 | QML 属性、TTS/播放器状态、剪贴板缓存 | 壳把查词命中接到语音；QML 调 voice/platform；`qmlui/main.cpp:190`、`qmlui/MainDesktop.qml:527` | `ui`、`selection`、`quicklookup` 下 QSettings 键；`qmlui/MainDesktop.qml:598` | 对象随壳存活；监控按用户开关启停；`qmlui/clipboard_monitor.cpp:14` |
| 客户端同步引擎 | L7/L3 | `engine_state.json`；元数据在 QSettings | `SyncManagerQt::syncNow` 经密封传输推拉；当前 enqueue 主要由核心/测试驱动，备份则物化存储快照；`adapters/qt/sync_manager_qt.cpp:214`、`core/std/sync_engine_std.cpp:454` | `sync/enabled`、`sync/gid`、`sync/relayUrl`、`sync/hostedUrl`、`sync/transportForm`；`adapters/qt/sync_manager_qt.cpp:34` | 构造时恢复状态；默认关闭；显式 enable/sync/save；`adapters/qt/sync_manager_qt.cpp:81` |
| 中继协议服务 | L7 | D1（`groups`、`ops`、`snapshots`）、C++ 行式文件或进程内存 | 客户端 JSON over HTTP；LAN 宿主复用 `RelayRoutes`；`server/sync_relay/PROTOCOL.md:25`、`server/sync_relay/cpp/lan_host_std.h:37` | bind host/port、data dir、Worker `DB` 绑定与限额；`server/sync_relay/cpp/main.cpp:23`、`server/sync_relay/worker/src/worker.js:15` | CLI relay 运行到停止；LAN 宿主显式启停；Worker 按请求隔离并懒建 schema；`server/sync_relay/worker/src/worker.js:114` |

## 配置面

- **构建拓扑**：`UNIDICT_BUILD_STD_CORE`、Qt core/adapter/apps/tests、CLI、crash、pronunciation 等开关定义在 `cmake/BuildOptions.cmake:74`，根组装在 `CMakeLists.txt:85`。
- **运行时路径**：`UNIDICT_DATA_DIR`、`UNIDICT_CACHE_DIR` 覆盖 std 默认路径（`core/std/path_utils_std.cpp:24`）；`UNIDICT_DICTS` 在两个壳中使用平台路径分隔符（`qmlui/lookup_adapter.cpp:21`、`gui/main.cpp:163`）；`UNIDICT_STATE_DIR` 重定向 Widgets 词典状态（`gui/main.cpp:74`）。
- **UI 设置**：`SettingsQt` 通过 `QSettings` 写 INI（`adapters/qt/settings_qt.cpp:1`）；QML 读写 `ui/theme`、`ui/lang`、`textnorm/profile`、`selection/enabled`、`quicklookup/enabled` 等键（`qmlui/MainDesktop.qml:41`、`qmlui/MainDesktop.qml:224`、`qmlui/MainDesktop.qml:615`）。
- **协议 schema**：中继请求/响应与限额在 `server/sync_relay/PROTOCOL.md:25`；D1 表 schema 同时落在 `server/sync_relay/worker/schema.sql:1` 与 Worker 代码（`server/sync_relay/worker/src/worker.js:32`）。

## 调用通道

1. **进程内直接 C++ 调用**连接 CLI/壳与领域服务；查词、导入、渲染、评分都不经 IPC。
2. **Qt 信号与上下文属性**连接 QML/C++，并把查词命中与语音解耦（`qmlui/main.cpp:190`、`qmlui/main.cpp:229`）。
3. **JNI 句柄**把同一 std manager/store 暴露给 Android（`android/app/src/main/kotlin/dev/unidict/mobile/UnidictCore.kt:63`、`adapters/android/jni/dict_jni.cpp:1`）。
4. **JSON over HTTP**只用于同步中继客户端、自建中继和局域网直连（`adapters/qt/sync_http_transport_qt.cpp:58`、`server/sync_relay/cpp/relay_routes.h:1`）。
5. **UDP 发现**只携带元信息，数据仍走 HTTP 中继契约（`server/sync_relay/PROTOCOL.md:147`）。
6. **外部进程**只出现在 CLI 模型拉取的 curl 配置桥（`cli-std/main.cpp:63`）。

## 拆分理由

1. **L1 独立，因为三个产品的生命周期契约不同。** CLI 解析 argv 后退出（`cli-std/main.cpp:304`）；QML 先装配上下文属性再加载主窗（`qmlui/main.cpp:229`、`qmlui/main.cpp:270`）；Widgets 还维持窗口几何与设置（`gui/main.cpp:294`）。
2. **L2 是 Qt 边界，不是重复包装。** `LookupAdapter` 持有核心不知道的 QML 导入进度、stamp 和导航栈（`qmlui/lookup_adapter.h:18`、`qmlui/lookup_adapter.h:173`）；`DataStoreQt` 用 Qt 类型包住 `DataStoreStd` 并提供单例（`adapters/qt/data_store_qt.cpp:13`）。
3. **L3 承担跨书与跨设备策略。** 启用态、优先级、标签、隔离和分组降级集中在 `DictionaryManagerStd`（`core/std/dictionary_manager_std.h:19`、`core/std/dictionary_manager_std.h:117`）；确定性回放属于 `SyncEngineStd`（`core/std/sync_engine_std.h:125`）。
4. **L4 独立，因为不同格式有不同失败规则和伴生文件。** 注册表工厂把「不支持扩展名」和「解析失败」分开（`core/std/dictionary_std.cpp:19`）；MDX 挂伴生 `.mdd`，资源失败不拒绝词典本体（`core/std/dictionary_std.cpp:56`）。
5. **L5 独立，因为查询成本与缓存失效和领域所有权不同。** 词形键有归一化版本（`core/std/text_norm_std.h:86`）；全文倒排有分词器版本和签名（`core/std/fulltext_index_std.h:19`）；HTML 清洗是独立 std API（`core/std/html_renderer_std.h:21`）。
6. **L6 可选且受平台限制。** ONNX 只有 `UNIDICT_BUILD_PRON` 打开才链接（`cmake/BuildOptions.cmake:68`、`adapters/pron/CMakeLists.txt:68`）；剪贴板/选区依赖 Qt 平台能力（`qmlui/platform_adapter.h:1`、`qmlui/clipboard_monitor.cpp:14`）。
7. **L7 只在字节跨进程/设备时存在。** 客户端先密封再 HTTP（`core/std/sync_sealed_transport_std.h:24`、`adapters/qt/sync_http_transport_qt.cpp:214`）；中继只校验、排序密文，不解释明文（`server/sync_relay/PROTOCOL.md:7`、`core/std/sync_relay_state_std.cpp:21`）。
8. **L8 独立，因为各存储的 durability 与兼容契约不同。** `unidict.json` 每次变更即时写（`core/std/data_store_std.cpp:426`）；词形索引是行式文件（`core/std/index_engine_std.cpp:384`）；全文索引是版本化二进制（`core/std/fulltext_index_std.cpp:332`）；同步状态服务离线续传（`core/std/sync_engine_std.cpp:629`）；中继数据按形态落 D1 或行式文件（`server/sync_relay/worker/schema.sql:1`、`core/std/sync_relay_state_std.cpp:45`）。

## UI、文案与协议落位

- **运行时文案**内联在双语 QML，中英文与控件同位（`qmlui/MainDesktop.qml:231-232`、`qmlui/MainDesktop.qml:309-311`）。
- **可复用视觉语言**集中在主题 token 和图标路径表（`qmlui/theme_tokens.h:41`、`qmlui/components/Icon.qml:27`）。
- **产品文档文案**在 VitePress 源页面，`docs/site/*.md` 与 `docs/site/zh/*.md` 同构镜像；页面注册在 `docs/site/.vitepress/config.mts:13`。
- **协议文案**唯一真源是 `server/sync_relay/PROTOCOL.md:1`；Worker、C++ 中继、LAN 宿主、客户端传输和测试实现同一契约。
