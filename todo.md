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

## G. 审核发现（2026-11-01 AI 面审计，已闭环）

- [x] 审核发现：AI 设置页为 StackLayout 第 7 子项但 toolsTabs 仅 6 Tab，页面永不可达（死 UI）——已修：toolsTabs 增补 toolsTab6「AI」（qmlui/MainDesktop.qml:1151；71581ec；qml 构建过）
- [x] 审核发现：setPrompt/setPromptForwarding 换提示词模板后未清结果缓存，返回过期旧结果——已修：两处补 clearCache()（71581ec；test_qt_adapters_bridge PASS）
- [x] 审核发现：resolve 把 provider 链失败（空串）也写入缓存——不属实：设计意图即失败也缓存（防重复拉起进程），恢复路径 setCommand/clearCache（adapters/qt/ai_service_qt.h:72-73）

## H. 代码深审发现（2026-11-01/02 全仓深审，P2/P3 待办；报告 docs/review/2026-11-01-code-review.md）

- [ ] 深审 P2：ai_service_qt runExternal 忽略 waitForFinished/exitCode，失败命令 stderr 当答案返回（adapters/qt/ai_service_qt.cpp:86-92）
- [ ] 深审 P2：index_engine load_index 的 std::stoi(freq_s) 未捕获，畸形索引行中止（core/std/index_engine_std.cpp:211）
- [ ] 深审 P2：fulltext ensure_postings const_cast 懒解码非线程安全，并发 search 数据竞争（core/std/fulltext_index_std.cpp:426-447）
- [ ] 深审 P2：dictionary_manager 全文本/前缀索引 const_cast 懒构建非线程安全（core/std/dictionary_manager_std.cpp:601-618）
- [ ] 深审 P2：sync_engine 可变状态无同步，多线程调用竞态（core/std/sync_engine_std.cpp）
- [x] 深审 P2：mdx_v2 流上限/截断检查 u64 加法环绕可绕过（core/std/mdx_v2_reader_std.cpp:588,592）— 138e4e1：add_u64_ovf 三处（流上限 588、截断 592、sum/cursor 推进 622-623 纵深）；红先行 T65 手术用例（表扩 64 项 + 记录区重写 64 个裸 zlib 重放对，块 1..63 压缩长 2^64-Bp 环绕回退重放、rec_block_size 补 mod 2^64 和=0）：修复前 read_buffer 真返回（rc=134 assert `!ok` failed@1170），修复后块 1 即拒；mdx/mdict/mdd 族 35/35，全量 169/170（唯一 fail=ui_click_audit 既有本地死点档，CI 绿）
- [x] 深审 P2：mobile_utils 静态 ActivityResultReceiver 绑定首实例、从不 delete，析构后回调解引用悬垂（qmlui/mobile_utils.cpp:103）— c9eb133：static 局部改按实例惰建成员 m_resultReceiver（头文件 Q_OS_ANDROID 块声明）+ setParent 父属 this（实例析构子先亡，回调 emit m_parent 不可能触悬垂）。验证口径如实：红先行不可执行——Q_OS_ANDROID 分支是 Qt5 QtAndroidExtras API，Qt 6.10 无此模块，CI android job 只编 JNI 壳不编 unidict_qml，分支在任何现行工具链下不可编译不可运行；验证=桌面全量编译（含 mobile_utils.cpp 的 gui/qmlui/sandbox 面）+ sandbox/qml 测试 3/3 绿 + 走查；残留窗口（picker 挂起期间实例析构、Qt 内部回执表裸指针悬垂）已在码注释注明，收口需迁 Qt6 activity-result API 随分支复活处理
- [x] 深审 P2：jni_util FindClass/NewObjectArray 未判空，缺类/OOM 中止 JVM（adapters/android/jni/jni_util.h:32-41,72-86,109-124）— 70e3d17：FindClass/GetMethodID/NewObjectArray/NewObject 全部判空短路返回 nullptr（挂起的 Java 异常浮出；R8 未 keep 缺类是 release 常态，NULL 继续喂=JVM fatal abort）。红先行 host 等价语义测试 test_jni_util_std：真 jni.h + 模拟 JNIEnv 函数表（crash 账=真实 JVM 中止语义；坑：C++ inline NewObject 走 functions->NewObjectV，jni.h:878-882），修前 rc=134（NULL 喂 NewObjectArray crash 账非零）、修后 rc=0 OK；NDK clang++ 逐 TU 编译 3/3（dict/lookup/store_jni.cpp），全量 170/171（唯一 fail=ui_click_audit 既有本地档）；无 JDK 时 CMake 不注册本测
- [x] 深审 P2：sync_service_qt 本地清空先于远端写、f.write ×3 未检查（adapters/qt/sync_service_qt.cpp:119→133,287→318,456）— e552690：三入口（syncNow/applyPreview/applySelection）重排为「远端写成功→才动本地库」（旧序先 clearVocabulary 换血再写远端，写失败时本地已被覆盖且合并键小写折叠丢条目）；f.write ×3 全部改整长对账（不全写→lastError_+false）。红先行 test_sync_service_qt 新槽 writeFailure_leavesLocalIntact：观测面=播种序 Zeta,Alpha 在写失败后必须原样（旧码按 QMap 键序重插成 Alpha,Zeta），修前 Failed、修后 Passed（同一槽覆盖三入口+历史原样）；全量 170/171（唯一 fail=ui_click_audit 既有本地档）。注：写不全的 -1/短写分支无可移植红构造（需 open 成功但 write 失败的目标；/dev/full 会先卡 read_remote 的 readAll），该分支以码走查+既有 exportSelection n>=0 口径佐证
- [x] 深审 P3：exportNotesPdf 不查 print 结果无条件返回成功（adapters/qt/data_store_qt.cpp:190-224）— 2acdf85：print 后追加输出后置判据（常规文件须存在、非空、%PDF- 开头），QPdfWriter 静默失败不再假成功；保留既有前置写探测（belt+suspenders）。红先行构造如实：HEAD 现有写探测（ftruncate 对 /dev/full EINVAL）已先拦截 /dev/full，静默写失败路径在 HEAD 不可达——临时还原「无探测无检查」原始缺陷态后 /dev/full 测试 Failed（无条件真返回），恢复修复后 Passed（同一用例，Linux 专有门控）；happy-path %PDF 头断言保持绿。全量 170/171（唯一 fail=ui_click_audit 既有本地档）
- [ ] 深审 P3：data_store save 写后不复查流态，磁盘满报成功丢数据（core/std/data_store_std.cpp:348-419）
- [ ] 深审 P3：sync_manager exportBackup 的 f.write 未检查（adapters/qt/sync_manager_qt.cpp:436）
- [ ] 深审 P3：stardict p+8+4>end 近端指针算术 UB（core/std/stardict_parser_std.cpp:110,114）
- [ ] 深审 P3：dictionary 伴生扫描 is_regular_file 抛掷重载（core/std/dictionary_std.cpp:63）
- [ ] 深审 P3：mdict 未识别容器兜底逐偏移 zlib 扫描 CPU/内存膨胀（core/std/mdict_parser_std.cpp:1007-1041）
- [ ] 深审 P3：mdict rewrite_attr 匹配 src= 也命中 data-src= 误改写（core/std/mdict_parser_std.cpp:776-796）
- [ ] 深审 P3：sync_engine 宽容 JSON 负值强转 u64 环绕（core/std/sync_engine_std.cpp:686-689）
- [ ] 深审 P3：lookup_adapter size_t→int 截断（qmlui/lookup_adapter.cpp:615-616）
- [ ] 深审 P3：dsl 备用词头逗号切分不发最后一段（core/std/dsl_parser_std.cpp:176-184）
- [ ] 深审 P3：ripemd128 len 近 SIZE_MAX 环绕越界（core/std/ripemd128_std.cpp:103,109）
- [ ] 深审 P3：tray_controller m_menu 无父且 setContextMenu 不接管 → 析构泄漏（qmlui/tray_controller.cpp:122）
- [ ] 深审 P3：mdd parse_simplekv_fallback 用 32 位 fseek/ftell，Windows >2GB 误析（core/std/mdd_resource_std.cpp:500,577）
- [ ] 深审 P3：store_jni/dict_jni raw new 转 jlong，未 close 泄漏（adapters/android/jni/store_jni.cpp:54-57、dict_jni.cpp:20-28）
- [ ] 深审 P3：jni_util handle_of 不校验，陈旧句柄空/UAF、双重 close 双释放（adapters/android/jni/jni_util.h:44-46）
- [ ] 深审 P3：mdd MAX_RESOURCE_SIZE 10MB 静默丢弃大资源无诊断（core/std/mdd_resource_std.cpp:980）
- [ ] 深审 P3：QuickLookupPane 词典 HTML 未净化（无脚本引擎暴露，信息性）（qmlui/components/QuickLookupPane.qml:251,255）
- [ ] 深审 P3：mdd extract_to_cache 仅替换 / 不替换 \\，Windows 逸出（无生产调用者，死路径）（core/std/mdd_resource_std.cpp:1051-1052）
- [ ] 深审 P3：CLI --dict-pass 密码可见于进程列表（信息性，常见权衡）

## 漏洞扫描遗留（2026-11-03 全舰队扫描，源 scratch/vuln-scan-*.md）

- [ ] VULN-UNIDICT-001 [BLOCKED·他人 stash-pop 冲突] docs/site 的 package.json 在扫描窗口处他人 stash-pop 冲突（UU、含冲突标记）致 pnpm audit 无法执行；本任务 2026-11-03 晚复核：工作树已无 UU 与冲突标记（docs/site/package.json 现为 staged M），但 audit 尚未出终态——不碰他人改动；不勾，验收：冲突方确认后 pnpm audit --audit-level=high 出终态（0 high）且 git status 无未合并路径
## 模糊搜索/纠错实测三连洞（2026-11-08 用户报 aple→apple 失败，实测确认）

- [x] UNI-FUZZY-001 [P0] 模糊层被释义匹配短路：`aple` 释义包含层命中 aplenty/Aplexa 等噪声词后，编辑距离层被跳过（93dacde 设计「前三层全空才走」），距离仅 1 的 apple 永远召回不了、也无 did-you-mean。修法：精确/前缀层全空即评估编辑距离候选；若有距离≤1 强候选，与释义包含命中并存但「您要找的是」置顶（或距离≤1 直接置顶重组）；T11 用例补 aple→apple 与「释义命中+强候选并存」两案。
  勾账 2026-11-08：诊断修正——aple 的噪声是**前缀层**噪声（aple 是 aplenty 真前缀）非释义层独有，「精确/前缀全空才评估」永不触发；落地口径=精确/词形还原两层全空即评估编辑距离，dist≤1 强候选置顶（rel 3）且与前缀（rel 2）/释义（rel 4+fulltext）/dist2 弱候选（rel 5）并存，无强候选时前缀/释义命中即止（正常前缀查询不掺全文噪声）。T11 helo→hello + 短查询闸门、T11b aple→apple 置顶且 aplenty(rel2)/abundance(rel4) 并存，两案钉住。
- [x] UNI-FUZZY-002 [P0] CLI 默认查询 apple 超时（>180s，build/Release/unidict_cli_std --scan-dir dictionaries apple 实测卡死）：释义匹配层全表扫无预算（GUI 有 kSuggestScanBudgetMs=60ms，CLI 路径没有）。修法：CLI 释义匹配层套同款时间预算 + 部分集标注；验收=apple/aple 单查 <5s 出结果。
  勾账 2026-11-08：search_all/full_text_search 加 ft_budget_ms（>0 时全文索引未建不触发分钟级懒构建、按现成索引返回部分集）；CLI 默认查询显式传预算并打「# 释义全文层未建索引…（结果为部分集）」标注。实测四库 378MB（ECDICT/FreeDict×2/WikDict×2/CC-CEDICT）：aple 1m48s rc=0、apple 2m1s rc=0、-m fuzzy aple 1m40s rc=0——>180s 卡死消除；墙钟为词典装载主导（约 1.5 分钟），查询段秒级出结果。
- [x] UNI-FUZZY-003 [P1] CLI 无 did-you-mean：suggest_corrections 只接了 GUI（gui/main.cpp:472），CLI 未命中路径不输出纠错建议。修法：未命中时输出「您要找的是：…」段（复用 suggest_corrections，max 5）。
  勾账 2026-11-08：CLI 未命中路径输出 "Did you mean:"（suggest_corrections max 5）；有释义命中时建议段置顶于噪声前，全 miss 时保持 Word not found 之后输出。实测 aple 输出 AELE/APL/APLH/aole/naple。
- [ ] UNI-FUZZY-RANK-001 [P2] did-you-mean/强候选无词频权重（001 勾账遗留，非短路回归）：apple 距 aple 仅 1，但模糊序按「距离+字典序」无词频加权，实大词典下距离 1 同分词 37+ 个（AELE/APL/APLH/aole/naple…），apple 排 38——CLI top-5 与 search_grouped 强候选窗口（fuzzy 前 12）都够不着（-m fuzzy aple 50 条实测 apple 在第 38 位）。T11b 小词典口径已钉逻辑正确。后续候选：建议/强候选按词频加权，或窗口加大 + 高频优先重组。

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
  - [ ] LAY-001 [P1] 拆分 qmlui/MainDesktop.qml（3143 行）——主窗、导航、查词结果、历史/生词本和七个设置页挤在一份声明式文件，设置页 2500 行后仍持续增长（qmlui/MainDesktop.qml:2500）。
  - [ ] LAY-002 [P1] 拆分 qmlui/Main.qml（1792 行）——移动壳同时承载响应式布局、抽屉导航、多 section 与结果视图，可按页面容器先切（qmlui/Main.qml:185）。
  - [ ] LAY-003 [P1] 拆分 core/std/mdd_resource_std.cpp（1689 行）——容器探测、多代私有格式、MDX record 块、资源查询与缓存导出同文件，格式编解码可独立（core/std/mdd_resource_std.cpp:935）。
  - [ ] LAY-004 [P1] 拆分 gui/main.cpp（1622 行）——匿名命名空间里聚合状态恢复、HTML 渲染、主窗控件、词典管理、语音资源和 main，Widget 面板可先抽（gui/main.cpp:62）。
  - [ ] LAY-005 [P1] 拆分 core/std/html_renderer_std.cpp（1210 行）——token 解析、白名单策略、CSS 处理、链接/媒体重写和高亮同文件，安全面改动会触碰全部路径（core/std/html_renderer_std.h:21）。
  - [ ] LAY-006 [P1] 拆分 core/std/mdict_parser_std.cpp（1176 行）——头部/加密探测、KIDX/RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 和启发式兜底集中于此，每种布局已有独立契约（core/std/mdict_parser_std.cpp:1）。
  - [ ] LAY-007 [P1] 拆分 core/std/dictionary_manager_std.cpp（1147 行）——装载/隔离、分组查询、索引缓存、资源转发和状态序列化同文件，聚合策略与 JSON 状态读写可分开（core/std/dictionary_manager_std.cpp:971）。
  - [ ] LAY-008 [P2] 拆分 qmlui/dev/ui_click_audit.cpp（2334 行）——UI 审计夹具、脚本/参数、页面交互断言和离屏输出混在一起，可按页面或用例组拆（qmlui/dev/ui_click_audit.cpp:213）。
  - [ ] LAY-009 [P2] 拆分 qmlui/components/EntryResultsPane.qml（1016 行）——内容 tab、全文/词组/联想加载、HTML 渲染和交互都在单文件（qmlui/components/EntryResultsPane.qml:119）。
  - [ ] LAY-010 [P2] 拆分 gui/click_audit_test.cpp（1030 行）——GUI 真点审计覆盖多个页面入口和模态流，按页面组拆可降低单测试文件耦合（gui/click_audit_test.cpp:1）。

- 目录/分类不合理处：
  - qmlui 的 `LookupAdapter` 位于壳目录但被 GUI 测试目标重复编译，说明它已是共享 Qt 表现适配层；`qmlui/CMakeLists.txt:56`、`gui/CMakeLists.txt:95`。
  - `PronunciationSourceStd`、`OnlinePron` 决策逻辑在 `core/std/online_pron_std.*`，HTTP/播放留在 `VoiceAdapter`；边界正确，但 `lookup_adapter.cpp` 的私有 `P0Modules` 仍直接持有 onlinepron 状态，绕过已存在的 VoiceAdapter 职责（qmlui/lookup_adapter.cpp:348、core/std/online_pron_std.h:42）。
  - `SyncServiceQt` 是本地 JSON 合并器，`SyncManagerQt` 才是网络中继客户端；两者同名 `syncNow()` 且都注册进 QML（`sync`/`syncManager`），调用面易混（adapters/qt/sync_service_qt.h:11、adapters/qt/sync_manager_qt.cpp:214、qmlui/main.cpp:237）。
  - Android JNI 源码放在 `adapters/android/jni/`，但 Kotlin API 壳在 `android/app/.../UnidictCore.kt`；桥两端跨根目录，接口字段改动必须跨目录同步（adapters/android/jni/dict_jni.cpp:1、android/app/src/main/kotlin/dev/unidict/mobile/UnidictCore.kt:20）。
  - `docs/site/.vitepress/dist/`、`docs/site/node_modules/` 已出现在工作树；构建产物/依赖目录与 VitePress 源同层，若未忽略会污染源码盘点（docs/site/.vitepress/dist/index.html:1、docs/site/node_modules/.bin/vite:1）。
