# Unidict 架构边界

> Phase 2 · 2026-10-04 · 规则性文档：违反以下任一条 = 架构债（登记 TECH_DEBT，勿静默破口）。
> 现状核对：docs/CURRENT_ARCHITECTURE.md（2026-10-04 审计基线）。

## 1. 依赖方向（唯一）

**壳/App → 适配层 → core/std。core 不反向依赖。**

- `core/`（含 `core/std/`）不 include Qt（现有纪律，CI 持续检查）。
- **core/std 是唯一知识源**：新领域逻辑一律进 std。`core/`（legacy）只出不进——收敛目标，不新增 legacy 逻辑。
- 适配层薄：桥只做类型翻译与平台交互，领域逻辑必须留在 std（现状：legacy DictionaryManager 1200 行单例是**待收敛**的历史债，见 TD-105，不是边界允许的形状）。
- 双实现是过渡态不是常态：`*ParserQt` 桥（现为测试专用面，TD-102）不得被当作"接线完成"的证据。

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
| 学习状态 | 统计/复习历史 | 单一事实源（现状双存储 TD-113，收敛轨道），预留四技能字段位 |

加密：同步数据若走 Server 按 design/sync-engine.md 的 E2EE 语义；本机数据不强制加密（明示唯一下限：无明文日志）。

## 5. 测试与门禁

- core/std 测试零 Qt（`<cassert>`+main 或未来 Catch2）；Qt 面只在适配/壳层测试。
- 门禁口径：build-std 与 build(Qt) ctest 全绿 + `scripts/coverage.sh` lines 100% threshold（core 口径）。
- **新源码未经门禁不得提交**（先例：online_pron 未跑闸门导致 lines 回落 99.6% 后才补，fae5002 收口）。
- 测试只增不减；`GCOVR_EXCL` 必须带注释理由（结构性不可达才算数，不为凑数强凑）。

## 6. 演进方向（记录在案，不含实施）

这些是已登记的技术债轨道，未来批次按序收敛，任何新代码不得加深：

- **Dictionary vs DictionaryManager 分离**（TD-101~105）：词典实例（解析/检索/资源）与管理器（注册/分组/历史/隔离）解耦，std 单口径。
- **渲染抽象**：`HtmlRenderOptions` 死字段收敛 → 显式 `DictionaryContent` 抽象（TD-115）。
- **设置收敛**：settings_qt 键名领域化 + Account/Device 两分（TD-122）。
- **UI 出口**：QML 壳与 QWidget 壳服务于同一 core（现状双壳 TD-119），出口选型是壳层事务，不因此分裂 core。
- **CLI man 式**：纯查词与诊断（cli-std），学习管理在 GUI；CLI 不写历史（既有纪律，README）。
- **移动端**：原生壳 + JNI/FFI 复用 core/std（Android 已立；iOS/HarmonyOS 同构，未开始）。QML 移动壳（Main.qml）是已废路径，不复活。

## 7. 平台边界

- 平台特有功能（全局热键/开机自启/托盘）：Windows 先行，其余平台 **stub 必须明示**（现状：Linux/macOS stub 带「revisit on demand」注记，TD-151）。
- Android 壳与桌面壳并列，都只经一层适配面消费 core——不出现第三套领域逻辑。
- 词典资产不随仓库分发（含测试 fixture 只使用生成/极小样例）。

## 8. 违反即债（登记即修的口径）

页面任何一条被违反：开 TECH_DEBT 条目 + 写清违反位置与恢复轨道；修复优先于新功能（§6 轨道）。