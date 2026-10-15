# Unidict 实施需求清单

> 本文是需求清单：一条需求一句话；完成项保留一行证据（commit hash + 测试数）。
> 路线图与差距表：docs/roadmap.md、docs/pro_dictionary_gap.md；现状：docs/CURRENT_FEATURE_MATRIX.md；债务：docs/TECH_DEBT.md。
> 当前门禁基线：build ctest 161/161 · build-std 139（-R _std 子集 136）· coverage core lines 100% · qmllint 零错误（语法闸门口径，非零告警）。
> 2026-10-13 整理（审计单第 7 条）：1952 行流水存档压缩为下方需求 + 证据索引。

## A. 明确不做（生效禁区，roadmap 同条废止）

- 广告 / 强制登录 / 信息流 / 排行榜 / 签到 / 成就 / 课程化 / 暗模式（产品原则 §7）
- OCR 截图/相机取词、PDF/Word 取词（非五感核心，属另一产品形态）
- 语音搜索（已有 TTS 输出面；语音输入查询不做）
- 在线翻译引擎集成（翻译走 AI 外部命令桥，已覆盖）
- Anki 导出、学习统计可视化、遗忘曲线自定义算法（与「个人笔记、简单可预期」定位冲突）
- 插件动态加载 / JS/QML 插件引擎 / 插件市场（走 std 解析器工厂注册）
- 云同步设计稿（docs/design/sync-engine.md S1–S5）默认不动，仅当用户显式推进
- 鼠标悬停取词、移动手势取词（Quick Lookup 由剪贴板/热键承载）
- 本地加密 / 隐私模式 / 安全擦除（明示无日志口径已够，不做假安全承诺）
- 背单词/复习玩法化：用户拍板暂不做（2026-10-10）；TD-114 复习 UI 与 §11 LearningState 字段位保持预留——不删、不接活 UI，字典本职收口后再议

## B. 在轨任务（全部完成；证据 = commit hash，门禁全绿）

- [x] P-0 基线修正：三份基线文档与当前代码逐行对准（44425f8）
- [x] P-3 Core 稳定性：Dictionary 与 DictionaryManager 分离 + searchGrouped 降级语义进 std + 解析器工厂注册表（021c55c、3580d69、7cbc6ff）
- [x] P-4 查询体验：命中词高亮 + snippet 预览 + 文本节点级标注 + 结果区欧标重排（927bbb1、8b8d6b3、29cdba4、759f93e）
- [x] P-5 Quick Lookup：剪贴板/热键悬浮取词窗，贴光标浮窗 + 四动作 + 开关持久化（146fbb2）
- [x] P-6 Dictionary Library UI：导入/扫描/进度/后台索引（批次一~九 2026-10-09 收口：8653b14、34e5020 等）
- [x] P-7 Vocabulary 收敛：游戏化死码清理 + 双存储合一 + 四技能数据模型 + 复习入口简单化（d195c93）
- [x] P-8 Speech：TTS 输出面 + 口音面（83b38f7）
- [x] P-9 AI Provider 抽象：AiProvider 接口 + CommandAiProvider + HeuristicAiProvider（e5976aa）
- [x] P-10 Server：维持不做；永不进查词核心路径（账号面已取消，sync 由 B 系列承载）
      已代决（2026-10-14 巡检授权）：维持不做——本地优先定位、账号面已取消、sync 由 B 系列承载，Server 侧不进查词核心路径
- [x] P-11 产品体验三线：查词能力线（模糊层 93dacde / lemma 75f808f / Did-you-mean / CLI 结构化）+ UI 重设计 D1-D4（2496b7c 等）+ 字典随包 ≥6 部（10806a3）
- [x] 崩溃采集批：Crashpad 集成 + docs/crash_report_plan.md 简档（6dbce45）
- [x] B 系列同步服务重构：B1-B7 全交付，红线同步默认关、中转只见密文（840c2da 至 174cdab）
- [x] 字典本职质量批：大词库性能（eb1bb35）/ 聚合计分 / StarDict 资源链 / EPUB 版式 / MDict 样本 / legacy 残留全收口（702fe39、9ccf0bf）
- [x] UI 四批（2026-10-10 批）：结果页 / 生词本双视图 / 手机端 / 取词浮窗层级优化（0d743f9、00cd121 等，2026-10-12 收口）
- [x] 巡检续批 F1-F8：CSV 导入 / 优先级排序 / 历史置顶 / 亮暗切换 / 历史逐条删除 / CSV 导出 / 释义字号 / 词典移除（1aa8c2b、e066da8 等）
- [x] Linux/macOS 开机自启实装 + qmlui 接线（F9；4cf3778）
- [x] F10-F12：全局热键三平台 + 系统托盘 parity + 全文索引重建（e6f80ce、b8dbc5d、1d12740）
- [x] Android 续作 M7：Quick Lookup / Share 面（88ee401；iOS/HarmonyOS 未开始，见 D）
- [x] patrol-20261013-B1：CI macOS 超时预算放宽 + 例句喇叭接线修复（fae684a、b03c726；build 161/161）

## C. 下一批候选（按建议顺序消费，已收口）

- [x] 候选 #1-#4 消费：F10 热键 / F11 托盘 / F12 索引重建 / F9 自启
- [x] F13a Windows quick_lookup 热键读焦点窗选区（UIA TextPattern 触发式，空回落剪贴板）：3b3bf43、54ef2ef、edf91f9；windows CI 回环绿
- [x] F13b macOS 焦点窗选区读取（AX API）：已代决(巡检 R1)：取 d) a+b 组合——首次取词弹一次系统提示（AXIsProcessTrustedWithOptions）+ 设置页内联引导（未授权提示 + 「打开系统设置」深链）直到授权。实装 MacSelectionReader（读取/授权/提示/深链四事）+ PlatformAdapter 接线 + 取词热键与设置页接线 + 审计诚实门控（97d2911；audit 144/0、build 162/162、std 139/139；CI 三平台全绿 38541818883）
- [x] patrol-20261013-A1：未勾项与全仓 deferred 开口复核（零改动；全数卡用户门/凭据）
- [x] patrol-20261014-A1：未勾项复核 + AUD-17 voice 接线漂移登记（P-10/F13b 仍挂用户门）（d138b4e）
- [x] patrol-20261014-B1：ci.yml paths-ignore 落地（docs-only 提交不再触发全量 CI；README 测试口径补齐）（6e33451；build 161/161 + std 136/136）
- [x] patrol-20261014-C1：审计抽查表 4 处行号漂移复核登记（AUD-19）（5ac8e19）
- [x] patrol-20261014-D1：CURRENT_ARCHITECTURE.md 补齐抽查（枚举面列名零抽查缺口；AUD-20…26）（9d3257a）
- [x] patrol-20261014-E1：product-principles.md 抽查（TD 待办口径漂移三处；AUD-27…29）（6276ab4）
- [x] patrol-20261014-F1：architecture-boundaries.md 抽查（已收口 TD 轨道口径五条；AUD-30…32）（abe47f0）
- [x] patrol-20261014-G1：dev.md 抽查（Qt 最低版本 6.2 改 6.5；AUD-34）（3fef265）
- [x] patrol-20261014-H1：全仓悬空文件引用扫查（PLUGIN_DEVELOPMENT.md 出库；dictionary_candidates.md 路径修正；MDICT_ENCRYPTION_RESEARCH.md 保留）（49b01e3）
- [x] patrol-20261014-I1：perf-benchmarks.md 抽查 + benchmark.sh 三缺陷修复（死 flag/CLI 定位/内存测试跨平台）（985046a；build-std 139/139）
- [x] patrol-20261014-J1：pronunciation-plan.md 抽查（M10 自举下载补录；M1-M10 已实装口径）（f7142b3）
- [x] patrol-20261014-K1：crash_report_plan.md 抽查（vendored 接入口径；uploadUrl 留位描述对齐实际签名）（a4fff21）
- [x] patrol-20261014-L1：gui-design.md 抽查（TD-119 桌面出口修订 banner）（b48991d）
- [x] patrol-20261014-M1：Qt-free 边界口径收口 + CI 边界扫描落地（architecture-boundaries/AGENTS.md 对齐现实；四处悬空行号注释改指）（52083e2；build 161/161 + std 139/139）
- [x] patrol-20261014-N1：gui 文档簇 artifact 换向回写（smoke-checklist 定位；ui-structure banner；gui-design 6.10.3 与主出口）（e3a404a）
- [x] patrol-20261014-P1：门禁基线四项全验（coverage 10301/10301；qmllint 零错误；gui-design 本地 Qt 模块口径）（172a230）
- [x] patrol-20261014-Q1：todo 台账格式归一（一行一需求、去箭头/emoji、完成行留证据）（791de16）
- [x] patrol-20261014-Q2：QML 属性遮蔽修复 Main.qml 同步预览 data→payload（qmllint duplicate-binding 26→0，零错误保持）（d6fb55a；build 161/161 + std 139/139）
- [x] patrol-20261014-R1：移动端同步预览修复（model.get 断链 8 处改走 delegate Label；预览/错误对话框尺寸钉 Dialog 本体 + contentWidth 钉视口宽打破 implicitWidth 死环；新增 S37 移动端审计场景）（dd7e099；audit 139/0、build 161/161、std 139/139）
- [x] docs 过期面刷新（2026-10-13）：pro_dictionary_gap 全量复核 + TD-151 删线 + 跨平台边界口径扫净（caf1e05、f6c55ba、0163773）
- [x] patrol-20261014-R2：macOS gui_click_audit 超时预算 300→900（APPLE 单独放宽、余平台维持 300，同 qmlui ui_sandbox 口径；161 例仅挂此 1 例、连红 3 轮）（e9150b3；configure 通过、Linux 侧 TIMEOUT 300 不变）
- [x] patrol-20261014-S1（点火标记 MK_unidict_R1）：F13b macOS 焦点窗选区实装（AX API + TCC 授权方案 d）+ 待拍板项复核代决（P-10 取 d 维持搁置；roadmap 六条逐条定性）（97d2911；audit 144/0、build 162/162、std 139/139）
- [x] patrol-20261014-S2：F13b macOS 回环用例 CI 守卫修正——CI 的 macOS runner 报已授权但 AX 读不到焦点元素，回环断言不成立；改为「已选中且已聚焦、AX 读回空」判定本会话读路径不可用→诚实 QSKIP，并加 textCursor().hasSelection() 前置证明排除假空（c161d8a；build 162/162；CI run 38541818883 三平台全绿）。注：macOS 端到端回环需真机授权会话验证，CI 仅验到跳过分支
- [x] patrol-20261014-T1（点火标记 MK_R3A_unidict）：P-10 Server 按用户 2026-10-14 巡检授权关闭（已代决：维持不做——本地优先定位、账号面已取消、sync 由 B 系列承载，不进查词核心路径）+ TD-152 现状复核勾（观测点「不得进默认安装包」三面实证：默认构建 OFF、模型不入 git、每日构建 stage 无模型无 ORT；docs-only 单 commit）；macOS AX 真机回环与 LLM provider 集成维持挂起（分别需真机授权会话与真实凭据+计费口径）
- [x] patrol-20261016-A1（核对码 PATROL1016）：roadmap open=11 逐条复核（复核条款路径，无合法开工项）——online store 与 multi-device cloud sync 随 P-10 关闭改口径「已裁决维持不做」；LLM（凭据+计费口径未到位）、custom theme（contrast 闸门仍在）、native notifications（同步默认关实证 sync_manager_qt.cpp:85、无后台任务）、mobile iOS/HarmonyOS（无推进信号）五条解锁条件不变维持原判；已裁决项零重开（docs-only 单 commit）

## D. 决策记录（2026-10-10 全部拍板）

- 双桌面壳出口选型：QML（unidict_qml）为唯一桌面主出口；gui（QWidget）保留为发音练习特性壳
- 云同步 S1–S5：默认不动，仅用户显式推进；B 系列（binlog 模式）在轨
- iOS / HarmonyOS：维持「低优先级填空」不排期（iOS 走 macOS runner、鸿蒙编译过即可）；启动须用户显式推进
- P-10 Server 范围：维持「仅用户显式推进」不启动；永不进查词核心路径

---

## 历史存档索引（2026-09 盘点流水，均已收口；细节见 git log 与 docs/TECH_DEBT.md）

- 覆盖率缺口盘点：std 核心库 lines 100% 收口（P0/P1 补测批，2026-09-25 至 2026-10-04）
- 分支缺口巡检：B-1…B-12 簇 + path_utils 收口（39659f9、2a28278）；charset_codec 收口（2026-10-10）；剩余死边既定止步口径
- Qt 层缺口：legacy Qt 测试批处置（test_lookup_adapter 等 target 承接）
- TD 收口：TD-101 / 104 / 105 / 112-115 / 122 / 132 / 151 / 153（现状见 TECH_DEBT.md）
- 移动端：M0 选型 / M1-M3 壳 / M4 TTS / M5 口音 / M6 / M7 Quick Lookup + Share / M8-A 悬浮取词 / M8-B 桌面 widget（docs/mobile_plan.md）
- 门禁计数：真点审计 121/124/126/134；build 161/161、build-std 139、coverage core lines 100%
- 2026-10-04：online_pron 收口 + Phase 1 实现审计（docs-only）
