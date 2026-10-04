# Unidict 技术债清单

> Phase 1 · 现状基线 · 审计日 2026-10-04 · 基线提交 fae5002
> 现状审计的负面产出。只陈述事实与处置方向建议，不实施。
> 正面资产清单见文末（防只报忧误伤）。

## 六问速答

1. **当前已实现什么** → 见 CURRENT_FEATURE_MATRIX.md。能力面远超"查词器"：六格式双面解析、
   六种检索、聚合/去重/相关性、HTML 净化渲染、生词本+标签+笔记、历史、TTS+在线发音+口音、
   发音评分 M1–M9（门控）、文件级同步 MVP、AI 外部命令桥、Windows 桌面集成三件套、
   Android 原生壳 M0–M5、纯 std 主力 CLI、CI 三 OS+Android 每日构建。
2. **哪些功能重复** → ①解析器 4 对双实现（生产走 legacy Qt 四套，std 四套+测试专用桥闲置）；
   ②双桌面壳（gui QWidget 与 qmlui 都活、近月都有提交）；③双 CLI（cli 遗留 3 子句 vs cli-std 全功能）；
   ④学习数据双存储（DataStore 词本 vs learning_stats.json）；⑤聚合计分两套不共享；
   ⑥生词本 API 两家族（lookup_adapter.vocabulary* vs learningManager getAllStats）；
   ⑦剪贴板域两文件（职责尚清）。
3. **哪些 API 不稳定** → ①legacy/std 两 manager 公开面不对称（见 TD-103），交接无一一对应；
   ②lookup_adapter 76 方法上帝桥（TD-121）；③relatedLookup 键重载携带两个无关联义（TD-117）；
   ④HtmlRenderOptions 7/8 字段声明未读（TD-115）；⑤*ParserQt 桥是测试专用面，易被误当"接线完成"（TD-102）；
   ⑥settings_qt 键名由 QML 调用方自由发明（TD-122）。
4. **哪些地方耦合** → ①UI 主链 qmlui→lookup_adapter→legacy DictionaryManager（1200 行单例）→部分 std 引擎，
   生产链无法纯 std（TD-101/105）；②DataStore 双跳门面（TD-106）；③gui 音频三件套不进覆盖率（TD-154）；
   ④ONNX 依赖锁在 adapters/pron（Pimpl 隔离良好，正面）。
5. **哪些地方缺测试** → ①无 QML 自动化测试，视觉回归靠手动 sandbox 截图（TD-131）；
   ②平台债 5 项未修（TD-132）；③onnx_pron_scorer 不在 std 闸门、真模型不可入柜（TD-133）；
   ④benchmark 数据单次残留（TD-134）。正面：core/std lines/functions 100% 闸门、主题对比度
   WCAG AA 机器回归、charset_codec/ripemd128 间接覆盖无空洞。
6. **哪些偏离定位** → ①learning_manager 游戏化 API（成就/激励语/每日目标）为死码但存在且有存储
   （TD-113）；②635MB 评分模型 vs 轻量定位——已门控+自助下载，结构性可控（TD-152）；
   ③云级同步/server 设计稿与 local-first 定位并置文档面，易被误读为既定方向（TD-144）；
   ④「近反义词」名实不符（联想/近义，TD-117）。

---

## A. 双实现与过渡债（优先级最高，收敛期主战场）

- **TD-101 解析器四对双实现，生产走 legacy**：unidict_core.cpp:79-86 工厂硬连 Qt 解析器；
  std 四套与 *ParserQt 桥仅测试引用。风险：同一格式两套解析语义，修复可能只落一边；
  UI 面无法关掉 Qt 依赖。处置方向：以 std 为唯一真源做门面收敛（Phase 3 的自然输面）。
- **TD-102 *ParserQt 桥为测试专用面**：tests/qt_adapters_test.cpp 独苗。桥的存在易被误读为
  "std 已接线生产"，实际 UI 主链不消费。另：legacy plugin_manager 插件注册表生产**零消费**——
  唯一调用点 tests/legacy_parsers_test.cpp:532，qmlui/gui/cli 均不触碰；属纯架构预留，
  切桥时可整面不迁。处置方向：收敛时明确桥与注册表的存废。
- **TD-103 两 manager API 不对称**：std 无 searchGrouped/历史/失败隔离；legacy 无 substring/单查 fuzzy。
  交接无一一对应，迁移期双份维护成本为持续税。
- **TD-104 聚合计分双套不共享**：UI 主链聚合 = legacy searchGrouped 三层降级（桥面包装
  lookup_adapter.cpp:924-941）；std DictionaryAggregator（priority/profiles/去重/relevance）
  在 UI 链零消费（仅 cli-std 与 std 单测活）。两套并存不共享（BUG-009 曾以返回形态差异暴露）。
  处置方向：P-3 3.3 把三层降级语义移植进 std 聚合器，切桥时统一单口径。
- **TD-105 legacy DictionaryManager ~1200 行单例**：注册/状态/历史/隔离/检索全在一类。
  "Dictionary vs DictionaryManager 分离"的直接靶子。
- **TD-106 DataStore 双跳门面**：48 行门面 → DataStoreQt → DataStoreStd。干净但多一跳；
  plugin_manager 仍持 legacy 头（唯一 legacy 消费点之一）。

## B. 死代码与定位漂移（"不为增加功能而增加功能"的清理清单）

- **TD-111 MainModern 全套死树**：~5.5K 行 14 组件（ModernHistoryPage 626 行 / ModernVocabularyPage
  939 行 / ModernSettingsPage 756 行等）零引用，不在 qrc。删前无依赖（全库 grep 无命中）。
- **TD-112 Main.qml 死路径仍编 qrc**：移动壳已取代；它是 learningManager 唯一消费方。
  删除即把学习统计存储面一并带走，需先定 learning_manager 存废。
- **TD-113 learning_manager 游戏化 API + 双存储**：成就/激励语/每日目标/进度统计（getAchievements/
  getMotivationalMessage/getDailyTarget/getProgressStats/getWeakWords）只在死 Main.qml 消费；
  learning_stats.json（AppDataLocation）与 DataStore 词本呈双存储。与"不做排行榜/签到/成就"定位
  直接冲突——功能面死码，先定数据模型再清。另：**历史同样双写**——legacy manager 状态文件
  （dictionary_state.json history 段）+ lookup_adapter.cpp:143/976 向 DataStore 双写；
  qmlui 历史 tab 读 DataStore、gui 读 manager 侧。双存储族清理时一并定历史单一事实源。
- **TD-114 复习/遗忘曲线无活路径**：roadmap [x] 与用户可达不符；复习 UI 只在死路径。
- ~~**TD-115 HtmlRenderOptions 7/8 字段声明未读**~~：已收口（2026-10-04）——七死字段移除，
  仅留 resolve_links；内容允许面归 sanitize 白名单，自定义解析走实例级 set_link_resolver。
- ~~**TD-116 aggregate examples/pronunciation 计分结构性死分支**~~：已收口（2026-10-04）——
  核心侧无结构化发音/例句来源（QML 卡片音标走自身 extractPhonetics），字段 + 计分臂 + 
  GCOVR_EXCL 一并移除，头文件留注说明；消费方仅单测，已同步改。
- **TD-117 "近反义词"名实不符**：relatedLookup(word,"related") 实为联想/近义集合；"phrases" 键同形
  复用承载全文检索 tab。UI 文案与语义漂移。
- **TD-118 双 CLI 功能面不对等**：cli(Qt) 3 子句 vs cli-std 全功能（六格式/六模式/索引运维/评分/下载）。
- **TD-119 双桌面壳并存**：gui（QWidget）与 qmlui（Qt Quick）都活、近月各有提交（759f93e 欧路重排走 qmlui；
  发音 M5–M9 走 gui）。产品出口未定时双份维护，是收敛期的保留观察项。
- **TD-120 qmlui 约 11K 行 QML 仅 ~2.1K 在用户路径**：qrc 只编 7 文件；mobile/ 脚手架残留。

## C. API 稳定与耦合

- **TD-121 lookup_adapter 76 Q_INVOKABLE 上帝桥（44KB）**：查词/渲染/学习/平台全一类；
  rewriteResourceUrls/setRelevanceMode 等字节级细节外露。QML–Qt 职责边界已模糊。
  且含**空桩**：setDictionaryPriority / setDictionaryEnabled 为 Q_UNUSED 占位（注释引
  DictionaryAggregator 但未接线），qmlui Drawer「词典」页纯只读——词典状态 UI 实际只在
  gui（QWidget）对话框；查词/历史/词本之桥直连三类后端（legacy manager / DataStoreQ/壳内服务）。
- **TD-122 settings_qt 泛型无领域键**：键名由 QML 调用方自由决定，拼写/迁移风险。
- **TD-123 剪贴板域两文件**：clipboard_monitor（轮询取词）vs ClipboardQt（读写）——职责尚清，
  概念域重复，收敛期可二合一。
- **TD-124 词本 API 两家族**：adapter.vocabulary* 与 learningManager.getAllStats 等价面并存。

## D. 测试缺口

- **TD-131 无 QML 自动化测试**：unidict_ui_sandbox（UNIDICT_BUILD_UI_SANDBOX 默认 OFF）离屏拼真实
  MainDesktop 截 16 图 + 像素断言，手动运行、无 CI 接线；视觉回归靠人工（欧路重排即此路径）。
- **TD-132 CI 平台债 ③–⑦ 五项未修**（todo.md 记录 "待专批处理"，均有初步根因结论）：
  ③ Windows mdict 链接替换后 file:// 判定挂；④ Windows test_qt_adapters_bridge 0.13s 闪败待定位；
  ⑤ cli_main list 输出 Windows/macOS 双挂（期望串精确比较）；⑥ macOS PronunciationPanel
  play->isEnabled()（无音频后端门控）；⑦ macOS test_sha256_std arm64 runner 双 job 同挂。
- **TD-133 onnx_pron_scorer 测试不在 std 闸门**：仅 Qt 构建 + PRON 门控；真模型 635MB 不可入柜
  （fake CTC fixture 只到 unit 面），无端到端自动验证。
- **TD-134 benchmark CSV 为单次残留**：benchmark_results.csv 13 行 / memory_results.csv 4 行数值全同，
  无历史趋势，数据不可信。
- 正面脑筋线：core/std lines/functions 100% 闸门、分支 71.1% 趋势在涨、theme_tokens_contrast
  WCAG AA 机器回归、charset_codec/ripemd128 由相邻测试间接覆盖无真空洞。

## E. 文档债

- **TD-141 根目录三份分析文档疑似过时**：core_analysis.md / CORE_ANALYSIS_INDEX.md /
  core_quick_reference.md 无日期标注，与 core/std 现状对应关系存疑（与 docs 文档站并存易混）。
- **TD-142 README 为 nightly 分发导向**（badge+每日构建+一键安装），非产品定位文案——Phase 2 收敛靶。
- **TD-143 roadmap 与现状漂移**：Markdown 条目重复（一勾一空）、发音条目停在 M3 而实际 M1–M9 全实装、
  复习 [x] 不可达、游戏化 [ ] 但死码已实现。
- **TD-144 云级设计稿并置文档面**：server_plan.md / design/sync-engine.md 与 local-first 定位同目录，
  已有"未实现/审核前不动"标注，风险在后续会话误读为既定方向。
- **TD-145 MOBILE_ADAPTATION_REPORT.md 命名误导**：实为 qmlui 响应式改造报告（2026-09-29 注），
  易被当作手机 App 交付物。

## F. 平台与依赖

- **TD-151 热键/开机自启 Linux/macOS stub**：roadmap 明文 "revisit on demand"——显式取舍，
  与跨平台定位有隙，收敛期重估。
- **TD-152 635MB 评分模型 vs 轻量定位**：已三重复合软着陆（UNIDICT_BUILD_PRON 默认关、模型不入库
  运行时给路径、M10 自助下载）。观测点：不得进入默认安装包。
- **TD-153 M10 自举下载依赖外部 curl**：cli-std 写 curl config 实现断点续传；Windows 无内置 curl
  时该功能静默不可用（应反馈提示）。
- **TD-154 gui 音频三件套不进覆盖率**（QT_EXCLUDES 逐项注释给理由）：recorder/playback/waveform
  为平台薄壳可接受，但 pcm_util 纯逻辑应续保。

---

## 正面资产（防只报忧）

- **闸门纪律**：coverage.sh 退出码唯一可信 + build-std 115/115 + build(Qt) 137/137 + 分支趋势在涨；
  "不为凑数强凑"既定口径（GCOVR_EXCL 均有注释理由）。
- **core 零 Qt 成立**：core/std 68 文件 include 扫描无 Qt 命中；cli-std/jni 双无 Qt 生产贝已验证。
- **local-first 基线**：查词核心路径无网络依赖；在线发音仅外发查询词且 UI 明示。
- **CI 面**：三 OS ×（std+Qt）+ Android 每日构建 + Windows 安装器静默验证 + 单出口文档部署。
- **隐私/安全面**：URL path 转义防路径穿越、HTML 白名单净化、加密词典支持、SHA-256 校验链。

处置方向总注：B 类以"先定数据模型/API 存废、再删死树"为序，不并行的清理；A/C 类随
Phase 3 Core 稳定性一并收敛；D 类平台债专批处理；E/F 类随 Phase 2 文档收敛。