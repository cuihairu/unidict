# Unidict 实施需求清单

> 本文是需求清单：一条需求一句话；完成项保留一行证据（commit hash + 测试数）。
> 路线图与差距表：docs/roadmap.md、docs/pro_dictionary_gap.md；现状：docs/CURRENT_FEATURE_MATRIX.md；债务：docs/TECH_DEBT.md。
> 当前门禁基线：build ctest 161/161 · build-std 139（-R _std 子集 136）· coverage core lines 100% · qmllint 0。
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

- [x] P-0 基线修正：三份基线文档与当前代码逐行对准（2026-10-05 收口）
- [x] P-3 Core 稳定性：Dictionary 与 DictionaryManager 分离 + searchGrouped 降级语义进 std + 解析器工厂注册表（021c55c、3580d69、7cbc6ff）
- [x] P-4 查询体验：命中词高亮 + snippet 预览 + 文本节点级标注 + 结果区欧标重排（927bbb1、8b8d6b3、29cdba4、759f93e）
- [x] P-5 Quick Lookup：剪贴板/热键悬浮取词窗（贴光标浮窗 + 四动作 + 开关持久化）
- [x] P-6 Dictionary Library UI：导入/扫描/进度/后台索引（批次一~九 2026-10-09 收口：8653b14、34e5020 等）
- [x] P-7 Vocabulary 收敛：游戏化死码清理 + 双存储合一 + 四技能数据模型 + 复习入口简单化（2026-10-10 收口）
- [x] P-8 Speech：TTS 输出面 + 口音面（M4/M5 交付，被 pronunciation-plan 吸收）
- [x] P-9 AI Provider 抽象：AiProvider 接口 + CommandAiProvider + HeuristicAiProvider（core 不带 AI，失败无感）
- [ ] P-10 Server：仅当用户显式推进（account / catalog / distribution / sync）；永不进查词核心路径（账号面已取消，sync 由 B 系列承载）
- [x] P-11 产品体验三线：查词能力线（模糊层 93dacde / lemma 75f808f / Did-you-mean / CLI 结构化）+ UI 重设计 D1-D4（2496b7c 等）+ 字典随包 ≥6 部（10806a3）
- [x] 崩溃采集批：Crashpad 集成 + docs/crash_report_plan.md 简档
- [x] B 系列同步服务重构：B1-B7 全交付（中转核心 / 引擎 / 密码学原语 / SPAKE2+ / 设备面 / unidict-relay 进产物 / 安装清单 / UI 自救口；红线：同步默认关、中转只见密文）
- [x] 字典本职质量批：大词库性能（eb1bb35）/ 聚合计分 / StarDict 资源链 / EPUB 版式 / MDict 样本 / legacy 残留全收口（702fe39、9ccf0bf）
- [x] UI 四批（2026-10-10 批）：结果页 / 生词本双视图 / 手机端 / 取词浮窗层级优化（0d743f9、00cd121 等，2026-10-12 收口）
- [x] 巡检续批 F1-F8：CSV 导入 / 优先级排序 / 历史置顶 / 亮暗切换 / 历史逐条删除 / CSV 导出 / 释义字号 / 词典移除（1aa8c2b、e066da8 等）
- [x] 巡检续批④：Linux/macOS 开机自启实装 + qmlui 接线（用户点名候选）
- [x] 巡检续批⑤ F10-F12：全局热键三平台（e6f80ce）/ 系统托盘 parity / 全文索引重建；macOS CI 两轮修复（b8dbc5d、1d12740）
- [x] Android 续作 M7：Quick Lookup / Share 面（2026-10-07 交付；iOS/HarmonyOS 未开始，见 D）

## C. 下一批候选（按建议顺序消费，已收口）

- [x] 候选 #1-#4 消费：F10 热键 / F11 托盘 / F12 索引重建 / F9 自启（见 B 节续批④⑤）
- [x] F13a Windows quick_lookup 热键读焦点窗选区（UIA TextPattern 触发式，空回落剪贴板）：3b3bf43、54ef2ef、edf91f9；windows CI 回环绿
- [ ] F13b macOS 焦点窗选区读取（AX API）：待用户拍板（解锁条件 = TCC 辅助功能授权交付方式定案）
- [x] patrol-20261013-A1：未勾项与全仓 deferred 开口复核——全数卡在用户门槛/凭据，零代码零 commit
- [x] docs 过期面刷新（2026-10-13）：pro_dictionary_gap 全量复核 + TD-151 删线 + 跨平台边界口径扫净

## D. 决策记录（2026-10-10 全部拍板）

- 双桌面壳出口选型：QML（unidict_qml）为唯一桌面主出口；gui（QWidget）保留为发音练习特性壳
- 云同步 S1–S5：默认不动，仅用户显式推进；B 系列（binlog 模式）在轨
- iOS / HarmonyOS：维持「低优先级填空」不排期（iOS 走 macOS runner、鸿蒙编译过即可）；启动须用户显式推进
- P-10 Server 范围：维持「仅用户显式推进」不启动；永不进查词核心路径

---

## 历史存档索引（2026-09 盘点流水，均已收口；细节见 git log 与 docs/TECH_DEBT.md）

- 覆盖率缺口盘点：std 核心库 lines 100% 收口（P0/P1 补测批，2026-09-25 → 2026-10-04）
- 分支缺口巡检：B-1…B-12 簇 + path_utils 收口（39659f9、2a28278）；charset_codec 收口（2026-10-10）；剩余死边既定止步口径
- Qt 层缺口：legacy Qt 测试批处置（test_lookup_adapter 等 target 承接）
- TD 收口：TD-101 / 104 / 105 / 112-115 / 122 / 132 / 151 / 153（现状见 TECH_DEBT.md）
- 移动端：M0 选型 / M1-M3 壳 / M4 TTS / M5 口音 / M6 / M7 Quick Lookup + Share / M8-A 悬浮取词 / M8-B 桌面 widget（docs/mobile_plan.md）
- 门禁计数：真点审计 121→124→126→134；build 161/161、build-std 139、coverage core lines 100%
- 2026-10-04：online_pron 收口 + Phase 1 实现审计（docs-only）
