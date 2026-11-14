# Unidict 实施需求清单

> 本文是需求清单：一条需求一句话；完成项保留一行证据（commit hash + 测试数）。
> 路线图与差距表：docs/roadmap.md、docs/pro_dictionary_gap.md；现状：docs/CURRENT_FEATURE_MATRIX.md；债务：docs/TECH_DEBT.md。
> 当前门禁基线：build ctest 164/164（ui_click_audit 环境性失败除外 163/163）· build-std 140（-R _std 子集 138）· coverage core lines 100% · 真点审计 147/0（2026-10-16 / 48eff28 起，text-norm profile 三新增测试文件 + S38 场景）。qmllint 零错误（语法闸门口径，非零告警）。
> 2026-10-13 首次整理（1952 行流水存档压缩）；2026-11-02 二次整饬（审计单第 7 条）：长叙述行压缩为需求句+单行证据，双行/多行子项归一。

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
- 背单词/复习玩法化（2026-10-10 用户拍板不做；TD-114 复习 UI 与 §11 LearningState 字段位仅预留、不接活 UI）

## B. 在轨任务（全部完成；证据 = commit hash，门禁全绿）

- [x] P-0 基线修正：三份基线文档与当前代码逐行对准（44425f8）
- [x] P-3 Core 稳定性：Dictionary/DictionaryManager 分离 + searchGrouped 降级语义进 std + 解析器工厂注册表（021c55c、3580d69、7cbc6ff）
- [x] P-4 查询体验：命中词高亮 + snippet 预览 + 文本节点级标注 + 结果区欧标重排（927bbb1、8b8d6b3、29cdba4、759f93e）
- [x] P-5 Quick Lookup：剪贴板/热键悬浮取词窗 + 贴光标浮窗 + 四动作 + 开关持久化（146fbb2）
- [x] P-6 Dictionary Library UI：导入/扫描/进度/后台索引（8653b14、34e5020 等）
- [x] P-7 Vocabulary 收敛：游戏化死码清理 + 双存储合一 + 四技能数据模型 + 复习入口简单化（d195c93）
- [x] P-8 Speech：TTS 输出面 + 口音面（83b38f7）
- [x] P-9 AI Provider 抽象：AiProvider 接口 + Command/Heuristic 两实现（e5976aa）
- [x] P-10 Server：维持不做、永不进查词核心路径（2026-10-14 巡检授权代决）
- [x] P-11 产品体验三线：查词能力线（模糊层 93dacde / lemma 75f808f / Did-you-mean / CLI 结构化）+ UI 重设计 D1-D4（2496b7c 等）+ 字典随包 ≥6 部（10806a3）
- [x] 崩溃采集批：Crashpad 集成 + docs/crash_report_plan.md 简档（6dbce45）
- [x] B 系列同步服务重构：B1-B7 全交付，同步默认关、中转只见密文（840c2da 至 174cdab）
- [x] 字典本职质量批：大词库性能（eb1bb35）/ 聚合计分 / StarDict 资源链 / EPUB 版式 / MDict 样本 / legacy 残留收口（702fe39、9ccf0bf）
- [x] UI 四批（2026-10-10）：结果页 / 生词本双视图 / 手机端 / 取词浮窗层级（0d743f9、00cd121 等，2026-10-12 收口）
- [x] 巡检续批 F1-F8：CSV 导入/导出、优先级排序、历史置顶、亮暗切换、历史逐条删除、释义字号、词典移除（1aa8c2b、e066da8 等）
- [x] F9 Linux/macOS 开机自启实装 + qmlui 接线（4cf3778）
- [x] F10-F12：全局热键三平台 + 系统托盘 parity + 全文索引重建（e6f80ce、b8dbc5d、1d12740）
- [x] Android M7：Quick Lookup / Share 面（88ee401）
- [x] patrol-20261013-B1：CI macOS 超时预算放宽 + 例句喇叭接线修复（fae684a、b03c726；build 161/161）

## C. 巡检与专项批次（全部完成）

- [x] 候选 #1-#4 消费：F10 热键 / F11 托盘 / F12 索引重建 / F9 自启
- [x] F13a Windows quick_lookup 热键读焦点窗选区（UIA TextPattern，空回落剪贴板）（3b3bf43、54ef2ef、edf91f9；CI 三平台绿）
- [x] F13b macOS 焦点窗选区读取（AX API + TCC 授权引导 + 设置页深链 + 审计诚实门控）（97d2911；audit 144/0、build 162/162、std 139/139）
- [x] patrol-20261013-A1：未勾项与 deferred 开口复核，零改动（全数卡用户门/凭据）
- [x] patrol-20261014-A1：未勾项复核 + AUD-17 voice 接线漂移登记（d138b4e）
- [x] patrol-20261014-B1：ci.yml paths-ignore 落地，docs-only 提交不再触发全量 CI（6e33451；build 161/161、std 136/136）
- [x] patrol-20261014-C1：审计抽查表 4 处行号漂移登记 AUD-19（5ac8e19）
- [x] patrol-20261014-D1：CURRENT_ARCHITECTURE.md 补齐抽查（AUD-20…26）（9d3257a）
- [x] patrol-20261014-E1：product-principles.md 抽查（AUD-27…29）（6276ab4）
- [x] patrol-20261014-F1：architecture-boundaries.md 抽查（AUD-30…32）（abe47f0）
- [x] patrol-20261014-G1：dev.md 抽查，Qt 最低版本 6.2→6.5（AUD-34）（3fef265）
- [x] patrol-20261014-H1：全仓悬空文件引用扫查（PLUGIN_DEVELOPMENT.md 出库、dictionary_candidates.md 路径修正）（49b01e3）
- [x] patrol-20261014-I1：perf-benchmarks.md 抽查 + benchmark.sh 三缺陷修复（985046a；build-std 139/139）
- [x] patrol-20261014-J1：pronunciation-plan.md 抽查，M10 自举下载补录（f7142b3）
- [x] patrol-20261014-K1：crash_report_plan.md 抽查，vendored 口径对齐（a4fff21）
- [x] patrol-20261014-L1：gui-design.md 抽查，TD-119 桌面出口修订 banner（b48991d）
- [x] patrol-20261014-M1：Qt-free 边界口径收口 + CI 边界扫描落地（52083e2；build 161/161、std 139/139）
- [x] patrol-20261014-N1：gui 文档簇 artifact 换向回写（e3a404a）
- [x] patrol-20261014-P1：门禁基线四项全验（coverage 10301/10301、qmllint 零错误）（172a230）
- [x] patrol-20261014-Q1：todo 台账格式归一（一行一需求、完成行留证据）（791de16）
- [x] patrol-20261014-Q2：QML 属性遮蔽修复 data→payload（qmllint duplicate-binding 26→0）（d6fb55a；build 161/161、std 139/139）
- [x] patrol-20261014-R1：移动端同步预览修复（model.get 断链 8 处改 delegate + 新增 S37 审计场景）（dd7e099；audit 139/0、build 161/161、std 139/139）
- [x] docs 过期面刷新（2026-10-13）：pro_dictionary_gap 全量复核 + TD-151 删线 + 跨平台边界扫净（caf1e05、f6c55ba、0163773）
- [x] patrol-20261014-R2：macOS gui_click_audit 超时预算 300→900（仅 APPLE 放宽）（e9150b3）
- [x] patrol-20261014-S1：F13b macOS 选区实装 + 待拍板项代决（P-10 维持搁置、roadmap 六条定性）（97d2911；audit 144/0、build 162/162、std 139/139）
- [x] patrol-20261014-S2：macOS 回环用例 CI 守卫修正（AX 读空判定诚实 QSKIP + textCursor 前置证明）（c161d8a；build 162/162）
- [x] patrol-20261014-T1：P-10 关闭代决 + TD-152 现状复核勾（模型不入默认包三面实证）；macOS AX 真机回环与 LLM provider 集成挂起待用户输入（2026-10-30 决议见 docs/review/2026-10-30-decisions.md）
- [x] patrol-20261016-A1：roadmap open=11 逐条复核，零重开、五条解锁条件维持原判（PATROL1016）
- [x] patrol-20261016-B1：text-norm UI profile 交付（三档 profile 落核 + 解析器折叠缓存换档 + QML 外观页三 radio + S38 审计场景；std 140/140、build 163/163、audit 147/0）（48eff28）
- [x] patrol-20261016-B2：roadmap 复核 + docs 漂移订正（text-normalization §4/§5、FEATURE_MATRIX §2）+ 门禁基线刷新 163/140
- [x] patrol-20261016-C1：docs/ 根目录 20 篇全量双语配对（README 正文 CJK 清零、42 文件 0 死链、表管线逐块一致）
- [x] patrol-20261016-D1：子目录双语收口（design/research/ui/site 16 对 32 文件、74 文件 0 死链）
- [x] patrol-20261016-F1：文档-代码增量审计 AUD-52/53（ARCHITECTURE.md 缺口摘要回写、TD-131 残余行订正）
- [x] patrol-20261016-E1：VitePress 单 locale 收口（rewrites + locales.root/en、i18nRouting=false、dist 23 页 0 死链）
- [x] audit-fix-20261112：审核修复批五项收口（scratch unidict-audit-fix.md；f24aeae，CI 绿 run 43091827414）
  - P0 同步重入死锁：SyncManagerQt::syncNow in-flight 闸门（嵌套循环重入当场拒绝「同步进行中」）+ SyncEngineStd 三段式重构（锁只护内存段，推拉网络收发全程不持锁；CAS 单飞闸门 RAII 复位；maybe_snapshot 同口径去锁跨网络）。回归：罐头 relay + 零间隔定时器重入复现（sync_manager_syncnow_qt_test）修前第二轮被放行、修后 3/3 绿；std 侧 ReentryTransport 回调内重入 sync + 窗口期 enqueue + 快照重建撞窗口指令收敛双探针（sync_engine_std_test 17 函数）修前 rc=124 同线程递归加锁挂死、修后全绿；race 测试按单飞合同改「闸门拒绝即重试」（三连跑稳定）。
  - .mdd 解压硬帽：decompress_zlib 改带期望解压长度（谎报拒绝：声明耗尽未解完/实际不足声明即拒；512MB 硬帽对齐 mdx_v2）。回归：513MB zlib 炸弹（流式 level-1 deflate）修前注册膨胀条目（rc=134）修后拒绝装载（mdd_resource_std_full_test）；谎报尺寸双 pin（gbig/gcap，mdd_mdict_std_test）。
  - 词典双份装载：FullTextManagerQt 新增借用 ctor（shared manager 跳过重装，空指针回落自持）+ LookupAdapter::dictionaryManager() 装配缝；qmlui main 单 manager。回归：借用跳装/空回落两臂（fulltext_manager_qt_test 33/33）。
  - Windows UTF-8 路径：u8_path 统一收口（stardict 解析器含 gzopen_w、stardict 资源、fulltext_index 存取、dictionary_manager 存取档）。回归：fs_path_std_test 字节保真 + 真实 FS 中日韩文件名建/写/读/删链路。
  - 联想查询预算：scan_budget_ms_（缺省 150ms，setScanBudgetMs 测试缝）进 suggestFuzzy/searchWildcard/relatedLookup。回归：2 万词条通配正则预算截停（预算 1ms 命中数 << 不设上限；预算参数丢弃即红，lookup_adapter_test 29/29）。
  - 验收：std 146/148（2 relay Not Run 本机基线）；Qt 170/173（ui_click_audit 本机 79 项环境性失败与 HEAD 同二进制逐字节同影、以 CI 三平台为准；2 relay Not Run）；Qt/std/vitepress 构建全绿；沙盒 40 张截图 32 张与 HEAD 逐字节同，余 8 张（suggest/notfound）经同二进制重跑对照定性为运行间抖动（same-binary 同影），非代码回归。
- [x] sweep-batch1-20261112：积压清剿 + 覆盖率拉齐批次 1 收口（scratch sweep-unidict.md；覆盖 2026-11-12 全天批次）
  - 修复笔：c6003d2（stardict 解析边界与目录扫描守卫）、875a821（导出备份写检 + tray 菜单所有权）、9c2e108（mdd 64 位 seek + 丢弃诊断 + 缓存名转义）、1f6c9ca（DSL 尾段 + ripemd 长度守卫）、f9edca6（mdict 属性边界 + zlib 扫描跳段）、ddf3885（词典管理器懒构建 mutex）、e6a486b（同步引擎公共入口序列化）、755a3f0（同步计数器负值钳 0 + 资源长度守卫）、8ce5a58（jni 句柄活注册表）、fdb944e（模糊候选词频加权）、f24aeae（同步重入闸门 + mdd 解压硬帽 + UTF-8 路径 + 联想预算）、eb83413（沙盒抽屉内 sync 提示对捕获）、0bc2fe2（产品图标 v2）。
  - 门禁笔：8e4e27c（gcovr 7 钉版 + 原子 profile 计数器）、acc70cd（通配扫描/utf8 切分/归一 profile 覆盖）、8d423ca（mdd 存储口径谎报三 pin：块表谎报 600MB 小条目过资源帽触 512MB 硬帽、comp_type 0 声明核对终检、info 声明 0 解压成功后核对拒——变异守卫即红 134、复原即绿）、1f1ba51（11-01 沙盒截图 32 张归档）、b8e7cc1（搜索索引对标双语篇）。
  - 覆盖率：core/ (std-only) lines 10684/10684 = 100%（8d423ca 收满；此前 99.9% 缺 mdd_resource_std.cpp 103/856/1020 三行，根因=单键夹具条目吃 10MB 资源帽进不到块层，gcapz 双键夹具后真触 512MB 硬帽行）。
  - 沿途勾账：深审 H 批 5 笔（sync_engine 负值、lookup_adapter 截断、store_jni/dict_jni、jni_util handle_of、UNI-FUZZY-RANK-001 词频加权）见上文各行 hash；CI 全绿（f24aeae run 43091827414、8d423ca run 43099692213；mac 一次 artifact 上传基建闪败 rerun --failed 即绿）。
  - 开放项去向（不属本批）：VULN-UNIDICT-001（BLOCKED·他人 stash-pop 冲突，等冲突方确认后 pnpm audit 终态）；LAY-001…010（源码分层拆分候选，独立排期）。

## D. 决策记录（2026-10-10 拍板；P-10 于 2026-10-14 巡检授权代决）

- 双桌面壳出口：QML（unidict_qml）为唯一桌面主出口；gui（QWidget）保留为发音练习特性壳
- 云同步 S1–S5：默认不动，仅用户显式推进；B 系列（binlog 模式）在轨已交付
- iOS / HarmonyOS：维持低优先级填空不排期（iOS 走 macOS runner、鸿蒙编译过即可）；启动须用户显式推进
- P-10 Server：维持不做，永不进查词核心路径

---

## 历史存档索引（2026-09 盘点流水，均已收口；细节见 git log 与 docs/TECH_DEBT.md）

- 覆盖率缺口盘点：std 核心库 lines 100% 收口（2026-09-25 至 2026-10-04）
- 分支缺口巡检：B-1…B-12 簇 + path_utils + charset_codec 收口；剩余死边既定止步口径
- Qt 层缺口：legacy Qt 测试批处置（test_lookup_adapter 等 target 承接）
- TD 收口：TD-101 / 104 / 105 / 112-115 / 122 / 132 / 151 / 153
- 移动端：M0 选型 / M1-M3 壳 / M4 TTS / M5 口音 / M6 / M7 Quick Lookup + Share / M8-A 悬浮取词 / M8-B 桌面 widget（docs/mobile_plan.md）
- 门禁计数：真点审计 121/124/126/134；build 161/161、build-std 139、coverage core lines 100%
- 2026-10-04：online_pron 收口 + Phase 1 实现审计（docs-only）

## E. 双语配对登记（2026-10-16 用户令「docs 补双语」；C1/D1/E1 三轮收口）

- [x] docs/design/ 3 篇 + docs/research/ 1 篇 + docs/ui/compare/ 1 篇配对（代码块/表行/图路径逐对相等）
- [x] docs/site/ 内容页 11 篇配对（front matter 特判已记；代码内容冻结）
- [x] VitePress 英文栏收口：rewrites + locales 双栏，英文页 lang/nav/sidebar 生效，20 链接改 /en/ 路由

## F. 待办（开放项）

- [x] UI-AUDIT · P2 · 原生桌面界面补审（impeccable 流程）：层级/排版/对比度 a11y/布局/边界态审计，报告+截图入 docs/review/，坏损当场修
  勾账 2026-11-09：离屏 40 图（10 视图 × 亮暗 × 中英）入 docs/review/ui-audit-2026-11-09/（md5 40/40 唯一、亮暗灰度均值 246/20 可区分）；test_theme_tokens_contrast 全绿（白/品牌玫瑰 6.58:1 AA）。2026-11-02 critique 的两个 P1（强调色分裂、图标 tofu）与条目层级 P1 均已解决——单一品牌玫瑰、SVG 图标、headword>音标>释义层级。边界态（空态/未收录/建议段/取词窗/词典库）全过。仅 3 条 P3 观感（底部极淡箭标疑离屏动画冻结、quicklookup 文件名标 1200×760 实为 420×260 浮窗、结果卡短条目留白大），无需修复。无 P0-P2。
- [x] DOC-GIF · P2 · 配置开关效果动态图：枚举全部生效开关，流水线生成 off/on 截图 + GIF，docs「配置效果」页双语嵌入，兼作配置效果回归测试
  勾账 2026-11-09：沙盒开关对趟（af8c81e）逐开关截「拨动前/拨动后」真帧——真点优先，本机离屏合成点带陈旧点位态点不进（QEventPoint state 被 delivery 翻成 Released，环境现象、CI 审计绿），回退真 handler（setLang/setThemeMode/applyNormProfile）或 checked/syncStateLabel setProperty 驱动，落位后属性级回读断言 + 帧差门双把关。9 对 GIF（clipboard/quicklookup/startup/dict_enable/sync/lang/theme/textnorm/aicache，selection 平台隐藏跳过）入 docs/site/config-effects/assets/，客观度量过验：theme 帧差 91 万 px（灰度均值 246.9→22.6 全窗翻暗）、lang 4.1 万 px、开关翻面 1-2.5 千 px；lang_0/quicklookup_0/startup_0 md5 同一（基线稳定）。双语「配置效果」页（config-effects.md + zh/）入 Usage 导航，VitePress build 过、9 GIF 双栏齐嵌；沙盒外开关（划词 X11、LAN 宿主、macOS 读取）表格登记。ctest：ui_sandbox_switch_effects 新测试 62-73s Passed；全套 166/167 绿，唯一挂的 ui_click_audit 是本机死点既档（79 FAIL 同档，CI 绿）。
- [x] SANDBOX-SYNC-RED [P1] CI ui_sandbox_switch_effects 三平台稳定红（自 af8c81e 引入起全红；run 42559282955 win/mac、42550878036 ubuntu 41.87s 亦挂——「仅 win/mac」系 ubuntu 作业死于 Setup Ninja 的假象）：2026-11-10 日巡只诊断不修钉根因，修复于同日（见勾账）——
  诊断 2026-11-10（离屏复现 + 几何倾倒，诊断脚手架未入库、已还原）：sync 对的效果控件 syncStateLabel（MainDesktop.qml:2387，visible: text.length>0）在抓帧滚动位落在抽屉 ScrollView 视口外——同步 tab 内容高 784px 装在 375px clip:true 视口，scrollIntoView(syncSwitch) 目标是内容首项、居中 target 为负钳到 contentY=0，视口只见顶部 375px；hint 文案落位后 label 才进 ColumnLayout 排到内容 y≈545，视口外 ~170px，setProperty 设 text 后整窗抓帧零像素变化 → 「frames identical (binding broken?)」。CI 三平台合成真点全死（既档离屏现象）→ 必走 fallback（设 text、属性回读虚过）→ 帧比较必败，非 flake。本机绿是假绿：clickWorked=(before!=after) 与终判帧差全由 Material 按压涟漪残影撑起（终判倾倒 textLen=0、vis=0——两次真点均未触发 onToggled），效果从未入帧；其余 9 对无恙（fallback 驱动的控件本体就在视口内）。
  勾账 2026-11-10：修复 eb83413——sync 对改 doSyncHintPair 自足路径：效果=提示文案出现，帧对以「提示区」为同滚动位（锚=确认按钮 syncApplyButton，before=提示空、after=提示文案现，帧差=真实效果非涟漪），clickWorked 以属性回读（文案非空）为真信号而非帧差；真点仍优先尝试（多数宿主点不到，无妨）。docs sync.gif 重生成（旧对两帧同区仅涟漪残影、未示真实效果；lang.gif 同批变动系 lang_0 before 帧既有 run-to-run 非定差、与本次无关，已还原）。验证：CI run 42799339590 全绿——三平台 Qt Test 步 171/171（#3 ui_sandbox_switch_effects Passed：ubuntu 42.68s / windows 45.32s / macos 198.77s，此前稳定红 41.87s / 45s / 184s）；std-only 147 全绿；coverage 绿。本地全量 171 中唯一 fail=ui_click_audit 既档本地死点（CI 绿）；sync_relay_protocol/cpp_protocol 本地 Not Run 系构建缓存 Python 指向已消失的 hermes venv（直跑脚本 16 用例 OK，与本次改动无关，CI 正常）。
- [x] ICON-002 [P2] 产品 icon 重设计（基于现有 logo 演进）：保留打开的书轮廓 / A-Z 识别 / 洋红主色 / 圆角线性语言；演进=双色书页（后页深洋红 #C2255C 上移 30px 露边 + 前页主洋红 8% 明度对角渐变 #C31C6E→#9F165A）+ 书脊凹槽深色阴影线 #690F3C + 中央圆润大写「U」（uni 首字母，76px 笔画）+ A / Z 缩小分列底部书脊圆凸两侧（圆凸充当分隔符）；两版交付（透明底主文件 + 白底圆角应用版，圆角半径=边长 22.4%）——
  勾账 2026-11-11：0bc2fe2——assets/icons/redesign/{logo-v2.svg（1024 viewBox 手写 path，书形轮廓 verbatim 复用原 mark 的 carve 结构）、logo-v2-app.svg、logo-v2-{256,128,64,32,16}.png、logo-v2-app-512.png}、assets/icons/{unidict_v2.png（512）、unidict_v2.ico（16/24/32/48/64/128/256 七档，identify 全档可解析）、unidict_v2.icns（icp4/icp5/ic11/ic12/ic07/ic08/ic09/ic13/ic14 九 chunk，结构走查+每块 PNG 全量解码验证，file 认 icns）}、对比页 docs/icons/icon-v2-preview.html（离线零外链：新旧并排 + 五档 1:1 + 16px×8 最近邻放大 + 色值表 + 改进说明 3 条）。16px 口径如实：v2 中央 U 保留竖笔槽与圆弧底、书脊圆凸以底部小凸起出现，形体强于 v1（v1 的 A·Z 同尺寸已糊成横带并与书框粘连）；逐行 ink-run 探针证两版书形框架几何一致（差异仅后页露边与新内容）。权威门禁=CI run 42852899186 全绿（assets 提交触发：Qt 三平台 + std 三平台 + coverage + 语法门 8 作业全 success）。线上 docs/site/public/logo.svg 未动，等用户选型确认后再换

## G. 审核发现（2026-11-01 AI 面审计，已闭环）

- [x] 审核发现：AI 设置页为 StackLayout 第 7 子项但 toolsTabs 仅 6 Tab，页面永不可达（死 UI）——已修：toolsTabs 增补 toolsTab6「AI」（qmlui/MainDesktop.qml:1151；71581ec；qml 构建过）
- [x] 审核发现：setPrompt/setPromptForwarding 换提示词模板后未清结果缓存，返回过期旧结果——已修：两处补 clearCache()（71581ec；test_qt_adapters_bridge PASS）
- [x] 审核发现：resolve 把 provider 链失败（空串）也写入缓存——不属实：设计意图即失败也缓存（防重复拉起进程），恢复路径 setCommand/clearCache（adapters/qt/ai_service_qt.h:72-73）

## H. 代码深审发现（2026-11-01/02 全仓深审，P2/P3 待办；报告 docs/review/2026-11-01-code-review.md）

- [x] 深审 P2：ai_service_qt runExternal 忽略 waitForFinished/exitCode，失败命令 stderr 当答案返回（adapters/qt/ai_service_qt.cpp:86-92）— 34ec205（2026-11-09 并发批次入库，本项 2026-11-10 复核勾账）：waitForFinished(15000) 假→kill+reap+返回空（半截输出不作数）；exitStatus 非 NormalExit 或 exitCode≠0→返回空，链上回落启发式 provider（报错文本不再冒充释义）；stderr 兜底只服务正常退出的哑输出命令。红先行：qt_adapters q7 增 fail.sh 用例（echo boom >&2; exit 1 → 断言回落 [Mock Translation]，修复前会把 boom 当答案）
- [x] 深审 P2：index_engine load_index 的 std::stoi(freq_s) 未捕获，畸形索引行中止（core/std/index_engine_std.cpp:211）— 98ac446（同批）：try/catch 包 stoi，畸形频次列跳行 continue 不中止装载（好行照载）。红先行：edge 测试写 build-local 畸形索引（goodword/3/D1 + badword/notanumber/D2 + 空列行），断言 loaded 真、word_count==1、exact_match 命中好行（修复前 invalid_argument 冲出装载）。注：规格括注「计数 warn」未加——现口径静默跳行，从 try/catch 选项从宽；需观测面后续补
- [x] 深审 P2：fulltext ensure_postings const_cast 懒解码非线程安全，并发 search 数据竞争（core/std/fulltext_index_std.cpp:426-447）— 640a0ae（同批）：头文件加 mutable std::mutex decode_mu_，ensure_postings 解码段无条件 lock_guard（快路读 compressed 本身也与解码写互斥，双检锁的无锁前检是假省；解码完翻 compressed 后锁内快路直返）。红先行=TSan 实证（commit 注明 race 定位 ensure_postings，修后 5 连净）；新增 test_fulltext_index_std_race（89 行，UDFT3 装载后并发 search 冒烟）+ CMake 注册
  三项门禁：本地三项测试 3/3 绿、全量 168/171（唯一 fail=ui_click_audit 既档本地死点、relay ×2 Not Run=构建缓存 Python 指已消失 hermes venv，直跑 16 用例 OK——均 CI 正常）；权威门禁=CI run 42799339590 全绿（三平台 Qt 171/171 + std-only 147 + coverage，祖先链含同批全部修复）。同批另有 dictionary_manager ddf3885 / sync_engine e6a486b 两项竞态修复入库，属下两条待勾项，本任务（P2 前 3 项）范围外未动
- [x] 深审 P2：dictionary_manager 全文本/前缀索引 const_cast 懒构建非线程安全（core/std/dictionary_manager_std.cpp:601-618）— ddf3885（2026-11-09 并发批次入库，本项 2026-11-11 复核勾账）：前缀自愈与全文本惰建各持一把 mutex，脏位/已建快照改 atomic，双检锁锁内复查（ensure_prefix_index_built 387-391 / ensure_fulltext_index_built 757-780），reset/load 位同步快照。复核=码走查（仅余 const_cast 两处均在 DCL 内）+ test_dictionary_manager_std_race（tests/dictionary_manager_std_race_test.cpp）随全量 std 139/139 绿
- [x] 深审 P2：sync_engine 可变状态无同步，多线程调用竞态（core/std/sync_engine_std.cpp）— e6a486b（同批）：单 mutex mu_ 罩全部公开入口（getter 逐锁 146-153、入口 459/477/624/636/676 lock_guard），enqueue 阻塞至运行中同步轮完再落 outbox（保持串行语义）。红先行=TSan 124 races 报告修后净；新增 tests/sync_engine_std_race_test.cpp（120 行）随全量 std 139/139 绿
- [x] 深审 P2：mdx_v2 流上限/截断检查 u64 加法环绕可绕过（core/std/mdx_v2_reader_std.cpp:588,592）— 138e4e1：add_u64_ovf 三处（流上限 588、截断 592、sum/cursor 推进 622-623 纵深）；红先行 T65 手术用例（表扩 64 项 + 记录区重写 64 个裸 zlib 重放对，块 1..63 压缩长 2^64-Bp 环绕回退重放、rec_block_size 补 mod 2^64 和=0）：修复前 read_buffer 真返回（rc=134 assert `!ok` failed@1170），修复后块 1 即拒；mdx/mdict/mdd 族 35/35，全量 169/170（唯一 fail=ui_click_audit 既有本地死点档，CI 绿）
- [x] 深审 P2：mobile_utils 静态 ActivityResultReceiver 绑定首实例、从不 delete，析构后回调解引用悬垂（qmlui/mobile_utils.cpp:103）— c9eb133：static 局部改按实例惰建成员 m_resultReceiver（头文件 Q_OS_ANDROID 块声明）+ setParent 父属 this（实例析构子先亡，回调 emit m_parent 不可能触悬垂）。验证口径如实：红先行不可执行——Q_OS_ANDROID 分支是 Qt5 QtAndroidExtras API，Qt 6.10 无此模块，CI android job 只编 JNI 壳不编 unidict_qml，分支在任何现行工具链下不可编译不可运行；验证=桌面全量编译（含 mobile_utils.cpp 的 gui/qmlui/sandbox 面）+ sandbox/qml 测试 3/3 绿 + 走查；残留窗口（picker 挂起期间实例析构、Qt 内部回执表裸指针悬垂）已在码注释注明，收口需迁 Qt6 activity-result API 随分支复活处理
- [x] 深审 P2：jni_util FindClass/NewObjectArray 未判空，缺类/OOM 中止 JVM（adapters/android/jni/jni_util.h:32-41,72-86,109-124）— 70e3d17：FindClass/GetMethodID/NewObjectArray/NewObject 全部判空短路返回 nullptr（挂起的 Java 异常浮出；R8 未 keep 缺类是 release 常态，NULL 继续喂=JVM fatal abort）。红先行 host 等价语义测试 test_jni_util_std：真 jni.h + 模拟 JNIEnv 函数表（crash 账=真实 JVM 中止语义；坑：C++ inline NewObject 走 functions->NewObjectV，jni.h:878-882），修前 rc=134（NULL 喂 NewObjectArray crash 账非零）、修后 rc=0 OK；NDK clang++ 逐 TU 编译 3/3（dict/lookup/store_jni.cpp），全量 170/171（唯一 fail=ui_click_audit 既有本地档）；无 JDK 时 CMake 不注册本测
- [x] 深审 P2：sync_service_qt 本地清空先于远端写、f.write ×3 未检查（adapters/qt/sync_service_qt.cpp:119→133,287→318,456）— e552690：三入口（syncNow/applyPreview/applySelection）重排为「远端写成功→才动本地库」（旧序先 clearVocabulary 换血再写远端，写失败时本地已被覆盖且合并键小写折叠丢条目）；f.write ×3 全部改整长对账（不全写→lastError_+false）。红先行 test_sync_service_qt 新槽 writeFailure_leavesLocalIntact：观测面=播种序 Zeta,Alpha 在写失败后必须原样（旧码按 QMap 键序重插成 Alpha,Zeta），修前 Failed、修后 Passed（同一槽覆盖三入口+历史原样）；全量 170/171（唯一 fail=ui_click_audit 既有本地档）。注：写不全的 -1/短写分支无可移植红构造（需 open 成功但 write 失败的目标；/dev/full 会先卡 read_remote 的 readAll），该分支以码走查+既有 exportSelection n>=0 口径佐证
- [x] 深审 P3：exportNotesPdf 不查 print 结果无条件返回成功（adapters/qt/data_store_qt.cpp:190-224）— 2acdf85：print 后追加输出后置判据（常规文件须存在、非空、%PDF- 开头），QPdfWriter 静默失败不再假成功；保留既有前置写探测（belt+suspenders）。红先行构造如实：HEAD 现有写探测（ftruncate 对 /dev/full EINVAL）已先拦截 /dev/full，静默写失败路径在 HEAD 不可达——临时还原「无探测无检查」原始缺陷态后 /dev/full 测试 Failed（无条件真返回），恢复修复后 Passed（同一用例，Linux 专有门控）；happy-path %PDF 头断言保持绿。全量 170/171（唯一 fail=ui_click_audit 既有本地档）
- [x] 深审 P3：data_store save 写后不复查流态，磁盘满报成功丢数据（core/std/data_store_std.cpp:348-419）— 879070e：save 末尾 flush()+good() 复查流态（磁盘满 operator<< 静默置 failbit，flush 把内核缓冲写失败顶到流态再判）。红先行：/dev/full 直调 save()（绕开装载路径——store 走 load 读 /dev/full 会无限读零挂死，判据外），修前 assert `!df.save()` 失败 rc=134，修后绿；data_store 族 11/11。注：测试初版经 add_vocabulary_item 触发 load 路径曾挂死 300s+，已改为空库直调口径
- [x] 深审 P3：sync_manager exportBackup 的 f.write 未检查（adapters/qt/sync_manager_qt.cpp:436）
  勾账 2026-11-12：写失败即抛（backup 报错浮出，静默半备份根除）；同笔修 tray 菜单所有权。875a821，Qt 三平台 CI 绿（run 43038003209）。
- [x] 深审 P3：stardict p+8+4>end 近端指针算术 UB（core/std/stardict_parser_std.cpp:110,114）
  勾账 2026-11-12：近端算术改距离比较（end-p>=n 判余量，不造越界指针）；畸形 idx 矩阵测试钉住。c6003d2，CI 绿（run 43070382993）。
- [x] 深审 P3：dictionary 伴生扫描 is_regular_file 抛掷重载（core/std/dictionary_std.cpp:63）
  勾账 2026-11-12：改 error_code 重载（fs::is_regular_file(p, ec)），伴生扫描遇无权限/悬空路径不再抛穿装载循环。c6003d2 同笔。
- [x] 深审 P3：mdict 未识别容器兜底逐偏移 zlib 扫描 CPU/内存膨胀（core/std/mdict_parser_std.cpp:1007-1041）
  勾账 2026-11-12：inflate 命中即跳过整个已消费压缩段（off += total_in-1 补回循环 ++off），不再逐字节在流内部重复探测候选头。f9edca6，CI 绿（同上 run）。
- [x] 深审 P3：mdict rewrite_attr 匹配 src= 也命中 data-src= 误改写（core/std/mdict_parser_std.cpp:776-796）
  勾账 2026-11-12：attr_boundary 判前一字符（空白/`<`/`/`/串首），data-src= 尾巴续找下一个真边界。f9edca6 同笔。
- [x] 深审 P3：sync_engine 宽容 JSON 负值强转 u64 环绕（core/std/sync_engine_std.cpp:686-689）
  勾账 2026-11-12：计数器专用 sy_obj_u64 负值钳 0（-9 → 2^64-9 回绕根除）；手改负值状态文件测试钉住（tests/sync_engine_std_test.cpp）。755a3f0，CI 绿（run 43085244917）。
- [x] 深审 P3：lookup_adapter size_t→int 截断（qmlui/lookup_adapter.cpp:615-616）
  勾账 2026-11-12：<limits> int 上限守卫（超限钳 int-max，不再隐式截断回绕负值）。755a3f0 同笔，CI 绿（run 43085244917）。
- [x] 深审 P3：dsl 备用词头逗号切分不发最后一段（core/std/dsl_parser_std.cpp:176-184）
  勾账 2026-11-12：逗号切分补发尾段（末段无逗号闭合时照常产出词条）。1f6c9ca，CI 绿（run 43070382993）。
- [x] 深审 P3：ripemd128 len 近 SIZE_MAX 环绕越界（core/std/ripemd128_std.cpp:103,109）
  勾账 2026-11-12：len+8 环绕防御（length_error 拒绝）；测试 ripemd128(nullptr, SIZE_MAX-8) 断言抛出。1f6c9ca 同笔（tests/mdict_crypto_std_test.cpp §8）。
- [x] 深审 P3：tray_controller m_menu 无父且 setContextMenu 不接管 → 析构泄漏（qmlui/tray_controller.cpp:122）
  勾账 2026-11-12：tray 自持菜单所有权（父链挂接），析构随 tray 释放。875a821 同笔（与 137 一并，Qt 三平台 CI 绿）。
- [x] 深审 P3：mdd parse_simplekv_fallback 用 32 位 fseek/ftell，Windows >2GB 误析（core/std/mdd_resource_std.cpp:500,577）
  勾账 2026-11-12：全文件 I/O 换 tell64/seek64（Windows _ftelli64/_fseeki64，POSIX ftell/fseek 包一层），>2GB .mdd 偏移不再截断。9c2e108，CI 绿（run 43075898592）。
- [x] 深审 P3：store_jni/dict_jni raw new 转 jlong，未 close 泄漏（adapters/android/jni/store_jni.cpp:54-57、dict_jni.cpp:20-28）
  勾账 2026-11-12：活动句柄注册表托管（create/close 全走注册表，泄漏根除）。8ce5a58，CI 绿（run 43085244917）。
- [x] 深审 P3：jni_util handle_of 不校验，陈旧句柄空/UAF、双重 close 双释放（adapters/android/jni/jni_util.h:44-46）
  勾账 2026-11-12：handle_of 查注册表（陈旧/陌生句柄返回空对象，双重 close 安全空转）；jni_util_std_test 注册表往返 pin。8ce5a58 同笔，CI 绿（run 43085244917）。
- [x] 深审 P3：mdd MAX_RESOURCE_SIZE 10MB 静默丢弃大资源无诊断（core/std/mdd_resource_std.cpp:980）
  勾账 2026-11-12：超帽资源记 stderr 诊断（资源名+尺寸+帽值）再跳过，不再静默吞。9c2e108 同笔。
- [x] 深审 P3：QuickLookupPane 词典 HTML 未净化（无脚本引擎暴露，信息性）（qmlui/components/QuickLookupPane.qml:251,255）
  勾账 2026-11-12：不修——信息性复核确认：QuickLookupPane 的词典 HTML 只进 Text/Browse 组件渲染，无 JS 引擎暴露面，无本地文件/网络桥可被脚本触达；净化收益为零还可能破坏词典排版。维持现状。
- [x] 深审 P3：mdd extract_to_cache 仅替换 / 不替换 \\，Windows 逸出（无生产调用者，死路径）（core/std/mdd_resource_std.cpp:1051-1052）
  勾账 2026-11-12：缓存名转义补 \\（Windows 盘符/反斜杠不再逸出缓存根）；路径无生产调用者维持死路径口径。9c2e108 同笔。
- [x] 深审 P3：CLI --dict-pass 密码可见于进程列表（信息性，常见权衡）
  勾账 2026-11-12：不修——与 mdx 工具生态同口径的常见权衡（ps 可见 vs 环境变量/交互输入的可用性损失）；CLI 是本地单人场景，无多用户主机威胁模型。文档已提示环境变量替代（UNIDICT_MDX_PASSWORD）。维持现状。

## 漏洞扫描遗留（2026-11-03 全舰队扫描，源 scratch/vuln-scan-*.md）

- [ ] VULN-UNIDICT-001 [BLOCKED·他人 stash-pop 冲突] docs/site 的 package.json 在扫描窗口处他人 stash-pop 冲突（UU、含冲突标记）致 pnpm audit 无法执行；本任务 2026-11-03 晚复核：工作树已无 UU 与冲突标记（docs/site/package.json 现为 staged M），但 audit 尚未出终态——不碰他人改动；不勾，验收：冲突方确认后 pnpm audit --audit-level=high 出终态（0 high）且 git status 无未合并路径
## 模糊搜索/纠错实测三连洞（2026-11-08 用户报 aple→apple 失败，实测确认）

- [x] UNI-FUZZY-001 [P0] 模糊层被释义匹配短路：`aple` 释义包含层命中 aplenty/Aplexa 等噪声词后，编辑距离层被跳过（93dacde 设计「前三层全空才走」），距离仅 1 的 apple 永远召回不了、也无 did-you-mean。修法：精确/前缀层全空即评估编辑距离候选；若有距离≤1 强候选，与释义包含命中并存但「您要找的是」置顶（或距离≤1 直接置顶重组）；T11 用例补 aple→apple 与「释义命中+强候选并存」两案。
  勾账 2026-11-08：诊断修正——aple 的噪声是**前缀层**噪声（aple 是 aplenty 真前缀）非释义层独有，「精确/前缀全空才评估」永不触发；落地口径=精确/词形还原两层全空即评估编辑距离，dist≤1 强候选置顶（rel 3）且与前缀（rel 2）/释义（rel 4+fulltext）/dist2 弱候选（rel 5）并存，无强候选时前缀/释义命中即止（正常前缀查询不掺全文噪声）。T11 helo→hello + 短查询闸门、T11b aple→apple 置顶且 aplenty(rel2)/abundance(rel4) 并存，两案钉住。
- [x] UNI-FUZZY-002 [P0] CLI 默认查询 apple 超时（>180s，build/Release/unidict_cli_std --scan-dir dictionaries apple 实测卡死）：释义匹配层全表扫无预算（GUI 有 kSuggestScanBudgetMs=60ms，CLI 路径没有）。修法：CLI 释义匹配层套同款时间预算 + 部分集标注；验收=apple/aple 单查 <5s 出结果。
  勾账 2026-11-08：search_all/full_text_search 加 ft_budget_ms（>0 时全文索引未建不触发分钟级懒构建、按现成索引返回部分集）；CLI 默认查询显式传预算并打「# 释义全文层未建索引…（结果为部分集）」标注。实测四库 378MB（ECDICT/FreeDict×2/WikDict×2/CC-CEDICT）：aple 1m48s rc=0、apple 2m1s rc=0、-m fuzzy aple 1m40s rc=0——>180s 卡死消除；墙钟为词典装载主导（约 1.5 分钟），查询段秒级出结果。
- [x] UNI-FUZZY-003 [P1] CLI 无 did-you-mean：suggest_corrections 只接了 GUI（gui/main.cpp:472），CLI 未命中路径不输出纠错建议。修法：未命中时输出「您要找的是：…」段（复用 suggest_corrections，max 5）。
  勾账 2026-11-08：CLI 未命中路径输出 "Did you mean:"（suggest_corrections max 5）；有释义命中时建议段置顶于噪声前，全 miss 时保持 Word not found 之后输出。实测 aple 输出 AELE/APL/APLH/aole/naple。
- [x] UNI-FUZZY-RANK-001 [P2] did-you-mean/强候选无词频权重（001 勾账遗留，非短路回归）：apple 距 aple 仅 1，但模糊序按「距离+字典序」无词频加权，实大词典下距离 1 同分词 37+ 个（AELE/APL/APLH/aole/naple…），apple 排 38——CLI top-5 与 search_grouped 强候选窗口（fuzzy 前 12）都够不着（-m fuzzy aple 50 条实测 apple 在第 38 位）。T11b 小词典口径已钉逻辑正确。后续候选：建议/强候选按词频加权，或窗口加大 + 高频优先重组。
  勾账 2026-11-12：模糊候选按词频加权（距离同分词频降序、同频回退字典序，core/std/index_engine_std.cpp frequency_of）；apple→aple 场景回归 pin（tests/index_engine_std_ops_test.cpp UNI-FUZZY-RANK-001，高频词置顶断言）。fdb944e，CI 绿（run 43082488618）。

## 源码分层分析 2026-11-07

- 抽象层清单（详见 docs/site/architecture.md 与 docs/site/zh/architecture.md）：
  - L1 应用壳：启动一个产品面并拥有命令解析、窗口/事件循环生命周期——`cli-std/main.cpp:304`、`qmlui/main.cpp:75`、`gui/main.cpp:1604`。
  - L2 表现与服务绑定：把 C++ 服务发布给 UI、转换 Qt/std 类型并装配壳内信号——`qmlui/main.cpp:229`、`qmlui/lookup_adapter.cpp:36`。
  - L3 领域服务：拥有词典聚合策略、用户学习数据和确定性同步物化状态——`core/std/dictionary_manager_std.h:19`、`core/std/data_store_std.h:63`、`core/std/sync_engine_std.h:103`。
  - L4 知识编解码：把外部词典格式与伴生资源读入领域对象——`core/std/parser_registry_std.cpp:15`、`core/std/mdd_resource_std.cpp:986`。
  - L5 检索与词条呈现：规范化查词键、维护词形/全文索引、排序结果并清洗/重写 HTML——`core/std/index_engine_std.h:39`、`core/std/fulltext_index_std.h:15`、`core/std/html_renderer_std.h:21`。
  - L6 智能与感知：提供可选推理评分、本地/在线语音与剪贴板/选区/热键感知——`adapters/pron/onnx_pron_scorer.h:22`、`qmlui/voice_adapter.cpp:166`、`qmlui/platform_adapter.h:1`。
  - L7 同步通信：密封指令经 HTTP 推拉，宿主执行中继契约和 LAN 发现——`adapters/qt/sync_http_transport_qt.cpp:58`、`server/sync_relay/cpp/relay_routes.h:1`、`server/sync_relay/cpp/lan_host_std.h:23`。
  - L8 状态基础设施：按不同 durability/兼容契约持久化用户数据、索引、状态、设置、密钥和中继数据——`core/std/data_store_std.cpp:30`、`core/std/dictionary_manager_std.cpp:971`、`adapters/qt/sync_manager_qt.cpp:52`。

- 超大文件候选（>1000 行）：
  - [x] LAY-001 [P1] 拆分 qmlui/MainDesktop.qml（3143 行）——主窗、导航、查词结果、历史/生词本和七个设置页挤在一份声明式文件，设置页 2500 行后仍持续增长（qmlui/MainDesktop.qml:2500）。**e9ea4af**：七个设置页按原 StackLayout 序拆入 `qmlui/components/Settings{Lookup,Voice,Dict,Keys,Sync,Theme,Ai}Page.qml`（7 新文件），主窗 3143→1815 行（+38/-1366）。`objectName` 逐页保留故沙盒/审计子项查找不变；窗口状态经注入 `win` 引用、C++ 服务经根作用域 `*Service` 别名注入（`settingsService`/`startupService`/`syncManagerService`/`aiService`，`qmlui/MainDesktop.qml:119-122`——实例化处裸名会自引用取默认值，同 `SidebarPanel` 实例化处口径），别名实例化见 `qmlui/MainDesktop.qml:1411-1464`。随迁：`syncVoiceSelections` 与根 `Component.onCompleted` 语音面初始化入 `SettingsVoicePage.qml:22-37`，`syncManager` Connections 入 `SettingsSyncPage.qml:25-29`；词典页 `FolderDialog`+`ScrollView` 改经 `Item` 转交铺满 wrapper（`SettingsDictPage.qml:13-26`，原由 StackLayout 直接撑满）。行为不变，证据：ui_sandbox_screenshots/ui_sandbox_switch_effects Passed、40 张截图 32 张与基线逐字节同余 8 张全落 suggest/notfound 抖动面（灰度均值同 0.1、尺寸差 <130B）、ui_click_audit 52/79 与基线同档、qmllint 7 文件+主窗零错误、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay 构建缓存 Python 指已消失 venv 的 Not Run，均 CI 口径）、build-std `-R _std` 144/144。
  - [x] LAY-002 [P1] 拆分 qmlui/Main.qml（1792 行）——移动壳同时承载响应式布局、抽屉导航、多 section 与结果视图，可按页面容器先切（qmlui/Main.qml:185）。**e027338**：StackLayout 五页按原序拆入 `qmlui/components/Mobile{Search,History,Vocab,Voice,Review}Page.qml`（5 新文件，原注释随迁），主壳 1792→341 行（+1615/-1477）。页面经注入 `win` 引用读写窗口状态；`responsive`/`suggestionModel` 是 Main.qml 文件级 id、组件文件看不见，经本页同名字段注入并在实例化处同 id 名赋值（文档 id 优先于目标对象自身属性——与 `win: win` 同律），见 `qmlui/Main.qml:250-285`、字段声明 `MobileSearchPage.qml:21-23`；`Main.qml:6` 补 `import "components"`（Settings*Page 同款，原 MainDesktop 已有、Main 缺）。`objectName` 逐页保留故沙盒/审计子项查找不变。行为不变，证据：`unidict_ui_sandbox --entry qrc:/Main.qml` 与拆前基线对照——16 张截图 16/16 逐字节同、同 2 条既有错误（`Dialog` implicitWidth 绑定环、delegate `parent.width` 空，均已确认在拆前基线同现）；ui_click_audit 52/79 与拆前报告逐项同标签（移动端同步预览三条一致）；qmllint 5 文件+主壳零错误；build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay 构建缓存 venv 消失的 Not Run，均 CI 口径）、build-std `-R _std` 144/144。
  - [x] LAY-003 [P1] 拆分 core/std/mdd_resource_std.cpp（1689 行）——容器探测、多代私有格式、MDX record 块、资源查询与缓存导出同文件，格式编解码可独立（core/std/mdd_resource_std.cpp:935）。**c742ed9**：缓存面（落盘、元数据、prune_by_size/age/access、stats、超长键 FNV 摘要压名）整体搬入 `core/std/mdd_resource_cache_std.cpp`（309 行、18 个 `MddResourceCache::` 定义、区域 `:31-308`），解析/格式/块编解码与 `MddResourceManager`（13 个定义）留在 `core/std/mdd_resource_std.cpp`，主文件 1732→1428 行、diff 纯删除 0 新增（304 行移出）。两 TU 共用的 `fnv1a64`/`file_extension` 升为内部头 `core/std/mdd_resource_helpers_std.h`（36 行，`mdd_detail` 命名空间、inline 免另建 .cpp、非对外 API），缓存 TU 两处调用加 `mdd_detail::` 限定（`mdd_resource_cache_std.cpp:279,282`），缓存长度常量 `MAX_CACHE_NAME_LEN/PATH_LEN` 随缓存面带走（`:20-29`，解析 TU 已不引用）；`core/CMakeLists.txt:73-74` 登记新 TU + 内部头。行为不变，证据：build-std 146/148（2 项 `sync_relay*` Not Run = 构建缓存 Python 指已消失 venv，CI 口径）、7 个 mdd 测试全绿（`test_mdd_resource_std`/`_branches`/`_full`/`_cover`/`_cover2`/`test_mdd_mdict_std`/`test_mdict_mdd_resources_std`）、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；core Qt-free 由 build-std 编译通过证明（新 TU 在无 Qt 树里构建）。
  - [x] LAY-004 [P1] 拆分 gui/main.cpp（1622 行）——匿名命名空间里聚合状态恢复、HTML 渲染、主窗控件、词典管理、语音资源和 main，Widget 面板可先抽（gui/main.cpp:62）。**3dde04c**：三块按依赖序拆出——①共享状态入口 `gui/dict_state.h`（74 行，`dictMgr` 单例 + 状态文件定位 + 环境装载 + 落盘，全 `inline`：inline 函数静态局部对象全 TU 唯一，click_audit/ui_sandbox 以 `#include "main.cpp"` 复用 MainWindow 的编译单元与主程序仍拿同一实例，`dict_state.h:21-72`）；②主题三态 `gui/theme_manager.h`（76 行，`darkPalette` + `ThemeManager` 原样搬移，`theme_manager.h:15-41`）；③词典管理对话框 `gui/dictionary_manager_dialog.{h,cpp}`（15+260 行，`showDictionaryManager` 232 行方法体改自由函数 `runDictionaryManagerDialog(parent, onTagChanged, onFinished)`，成员函数 `connect` 改 `QObject::connect`、`QDialog dialog(this)`→`(parent)`、分组刷新 `refreshGroupBox()`→`onTagChanged()` 回调、关闭刷新 `refreshAll()`→`onFinished()` 回调——MainWindow 定义在匿名命名空间不反向暴露），main.cpp 留 3 行包装 `gui/main.cpp:1213-1220`，主文件 1622→1280 行（diff +18/-350 纯搬移），`gui/CMakeLists.txt:25-28` 登记 + `:103,147` audit/sandbox 两 target 补链对话框 TU。行为不变，证据：gui_click_audit Passed（11.88s，真点审计吃 main.cpp 文本包含链）、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；core 未触碰故 build-std 面不受影响。
  - [x] LAY-005 [P1] 拆分 core/std/html_renderer_std.cpp（1210 行）——token 解析、白名单策略、CSS 处理、链接/媒体重写和高亮同文件，安全面改动会触碰全部路径（core/std/html_renderer_std.h:21）。**fd3c52e**：按类切缝——`DefaultResourceResolverStd`（.mdd/目录资源 file:// 解析 + data URL 缓存）、`HtmlRendererFactory` 四预设、`ResourceResolverStd` 基类默认实现整体搬入 `core/std/html_resource_resolver_std.cpp`（359 行，区域 `:18-357`），渲染 TU 留清洗/分词/高亮主链，主文件 1210→868 行（diff 纯删除，+2 行=helpers include + 1 处 `html_detail::` 限定）。两 TU 共用的 `file_url_from_local_path` 升内部头 `core/std/html_renderer_helpers_std.h`（`html_detail` 命名空间、inline、非对外 API，`html_renderer_helpers_std.h:14-21`），渲染侧 1 处、解析侧 1 处调用加 `html_detail::` 限定；类声明全部留在 `html_renderer_std.h` 未动（消费方 include 面不变）。行为不变，证据：build-std `-R html` 6/6（`test_html_renderer_std`/`_branches` 等）、build-std 146/148（2 项 relay Not Run 既档）、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；拆出 TU 的 tail 逐行核对原文件 868..1208 逐字同（仅 1 处限定）。
  - [x] LAY-006 [P1] 拆分 core/std/mdict_parser_std.cpp（1192 行）——头部/加密探测、KIDX/RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 和启发式兜底集中于此，每种布局已有独立契约（core/std/mdict_parser_std.cpp:1）。**8b2c378**：布局/编解码面整体拆入 `core/std/mdict_layout_std.cpp`（490 行）+ 内部头 `core/std/mdict_layout_std.h`（198 行，`mdict_detail` 命名空间，非对外 API）：7 布局编解码（parse_simple_kv/kidx_rdef/keyb_recb/kbix_rbix/kbix_multirb/mdxk_mdxr/mdx_heuristic_real）+ `extract_attr`/`get_mdict_password_env`/兜底驱动 `parse_mdict_body_from_file_best_effort`（去 static 外链化，定义 `mdict_layout_std.cpp:63-410,415-488`），TU 内 statics（`find_bytes`/`be16`/`be32u`/`MAX_*`/`safe_inflate`/`decompress_all_zlib_blocks`/`is_wordish_char`/`heuristic_parse_key_index`）随行搬入匿名命名空间（`:29-56`）；`fnv1a64`/`lcase_ascii`/`url_encode_component`/`make_file_url`/`normalize_resource_key`/`sanitize_relative_path`/`read_head`/`utf16_bom_to_utf8` 升头内 inline（`mdict_layout_std.h:26-154`——`fnv1a64` 被 `load_companion_mdd`（解析侧 `:724`）共用故须跨 TU，其余为声画面配套）。解析 TU 1192→591 行，18 条 `using mdict_detail::`（`mdict_parser_std.cpp:24-43`）保调用点原样，成员方法/`rewrite_mdict_link_markers`/`render_entry_for_ui`/`load_dictionary`/访问器逐字未动；`core/CMakeLists.txt:60-62` 登记新 TU + 内部头。行为不变，证据：build-std `-R "mdict|mdd"` 34/34 全绿、build-std 146/148（2 项 relay Not Run 既档）、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；搬移块对 HEAD 逐字比对（仅 static 前缀交换 + driver 签名续行对齐重排）通过。
  - [x] LAY-007 [P1] 拆分 core/std/dictionary_manager_std.cpp（1228 行）——装载/隔离、分组查询、索引缓存、资源转发和状态序列化同文件，聚合策略与 JSON 状态读写可分开（core/std/dictionary_manager_std.cpp:971）。**baffdde**：状态序列化面按 todo 点名缝整体拆入 `core/std/dictionary_state_std.cpp`（298 行）——手写 JSON 读写器横幅（`dictionary_state_std.cpp:22-24`）+ 8 个 `state_*` static 助手（parse_json_string/obj_val/obj_int/obj_bool/obj_str_array/find_section/for_each_object/json_escape，`:26-223`，仅本 TU 使用，全文件核查无外部引用）+ `save_state`/`load_state` 成员定义（`:225-290`，成员声明在 dictionary_manager_std.h 不变）；聚合 TU 1228→951 行（diff 纯删除 -277），装载/隔离、分组查询、索引缓存、fulltext、资源转发全部原位未动；`core/CMakeLists.txt:71-72` 登记新 TU。行为不变，证据：build-std 146/148（2 项 relay Not Run 既档）、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；搬移块 889..1164 对 HEAD 逐字比对通过（新 TU 仅 + include/namespace 头尾）。
  - [x] LAY-008 [P2] 拆分 qmlui/dev/ui_click_audit.cpp（2334 行）——UI 审计夹具、脚本/参数、页面交互断言和离屏输出混在一起，可按页面或用例组拆（qmlui/dev/ui_click_audit.cpp:213）。**e57c4c2**：按场景组拆 6 批——匿名命名空间 17 个助手（audit 计数/真点/滚入/按键/视觉树查找/popup 读态/ClickCounter）升 `qmlui/dev/click_audit_helpers.h`（185 行，`ui_audit` 命名空间，inline 函数 + inline 变量保各 TU 共享同一计数与日志，`click_audit_helpers.h:25-176`）；`AuditCtx`（win 指针 + 13 适配器引用 + scratch/outDir + hasTts/drawer 两跨场景状态）+ `registerAuditContext`（main 的 registerCtx lambda 自由函数化，S37 第二引擎复用）+ 36 场景声明见 `qmlui/dev/click_audit_sections.h`（92 行）；36 个 banner 场景（S0..S37，含 S17 吞 S24、S23 吞 S26/S31/S35 的嵌套块）变 `audit_sXX(AuditCtx&)` 自由函数，按序装入 `click_audit_sections_{1..6}.cpp`（338/364/301/407/357/450 行），调用序列与 banner 注释在 main 原位保留，主文件 2334→284 行。拆前对全文件做跨场景局部变量审计：真跨场景共享仅 hasTts（S1→S17）与 drawer（S14→尾部 6 处关回），其余名义重名均为各场景独立声明或字符串/注释误报（剥字面量复核）。ClickCounter 的 Q_OBJECT moc 由 AUTOMOC mocs_compilation 自动编入（头名避开 AUTOUIC `ui_*.h` 生成物模式）。行为不变，证据：build 全量构建绿、build ctest 170/173（3 项=ui_click_audit 本机既档 + 2 relay Not Run，与拆前同档）；审计输出签名逐行比对一致（52 passed/79 failed 的全部 [PASS]/[FAIL] 行 + 汇总行，与拆前基线 diff 为空）。
  - [ ] LAY-009 [P2] 拆分 qmlui/components/EntryResultsPane.qml（1016 行）——内容 tab、全文/词组/联想加载、HTML 渲染和交互都在单文件（qmlui/components/EntryResultsPane.qml:119）。
  - [ ] LAY-010 [P2] 拆分 gui/click_audit_test.cpp（1030 行）——GUI 真点审计覆盖多个页面入口和模态流，按页面组拆可降低单测试文件耦合（gui/click_audit_test.cpp:1）。

- 目录/分类不合理处：
  - qmlui 的 `LookupAdapter` 位于壳目录但被 GUI 测试目标重复编译，说明它已是共享 Qt 表现适配层；`qmlui/CMakeLists.txt:56`、`gui/CMakeLists.txt:95`。
  - `PronunciationSourceStd`、`OnlinePron` 决策逻辑在 `core/std/online_pron_std.*`，HTTP/播放留在 `VoiceAdapter`；边界正确，但 `lookup_adapter.cpp` 的私有 `P0Modules` 仍直接持有 onlinepron 状态，绕过已存在的 VoiceAdapter 职责（qmlui/lookup_adapter.cpp:348、core/std/online_pron_std.h:42）。
  - `SyncServiceQt` 是本地 JSON 合并器，`SyncManagerQt` 才是网络中继客户端；两者同名 `syncNow()` 且都注册进 QML（`sync`/`syncManager`），调用面易混（adapters/qt/sync_service_qt.h:11、adapters/qt/sync_manager_qt.cpp:214、qmlui/main.cpp:237）。
  - Android JNI 源码放在 `adapters/android/jni/`，但 Kotlin API 壳在 `android/app/.../UnidictCore.kt`；桥两端跨根目录，接口字段改动必须跨目录同步（adapters/android/jni/dict_jni.cpp:1、android/app/src/main/kotlin/dev/unidict/mobile/UnidictCore.kt:20）。
  - `docs/site/.vitepress/dist/`、`docs/site/node_modules/` 已出现在工作树；构建产物/依赖目录与 VitePress 源同层，若未忽略会污染源码盘点（docs/site/.vitepress/dist/index.html:1、docs/site/node_modules/.bin/vite:1）。
