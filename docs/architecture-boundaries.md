> 语言: **简体中文** | [English](./architecture-boundaries.en.md)

# Unidict 架构边界

> Phase 2 · 2026-10-04 · 规则性文档：违反以下任一条 = 架构债（登记 TECH_DEBT，勿静默破口）。
> 现状核对：docs/CURRENT_ARCHITECTURE.md（2026-10-04 审计基线，2026-10-14 增量刷新）。

## 1. 依赖方向（唯一）

**壳/App → 适配层 → core/std。core 不反向依赖。**

- `core/std/` 不 include Qt（**零例外**，实测 97 文件零命中）。`core/` 根现存两个 Qt legacy 残量：`path_utils.{h,cpp}`（QString 路径 API）与 `unidict_core.h`（词本 DTO），只编译进 Qt 适配目标 `unidict_core_qt`，不进 std 构建链——CI std-only job 的「Qt-free core boundary scan」步以白名单钉死这两个文件，白名单外出现 Qt include 即红。
- **core/std 是唯一知识源**：新领域逻辑一律进 std。`core/`（legacy 残量：path_utils/types/unidict_core 薄层）只出不进——收敛目标，不新增 legacy 逻辑。
- 适配层薄：桥只做类型翻译与平台交互，领域逻辑必须留在 std（历史教训：legacy DictionaryManager ~1200 行单例已随 TD-105 收口退役，2026-10-10 927f376——那种形状从来不是边界允许的）。
- 双实现是过渡态不是常态：`*ParserQt` 桥已随 legacy 链整链删除（TD-102 勾，2026-10-10）——桥不得被当作"接线完成"的证据。

## 2. 查词核心路径不发网络请求（local-first 硬约束）

- 查词、聚合、渲染、历史、生词本、笔记**全部离线可用**，含首次安装。
- 网络只允许出现在壳层的显式用户动作里（在线发音、AI、手动同步）。
- **Server 不进查词核心路径**——查一次词不依赖服务器、不依赖登录态。
- 违规即 P0 架构债（登记 TECH_DEBT 并回滚口径）。

## 3. AI 可替换性

- AI 能力走 provider 抽象（本地 / OpenAI 兼容 / 免费 / 自定义 / System AI），接口定义在壳层适配器面，core 不带 AI。
- AI 失败或缺省降级**无感**（返回空白/跳过而非报错打断）。
- 不绑定单一厂商；密钥/端点由用户自管（环境变量或设置面，不写死）。

## 4. 数据边界

| 类 | 语义 | 约束 |
|---|---|---|
| 词典文件 | 用户资产 | 只读加载（路径/env/扫描）；不入库、不提交仓库、**不同步内容** |
| 用户数据 | 历史/生词本/标签/笔记 | 本机持久化（JSON 现状），可导出（CSV），同步（若启用）只同步这类 |
| 设置 | 偏好 | 两分：**Account**（跨设备同步面）vs **Device**（本机）；键名收敛（TD-122 轨道） |
| 学习状态 | 统计/复习历史 | 单一事实源（TD-113 已收口：双存储合一进 DataStoreStd，2026-10-10）；四技能字段位已落（set_vocabulary_skill） |

加密：同步数据若走 Server 按 design/sync-engine.md 的 E2EE 语义；本机数据不强制加密（明示唯一下限：无明文日志）。

## 5. 测试与门禁

- core/std 测试零 Qt（`<cassert>`+main 或未来 Catch2）；Qt 面只在适配/壳层测试。
- 门禁口径：build-std 与 build(Qt) ctest 全绿 + `scripts/coverage.sh` lines 100% threshold（core 口径）。
- **新源码未经门禁不得提交**（先例：online_pron 未跑闸门导致 lines 回落 99.6% 后才补，fae5002 收口）。
- 测试只增不减；`GCOVR_EXCL` 必须带注释理由（结构性不可达才算数，不为凑数强凑）。

## 6. 演进方向（记录在案，不含实施）

这些是已登记的技术债轨道——**前四条已于 2026-10-10/11 批次收口勾结**（TD-101~105、TD-115、TD-122、TD-119），记录在案防回潮，任何新代码不得重开：

- ~~**Dictionary vs DictionaryManager 分离**（TD-101~105）~~：已收口（927f376）——词典实例与管理器解耦，std 单口径。
- ~~**渲染抽象**：`HtmlRenderOptions` 死字段收敛（TD-115）~~：已收口（七死字段移除）；显式 `DictionaryContent` 抽象未建（死字段既除，无现需求牵引，§8 口径下再议）。
- ~~**设置收敛**：settings_qt 键名领域化 + Account/Device 两分（TD-122）~~：已收口（十五键盘点零漂移）。
- ~~**UI 出口**：QML 壳与 QWidget 壳服务于同一 core（双壳 TD-119）~~：已收口（419d58b）——QML 定为唯一桌面主出口，gui 为发音练习特性壳；出口选型是壳层事务，不因此分裂 core。
- **CLI man 式**：纯查词与诊断（cli-std），学习管理在 GUI；CLI 不写历史（既有纪律，README）。
- **移动端**：原生壳 + JNI/FFI 复用 core/std（Android 已立；iOS/HarmonyOS 同构，未开始）。QML 移动壳（Main.qml）是已废路径，不复活。

## 7. 平台边界

- 平台特有功能（全局热键/开机自启/托盘）：三桌面平台实装（F9/F10/F11），平台边界处 **stub/失败必须明示、不假装可用**（现状：Wayland 会话热键注册诚实失败、macOS/Linux 无托盘环境降级为关窗即退出）。同一能力的三处 target 必须走同一口径 helper（热键 `unidict_hotkey_platform`、自启 `StartupLauncher`），借编方一并补齐——缺桥的漏补在 macOS CI 才暴露（undefined symbols）。
- Android 壳与桌面壳并列，都只经一层适配面消费 core——不出现第三套领域逻辑。
- 词典资产不随仓库分发（含测试 fixture 只使用生成/极小样例）。

## 8. 违反即债（登记即修的口径）

页面任何一条被违反：开 TECH_DEBT 条目 + 写清违反位置与恢复轨道；修复优先于新功能（§6 轨道）。