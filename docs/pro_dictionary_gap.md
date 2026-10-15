# Unidict 作为“专业词典”仍缺的关键能力清单

本文从“可日常替代 GoldenDict / 欧路 / macOS Dictionary、并服务重度语言学习”的角度，盘点 Unidict 已具备能力与仍明显缺失/不完整的能力，并给出优先级建议（P0 最高）。

> 口径：只按仓库当前实现（core/std + Qt adapters + qmlui/cli）做差距分析；“词典内容本身是否包含例句/音频”等不在本清单范围，但需要 App 能承载与呈现这些内容。

> **2026-10-13 刷新（巡检复核）**：初版列出的多数 P0/P1 已交付（下附
> 逐条命运标注）。
>
> **2026-10-16 刷新（巡检复核）**：text-norm UI profile 已交付（48eff28，
> 三档设置页 + core/engine/解析器全线消费）；非 X11 划词实为陈旧条目
> ——F13a/F13b 焦点窗选区读取已交付并被取词窗消费。当前真实剩余缺口
> 收敛为：Wayland 热键（外部协议门槛）、真实词典兼容性矩阵（资产/
> 授权门槛）；其余开口（云同步/商店/插件/主题/原生通知）均在用户门槛
> 或 §A 已定放弃，不再列为可自主推进。

## 已具备（足以算“可用”）

- 离线词典加载：`MDict(.mdx)`、`StarDict(.ifo/.idx/.dict/.dict.dz)`、`DSL(.dsl)`、`JSON/CSV/TSV`
- 多种查找：`exact/prefix/fuzzy/wildcard/regex`，以及全文检索（倒排索引 + TF/IDF + 持久化/版本协商）
- 富文本渲染链路：`html_renderer_std`（HTML/CSS 子集 + 安全清洗）+ 资源解析/缓存/URL 重写（MDict `.mdd` → `mdd_resource_std`；StarDict 资源 → `stardict_resource_std`）+ 文本节点级命中高亮（`highlightHtml`）
- 交叉引用：`@@@LINK=` 跳转与历史栈（`cross_reference_std`，std 侧维护 / Qt 侧展示）
- StarDict `ifo` 关键字段：`sametypesequence`/`charset`/`description` 解析 + `charset_codec_std` 编解码与漏写/错标兜底（`charset_salvaged`）
- 生词本与历史：本地持久化、导出 CSV、基础复习/学习页（QML）
- 多词典工作流：按优先级聚合（`search_grouped` 五层降级 + 组内词头去重）、启用/禁用、优先级 P↑/P↓、分组、失败隔离 + 重试/移除
- 快速取词：剪贴板监听、划词取词（X11 选中即查）、悬浮取词窗、全局热键三桌面平台（Windows RegisterHotKey / macOS Carbon / Linux XGrabKey——Wayland 会话注册时诚实失败）
- 系统托盘（F11）与开机自启（F9，Win 注册表 / Linux XDG / macOS launchd）
- 基础同步：`SyncServiceQt`（本地 JSON 文件双向合并 + 预览/选择导入导出）+ 加密备份恢复（`sync_backup_std`，PBKDF2 + XChaCha20-Poly1305）
- AI 与语音：`AiServiceQt`（外部命令桥接 + mock fallback）、Qt `TextToSpeech`；gui 面发音评分实验功能（录音/评分/模型自举，默认不构建）

## 缺失/不完整（“专业词典”关键差距，2026-10-13 复核）

### P0：词条呈现与资源（决定体验上限）——初版三项已全部交付
- ~~富文本/HTML 渲染不完整~~ → 已交付（`html_renderer_std` + `sanitizeHtml`/`presentEntry` 管线）
- ~~资源文件（图片/音频）链路缺失~~ → 已交付（`mdd_resource_std` + `stardict_resource_std` + `rewriteResourceUrls`/`loadDictionaryResourceData`）
- ~~交叉引用/链接不可点击~~ → 已交付（`cross_reference_std` + `navigateToWord`/`goBack` 历史栈）

### P0：词典兼容性与编码（决定能否“加载你现有的词典库”）
- ~~StarDict `ifo` 关键字段未完整支持~~ → 已交付（`sametypesequence`/`charset`/`description` 全解析 + `charset_codec_std` 兜底）
- **真实词典兼容性矩阵**（仍缺）：仓库不存词典资产（既定口径），兼容性以合成夹具覆盖；真实词典矩阵需 CI 资产注入，尚未系统化
- ~~字符归一化缺 UI 配置入口~~ → 已交付（2026-10-16，48eff28）：`text_norm_std` 三档 profile（宽容/谨慎/精确）设置页 radio + `textnorm/profile` 落盘 + 启动装载前落档；core 面 engine 重灌与 json/stardict/mdict 解析器折叠缓存同步换档，查询/去重/建议链全线消费同档 Options

### P1：结果组织与多词典工作流（决定“查得到且好用”）——已全部交付
- ~~多词典结果的聚合展示~~ → 已交付（`search_grouped` 五层降级 + 组内词头去重，UI/CLI 单口径）
- ~~词典管理能力不足~~ → 已交付（启用/禁用、优先级、分组、失败隔离+重试）
- ~~搜索高亮与片段预览~~ → 已交付（`highlight_term` 文本节点级标注 + 全文/例句 snippet）

### P1：快速取词（专业用户的“第二入口”）——主链已交付，剩协议门槛
- ~~剪贴板监听/全局热键/悬浮窗~~ → 已交付（剪贴板监控 + 悬浮取词窗 + 三桌面平台热键 F10）
- ~~系统托盘/菜单栏~~ → 已交付（F11，qmlui/gui 同形状；托盘不可用自动降级直接退出）
- ~~选中文本取词非 X11 平台~~ → 已交付（F13a Windows UIA / F13b macOS AX，`PlatformAdapter::readFocusedSelection` 分发，取词窗消费；热键触发的跨进程选区抓取分支已决不做——剪贴板取词承载同一意图，见 platform_adapter.cpp 落点注释）。2026-10-16 复核：本条此前误标「仍缺」，系文档陈旧
- **Wayland 全局热键**（仍缺）：Wayland 会话无 XGrabKey 路径，当前诚实失败；需等全局快捷键协议/portal 方案（外部协议门槛）

### P1：文本归一化 UI（初版 P0 归一化的剩余面）——已全部交付
- ~~text-norm UI profile 配置~~ → 已交付（2026-10-16，已代决——PATROL1016 授权口径：core 已备 + 100% 门控下能力对用户不可见，属「可自主决策直接拍板」面；三档 radio + 落盘 + 查询链消费，48eff28）

### P2：同步、隐私与可迁移性
- ~~完整备份/恢复~~ → 已交付（`sync_backup_std`：加密备份覆盖词条/笔记/标签/历史/装过的词典，恢复按缺失项合并）
- **云同步/多端冲突**：P-10 server 面（红线：仅当用户显式推进；永不进查词核心路径）

### P2：语言学习深度（从“能查”到“能学”）——§A 已定放弃/不动项
- SRS 算法与学习进度 UI：背单词/学习进度属 §A 明确不做（“简单可预期”定位）
- Anki/通用格式导出：§A 已定放弃（见 roadmap Import/Export 节删除线）
- ~~发音练习/评测~~ → 实验功能已交付（gui 面录音/逐音素反馈/模型自举，默认不构建）

### P3：插件生态与在线能力——已定放弃或用户门槛
- 第三方插件动态加载：§A 已定放弃（走 std 解析器工厂注册表）
- 在线词典/词典商店：P-10 用户门槛

## 建议的下一步（按价值/难度比排序，2026-10-16 复核）

1. **P0：真实词典兼容性矩阵**：CI 资产注入 + 逐格式回归（解锁条件：词典资产与授权来源，用户门槛）
2. **Wayland 全局热键**：等全局快捷键协议/portal 方案（外部协议门槛）
3. 其余开口（云同步/商店/插件/主题/原生通知）均在用户门槛或 §A 已定放弃，不列入自主推进

> 2026-10-16 已代决交付：P1 text-norm UI profile（原排序 #1，48eff28）；
> 非 X11 划词条目系文档陈旧（F13a/F13b 已交付），已订正。
