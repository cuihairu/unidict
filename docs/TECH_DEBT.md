# Unidict 技术债清单

> Phase 1 · 现状基线 · 审计日 2026-10-04 · 基线提交 fae5002
> 现状审计的负面产出。只陈述事实与处置方向建议，不实施。
> 正面资产清单见文末（防只报忧误伤）。

## 六问速答

> 2026-10-10 增量更新（下文保留审计日口径，此处只记收口差异）：双 CLI 已退役
> （0f688c8，cli-std 唯一）；双桌面壳已选型收口（QML 唯一主出口，gui 转发音练习
> 特性壳）；词本 API 已单家族（LearningManager 退役，TD-124 勾）；平台债③–⑦全收口
> （TD-132 勾）；benchmark CSV 已清（TD-134 勾）；Android 壳 M0–M5+M7 全交付
> （余发布签名等密钥）；roadmap 漂移修正（TD-143 勾）；charset_codec 分支巡检
> 收口（72%→82%，eb18b4e）；TD-120 零死 QML 勾；TD-121 现状复核（102
> Q_INVOKABLE、空桩已接真、Drawer 已落 qmlui）；TD-101/105 口径收窄（主链
> 已切 std，legacy 管家仅剩 gui 特性壳消费）；同日晚 gui 也切 std（9ccf0bf），
> legacy manager 生产消费面清零；同日增量 B 连码退役（927f376）：legacy 链
> （unidict_core.cpp + 四套 Qt 解析器 + plugin_manager + 桥接层 +
> core_lookup_tests）全删，TD-101/102/105 勾；TD-117 标签正名（近反义词→
> 近义联想，两壳+活文档全量同步）；TD-122 十五键盘点零漂移勾；TD-112/113/114
> 现状复核勾（Main.qml=移动端活入口、learning_manager 面已随 P-7 批四清零、
> roadmap 复习条目已对账）。卡点复核
> 维持：TD-131 残余（视觉像素回归需人工对照）、TD-133（真模型 635MB 不可
> 入柜）；记录止步：TD-123（剪贴板两文件职责尚清，二合一属跨壳耦合
> churn）；用户域不动：TD-142（README 产品文案定位）、TD-144（云级设计稿
> 已有"未实现"标注）。

1. **当前已实现什么** → 见 CURRENT_FEATURE_MATRIX.md。能力面远超"查词器"：六格式双面解析、
   六种检索、聚合/去重/相关性、HTML 净化渲染、生词本+标签+笔记、历史、TTS+在线发音+口音、
   发音评分 M1–M9（门控）、文件级同步 MVP、同步中转 relay B1（双参考实现）、AI 外部命令桥、
   Windows 桌面集成三件套、Android 原生壳 M0–M3-C（M4 TTS、M5 出包未做）、纯 std 主力 CLI、
   CI 三 OS+Android 每日构建。
2. **哪些功能重复** → ①解析器 4 对双实现（2026-10-10 全清——legacy 四套随增量 B 连码退役，std 单口径）；
   ②双桌面壳（gui QWidget 与 qmlui 都活、近月都有提交）；③双 CLI（cli 遗留 3 子句 vs cli-std 全功能）；
   ④学习数据双存储（DataStore 词本 vs learning_stats.json）；⑤聚合计分两套不共享；
   ⑥生词本 API 两家族（lookup_adapter.vocabulary* vs learningManager getAllStats）；
   ⑦剪贴板域两文件（职责尚清）。
3. **哪些 API 不稳定** → ①legacy/std 两 manager 公开面不对称（见 TD-103）——2026-10-10 起
   legacy manager 已退役，此条随 TD-105 一并消解；②lookup_adapter 102 方法上帝桥（TD-121）；
   ③relatedLookup 键重载携带两个无关联义（TD-117）；
   ④HtmlRenderOptions 7/8 字段声明未读（TD-115）；⑤*ParserQt 桥随 legacy 链退役（TD-102 勾）；
   ⑥settings_qt 键名由 QML 调用方自由发明（TD-122）。
4. **哪些地方耦合** → ①UI 主链 qmlui→lookup_adapter→DictionaryManagerStd（gui 同切 std，
   legacy manager 已删，TD-101/105 勾）；②DataStore 双跳门面（TD-106）；③gui 音频三件套不进覆盖率（TD-154）；
   ④ONNX 依赖锁在 adapters/pron（Pimpl 隔离良好，正面）。
5. **哪些地方缺测试** → ①QML 自动化仅有 sandbox 截图 + 57 项真点审计，像素回归仍靠人工（TD-131）；
   ②平台债 5 项未修（TD-132）；③onnx_pron_scorer 不在 std 闸门、真模型不可入柜（TD-133）；
   ④benchmark 数据单次残留（TD-134）。正面：core/std lines/functions 100% 闸门、主题对比度
   WCAG AA 机器回归、charset_codec/ripemd128 间接覆盖无空洞。
6. **哪些偏离定位** → ①learning_manager 游戏化 API（成就/激励语/每日目标）为死码但存在且有存储
   （TD-113）；②635MB 评分模型 vs 轻量定位——已门控+自助下载，结构性可控（TD-152）；
   ③云级同步/server 设计稿与 local-first 定位并置文档面，易被误读为既定方向（TD-144）；
   ④「近反义词」名实不符（联想/近义，TD-117）。

---

## A. 双实现与过渡债（优先级最高，收敛期主战场）

- ~~**TD-101 解析器四对双实现，生产走 legacy**~~（2026-10-10 收口，927f376）：
  legacy 四套 Qt 解析器（stardict/mdict/json/epub）与 unidict_core.cpp 工厂
  连同其桥接层（adapters/qt/*_parser_qt）全部删除。core/unidict_core.h 仅留
  DictionaryEntry DTO（DataStore 门面与 adapters/qt 共用形状）。生产链 std 单
  口径：DictionaryManagerStd + core/std 四套 std 解析器。
- ~~**TD-102 *ParserQt 桥为测试专用面**~~（2026-10-10 收口，927f376）：
  桥接层（json_parser_qt / mdict_parser_qt / stardict_parser_qt /
  plugin_manager_qt）随 TD-101 一并删除，tests/qt_adapters_test.cpp 三座桥
  slot 退役（AI 服务/剪贴板/设置项保留）。plugin_manager 插件注册表生产零
  消费面确认——全链零调用点。
- ~~**TD-103 两 manager API 不对称**~~（2026-10-10 随 legacy 链退役消解，927f376）：
  legacy DictionaryManager 整面删除，std DictionaryManagerStd 单口径；
  迁移期双份维护税终结。
- ~~**TD-104 聚合计分双套不共享**~~：已收口（2026-10-10，随字典本职质量批 ② 复核）——
  复核发现该条前提已漂移：UI 主链 aggregateLookup 早在 P-6 批一切桥时改走
  DictionaryManagerStd::search_grouped（五层降级，qmlui/lookup_adapter.cpp:1129 起），
  「legacy searchGrouped 包装」描述过时；CLI 用 manager 各检索面、从不用聚合器。
  真实残余 = std DictionaryAggregator（aggregate_lookup_std ~1070 行）生产零消费孤儿，
  双套分歧风险只在有人再接线时成立。处置：按消费面扫描惯例连码退役（源+双测试 target
  +cover 测试 aggregate 段），聚合单口径=manager.search_grouped 永久收口。
- ~~**TD-105 legacy DictionaryManager ~1200 行单例**~~（2026-10-10 收口，927f376）：
  单例与实现（core/unidict_core.cpp）随 legacy 链退役，gui 改走
  DictionaryManagerStd（9ccf0bf），tests/core_lookup_tests.cpp 整文件退役
  （1006 行 Qt Test，七处真实回归缺口先移植进 std 测试面再删）。
- **TD-106 DataStore 双跳门面**：48 行门面 → DataStoreQt → DataStoreStd。干净但多一跳；
  plugin_manager 随 legacy 链退役（927f376），唯一 legacy 消费点随之消失。

## B. 死代码与定位漂移（"不为增加功能而增加功能"的清理清单）

- ~~**TD-111 MainModern 全套死树**~~：已收口（2026-10-08）——qmlui/MainModern.qml +
  qmlui/modern/ 整树（19 文件 ~8.8K 行）删除；删前复判全库零引用（qrc/CMake/dev 工具/
  tests 均无命中），活面 qmlui 只剩 8 个 QML。出口=UI 重设计批次清场，不再保留死树候选。
- ~~**TD-112 Main.qml 死路径仍编 qrc**~~（2026-10-10 复核勾）：「死路径」判断
  不成立——qmlui/main.cpp:223-228 按 `Q_OS_ANDROID || Q_OS_IOS` 分支加载，
  Main.qml 是移动端（Android 壳 M0–M5+M7）活入口，必须编 qrc。原记录
  「learningManager 唯一消费方」也已过时：qmlui 全树 learningManager 引用
  已随 P-7 批四清零（grep 零命中）。
- ~~**TD-113 learning_manager 游戏化 API + 双存储**~~（2026-10-10 复核勾）：
  LearningManager 类随 P-7 批四退役，游戏化 API 与 learning_stats.json 存储
  一并消失（全库 grep 零命中，仅 theme_tokens.h 一处注释提及已顺手改）。
  历史残余重定性：qmlui（std 链→DataStore，data_store_std.cpp:424）与 gui
  （legacy manager→dictionary_state.json）**各写各源、单路径无双写**；
  跨壳历史不同源是双壳并存的固有面，std 管理器 history 缺口已注记
  （dictionary_manager_std.h:70），随壳收敛决策走。
- ~~**TD-114 复习/遗忘曲线无活路径**~~（2026-10-10 复核勾）：roadmap 复习
  条目已随 TD-143 对账清除（9ef59a2，「[x] 与用户可达不符」已消）；复习
  会话 UI 无活面；词卡四技能标记写入口活（Main.qml cycleSkill，P-7 复习面
  数据预留），将来做复习面时数据侧就绪。
- ~~**TD-115 HtmlRenderOptions 7/8 字段声明未读**~~：已收口（2026-10-04）——七死字段移除，
  仅留 resolve_links；内容允许面归 sanitize 白名单，自定义解析走实例级 set_link_resolver。
- ~~**TD-116 aggregate examples/pronunciation 计分结构性死分支**~~：已收口（2026-10-04）——
  核心侧无结构化发音/例句来源（QML 卡片音标走自身 extractPhonetics），字段 + 计分臂 + 
  GCOVR_EXCL 一并移除，头文件留注说明；消费方仅单测，已同步改。
- ~~**TD-117 "近反义词"名实不符**~~（2026-10-10 收口）：`related` 键实义 =
  前缀+模糊候选词（近义/联想），无任何反义词数据源——qmlui EntryResultsPane
  与 gui 五视图两壳标签统一改「近义联想」，注释/审计断言/README 双语/
  USER_GUIDE/CURRENT_ARCHITECTURE/FEATURE_MATRIX/product-principles 活文档
  全量同步。顺带纠正 ARCHITECTURE 原记录错误：全文检索走独立 `fullTextLookup`，
  `relatedLookup("phrases")` 只承载词组 tab——「phrases 键同形复用承载全文
  检索」原判不成立，两键同为词头关联域（键义注释钉 lookup_adapter.h:169）。
  gui 侧市场调研文档（dictionary-market-survey.md 述竞品）按存档口径不回改。
- ~~**TD-118 双 CLI 功能面不对等**~~（2026-10-10 收口，0f688c8）：cli(Qt) 3 子句 vs cli-std 全功能（六格式/六模式/索引运维/评分/下载）——cli(Qt) 连 target/test/打包行退役，cli-std 为唯一 CLI。
- ~~**TD-119 双桌面壳并存**~~（2026-10-10 收口，419d58b）：QML 定为唯一桌面主出口（产品 UI/自动化/daily-build 主程序全在 QML）；gui 经消费面扫描为发音练习专用壳（录音/跟读/评分/模型下载仅此有）保留特性壳身份，ci.yml Windows artifact 对齐 unidict_qml。
- ~~**TD-120 qmlui 约 11K 行 QML 仅 ~2.1K 在用户路径**~~（2026-10-10 收口）：
  「11K 行」大头是被 TD-111 删除的 MainModern 死树（19 文件 ~8.8K 行）——现树
  5185 行/8 文件全数入 qrc 且全有实例化引用（Main.qml mobile 面引 mobile/common
  三件：MobileFileDialog 4 处、DefinitionContent 2 处、ResponsiveLayout 1 处；
  MainDesktop.qml 引 components 三件共 6 处）——**零死 QML 文件**，脚手架残留
  判断不成立。残余口径：条件分支内的可达性（移动限定区/暗主题臂）未逐行复扫，
  属性级粒度按需再做。

## C. API 稳定与耦合

- **TD-121 lookup_adapter 102 Q_INVOKABLE 上帝桥**：查词/渲染/学习/平台全一类；
  rewriteResourceUrls/setRelevanceMode 等字节级细节外露。QML–Qt 职责边界已模糊。
  （2026-10-10 现状复核：原记录三处过时已按实改——①空桩 setDictionaryPriority/
  setDictionaryEnabled 已随 P-7 批一接真（启停只影响查询路径、变更发 stamp）；
  ②qmlui Drawer「词典」页已随 P-6 批九落地词典状态 UI（原「只在 gui 对话框」不实）；
  ③后端已切 DictionaryManagerStd 成员，历史/生词本仍经 DataStoreQt 转发器。残余债
  收窄为「单类大面」本身。）
- ~~**TD-122 settings_qt 泛型无领域键**~~（2026-10-10 收口）：全仓 15 键盘点
  （QML 侧 6 键 13 处调用 + C++ 直连 9 键 10 文件）全部「域/字段」一致格式，
  零漂移零拼写分叉；包装层实为 49 行薄面。键目录化重构属无观察需求的
  churn，记录止步。
- **TD-123 剪贴板域两文件**：clipboard_monitor（轮询取词）vs ClipboardQt（读写）——职责尚清，
  概念域重复，收敛期可二合一。
- ~~**TD-124 词本 API 两家族**~~（2026-10-10 收口）：LearningManager 随 P-7 批四退役后
  getAllStats 家族消失（全库 grep 零命中），词本 API 只剩 adapter.vocabulary* 单家族。

## D. 测试缺口

- **TD-131 QML 自动化测试薄**（2026-10-05 部分收口）：unidict_ui_sandbox（UNIDICT_BUILD_UI_SANDBOX
  默认 OFF，CI qt job 开）离屏拼真实 MainDesktop 截 16 图 + 像素断言；新增 unidict_ui_click_audit
  57 项真点断言（press+release 送窗口，弹层/切页 settle，core 双证）已入 ctest（build 147 之列）。
  残余：视觉像素回归仍靠人工截图对照（BUG-010 双端对照未完）。
- ~~**TD-132 CI 平台债 ③–⑦ 五项未修**~~（2026-10-10 收口）：④⑤⑥⑦ 7afd4c0+3973dfe 修复（CI 全平台绿实证）；
  ③ 资源本地路径直拼 file:// 链接（反斜杠进链接+盘符缺前导 /）8c9d914 发射点统一归一修复。
- **TD-133 onnx_pron_scorer 测试不在 std 闸门**：仅 Qt 构建 + PRON 门控；真模型 635MB 不可入柜
  （fake CTC fixture 只到 unit 面），无端到端自动验证。
- ~~**TD-134 benchmark CSV 为单次残留**~~（2026-10-10 清，38a1355）：两份 CSV 出库 + .gitignore（benchmark.sh 每次运行重写，需时现跑）。
- 正面脑筋线：core/std lines/functions 100% 闸门、分支 71.1% 趋势在涨、theme_tokens_contrast
  WCAG AA 机器回归、charset_codec/ripemd128 由相邻测试间接覆盖无真空洞。

## E. 文档债

- ~~**TD-141 根目录三份分析文档疑似过时**~~（2026-10-10 清，38a1355）：三份出库（含 12 处已退役模块引用，无入链）；architecture_diagram.txt 活文档保留。
- **TD-142 README 为 nightly 分发导向**（badge+每日构建+一键安装），非产品定位文案——Phase 2 收敛靶。
- ~~**TD-143 roadmap 与现状漂移**~~（2026-10-10 收口）：roadmap.md 全表对账——§A 禁区条目
  （Visual Lookup/voice search/悬停取词/手势取词/在线翻译/插件运行时/Anki 导出/复习算法自定义/
  学习统计/游戏化/本地加密三件）逐条划线注明 dropped per §A；发音练习 M1–M9 勾账（停 M3 的旧注
  换成简洁交付行）；重复条目（词本两行、Markdown 两行）去重；Share menu integration 勾账
  （Android M7）；In-text lookup 勾账（剪贴板+热键承载）；增量同步算法勾账（B5 relay）；v1.x
  分阶段叙事同步。
- **TD-144 云级设计稿并置文档面**：server_plan.md / design/sync-engine.md 与 local-first 定位同目录，
  已有"未实现/审核前不动"标注，风险在后续会话误读为既定方向。
- ~~**TD-145 MOBILE_ADAPTATION_REPORT.md 命名误导**~~（2026-10-10 收口，bd82ca3）：改名
  docs/qmlui_mobile_adaptation_report.md（git mv，文件头 2026-09-29 注保留），
  mobile_plan.md 三处活引用同步；todo.md 历史行按存档口径不回改。

## F. 平台与依赖

- **TD-151 热键/开机自启 Linux/macOS stub**：roadmap 明文 "revisit on demand"——显式取舍，
  与跨平台定位有隙，收敛期重估。
- **TD-152 635MB 评分模型 vs 轻量定位**：已三重复合软着陆（UNIDICT_BUILD_PRON 默认关、模型不入库
  运行时给路径、M10 自助下载）。观测点：不得进入默认安装包。
- ~~**TD-153 M10 自举下载依赖外部 curl**~~（2026-10-10 收口）：cli-std 下载入口
  加 curl 可用性预检（`curl --version` 探测，Windows 走 `>nul`），缺失时定向
  提示（安装 curl 或手动下载资产到模型目录）并 return 5 同族退出码——不再让
  用户对着截断退出码（cmd 9009&0xff 与真错误码撞车）猜。断点续传/坏包
  重来逻辑不变。
- **TD-154 gui 音频三件套不进覆盖率**（QT_EXCLUDES 逐项注释给理由）：recorder/playback/waveform
  为平台薄壳可接受，但 pcm_util 纯逻辑应续保。

---

## 正面资产（防只报忧）

- **闸门纪律**：coverage.sh 退出码唯一可信 + build-std 123/123 + build(Qt) 147/147 + 分支趋势在涨；
  "不为凑数强凑"既定口径（GCOVR_EXCL 均有注释理由）。
- **core 零 Qt 成立**：core/std 68 文件 include 扫描无 Qt 命中；cli-std/jni 双无 Qt 生产链已验证。
- **local-first 基线**：查词核心路径无网络依赖；在线发音仅外发查询词且 UI 明示。
- **CI 面**：三 OS ×（std+Qt）+ Android 每日构建 + Windows 安装器静默验证 + 单出口文档部署。
- **隐私/安全面**：URL path 转义防路径穿越、HTML 白名单净化、加密词典支持、SHA-256 校验链。

处置方向总注：B 类以"先定数据模型/API 存废、再删死树"为序，不并行的清理；A/C 类随
Phase 3 Core 稳定性一并收敛；D 类平台债专批处理；E/F 类随 Phase 2 文档收敛。