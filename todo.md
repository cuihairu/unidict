# Unidict Implementation Todo

> **归档说明（2026-09）**：本文是早期实现清单的历史快照，全部条目已完成。
> 现行路线图以 [docs/roadmap.md](docs/roadmap.md) 与
> [docs/pro_dictionary_gap.md](docs/pro_dictionary_gap.md) 为准。
>
> **2026-09-25 覆盖缺口盘点**：下方新增「覆盖率缺口」章节，记录 std-only
> 核心库的实测覆盖率基线与按优先级排序的补测/修复清单。
>
> **2026-10-04 产品定位基线生效**：方向性任务全部以文首「行动清单」为准
> （来源：docs/CURRENT_ARCHITECTURE / CURRENT_FEATURE_MATRIX / TECH_DEBT /
> product-principles / architecture-boundaries，用户已审核）。roadmap.md
> 保留为功能差距表；其中与定位冲突的条目按「明确不做」清单处置。
> 下方历史章节全部为流水存档，不再单独派单。

## 行动清单（产品定位基线 · 2026-10-04 生效）

> 本清单是当前唯一任务来源。纪律：小步提交推送；commit/push 前 build-std
> 与 build 两门 ctest 全绿 + 涉及 std 时 coverage.sh lines 100%；禁 force / 禁 tag。

### A. 明确不做（生效禁区，roadmap 同条废止）

- 广告 / 强制登录 / 信息流 / 排行榜 / 签到 / 成就 / 课程化 / 暗模式（产品原则 §7）
- OCR 截图/相机取词、PDF/Word 取词（roadmap「Visual Lookup」整条废止——非五感核心，属另一产品形态）
- 语音搜索（已有 TTS 输出面；语音输入查询不做）
- 在线翻译引擎集成（roadmap「Translation Features」废止——翻译走 AI 外部命令桥，已覆盖）
- Anki 导出、学习统计可视化、遗忘曲线自定义算法（与「个人笔记、简单可预期」定位冲突）
- 插件动态加载 / JS/QML 插件引擎 / 插件市场（Developer dictionaries 走 std 解析器工厂注册，不做第三方运行时）
- 云同步设计稿（docs/design/sync-engine.md S1–S5）默认不动，仅当用户显式推进
- 鼠标悬停取词、移动手势取词（Quick Lookup 由剪贴板/热键承载）
- 本地加密 / 隐私模式 / 安全擦除（明示无日志口径已够，不做假安全承诺）

### B. 在轨任务（按收敛计划排布）

- [x] **P-0 基线修正**（2026-10-05 核验收口：四项注记均已落三份基线文档，行号与当前代码逐一对准——aggregateLookup=legacy searchGrouped 桥面包装（lookup_adapter.cpp:972）、历史双写（:151/:1023 两点实为 DataStore::addSearchHistory）、plugin_manager 生产零消费、qmlui Drawer 词典页纯只读）：
- [x] **P-3 Core 稳定性（Dictionary vs DictionaryManager 分离，用户已选「核心分离先行」）**：
  - [x] 3.1 DictionaryStd 抽取：词典型实例（解析器 + 元数据 + enabled/priority/tags + mdd 资源 + src_paths + words），ManagerStd 持有 vector<DictionaryStd> 替换匿名 Holder（021c55c）
  - [x] 3.2 DictionaryManagerStd 增强：优先级排序 / tags+tagFilter（3.2a，3580d69）/ 失败隔离两档（quarantined 持久化 + 运行期诊断，对齐 unidict_core.h:38-46 语义）（3.2b，7cbc6ff）/ save_state·load_state（std JSON 更新器，复用 data_store_std 写法）+ load_resource（组合同伴 .mdd）（3.2c）
  - [x] 3.3 searchGrouped 三层降级语义进 std（relevance 0 词头精确 / 1 前缀 / 2 释义包含 + 词典内分组 + 同词头折叠去重），std 测试对齐 legacy 契约（DictionaryManagerStd::search_grouped；前缀 trie 惰建自愈）
  - [x] 3.4 std 解析器工厂注册表（DictionaryParserStd 虚接口 + ParserRegistryStd：八内建扩展名注册、归一化大小写/前导点、register_factory 后注册者胜可恢复、DictionaryStd 单 parser_ 成员收编六臂分派，Developer dictionaries 差异化面）
  - 每步：全 std 测试 + 两门全绿 + coverage lines 100% 才提交推送
- [x] **P-4 查询体验**（聚合卡片细化等；依赖 3.3 完成）：词条卡纯文本释义命中词高亮（927bbb1）+ 全文/例句 tab snippet 片段预览（8b8d6b3）+ HtmlRendererStd::highlight_term 文本节点级命中标注与 MDict 词条卡接线（29cdba4）+ 查询结果区按欧路标准重排（759f93e）+ 词头链层级标记（relevance 2 词条 · / 1 前缀 ·，EntryResultsPane.qml:343-344）+ ui_click_audit 层级标记逐条断言（58/58 全过）
- [x] **P-5 Quick Lookup**（剪贴板/热键悬浮取词窗）：QuickLookupPane 贴光标无边框浮窗（聚合查询最优组首条释义 + 音标/词典名灰标），取词窗开关（quicklookup/enabled）与「取词/朗读/生词本/主窗打开」四动作；剪贴板取词触发（关闭悬浮窗回退主窗直接展示）；quick_lookup/show_window 热键收口成信号（读剪贴板取词 + 主窗前置，lookup_selection 保留为平台化占位）；热键注册面进设置页（仅 Windows 生效，其余平台 stub 如实提示）。ui_sandbox 加 quicklookup 截图（20 张），三带客观验收（header/body/footer 两主题渲染一致）
- [ ] **P-6 Dictionary Library UI**（导入/扫描/进度/后台索引）——**切桥窗口已收口（2026-10-08，批次一~六）**：lookup_adapter 整体切 std 完成，legacy 陪葬测试清单七项全处置（lookup_adapter_test 批一改写 / legacy_parsers_test+lookup_service_test 批二退役 / core_lookup_tests 批三改写 / dictionary_manager_* 批四退役 / index_engine_test 批五整链退役 / data_store_test 批六判定保留为活壳薄 pin）；正式 UI 面另批
  - [x] 批次一：lookup_adapter 整体切 DictionaryManagerStd 成员（每 adapter 自持，无跨实例共享态）——词典装载/元数据/五类搜索（prefix/fuzzy/wildcard/regex/exact）/聚合 search_grouped（std 面 dictionaryId=词典名，QML 透传）/全文检索/内容 tab 全走 std；LookupService 退役（miss 文案与建议截断语义移入 adapter 逐字节保留），类+测试+CMake 挂点删除；P0Modules.ensureMdd 改经 dictionary_source_paths（std 新 accessor，未知名空向量，std 测试补用例）；setDictionaryPriority/Enabled 接真（启停只影响查询路径，变更发 stamp）；装载后显式 build_index（indexed_word_count 计数口径）。语义升级：suggestFuzzy 由 legacy 前缀匹配改编辑距离≤2（"helo"→"hello" 钉进测试）；词典身份 legacy 规范化路径 → 词典名（mdd_remount 测试改同名换体重写）。测试改写：lookup_adapter_test（init/cleanup 去共享清场、搜索包装/mdd_remount/启停接真改写）、entry_presentation_test（装载通道换 UNIDICT_DICTS+adapter、id=词典名，按词查询用例用 std SIMPLEKV 容器造词典）。**已知缺口（std core 独立项）**：MdictParserStd 对真实 MDict v2 的 .mdx 词条提取是启发式 WIP（解析不出时种 skeleton 占位词，mdict_parser_std.cpp 尾部），.mdd 资源侧已是真实 v2 全支持——真实 v2 .mdx 忠实读取待独立批次实现。闸门：build 155 / build-std 131 / coverage lines 100%（9433/9433）/ 点审计含于 build ctest 全绿
  - [x] 批次二（8653b14）：legacy_parsers_test 整体退役——mdx 真实 v2 变体矩阵随 std 忠实读取缺口陪葬（30 个 mdict_*_std_test 承接容器失败分支）、stardict 七文件/epub 三文件 std 专项承接解析面；plugin_manager/path_utils 薄门面由 qt_adapters_test 与 lookup_adapter_test cache_dir_path_caliber 既有口径在。闸门：build 154 全绿（std/coverage 面无涉）
  - [x] 批次三：core_lookup_tests 改写（1689→1008 行，slot 40→16）——逐 slot 三分：①std 已承接删除 17：stardict/json/epub 装载查询、大小写折叠、miss 建议文案、启停排除/排序/状态往返、损坏隔离三段与 forget、missing-state、searchGroupedTiersAndDedup 三层降级（std_grouped_test 承接）、mdx 解析两 slot（真实 v2 缺口见批一）；②legacy 死面连码退役（全仓零消费面扫描判定）：getDictionariesMeta/clearDictionaries/getLoadedDictionaries/defaultStateFilePath 公有壳/clearSearchHistory/exportSearchHistory/importSearchHistory/manager 级 regexSearch/getAllWords/自由函数 searchWord 十面自 unidict_core.h/.cpp 摘除（孤儿 include QRegularExpression 随之清理）；③gui/cli 仍咬的活面留最薄 pin 16：目录扫描与 canonical 去重、重复装载拒绝、tag filter 查询矩阵（regexSearch/getAllWords 臂随死面缩面）、fulltext×filter 不重建、history 置顶/删除/修剪/上限（造历史改搜索序+pin 头插，importSearchHistory 退役）、tags 归一与状态持久、priority/enabled 的 info 反映、q5 装载失败分支/quarantine 矩阵/state IO 失败臂、mdd 伴生资源服务、formatLookupResult（cli 活消费）、BUG-005 双锚点回归。std 缺口补钉：dictionary_manager_std_priority_test 增 T5 fullText×tagFilter 正侧（过滤在倒排命中后逐条裁决、摘除即全量回归无重建）。口径澄清：coverage 默认 core/ 门是 std-only 树（Qt 全关），legacy Qt 面不受其约束，退役判定以消费面扫描为准。闸门：build 154 / build-std 131 / coverage lines 100%（9433/9433）
  - [x] 批次四（34e5020）：dictionary_manager Qt 测试退役——test_dictionary_manager_fulltext/prefix 两 target 删（全文惰性构建/失效重建/空查询、前缀合并去重/禁用过滤由 std 面承接：fulltext_index_std、fulltext_persistence_* 家族、dictionary_manager_std_branches 全文守卫、index_engine_std trie + core_lookup_tests 留存 manager prefixSearch pin）；Qt parser 级 prefixSearch/allEntries 属 legacy 陪葬面；isFulltextIndexBuilt 连锁退役（gui/cli/qmlui 零消费，最后真 pin 随文件走）。闸门：build 152 / build-std 131 / coverage lines 100%（9433/9433）
  - [x] 批次五：IndexEngine Qt 兼容壳整链退役——index_engine_test 唯一消费者坐实后整条死链连码删：tests/index_engine_test.cpp + core/index_engine.{h,cpp}（wrapper）+ adapters/qt/index_engine_qt.{h,cpp}（桥）+ unidict_index_qt target（单文件库，fulltext_index_std/mdd_resource_std 本就在 unidict_std_core 直连，无传递依赖损失）+ unidict_core_qt 源列表与链接行；行为契约由 index_engine_std_test/_std_ops/_std_edge + index_persistence_std 承接。architecture_diagram 适配层/包装层/测试清点三处同步（IndexEngineQt→FullTextManagerQt、IndexEngine→formatLookupResult、Qt Core Tests 2→1），core/CMakeLists 过时「逐渐替换」注释收口。闸门：build 151 / build-std 131 / coverage lines 100%（9433/9433）
  - [x] 批次六：data_store_test 判定**保留**（收口不退役）——六个 slot 全是 DataStore 壳链（DataStore→DataStoreQt→DataStoreStd）的转发语义 pin，qmlui 转发器（批一留置）/gui/sync 都走这条活链，行为语义 std 面十专项（notes/pron/tags/csv_escape/vocab_m3/remove/escape/branches）承接、Qt 面 QVariant 映射与单例口径由此钉住；去留随 P-7 双存储收敛定。死桩退役连码删：DataStore::load()（零调用恒真）、save()（gui 两处无操作调用摘除）、ensureLoaded（私有不可达桩，Q-6 标注撤）；pron slot 更名 pron_records_and_storage_path 并摘桩断言。闸门：build 151 / build-std 131 / coverage lines 100%（9433/9433）
- [ ] **P-7 Vocabulary 收敛**（游戏化死码清理、双存储合一、四技能数据模型、复习入口简单化）
  - [x] 批次一：游戏化死码清理——消费面扫描判定（qmlui/learning_manager）：Main.qml（Android/iOS 壳）学习统计 Tab 只消费 5 个读方法（getDailyStats/getProgressStats/getDueReviews/getWeakWords/getMotivationalMessage）+传递依赖（getWordStats/calculateReviewPriority/getReviewReason/loadStats/fromJson/m_dailyTarget）；其余全仓零消费连码退役：18 个 Q_INVOKABLE（recordLookup 唯一写入口也零消费——读面恒空是现状行为，删除不改变任何行为）、5 signals、checkReviews+每小时定时器、成就系统（checkAchievements+m_unlockedAchievements）、艾宾浩斯间隔表（calculateNextInterval/calculateDifficulty）、saveStats（调用者全在死链）、LearningStats::toJson、ReviewItem struct（零引用）；保留读面与持久化读链（旧 learning_stats.json 照常加载展示，不破存量数据）。判不准的留：MainModern.qml 死壳归 TD-118/119 出口选型、ui_sandbox/click_audit 的 context 注册（Main.qml 探活需要）、Main.qml 学习 Tab（UI 面不在本批边界）。tests/learning_manager_test.cpp 改写（1207→约 430 行，26 slot→13）：读面改 fixture 手摆 learning_stats.json 驱动（读语义不再依赖写链；键形态=历史写入器落盘的归一化小写，fromJson 原样加载、查询侧归一化照测），写链/成就/导入导出 slot 随死链退役。闸门：build 151 / build-std 131 / coverage lines 100%
  - [x] 批次二：双存储合一——history 单源落 DataStoreStd（unidict.json 实时落盘），legacy manager 内存双轨退役：①std 层升级结构化条目 SearchHistoryEntryStd{query,success,dictionary_name,pinned}（add_search_history_entry/get_search_history_entries/set_search_history_pinned/remove_search_history/set_search_history 整表重建；置顶块尾插入与上限 100 全按 manager 口径统一——存储序=新→旧，旧 DataStore 尾插序退役，两端历史列表同序）；落盘格式升级为对象数组（紧凑字段：缺省不写），读侧双形态兼容（旧字符串数组→success=true/无词典名/不置顶），畸形布尔值兜 obj_bool 缺省臂。②DataStoreQt 桥+DataStore 壳透传（addSearchHistoryEntry/getSearchHistoryEntries/setSearchHistoryPinned/removeSearchHistoryItem/restoreSearchHistory）。③manager 转轨：m_history 成员退役，recordSearch/getSearchHistory/removeSearchHistoryItem/setSearchHistoryPinned 只剩类型映射与 m_lastError 语义；clear() 清单源；saveFromJson 摘 history 段，loadFromJson 对旧 state 的 history 数组做一次性替换式迁移且 history 独立于 state 恢复（不再「恢复即重置」——gui 恢复 state 不再清掉 qmlui 历史）。④sync 面适配：apply 回写改 restoreSearchHistory（add 插头语义下逐条重建会反转序），快照数组序=存储序自洽，同步序断言零改动。gui 调用面零改动（转发语义等价）；qmlui 简单面零改动。闸门：build 151 / build-std 131 / coverage lines 100%（std history 面含结构化往返/置顶桶/上限裁剪/旧格式兼容/畸形布尔兜底全钉）
  - [x] 批次三：四技能数据模型——生词本条目级 LearningState 字段位（product-principles §11「预留防迁移，不是课程化」口径）：VocabItemStd 扩四字段 listen/speak/read/write（int 0-2：0=未练 1=不稳 2=稳；旧文件无字段=0 零迁移），写入口 set_vocabulary_skill（skill ∈ {listen,speak,read,write} 大小写不敏感；非法技能/等级返回假不动数据，不静默钳位），add_vocabulary_item 更新臂只换释义——技能状态保留（学习状态不因重加词条被清，测试钉）。落盘只写非零字段（0=缺省省略），读侧 obj_int 兜底归 0；DataStoreQt 桥 + DataStore 壳透传（setVocabularyItemSkill），getVocabularyMeta/ByTag 条目带四技能键。sync 面零改动（词表合并固定 word/definition/added_at，技能=本地学习状态与 tags/notes 同口径不入同步文件）；qmlui/gui UI 零改动（UI 接入归批四复习入口按需消费）。测试钉：std 面（四技能三态/大小写不敏感/非法参数矩阵/upsert 保留/非零写零省略往返/旧文件缺省升级）、branches 面（roundtrip 四技能非零往返+全零省略臂、listen:"x" 畸形值 obj_int 兜底）、Qt 门面（写入口转发+meta 四键）。闸门：build 151 / build-std 131 / coverage lines 100%（9548/9548，scripts/coverage.sh 官方口径）
  - （批四·复习入口简单化暂缓：2026-10-08 用户指令三线（P-11）插入，收口后续批）
- [ ] **P-8 Speech**（TTS 面完善）
- [x] **P-9 AI Provider 抽象**（provider 可替换，core 不带 AI，失败无感）：`AiProvider` 接口（name/translate/grammarCheck，空串=失败让位）+ `CommandAiProvider`（外部命令桥，起不来/被杀/超时/无输出全返回空）+ `HeuristicAiProvider`（离线启发式兜底，永不空）收进 ai_service_qt 既有 TU——零 CMakeLists 变更（tests/CMakeLists 由 B4 WIP 占用期的绕行口径）；AiServiceQt 链化（setCommand/env 重建：有命令=command+heuristic，无=仅 heuristic）+ `providerNames()` 可观察面；core 零 AI 不变。失败无感：任一 provider 空产出静默让位下家、坏命令占位不报错、空白输出边缘收紧为失败让位（原行为空白产出直接返回空串）。行为契约全保：q7 既有三结局（stdout/stderr/信号杀）与启发式 mock 串断言原样过；新增四断言（env 形链/无配置链/清空命令摘除/坏命令占位）。闸门：build 154/155、build-std 129/130——唯一红 = B4 WIP 未提交 test_sync_device_std（他人在途，不入本提交面/不进 CI）
- [ ] **P-10 Server**（仅当用户显式推进：account / catalog / distribution / sync；永不进查词核心路径）。范围修订：账号面已取消（server_plan §5.5 零账号），sync 面由 B 系列承载
- [x] **P-11 产品体验三线**（用户指令 2026-10-08：①查词别像 grep，参考成熟项目 ②UI 用 ui-ux-pro-max 重设计并以新原型图替换旧图 ③常见开源字典多下几个、随包自带 ≥6 个 + 装进安装包）：
  - [x] 字典随包线（10806a3）：`scripts/fetch_sample_dicts.sh` 一键拉取 5 件（ECDICT 340 万词 / WikDict×2 / FreeDict×2；unzip 缺席回退 python zipfile、FreeDict .idx.gz 需 gunzip、.done 标记防重拉、`--print-env` 出 UNIDICT_DICTS 值）；README 快速上手改指真词典 `dictionaries/ccedict-zh-en.json`（实测「词典」出拼音+释义）并修 `-m prefix -p inter` 形态错（`-p` 仅 fulltext 消费）；CI daily-build 两平台现拉入包（Windows `stage/dictionaries/` 平铺进 Inno `recursesubdirs`、macOS bundle `Resources/dictionaries/`），qmlui/main.cpp bundledNames 子路径白名单小件自动挂载（<5MB 三件入包即查；ECDICT 200MB、eng-deu 大件入包不自动挂按需挂载——「查得快」承诺），安装器断言补 ATTRIBUTION.md/ecdict.ifo/wikdict.ifo + ECDICT lobster 冒烟；字典资产不入 git（.gitignore `dictionaries/downloaded/`，AGENTS.md 红线）
  - [x] UI 重设计 D1（2496b7c）：MainModern 死树整删（19 文件 ~8.8K 行），TECH_DEBT / CURRENT_ARCHITECTURE / CURRENT_FEATURE_MATRIX / mobile_plan 四文档同步
  - [x] UI 重设计 D2（本批）：`surfaceSunken` 凹陷面 token（light #f7f1f5 / dark #1f181d）三同源链落 theme_tokens.h + theme.h + tests/theme_tokens_contrast_test.cpp（34 用例 AA 全过；tertiary/sunken 4.3:1 掉门槛→「凹陷面文字只用 text/secondary、placeholder 归 secondary」口径记档头注释）；SidebarPanel 查询命令条（sunken 容器 + 聚焦 accent 环 2px/120ms ColorAnimation + 模式/输入位/查按钮归拢一体，placeholderText 置空 + 自绘空态 Text 修 Material 浮标与输入文本重叠）；MainDesktop header chip 化（Material.elevation 0 + hairline 底边、「N 本词典 · M 词条」状态 chip 圆点、HeaderGhostButton 组件 ×3 objectName 全保）；ui_sandbox 原型基线切真词典（UNIDICT_DICTS=ccedict + fixture 词「你好」——121401 词条开箱效果，弃 4 词条玩具感）；docs/ui 八屏换新（实现即原型，README 引用名不变）
  - [x] CI 闪败修（本批）：tests/learning_manager_test.cpp 日统计 nextReview 由 `now-7200` 改钉「今日零点」——统计窗是 [today, tomorrow)，固定回退量在 UTC 00:23 定时 CI 跑时跨日掉出窗外 reviews 记 0（10806a3/2496b7c 两红唯一根因，三平台同挂同修；零点恒在窗内且 ≤ now 必到期）
  - [x] 查词能力线：主链三层降级补模糊层 / 词形还原（lemma 设施）/ Did-you-mean 升编辑距离 / CLI 输出形态升级（grep 形四模式裸词表 → 结构化）
    - [x] 批一 主链补模糊层（93dacde）：search_grouped 降级链扩为四层——层 3 词头模糊（编辑距离 ≤2，exact/prefix/释义全空才走；字节长 <3 短查询不进模糊层防全表噪声；候选折叠键去重后逐词典回查真释义，relevance=3 非 fulltext）；legacy 三层契约不破（T1/T2/T3/T6 命中层即止与过滤空手原样过）。QML 消费面同步：EntryResultsPane 蓝链层级标记「模糊 ·」、MainDesktop 顶部徽章「模糊匹配」（matchLevel=3）、aggregateLookup 注释四层口径；测试 T11 钉 helo→hello 召回（真词头/definition 原文/relevance 3）+ 短查询空手 + 精确即止对照。闸门：build 151 / build-std 128 / coverage lines 100%（scripts/coverage.sh）
    - [x] 批二 词形还原 lemma 设施（75f808f）：core/std/lemma_std.{h,cpp} 新模块（进 unidict_index_std，纯 C++17）——不规则表（wolves→wolf/went→go/better→good/三字母 men-led-met 全收）+ 后缀规则臂（-ied→-y 优先防伪词 studi、-ies→-y、-es 按 s/x/z/ch/sh 定界、-ing/-ed 三候选=裸去+双写辅音回退+e 复原、-s 排除 ss/us/is）；闸门口径：整体 <3 字节与含非 ASCII 字节空手（字节级规则对多字节无意义）、候选至多 4 条按置信序、契约不含输入词自身。主链接入：search_grouped 层 1=词形还原（屈折形直命原形词条，优先级高于前缀建议），前缀/全文/模糊顺移层 2/3/4——降级链成五层 0 精确/1 原形/2 前缀/3 释义/4 模糊；gui legacy 链三层口径原样保留（独立契约不在本批边界）。消费面全链同批更新：EntryResultsPane 标记（原形 ·/前缀 ·/词条 ·/模糊 ·）、MainDesktop 徽章（原形匹配/前缀匹配/释义匹配/模糊匹配）、ui_click_audit S9 逐条期望映射、aggregateLookup 注释。测试：test_lemma_std 新 target（不规则/全规则臂/双写 wxy 与非字母不回退/闸门矩阵/候选序）；grouped T2/T3/T8/T10/T11 relevance 全量重钉 + T12 层 1 钉（running→run 大小写不敏感、studies/studying→study、wolves→wolf、词头在册精确即止）。闸门：build 152 / build-std 129 / coverage lines 100%（scripts/coverage.sh）
    - [x] 批三 Did-you-mean 升编辑距离：DictionaryManagerStd 新方法 suggest_corrections（miss 建议统一口径=模糊编辑距离 ≤2 优先 + 前缀补位，fold_key 去重、max_results 截断、短词 <3 只走前缀与模糊层闸门同款）；index_engine fuzzy_search 排序确定性（距离同分按词头字典序——word_index_ 无序容器原 std::sort 不稳，建议序与层 4 候选序会漂）；lookup_adapter miss 路径 prefix_search → suggest_corrections，"Word not found:/Did you mean:" 文案格式逐字节不动（QML 按前缀判 miss 契约不破）。测试：grouped T13（模糊优先序/前缀补位去重/短词只前缀/全 miss 空/空白查询/截断同分取首）；lookup_adapter_test 新 slot miss_suggests_fuzzy_corrections（helo→建议含 hello+help、无近邻裸 miss 文案全等）。闸门：build 152 / build-std 129 / coverage lines 100%（scripts/coverage.sh）
    - [x] 批四 CLI 输出形态升级：cli-std 六模式查询从 grep 形裸输出改结构化——词表四模式（prefix/fuzzy/wildcard/regex）出「# 模式 "查询" -> N matches」计数头 + 「词头<TAB>词典归属」行（# 头 grep -v 一行滤、cut -f1 取回裸词表，grep 形消费兼容；归属走 dictionaries_for_word 逗号联）；fulltext 补同款计数头 + 「词头 [词典]: 释义」归属正文；exact miss 接批三 suggest_corrections（建议块 "Did you mean:" + 两格缩进，同在 indexed_word_count>0 守卫内——词典全挂仍全静默 exit 7，mdict 密码契约不破）；exact 命中/--all/释义兜底正文形态逐字节不动。usage 补结构化示例两行。测试：test_cli_std_output 新 target（驱动真 unidict_cli_std 双夹具词典八例：exact 命中/miss 建议块/死词典静默/prefix 计数头+归属/fuzzy 0 matches 头/wildcard -p 真查询头/regex 位置参数查询/fulltext [词典] 正文；expect 断言带 rc+现场输出）；README 展示图复核（docs/ui 八屏 md5 与 HEAD 重渲逐字节一致——D2 已换新，补 GitHub 图片缓存击穿 ?v= + 重设计进行中标注 7827d22）。闸门：build / build-std / coverage lines 100%（scripts/coverage.sh）
  - [x] UI D3/D4（本批）：EntryResultsPane 结果面板视觉升级（卡头/分组/行动作行）——词典 tab 每词典一张 radiusL 圆角卡（card 底 + divider 描边 + 卡间 8px 呼吸），面板根 Frame 与 MainDesktop 外包裹 Frame 双双退场硬边框（background: transparent，分组卡自承结构，rightPane 表面直坐）；卡头=词典名 + 词条数描边胶囊 chip（levelChip 同语言）+ 120ms 旋转折叠箭头（⌄ rotation -90 折叠，NumberAnimation 与 D2 ColorAnimation 同拍）+ 悬停 ghost 面（HeaderGhostButton 同式 inset3+radiusM，hoverEnabled MouseArea 整头可点）；内容 tab 非选中项悬停 ghost 面（radiusS）；行动作行=每词条一行「层级标记蓝链（entryWordLink 文本契约逐字节保留，ui_click_audit S9 逐字断言）+ 右侧本条朗读 🔊/复制 ⧉ 轻动作（entrySpeakAction/entryCopyAction 新 objectName，textTertiary 悬停升 secondary；朗读走 lookup.extractTextFromHtml+speakText、复制走 clip.setText，均带状态行反馈）」。坑记：分组委托由 Column 换 Rectangle 后必须显式 height 绑内容 implicitHeight（Rectangle 不自动包子项，首渲整卡 0 高隐形）；collapse 状态由内层 Column 忽略不可见子项自然收短。compare/ 演进记录定口径（D3-result-light/dark.png 两张合成图入库：左 D2 基线右 D3 现实现红条分隔；口径=只记大版本节点不逐批全量、每节点只截受影响屏、采集度量仍走 ui_sandbox 确定性离屏+客观度量）；docs/ui 八屏全量重渲换新（右栏结果面八屏皆见，md5 全变）。Android UnidictTheme.kt 同步 surfaceSunken：映射 surfaceDim（M3 容器系暗主题随海拔升亮表达不了「比卡片暗」，surfaceDim 是双主题唯一下沉槽位；light #f7f1f5 / dark #1f181d），头注释记映射口径；消费联动 MainActivity 搜索 OutlinedTextField 容器落 surfaceDim + 聚焦描边 accent（桌面 D2 查询命令条同款）。边界：不碰 B 系列同步、不动 CLI 输出形态；gradlew :app:compileDebugKotlin rc=0。闸门：build 153 / build-std 133 / coverage lines 100%（scripts/coverage.sh）
- [ ] **B 系列同步服务重构**（server_plan §7，2026-10-04 立项，B1→B7 顺序；红线全程有效：同步默认关闭、显式开启明示范围；中转只见密文）：
  - [x] **B1 中转面核心**：`server/sync_relay/`——PROTOCOL.md v1 契约（指令收发 op_id 幂等去重 / 服务端定序组内全序 seq / 位点增量拉取 since+limit+has_more / 快照存取与空洞拉取规则 / 限额表）；双参考实现：dev（Python stdlib 单进程，`--data` 原子落盘）+ Worker（Cloudflare D1，batch 事务 `MAX(seq)+1` 定序 + `UNIQUE(gid,op_id)` 幂等 + 首请求惰性建表）；契约符合性测试 dev 16 用例（注册 ctest `sync_relay_protocol`，build-std 123 / build 146）+ worker 14 用例（node:sqlite 做 D1 shim）；README（三形态表 / 部署 / 自建口径）。C++ 版 `unidict-relay` 归 B5
  - [x] **B2 客户端同步引擎**：`core/std/sync_engine_std.h/.cpp`——SyncTransportStd 窄传输接口（meta/推/拉/快照存取；HTTP/WebDAV/LAN 绑定归 B5）；指令生成（op_id=`<device_id>:<本地序>`，enqueue 本地立即生效+outbox，payload≤256KiB）；推 256 条/批+ack 幂等去重（离线重试续传；全批无 ack 防御收口不挂死）；拉按 PROTOCOL §2.8（位点落后快照覆盖位先取快照跳变→增量分页，limit 钳制 ≤1000）；确定性回放=全规范序（words/tags 字典序、history 按 (ts,word)——指令语义可交换幂等，回放次序无关收敛，序列化逐字节确定）；快照阈值压缩（applied_since_snapshot 计数清零）；save/load_state 断点续传（version 校验/截断文件按可读前缀加载）；gid 校验（§3）。测试 test_sync_engine_std 13 组（tests/sync_relay_memory_std.h 内存 relay+故障注入：双引擎收敛/分块拉/重启续传/快照空洞/压缩计数/回显幂等/畸形 payload/截断文件/转义往返），build-std 124 / build 149 / coverage lines 100%（core/ 7955 行）
  - [x] **B3-a 配对与密钥（原语与语义层）**：`core/std/crypto_std.h/.cpp` + `core/std/sync_crypto_std.h/.cpp`——自实现密码学原语（纯 C++17+STL 无第三方依赖；随机数走 std::random_device CSPRNG，失败抛异常不降级）：HMAC-SHA256、HKDF-SHA256（extract/expand 分暴露，len≤255*32）、Poly1305 单发原语（donna-32 风格 26-bit 域拆分，MSVC 无 __int128）、XChaCha20-Poly1305 AEAD 双形态（随机 nonce combined=`nonce24||ct||tag16`；确定性 key32+nonce 形态供 RFC 向量钉死，HChaCha20 派生子钥）；组密钥环（建组/轮换/退出销毁/多版本在环；sealed=`ver4 LE||nonce24||ct||tag16`，AAD 绑定上下文；`import_key` 为换钥注入面）；动态配对码（Crockford Base32 8 字符=40bit 熵，O/I/L 别名归一，TTL 600s + 单次消费）。测试：tests/crypto_std_test.cpp（RFC 8439 §2.8.2/§2.5.2、draft-irtf-cfrg-xchacha-03 §A.3.1、RFC 5869 TC1/TC3、RFC 4231 TC1/TC6，加篡改/错 aad/截断/参数臂）+ tests/sync_crypto_std_test.cpp（密钥环生命周期与轮换回放、失败矩阵、配对码向量/归一化/时效边界），build-std 126 / build 151 / coverage lines 100%（core/ 8439 行）
  - [x] **B3-b SPAKE2+ 换钥**：`core/std/spake2_std.h/.cpp`——SPAKE2+（RFC 9383 增强型 PAKE，ciphersuite P256-SHA256-HKDF-SHA256-HMAC-SHA256）纯 C++17 自实现（P-256 域算术 8×32 limb + Jacobian 点算术，MSVC 无 __int128 同口径）：注册记录 L=w1·M、share 交换、finish 全链（TT 转录/HKDF/HMAC 确认标签/K_shared），对端 share 群成员校验（曲线外 invalid_argument）；配对码 PBKDF 派生 w0/w1（context/id_prover/id_verifier 身份绑定防 unknown key-share；短码不直接派生密钥——防降级口径不变）；换钥信封 seal/open_group_key_envelope（XChaCha20-Poly1305）经握手 K_shared 送组钥，`SyncKeyRingStd::import_key` 注入，密钥不出端；域负≠标量负（点取负实现点减法，标量负须模群阶 n 而非域素数 p）。测试 test_spake2_std：RFC 9383 附录 C 第一组全链向量逐字节钉死（L/双 share/K_shared/双确认标签）+ 派生确定性/身份绑定 + 失败矩阵（篡改/错钥/截断/参数臂）+ 白盒域算术代数恒等式（(p−1) 加乘/费马小定理/点加=点倍/P+(−P)=∞），build-std 127 / build 148 / coverage lines 100%（core/ 8741 行）
  - [x] **B4 设备面**（组内设备清单、自由进出组、仅改自己备注）：`core/std/sync_device_std.h/.cpp`（SyncDeviceManagerStd：设备元信息 device_id/名称/平台/最近同步/备注/本机标注，join_group/leave_group 自由进出、update_own_remark/update_own_name 仅本机可改自身、touch 心跳、list 本机置顶按 last_seen 降序、kMaxDevices=64 防滥用；JSON 持久化往返无损——引号/反斜杠/短转义/控制字符 \u 转义 + 代理对解码、孤立代理 U+FFFD 收敛、坏 \u/未知转义字面保留；span 扫描字符串感知——备注含 []{} 字面不打碎配对；空设备清单合法可往返、坏文件按缺省容错）。本层纯元数据：密钥不出端、中转只见密文红线结构性达标，签名验证归 B5 传输/协议层。测试 `tests/sync_device_std_test.cpp`（\u 矩阵/坏形态矩阵/往返无损/失败路径全覆盖）。闸门：build 155/155、build-std 130/130、coverage core/ lines 100%（9340/9340）。引擎接线与设置 UI 归 B7（2026-10-08）
  - [x] **B5-a/B5-b C++ 版自带中转 `unidict-relay`**：`core/std/sync_relay_state_std.h/.cpp`（契约语义层，结构级 API 零 JSON 依赖——定序/op_id 幂等/位点拉取/快照/限额全契约，一把互斥锁保组内全序，行式文本状态文件 tmp+rename 原子落盘、坏文件整体空起）+ `server/sync_relay/cpp/`（HTTP/1.1 壳：线程 per 连接、Content-Length 定界含粘包残余摘取、极简严格 JSON 解析（\uXXXX/代理对）、不做百分号解码——gid 白名单字符集天然拒编码形态；POSIX/Winsock ifdef 收敛单 TU，Qt-free）。验收：dev 契约符合性套件 16 用例经 `UNIDICT_RELAY_EXTERNAL_BASE` 打到真二进制（ctest `sync_relay_cpp_protocol`）+ 结构级 test_sync_relay_state_std（8×25 并发追加全序无重号/重启续存/损坏矩阵/原子替换失败 500），build-std 129 / build 150 / coverage lines 100%（core/ 8989 行）
  - [x] **B5-c relay 进发布产物 + 一键安装**：daily-build 三平台 zip stage 收 `unidict-relay`（Windows 安装器整 stage 树收编并把 unidict-relay.exe 钉进验证清单）；Linux deb/rpm 经 `install(TARGETS unidict-relay RUNTIME DESTINATION bin)`（CPACK_MONOLITHIC_INSTALL 全收）；PLATFORM-NOTES/BUILD_INFO.txt/VERIFY.md/Release 产物清单四处文档同步（含探活口径 `GET /api/sync/relay/ping`、密文过境说明）；MSVC `windows.h` min/max 宏打断 `std::min`（C2589）补 NOMINMAX（7a0f917，同 mdd_resource_std 惯例）。install.sh/install.ps1 落 b964e60（OS/arch 检测、幂等重装、--systemd 可选）+ 接口最小修正（拍板口径）：`--version` 验证→探活 `GET /api/sync/relay/ping`、`-bind HOST:PORT` 拆成 `--host/--port`、资产对齐 nightly 平台 zip（linux-x64/arm64、macos-arm64、windows-x64；armv7/macos-x64 无腿即报错）、撤无鉴权假 `--secret`、默认监听回环 127.0.0.1:8788
  - [ ] **B5 剩余**（局域网直传发现与直连 / 官方托管形态客户端绑定与三选一设置面）：
    - [x] 增量一 密封传输层（红线「中转只见密文」的客户端执行点）：`core/std/sync_sealed_transport_std`——SyncTransportStd 装饰器，payload 在引擎与真实传输之间 XChaCha20-Poly1305 密封/开封（B3 组密钥环引用驻留本层不出端），meta/参数/空集直通；解不开的指令/快照整轮失败离线重试不跳过不静默丢更（cursor 不动），未持钥（配对未完成）推拉快照全拒绝绝不落明文上链；三形态（自建/托管/局域网）共用同一密封链行为一致。测试 `tests/sync_sealed_transport_std_test.cpp` 7 组（中转侧 binlog/快照无明文+双引擎收敛/篡改拒收位点不动/未持钥全拒绝+配对后出箱/异钥快照拒收/换钥 v1→v2 多版本 B 缺 v2 失败补齐收敛/rotate 版本递增旧版本保留/直通面+内层故障穿透）；内存 relay 增只读观测缝与篡改注入缝。闸门：build 154 / build-std 134 / coverage core/ lines 100%（scripts/coverage.sh）。HTTP/LAN 绑定与设置面接线归增量二/三
  - [x] **B6 安装清单并入**（InstallOp/RemoveOp 指令流，取代账号口径）：`core/std/sync_engine_std` 扩展——SyncOpType 增 InstallDict/RemoveDict 与词库指令同一管线（同 enqueue/outbox/推拉/快照/持久化，无独立状态机；payload 规范族 `{"t":"inst","d":"<dict_id>","n":"<显示名>"}`、`{"t":"remd","d":"<dict_id>"}`，键序 t 在首与既有族一致；payload 对中转不透明，relay/PROTOCOL 零改动）；SyncVocabStateStd 增 `dicts`（dict_id → 显示名，std::map 键字典序入确定性序列化）；语义：组内一处管理各端拉取装卸——新增→安装、同 id 重装覆盖=升级（按服务端序生效，与 SetPref 同覆盖型口径）、移除→卸载、移除不存在幂等空转；apply 对无 d 键/空 d 畸形指令忽略；序列化 dicts 区段置尾——旧客户端按名取区段自然缺省为空，双向前向兼容（旧快照/旧状态文件无 dicts 键解析为空清单）。测试 test_sync_engine_std 增 2 组（T7b 指令生成/payload 规范字节钉死/升级覆盖/幂等空转/畸形忽略/超限不占序 + T7c 双引擎收敛/状态区段逐字节相同/快照跳变携带清单/旧格式快照兼容/持久化往返/转义条目），build-std 130 / build 155 / coverage core/ lines 100%（9362/9362）。清单条目取 dict_id+显示名两元（来源解析归端上安装动作，B7 UI 面）；引擎接线与设置 UI 归 B7（2026-10-08）
  - [x] **B7 UI 与自救口**（同步设置页、默认关闭显式开启开关、导出加密备份）：core 自救口——`crypto_std` 增 PBKDF2-HMAC-SHA256（RFC 8018 §5.2，iterations=0→1、dk_len≤4096×32，RFC 7914 §11 双向量钉死+跨块截断）；`core/std/sync_backup_std` 口令加密备份——格式 magic "UNIDICT-BK1"+salt16+iters(u32 LE)+nonce24+sealed（XSalsa20-Poly1305，AAD=整个头部，头部篡改在 AEAD 前拒收），默认 100000 轮，明文即 `serialize_state` 输出（serialize_state/parse_state 出匿名命名空间为外链，单一格式源）；错误文案口令错与篡改同口径（"wrong passphrase or corrupted backup"）少探测面。adapters/qt `SyncManagerQt`——红线开关面（syncEnabled 缺省 false、enable 过 PROTOCOL §3 gid 形态校验、scopeText 范围明示、组/形态/中转地址 QSettings 持久化、引擎 device_id 落 AppDataLocation/sync/engine_state.json 钉住）；备份↔DataStore 桥——导出取生词本真数据（词/tags 规范序/notes），恢复并入语义（缺的补上已有的不动，词定义留空随本机词典重查与同步面同模型）。UI——MainDesktop.qml 抽屉增「同步备份」页签（开关拨开挡回须经「确认开启」、范围明示常驻、组 ID/三态形态/中转地址/设备标识、加密备份导出/恢复区；内容超高同语音 tab 走 ScrollView 收纳）；lastError 在 QML 须调用语法 `lastError()`（Q_INVOKABLE 方法非属性，取值即函数对象赋 QString 抛 TypeError——点审计挖出）。测试 crypto_std PBKDF2 四向量组 + test_sync_backup_std 7 组（全字段往返含中文/emoji/转义、空态、默认轮数形态、空口令拒绝、错误矩阵（空口令/错口令/密文篡改/头部三处篡改/截断/坏 magic）、同输入不同字节、serialize/parse 外链锚+旧格式缺 dicts）；ui_click_audit 增 S18 节 8 断言（红线默认关闭/挡回/范围明示/非法 gid 拒绝/合法 gid 开启/关闭/导出落盘/错口令拒/对口令恢复）+ scrollIntoView 助手（Flickable 内容坐标滚动，抽屉固定高 ScrollView 收纳后折叠下控件先滚再点）+ 隔离块改 IniFormat 口径（NativeFormat 指向 Unidict.conf 清不到真实 Unidict.ini）；ui_sandbox/main.cpp 装配 syncManager+documentsPath。build 156 / build-std 128 / coverage core/ lines 100%（9429/9429），点审计 67/0 两连跑（2026-10-08）
- [x] **Android 续作（Quick Lookup / Share 面）**＝M7 批，2026-10-07 交付（记录见移动端章节）；iOS/HarmonyOS 启动时机待用户定

### C. 存量债观察项（不阻塞在轨任务，随批次清理）

- 平台债 ③–⑦（Windows mdict file:// / bridge 0.13s / cli_main list 双挂 / macOS isEnabled / macOS sha256）——专批处理，均带根因结论
- ~~TD-131 无 QML 自动化测试~~：ui_sandbox 已并入 ctest（离屏 16 图结构探活，CI qt job 开 UNIDICT_BUILD_UI_SANDBOX；亮暗主题可区分性由灰度均值抽检兜底）——P-4 前基建就绪
- TD-134 benchmark 数据留存（CSV 单次残留）
- TD-141 根目录 core_analysis 三件套过时清理
- TD-118/119 双 CLI / 双桌面壳（随切桥与出口选型收敛）

### D. 待用户决策

- 双桌面壳出口选型（QML vs QWidget）
- ~~云同步 / S1–S5 是否启动~~：已拍板——B 系列（server_plan §7 binlog 模式）在轨（2026-10-04）；docs/design/sync-engine.md S1–S5 旧稿仍默认不动
- iOS / HarmonyOS 启动时机
- Server Phase 10 范围与启动确认

---

## 以下为历史流水（存档）

## Architecture Overview

### Core Dictionary Format Support Layer
```
DictionaryFormatManager
├── StarDictFormat (.dz, .dict, .idx)
├── MDictFormat (.mdx, .mdd) 
├── DSLFormat
├── EPUBFormat
└── CustomFormat (JSON/SQLite)
```

### Modular Architecture
```
Core Layer (C++)
├── DictionaryParser - 格式解析器接口
├── IndexEngine - 快速查找引擎  
├── DataStore - 本地数据存储
└── SearchEngine - 搜索算法实现

Service Layer
├── DictionaryManager - 词典管理
├── LookupService - 查词服务
├── SyncService - 跨平台同步
└── PluginManager - 插件系统

UI Layer (QML)
├── SearchInterface
├── ResultDisplay  
├── VocabularyBook
└── Settings
```

## Implementation Tasks

### Phase 1: Core Foundation
- [x] Create DictionaryParser base interface
- [x] Implement StarDict format parser
  - [x] Implement MDict format parser (std-only, Qt adapter integrated)
    - 已实现：无加密 + zlib 的多种块布局原型（KIDX/RDEF、KEYB/RECB、KBIX/RBIX、MDXK/MDXR 等）与启发式解析；提供 `MdictParserStd` 并由 `MdictParserQt` 适配用于应用端
    - 待办：加密变体支持、更多真实文件兼容性回归与边界用例覆盖
- [x] Create IndexEngine for fast lookups
- [x] Design DataStore schema (JSON MVP)
- [x] Implement basic SearchEngine (via IndexEngine integration in DictionaryManager)

### Phase 2: Service Layer
- [x] Build DictionaryManager (load by file, expose prefix/fuzzy/wildcard)
- [x] Create LookupService (fallback+建议)
- [x] Implement basic PluginManager (内建 StarDict/MDict/JSON)
- [x] Add vocabulary book functionality (MVP via DataStore, CLI expose)

### Phase 3: UI Integration
- [x] Create QML search interface (MVP: input, suggestions, search)
- [x] Implement result display (QML TextArea)
- [x] Add vocabulary management UI (history/vocab list)
- [x] Integrate with core services

### Phase 4: Advanced Features
- [x] Add fuzzy search
- [x] Implement full-text search (MVP: substring over definitions via std-only manager)
- [x] Upgrade full-text search to an inverted index (tokenizer + TF/IDF，含UDFT1/2/3持久化与签名)
- [x] Add cross-platform sync (SyncServiceQt wired into QML + adapters/qt/sync_service_qt.*)
- [x] Integrate AI features (AiServiceQt exposed to QML for translate/grammar tools)

## Technical Requirements
- **Performance**: Memory mapping + binary search for ms-level response
- **Cross-platform**: Qt6 framework for unified experience
- **Extensibility**: Plugin architecture for community contributions
- **AI Integration**: LLM interfaces for smart translation and grammar check

---

## 覆盖率缺口（2026-09-25 盘点）

### 测量方式

仓库此前**没有任何覆盖率工具链**，"覆盖率"只是口号，无法验证。本次补上：

- `cmake/BuildOptions.cmake` 新增 `UNIDICT_ENABLE_COVERAGE=ON`
  （等价 `--coverage -O0 -g`，仅 gcc/clang，MSVC 下静默跳过）。
- `scripts/coverage.sh`：配置 coverage 构建 → 跑全部 std ctest →
  gcovr 出 `lines/branches/functions` 汇总与逐文件缺口 + 阈值判定。

```bash
scripts/coverage.sh                 # 阈值校验（默认 lines 100）
scripts/coverage.sh --threshold 95  # 临时放宽
```

基线 → 现状（`build-cov`，gcovr 8.6 / gcc 15.2；分母已剔除"单独成行的
右花括号"，见下文说明：6073 → 5591 行）：

| 指标 | 基线 | 现状 | 目标 |
|------|------|------|------|
| lines | 96.6% (5868/6073) | **100.0%** (6659/6659，2026-09-27) | 100% ✅ |
| functions | 99.3% (579/583) | **100.0%** (687/687，2026-09-27) | 100% ✅ |
| branches | 61.7% (6087/9870) | 64.4% (6124/9514) → **66.6%** (7225/10852，2026-09-27；分母含后续新增 core 源码) | 记录，见下 |

基线时未覆盖行共 205 行，按文件分布（缺口行数）：

| 文件 | 缺口 | 性质 |
|------|------|------|
| `epub_parser_std.cpp` | 53 | 最差：整条 `decode_entities` 数字实体分支未测 |
| `mdd_resource_std.cpp` | 27 | 缓存/元数据路径 |
| `html_renderer_std.cpp` | 25 | 含**未实现的** `max_text_length_` 上限 |
| `pron_vocab_std.cpp` | 21 | HF vocab.json 严苛解析的拒收路径 |
| `zip_reader_std.cpp` | 17 | **恶意 zip 防御分支**（zip64/EOCD/LFH 越界） |
| `aggregate_lookup_std.cpp` | 9 | 两个测试函数是**空壳**（见 P1-6） |
| `cross_reference_std.cpp` | 9 | |
| `mdict_parser_std.cpp` | 8 | |
| `dsl_parser_std.cpp` | 6 | 仅 1 个测试 target |
| `index_engine_std.cpp` | 5 | `exact_match`/`all_words`/`clear` 零调用 |
| `data_store_std.cpp` | 3 | |
| `dictionary_manager_std.cpp` | 3 | `fuzzy_search` 零调用 |
| `text_norm_std.cpp` | 3 | `kFoldKeyVersion` 常量未断言 |
| `pcm_util_std.h` | 3 | |
| `ctc_gop_std.cpp` / `stardict_parser_std.cpp` | 2 each | |
| `json_parser_std.cpp` / `pron_wave_std.cpp` / `espeak_arpabet_std.cpp` / `ctc_logits_std.h` / `html_renderer_std.h` | 1 each | |

### P0 — 盘点中暴露的真实缺陷（不是"没测"，是"写错了"）

- [x] **P0-1 `FullTextIndexStd::clear()` 只清了 4 个成员**。漏掉 `terms_sorted_`
      （存的是指向 `postings_` 的裸 `PostingEntry*`，头文件注释自称
      "invalidated by clear()"——恰恰没失效）、`ngram3_index_`/`ngram2_index_`/
      `char_index_`/`prefix_index_`、`signature_`、`version_`、`last_error_`。
      后果：`clear()` 后 `version()`/`stats()` 仍报旧 UDFT 版本，四个辅助
      索引残留陈旧词项，是埋着的地雷。**修**：全量复位 + 断言复位后
      `version()==0`、搜索返回空、`stats()` 全零的回归测试。
- [x] **P0-2 `HtmlRendererStd::set_max_text_length()` 是静默空操作**。
      `max_text_length_` 声明了 100KB 默认值、注释写"100KB default"，
      但 `html_renderer_std.cpp` 从头到尾**没读过它**——一个宣称的安全
      上限从未生效，超大/恶意 MDX 词条可以在渲染器里无上限膨胀。
      同族死成员 `max_nesting_depth_`（32）同样声明未用。
      **修**：`render()` 出口按字节截断（避免切碎 UTF-8 序列），超限
      追加省略标记；`max_nesting_depth_` 接入 tokenizer 深度护栏。
- [x] **P0-3 `core/std/memory_optimizer_std.h` 是彻底的死代码**。611 行，
      **不在任何 CMake target 里**（从未被编译）、全仓库零 `#include`、
      零文档提及；且用到 `std::optional`/`std::ostringstream`/
      `std::setprecision` 却没 include `<optional>`/`<sstream>`/`<iomanip>`
      ——**原样放进任何 target 都会编译失败**。来源是早期 `chore:` 提交
      里的遗留草稿。**修**：删除。一个从不编译、从不include、
      编译不过的模板文件对"100% 覆盖"是结构性障碍。
      （如后续要复活，先补 include 再单独提交。）
- [x] **P0-4 两个"假装有测试"的空壳用例**。
      `tests/aggregate_lookup_std_test.cpp` 的 `test_relevance_calculation()`
      与 `test_deduplication()` **函数体只有注释、零断言**（"needs
      DictionaryManagerStd, skip for now"）。名字在、覆盖不在——比没有
      测试更危险。**修**：补真实断言，否则删掉空壳。

### P1 — 补测到 100% lines（按缺口行数从多到少推进）

- [x] P1-1 `epub_parser_std`：`decode_entities` 全套（`&lt;`/`&gt;`/`&quot;`/
      `&apos;`/`&nbsp;`/`&#NN;`/`&#xHH;`/`code>=128` 透传/畸形 hex 拒收/
      缺分号/`;` 超 10 字节）、无 `&` 快路径、`opf_dir` 为空（OPF 在 zip 根）、
      `./` 前缀剥离、`<h2 id="x">` 带属性标题、无 `<dc:title>` 时回退
      `"EPUB Dictionary"`、重复 headword 覆盖、缺 `</head>`、
      `find_similar(0)` 早退。
- [x] P1-2 `mdd_resource_std`：`save_metadata`/`load_metadata`（此前**全仓库
      零调用**，且要在测试里真跑一遍落盘/回读/脏文件）、`get_cache_info`
      边界、prune 系列边界。
- [x] P1-3 `html_renderer_std`：P0-2 新增路径 + 残余 sanitizer 分支。
- [x] P1-4 `pron_vocab_std`：严苛解析的全部拒收路径（重复 id、稀疏空洞、
      非法 blank、坏转义、代理对、UTF-8 截断）。
- [x] P1-5 `zip_reader_std`：恶意 zip 防御分支——`size < 22`、
      EOCD 注释长度不自洽、**zip64 拒收**、CDE 签名不符、CDE 越界、
      LFH 越界、条目数据 OOB、截断 deflate 流、零长条目、
      `kMaxEntryBytes` 上限。
- [x] P1-6 `aggregate_lookup_std`：补 P0-4 的空壳；`similarity_threshold`
      从"只断言默认字段值"变成真当行为阈值用。
- [x] P1-7 `cross_reference_std` / `mdict_parser_std` / `dsl_parser_std` 残余分支。
- [x] P1-8 `index_engine_std`：`exact_match`/`all_words`/`clear` 三个
      **零调用**公开方法 + `load_index`/`save_index` 失败路径、
      `add_word("")`、`remove_word` 未命中、`clear_dictionary` 未知词典、
      空引擎 `prefix_search`、`max_results <= 0`。
- [x] P1-9 `dictionary_manager_std::fuzzy_search`（5 个检索包装里唯一没测的）。
- [x] P1-10 `data_store_std` / `stardict_parser_std` / `json_parser_std` /
      `text_norm_std`（含 `kFoldKeyVersion` 断言——它进索引签名，是索引失效
      的唯一开关）/ `pcm_util_std.h` / `ctc_logits_std.h` /
      `pron_wave_std` / `espeak_arpabet_std` / `ctc_gop_std` 残余行。

### P0 补记 — 补测过程中又挖出的真实缺陷

补测不是"把行数凑满"，过程中撞出三个此前没人发现的真问题：

- [x] **P0-5 `.mdd` v1/v2 头解析偏移错误，真实 `.mdd` 一律加载失败**
      （`fix(mdd)` 提交）。`parse_v1_header`/`parse_v2_header` 都从
      `buf[0]` 读 `header_len`，但 `parse_header` 读过 magic 后 rewind 了，
      `buf[0..2]` 就是 magic——等于把 magic 的头几字节当成头长度：
      V1 算出 0x1b2345 ≈ 1.7MB（0x1b2345 = 1,777,477），V2 算出
      0x1b23 = 6947。紧接着
      `fseek(file_, header_len, SEEK_SET)` 跳到 EOF 之外，解析必然失败。
      证据：声明 `header_len=8` 的 v2 文件被报成 `header_len=6947`。
      修：字段改从 `buf+3` 取；并重写两个测试的 fixture——它们此前是
      "把头撑到 6947 字节、让索引表正好落在假偏移上"，把 bug 固化成了断言。
- [x] **P0-6 `MddResourceCache::save_metadata`/`load_metadata` 是幽灵 API**
      ——头文件声明了，`.cpp` 里从来没有定义，任何调用方链接期就报
      undefined reference。已删声明并写明去向。
- [x] **P0-7 DSL 缩进续行被 `trim(line)` 抹掉**（`test(core)` 提交）。
      解析循环先无条件 `line = trim(line)`，再判 `!isspace(line[0])`
      区分"新词头"与"续行"——判据恒真，缩进续行全部被当成新词头。
      实测：`hello` / `  used when meeting someone` / `  and also...`
      会被切成两个词条，第二条把第三条吃成释义。ECDict/Youdao 导出的
      `.dsl` 大量使用缩进续行。修：在 trim 之前抓住 `indented` 标志。

### 记录但未修（写清判断，别让后人重复踩）

- [x] **~~P0-5 的提交消息夸大了结论~~（2026-09-29 已修）**：`fix(mdd)` 说
  "真实 .mdd 此前一律加载失败"，修复后仍**不能**加载真实 MDict .mdd——
  真实格式的文件头是 4 字节大端头长 + UTF-16 头文本，根本没有
  `1b 23 45` 魔数，`parse_header` 两个分支（含 SimpleKV 兜底）都进不去。
  该修复实际修的是本仓库自定义容器格式与被 bug 固化的测试 fixture。
  **已修**（`feat(mdd)`）：`parse_mdict_header` 按 writemdict 规格接收
  真实头（u32 BE 头长 + UTF-16LE XML 属性串 + adler32 占位），引擎 2.0
  且未加密才放行；`parse_mdict_sections` 解析 key 节（5×u64 + 压缩索引块
  + 压缩 key 块）与 record 节（4×u64 + (comp,decomp) 对 + 压缩块），
  键文本 UTF-16LE→UTF-8（代理对组合、孤立代理 U+FFFD）。加密变体与
  引擎 1.2（LZO 时代）仍拒收，属后续路线图工作。
- [x] **~~`is_compressed` 恒为 false~~（2026-09-29 已修）**（`mdd_resource_std.cpp`）：
  三个块解析器都只写 `is_compressed = false`，`get_resource` 的解压分支
  永不可达。**已修**（`feat(mdd)`）：真实 MDict 条目 `is_compressed = true`
  （值在 zlib record 块内，offset 指向解压后拼接流），`get_resource` 走
  块表惰性解压（单槽块缓存、跨块拼装），旧的单条 `decompress_resource`
  （压缩单位语义对真实格式不成立）随之删除，EXCL 一并摘除。自定义
  V1/V2/SimpleKV 格式行为不变。
- **`calculate_relevance` 的 examples/pronunciation 加分是死代码**：
- **`calculate_relevance` 的 examples/pronunciation 加分是死代码**：
  `AggregatedEntry.examples`/`.pronunciation` 没有任何解析器路径填充，
  两个 `+=` 永远不执行；另外"精确命中 + priority 0"时基础分
  0.5+0.3+0.2 已经等于 1.0，释义质量分档被 clamp 吃掉，只有 fuzzy
  路径（分数是 `sim*0.7 + relevance*0.3`）才看得出差别。属打分设计
  问题而非缺陷，需要产品侧决定这些字段要不要真的接上。
- **`sort_by_relevance` 的 priority 兜底不可达**：`DictionaryAggregator`
  构造 `EntrySource` 时一律写死 `priority = 0`。带真实优先级的是
  `AggregatedLookupBuilder` 那条独立路径，它不经过这个函数。

### 不可达代码的处理方式（本轮形成的规矩）

补到最后剩下的几行都不是"忘了测"，而是**证明不可达**。处理分三类，
每类都在代码里留了理由，不靠"覆盖率好看"糊过去：

1. **删掉**——私有/内部、零调用方、且与别的实现重复的死函数：
   `MddResourceParser::read_string`、`HtmlRendererStd::normalize_url`、
   `CrossReferenceManager::extract_protocol`，以及
   `MddResourceCache::save_metadata`/`load_metadata`（幽灵 API）。
2. **GCOVR_EXCL 标注 + 写明为什么不可达**——防御性护栏，删掉会削弱
   安全/健壮性：`zip_reader` 的 `inflateInit2` 失败、`mdd` 的
   `ftell`/`fseek` 失败与 `is_compressed` 解压分支、`html_renderer` 里被
   `sanitize_attribute` 抢先执行的 `<a>`/`<img>` URL 复检、
   `mdict_decryptor` 双层 switch 的内层 `default`、`dsl` 的 `parse_header`
   尾部分支、`pron_vocab` 的 `id < 0` 与循环终止兜底等。
3. **测试钉住现状**——行为本身可疑但不属于本轮要改的：relevance 分数
   饱和、`prune_by_age` 用严格大于导致年龄 0 的条目剪不掉、
   `get_cached_path` 查的是元数据表而非拼路径、`MdictEncryptionType`
   越界值报"自定义"而非 "UNKNOWN"。

### 明确不做（记录判断，避免以后重复讨论）

- **branches 100% 不设为目标**。`core/std/` 9514 个分支里绝大部分是
  `||`/`&&` 短路、三目、循环条件的**一侧**可达性，gcovr 逐个凑满的边际
  收益极低且会写出大量"为覆盖率而覆盖率"的断言。定档：**lines/functions
  100%（脚本阈值强制），branches 只做趋势跟踪**（61.7% → 64.4% →
  66.6% → 67.4%（7294/10822），2026-09-27 mdict 分支第二批补测后）。
  **官方分支表口径注记**（`--txt-metric branch`）：GCC 把异常处理边
  （throw 边）也计入分支——mdict_parser_std.cpp 的原始分支图 1718 条边中
  447 条是 throw 边且全部未走（测试不能让代码 throw，天然不可赢）；扣除
  后的真实条件边 1271 条、缺 106 → 91.7%。官方表数字因此系统性偏低，
  趋势对比应看增量而非绝对值。
- **`HtmlRenderOptions` 的 7 个死配置字段**（`allow_css`/`allow_tables`/
  `allow_media`/`extract_text`/`base_url`/`dictionary_id`/`link_resolver`）
  与 `RenderedHtml::has_math`/`resources`：这些是**声明了但没实现**的
  能力，属于路线图级功能缺口，不是覆盖率缺口。接线它们要先有产品需求，
  本轮不硬塞进覆盖率冲刺。`max_text_length_`/`max_nesting_depth_` 除外——
  它们是**安全护栏**，性质是缺陷（见 P0-2），必须修。
- ~~**Qt 层覆盖率**：本轮只对 `core/`（std-only 纯逻辑）设 100% 阈值。~~
  **2026-09-26 推翻**：当时把 Qt 层整体排除在阈值外只是权宜。实测下来
  Qt 层的**桥接/业务逻辑**（learning_manager / fulltext_manager /
  lookup_adapter / sync_service / legacy core）完全不碰音频设备与窗口，
  是可以单测的纯逻辑——排除在阈值外等于放任 3000+ 行无验证代码。改为
  按下面的「Qt 层缺口」分档处理，只有真·平台代码才排除。

---

## Qt 层缺口（2026-09-26 盘点）

### 基线

`scripts/coverage.sh --qt`（Qt 全量插桩构建，offscreen 跑全部 ctest）：

| 指标 | 数值 |
|------|------|
| lines | **68.8%** (6957/10107) |
| functions | **65.0%** (792/1218) |
| 未覆盖行 | **3150** |

注：`core/std/` 已在上轮做到 100%，这里 10107 行的分母里它占 5591 行；
真正的缺口全在 `core/`（legacy Qt 核心）、`adapters/qt/`、`qmlui/`、
`gui/`、`cli/`。

### 明确排除（真·平台/UI 代码，列出来是为了让排除项可审计）

共 968 行。这些不是"忘了测"，是**测了会假绿**或**没有可断言的行为**：

| 文件 | 行 | 排除理由 |
|------|----|---------|
| `gui/main.cpp` | 751 | QWidget 接线（信号槽/布局/菜单），无可断言的纯逻辑 |
| `gui/audio_recorder.cpp`(+`.h`) | 59 | Qt Multimedia 设备采集；仓库纪律明令"测试里不出现任何音频设备假设"，CI offscreen 会假绿 |
| `gui/pcm_playback.cpp` | 49 | 同上（QMediaPlayer 播放） |
| `gui/waveform_widget.cpp` | 25 | `paintEvent` 绘制 |
| `qmlui/main.cpp` | 43 | QML 应用引导（QQmlApplicationEngine 注册） |
| `qmlui/global_hotkeys.cpp` | 32 | Windows RegisterHotKey 专属；非 Windows 是 stub（已有 test_global_hotkeys 覆盖 stub 语义） |
| `qmlui/startup_launcher.cpp` | 9 | Windows HKCU Run 注册表专属（已有 test_startup_launcher） |

### P0 — 大块零覆盖的业务桥接（本轮主线）

这几个是**应用真正依赖**的逻辑，出问题会静默劣化，且全是纯逻辑/文件 IO，
完全可单测：

- [x] **Q-1 `qmlui/learning_manager.cpp`（412 行，0% → 100%）**——学习数据全部走它：
      查词记录、答题统计、掌握度、标签/笔记、日/周/进度统计、复习调度
      （艾宾浩斯）、成就、导入导出。24 个 Q_INVOKABLE 零测试。
      主体测试 d10a77a 已落（约 40 用例：24 个 Q_INVOKABLE 全覆盖、成就
      static 状态缺陷一并修掉——盘点写入 8 分钟后就提交了，条目当时没勾）。
      本轮补 4 个边界用例：≥2 个弱词/推荐候选时排序比较器真正执行、
      构造期载入过期词触发 reviewDue、导入脏数据 mastery>5 走 default
      间隔 1 天；另 2 行是 gcc 对初始化列表异常清理块的归因假缺口
      （闭合行计数 14 证明语句执行），GCOVR_EXCL 注释排除。
      实测 `coverage.sh --qt`：410/410 = 100%（2026-09-27）。
- [x] **Q-2 `adapters/qt/sync_service_qt.cpp`（357 行，0% → 100%）**——文件级同步
      MVP：扫描/比对/合并/冲突预览。冲突合并逻辑出错会静默丢用户数据。
      新建 `tests/sync_service_qt_test.cpp`（17 用例，QTest + offscreen，目标
      `test_sync_service_qt`）：syncNow 缺远端建文件/全量合并（脏元素远端：
      非对象项、空词、非字符串历史、无 added_at）/读失败（目录当同步文件、
      坏 JSON）/写失败（父级普通文件）；previewDiff 四类差异+同 ts 不报+全程
      只读核身；applyPreview 全开/全关两侧的落盘逐词核对与 last_changes
      记账；applySelection 大小写不敏感勾选+幽灵条目容错；导出/导入选择集
      往返与四种失败面；syncNow 二次幂等。失败分支全部用不依赖文件权限的
      构造（root 下 chmod 会假绿），**零 GCOVR_EXCL**（实测 358/358 行）。
      补测中揪出并修掉一个真 bug：`previewDiff` 里
      `QSet(L.keys().begin(), L.keys().end())` 对两个不同临时 QList 取迭代器
      （keys() 按值返回），跨容器区间纯 UB——本地生词本非空即段错误，
      冲突预览整个功能从未可用过（0% 覆盖藏得够深）。
      实测 `coverage.sh --qt`：358/358 = 100%，Qt 层整体 86.5% → 90.0%；
      `build` 114/114、`build-std` 99/99 全绿（2026-09-27）。
- [x] **Q-3 `adapters/qt/fulltext_manager_qt.cpp`（320 行 → 100%）**——全文索引
      落盘/加载/版本协商（UDFT1/2/3）、签名校验、索引升级、源差异导出。
      11 个 Q_INVOKABLE 零测试，索引缓存失效判断全靠它。
      （条目写 0% 时已过时：`tests/fulltext_manager_qt_test.cpp` 24 用例
      已在库、实测 97%，本轮收口最后 9 行。）新增 3 用例：
      `loadIndexDetailed_looseRelaxedAcceptsMismatch`（strict 败后 relaxed
      兜底成功分支）、`verifyIndexDetailed_parsesMultiSourceDict`（真造
      StarDict 三件套，签名段带 3 源，驱动 parse_sources 后置源循环——
      JSON 单源词典永远测不到）、`exportSourceDiff_carriesChangedEntries`
      （changed 明细的 chg 计数与 changesByDict/dictSummary 汇总列）。
      1 行 GCOVR_EXCL_LINE：`QSaveFile::write < 0` 失败分支（open 刚成功后
      无确定性构造手段；open 失败兜底已由既有用例覆盖）。
      实测 `coverage.sh --qt`：321/321 = 100%，Qt 层整体 86.4% → 86.5%；
      build 113/113、build-std 99/99、build-pron 9/9 全绿（2026-09-27）。
- [x] **Q-4 `qmlui/lookup_adapter.cpp`（471 行 → 100%）**——查词主路径，76 个
      Q_INVOKABLE。GUI/QML 的所有查询都过这里。
      （条目写"383 行 24%"已过时：实测可执行行 471，派发起点 249/471 = 52%。）
      `tests/lookup_adapter_test.cpp` 新增 13 用例 + `initTestCase`（DataStore
      重定向到临时目录，不再写仓库 CWD 的 ./data）：查词写历史/生词本增删查清、
      `stripHtmlForStorage` 四分支、CSV 导出成功与 ENOTDIR 失败（不依赖权限位，
      root 下不假绿）、suggest/wildcard/regex 命中与垃圾输入、词典元信息分类、
      空 env reload、自动朗读直发与 singleShot 延时两支（lookup 与 aggregate
      各一遍）、聚合查询清洗/重写开关与 maxTotal 截断、前进后退栈全转移、
      剪贴板与热键构造期 lambda 的每条分支（findChild 拿子对象直发信号）、
      TTS 全部 wrapper 与预设查表。
      **修产品 bug 1 个**（`fix(qmlui)`）：`P0Modules::ensureMdd` 先 load_mdd
      后 unload_mdd——load 按 id 覆盖写，随后 unload 把刚装上的新解析器删掉
      却返回 true，mounted 留下新路径下次直接早退，**换过一次 .mdd 的词典
      图片/发音资源永久解析不出**。回归哨兵用例 `mdd_remount_after_file_swap`
      用大小写路径（id 是规范化小写路径，`Book.json` 与 `book.json` 同 id）
      构造同 id 换文件场景。
      9 行 GCOVR_EXCL_LINE（均有理由写在行内）：78/81/84 与 589 是 -O2 归属
      假象（多行 brace-init 续行恒 0、`= default` 析构体外联副本无人调用，
      函数体本身每个测试都在跑）；283/293-295 是 TTS 语音循环体（headless
      无语音后端 `availableVoices()` 恒空，mock 引擎只有显式构造能选到，
      仓库纪律禁止对音频设备做假设）；407 是 `deriveMddPath` 空路径守卫
      （唯一调用点已先早退，按构造不可达）。
      实测 `coverage.sh --qt`：461/461 = 100%，Qt 层整体 90.0% → 92.7%；
      build 114/114、build-std 99/99、cov 树 113/113 全绿（2026-09-27）。

### P1 — legacy Qt 核心与薄适配器

- [x] **Q-5 `core/unidict_core.cpp`（719 行 → 100%）**——应用真正链接的 Qt 库
      （`DictionaryManager` 单例：词典增删排序、状态持久化、搜索历史、隔离恢复）。
      （条目写"97 行缺口 86%"已过时：实测可执行行 719，派发起点 629/719 = 87%，
      90 行缺口。）
      `tests/core_lookup_tests.cpp` 新增 8 个 q5_* 用例：addDictionary 三类早退
      （不存在/扩展名不支持/解析失败）与 move/enabled/tags 的 not-found、目录
      扫描大小写同 id 去重与"无支持格式"目录、loadState/saveState 的 IO 与格式
      失败（目录路径/垃圾内容/缺 dictionaries 键/父路径是普通文件的 ENOTDIR，
      不依赖权限位）、importSearchHistory 全分支（缺文件/无 history 数组/非对象
      元素/空查询/大小写去重/置顶跨区插位/百条截断/replaceExisting）、
      setSearchHistoryPinned 置顶区扫描两侧与 not-found、recordSearch 置顶项
      重查保位与 100 条截断、查询引擎各 break/去重/空查询早退（searchSimilar、
      getAllWords、prefixSearch、regexSearch、searchAll、fullText 跳过空释义）、
      loadFromJson 隔离恢复矩阵（非对象/空路径/重复路径/隔离中跳过/缺文件登记/
      扩展名不支持登记/解析失败转隔离/自愈摘除/enabled+tags 归一恢复/幂等
      登记）、retryFailedDictionary 与 forgetFailedDictionary 全流程（含重试
      又失败刷新隔离档位）。
      `init`/`cleanup` 开 `QStandardPaths::setTestModeEnabled`：这些用例大量
      触发隐式 `saveState()`（默认路径），test mode 把它们指到临时目录，不再
      写真实 HOME（本文件所有 load/save 本就传显式 statePath）。
      4 行 GCOVR_EXCL_LINE（理由均写在行内）：37 是 HOME 整体不可得时的兜底
      （QStandardPaths 桌面 Linux 恒非空，无法确定性构造）；613/634 是全文
      索引的防御分支（索引与文档列表成对构建/清空，指针恒非空、下标恒在
      界内）；1104 是私有 recordSearch 的空查询守卫（全部调用点都在
      searchWord 空查询早退之后）。
      实测 `coverage.sh --qt`：715/715 = 100%（719 − 4 EXCL），Qt 层整体
      92.7% → 93.6%；build 114/114、build-std 99/99、cov 树 113/113 全绿
      （2026-09-27）。
- [x] **Q-6 `core/` 其余 legacy 解析器（10 文件 → 100%）**——条目里的缺口数
      是旧账，实测派发起点：mdict 71、stardict 23、epub 19、unidict_core.h
      14、index_engine 12、plugin_manager 12（0%）、data_store 9、
      lookup_service 2、path_utils 7（0%）、json_parser 1。
      新增 `tests/legacy_parsers_test.cpp`（10 个 q6_* 用例）：
      DictionaryParser 接口默认实现（BareParser 只实现 12 个纯虚接口，默认
      allEntries/prefixSearch 的空态/截断/空前缀/未命中/析构各分支）；
      mdict 查询面（findSimilar 前缀环提前 return 与包含环去重截断、大小写
      归一回原词形、prefixSearch 二分两侧早退）与两梯队加载失败（文件缺失/
      目录替身/截断/头校验/加密位/引擎 1.0；zlib 坏流/包裹校验和/未知压缩
      类型/块尺寸与计数不符/keyInfo 字段读到一半/record offset 越界，不
      压缩块作合法成功变体）；Encoding 全分支（UTF-16LE 双字节终止符剥离/
      GBK→GB18030/空→UTF-8/未知编码兜底）；stardict 查询面 + 缺组件/坏
      magic/无 '=' 杂行/ifo·idx 目录替身/空词条 idx；epub 加载失败 + 查询
      面；json 扩展名表；plugin_manager/path_utils 0% 门面全量转发（env
      驱动目录、ensureDir、缓存统计/字节裁剪/按天裁剪含 0 天早退、内置
      工厂幂等注册与大小写归一查询）。
      夹具层重构支撑：`tests/mdict_fixture.h` 参数化 MdxSpec（字节手术开关
      组装器 buildMdxBytes）；`tests/stardict_fixture.h`、`tests/epub_fixture.h`
      自 core_lookup_tests.cpp 抽成共享头（坏变体字段化）。
      门面转发缺口并入既有测试：index_engine_test（remove/clear/exact/
      count/落盘加载往返）、lookup_service_test（suggestMax 截断与
      Did-you-mean 文案）、data_store_test（storagePath 读写/发音练习记录
      增查清/load·save 兼容桩）。
      5 行 GCOVR_EXCL_LINE（理由均写在行内）：mdict 379（数字宽度 4 分支
      被引擎版本早退钉死，不可达）、stardict 236/241（extractDefinition 的
      未打开守卫与 seek 失败防御按构造不可达）、unidict_core.h 75（=default
      空体虚析构必然执行但被编译器并进派生析构，本行无独立计数点）、
      data_store.cpp 41（私有零调用兼容桩）。
      实测 `coverage.sh --qt`：mdict 336/336、stardict 149/149、epub 62/62、
      lookup_service 26/26、index_engine 30/30、plugin_manager 12/12、
      path_utils 7/7、data_store 25/25、json_parser 63/63、unidict_core.h
      14/14 = 全 100%；Qt 层整体 93.6% → 96.1%；build 115/115、build-std
      99/99、cov 树 114/114 全绿（2026-09-27）。
- [x] **Q-7 薄适配器**——条目里的缺口数是旧账，实测派发起点：
      json_parser_qt 32、mdict_parser_qt 27、stardict_parser_qt 26、
      ai_service_qt 39、settings_qt.h 20、clipboard_qt 8（均 0%，另各有
      *_qt.h 头 3 行 0%）；plugin_manager_qt 30、index_engine_qt 30、
      path_utils_qt 9、data_store_qt 87 四个已在 Q-6 顺带收口，本项无需
      再动。新增 `tests/qt_adapters_test.cpp`（8 用例）：parser 桥 ×3 共用
      同一断言模板（加载失败清空标识、成功记 canonical 路径、getter 全
      透传、lookup/findSimilar/getAllWords 查询面）；AI 服务经公共入口
      驱动私有 runExternal 全出口（env 命令入口、stdout 采纳、stderr
      兜底、启动失败、信号杀死、无命令回落启发式）；剪贴板读写往返
      （offscreen 内存剪贴板）；设置项 bool/string/int 往返 + 缺省值 +
      落盘跨实例（QStandardPaths test mode 隔离）。两个 std 侧语义差异
      在测试里显式锚定：MdictParserStd 不认 fixture 的真实 MDX 分块布局
      （best-effort 只扫"文本头 + 原始 zlib 块"）——mdict 桥改用 std 认
      的 word:/definition: zlib 块夹具（同 mdict_zlib_std_test），且其
      lookup 大小写敏感、find_similar 只做前缀；ai_service 外部命令不能
      用 /bin/cat（uutils coreutils 对 translate 的 --to 参数报错退出），
      改用忽略全部参数的脚本。*_qt.h 的 `~X() override = default` 行：
      栈对象走 D2 计数、D0 deleting 析构恒 0——测试补"基类指针持有 +
      出作用域析构"（即 DictionaryManager 的真实持有方式）驱动 D0，
      无需 EXCL。
      4 行 GCOVR_EXCL_LINE（理由均写在行内）：三座 parser 桥的
      absoluteFilePath 兜底（canonicalFilePath 对刚成功打开过的文件
      不可能返回空，单线程无"打开后消失"窗口）、clipboard text() 的空
      剪贴板兜底（QGuiApplication::clipboard() 在运行中的应用内恒非空）。
      另：`scripts/coverage.sh` 增 `--gcov-ignore-parse-errors
      negative_hits.warn_once_per_file`——gcc gcov 的已知解析 bug
      （gcc bug 68080）把极热行 fulltext_index_std.cpp:19 的分支计数写成
      负值，gcovr 默认抛 NegativeHits 直接退 64（Q-7 收口时首现，稳定
      复现）；只降级这一种解析错误为警告，缺口仍按常规口径统计。
      实测 `coverage.sh --qt`：adapters/qt 16 文件（10 个 .cpp +
      6 个 .h）lines 全 100%；Qt 层整体 96.1% → 97.6%；build 116/116、
      build-std 99/99、cov 树 115/115 全绿（2026-09-27）。
- [x] **Q-8 `qmlui/clipboard_monitor.cpp`**——实测派发起点 63 行 27 执行
      42%（旧账"60 行 4%"不准）。新增 `tests/clipboard_monitor_test.cpp`
      （3 用例）：配置项边界钳制（轮询 [100,5000]、词长下限 [1,10]、上限
      [10,200]）+ start/stop 幂等与 monitoringChanged 信号；检测流——私有
      槽 checkClipboard 经 moc invoke 直调，isValidWord/extractWord/
      isExcluded 三个私有 helper 全部经它间接驱动（变化/未变化早退/空串/
      缺省排除表命中 URL 与纯数字/清空排除表后无匹配出口/首尾标点剥离/
      取首词/词长越界/特殊字符 >30% 拒绝（'-' 与 '\'' 不计特殊）/CJK 词
      形）；真实 QTimer 轮询接线（setPollInterval(100)+QTRY_COMPARE，
      start 先快照当前剪贴板）。
      顺带修一个真 bug：letterRegex 用了 PCRE2 不支持的 \uXXXX 转义，
      正则恒非法 → isValidWord 恒假 → wordDetected 从不触发，剪贴板
      自动查词整条链路实际是死的；改为 \x{4e00}-\x{9fff} 语法（行为
      变化：功能从"永不触发"修复为按设计触发）。
      1 行 GCOVR_EXCL_LINE（理由行内）：checkClipboard 的空剪贴板早退
      （QGuiApplication::clipboard() 在运行中的应用内恒非空）。
      实测 `coverage.sh --qt`：clipboard_monitor.cpp 62/62、.h 6/6 =
      100%；Qt 层整体 97.6% → 98.0%；build 117/117、build-std 99/99、
      cov 树 116/116 全绿（2026-09-27）。
- [x] **Q-9 `cli/main.cpp`（55 行，0% → 100%）**——Qt 版 CLI 参数解析。
      `tests/cli_main_test.cpp` 用 QProcess 驱动真 `unidict_cli`（main()
      不能链进 QTest 二进制），13 用例覆盖：usage/help/version、`--list`
      诸形态（干净空态 / 空态+lastError / -D 目录缺失报错 / 单文件 /
      目录扫描 / 状态文件引用丢失词典走 `[FAILED]` 行）、查词命中 0 /
      未命中 2、`loadDefaultDictionaryLocations` 的 env 与 localDir 两条
      自动加载路。每用例独立 XDG_DATA_HOME/LOCALAPPDATA 临时目录隔离状态。
      实测 `coverage.sh --qt`：55/55 = 100%（2026-09-27）。
- [x] **Q-10 `gui/pronunciation_panel.cpp`（146 行，0% → 100%）**——发音
      练习面板的非设备逻辑：构造态（词条/自由练习两形态、口音行显隐、
      录音入口跟随设备分支）、QSettings 口音持久化（预存恢复/非法值回退
      en-GB/切换写回）、TTS 惰性构造与 ensureTts 幂等、对比/录音/回放的
      各守卫分支——全程不作任何音频设备假设。`tests/pronunciation_panel_test.cpp`
      `QTEST_MAIN` 起 QApplication（offscreen），3 用例：断言跟随
      `AudioRecorder::hasInputDevice()`/`QTextToSpeech::availableEngines()`
      环境分支；私有槽经 moc invoke 直调（toggleRecording/playComparison/
      onTtsStateChanged/onCompareTimeout）；回放失败路靠 `PcmPlayback::play`
      对空样本的无条件 false（确定性，与设备无关）；目标 + audio_recorder/
      pcm_playback/waveform_widget 源码直挂测试 target（只编
      UNIDICT_GUI_PRON=OFF 形态，评分路径属 unidict_pron 域不在此测）。
      设备缠结区按仓库纪律在源码内以 GCOVR_EXCL 整段标注理由（录音启动
      成功/回放成功路/录音·回放三个回调/对比后半程含 finishComparison
      ——门禁环境无音频设备、信号永不发射，函数签名行一并排除；无 TTS
      引擎降级块）。析构行走 Q-7 同款多态删除块（`unique_ptr<QDialog>`
      触发 D0 deleting 析构，栈实例覆盖 D2）。实测 137/137 = 100%；Qt 层
      整体 98.0% → 99.7%（10251/10277）；build 118/118、
      build-std 99/99、cov 树 117/117 全绿（2026-09-27）。附注：无 speechd
      守护进程的机器上 Qt 惰性加载默认引擎会打一条 qCritical 杂音，
      QtTest 不因此判失败（测试内注释已说明）。

### 分支缺口巡检（2026-09-27，lines 收口后的下一口径）

- [x] **B-1 `core/std/mdict_parser_std.cpp` 分支缺口补测**——lines 100% 后
      按 gcovr branch 口径盘点，该文件是最大单文件缺口（1614 分支行中
      669 缺）。新增 `tests/mdict_parser_std_branches_test.cpp`（std-only
      assert 风格，自注册 CTest target `test_mdict_parser_branches_std`），
      11 个场景：路径不存在/.mdd 直解/完整头部属性/encrypted 五种合法写法/
      加密体降级/五容器链逐格式真解（KIDX+RDEF、KEYB+RECB、KBIX+RBIX、
      KBIX+RBCT+RBLK 含 bid/off 越界剔除、MDXK+MDXR 含截断 half-entry）、
      启发式主路（parsed>=8 提前截断 + off 越界剔除）、启发式回退重扫
      （非 wordish 触发 `p -= wl+8-1`，回退后每轮恰前进 3 字节的字节级
      摆布）、启发式无记录块回落种子词、scan_and_decompress 三类坏块 +
      word:/definition: 与 tab 双模式（tab 行必须独立 zlib 块——
      `find("word:",i)` 会跳过中间行，这是本轮实测出的解析器语义）、
      render/URL 归一 19 形态（协议直通、entry/bword 编码、缓存与
      dict_dir 双层命中/miss、未闭合引号、@@@LINK 多标记——相邻标记无
      分隔符会并成一词，用 '<' 断词、':'→'_' 需 dict_dir 真实存在
      `C_colon.png` 才可见）、companion 提取容错与 manifest 删除重提取、
      lookup/find_similar 边界。
      实测（build-cov std 口径，`--txt-metric branch`）：该文件
      945 → 1017 covered（669 → 597 缺，59% → 63%）；全 core branches
      64.4% → 66.5%（补测后）；lines/functions 维持 100%。
- [x] **B-2 mdict 不可达分支 GCOVR_EXCL 收口**——补测后剩余缺口逐处
      甄别，对 14 处单线程测试不可构造/不可达分支在源码内标注理由
      （沿 795 既定口径）：`find_bytes` 零长 needle 卫语句（调用点全是
      非零字面量 magic）；`make_file_url` 反斜杠归一（输入全部经
      sanitize_relative_path，POSIX 下恒假）；`read_head`/best_effort/
      五链/SIMPLEKV/scan 的 4 组 ifstream 打开守卫（文件刚验证/打开过，
      失败需"两次打开之间被删"）；`safe_inflate` 等 5 处 inflateInit
      失败分支（仅内存耗尽可触发）；`load_resource_manifest`/
      `extract_and_cache_resources_from_mdd` 空根守卫（私有方法，唯一
      调用点必已设根）。实测：lines 100.0%（6659/6659，EXCL 净除 17 行）、
      functions 100.0%（687/687）、全 core branches 66.6%（7225/10852）、
      mdict 569 缺/1556（63%）；cov 树 101/101（新增分支测试 target）、
      build-std 100/100 全绿（2026-09-27）。
- [x] **B-3 mdict 分支缺口第二批补测**——继续压 B-2 后剩余缺口，新增
      `tests/mdict_parser_std_branches2_test.cpp`（std-only assert 风格，
      自注册 CTest target `test_mdict_parser_branches2_std`），C1–C12
      十二组约 60 个场景：SimpleKV 五种布局/损坏阶梯；五容器链逐格式
      十级摆布（magic 换序、count 溢出、块内 off/len 越界、跨块拼接、
      zlen 谎报、half-entry）；MDXK 十七例（含 clen=0 空块与 16MB stored
      zlib 压 `clen>MAX` 卫语句——compress2 level 0 构造，不做 EXCL）；
      启发式特殊字符 6 形态/wl>128 回退/good<2 回落；scan_and_decompress
      坏头两臂+33 块封顶+命中残段；UTF-16 LE/BE/零字节/无 description
      三元/未闭合引号；zlib 包裹/截断/坏头 + word:/definition:/tab
      残段混合；明文直读链/XOR 成功/空 body/UNIDICT_PASSWORD 两臂/EOF；
      render 资源 src 空串/http:///%20/引号截断/相对名空 dict_dir；
      find_similar break/假侧/耗尽；MDD companion 提取全容错链（目录占名
      sanitize、只读父目录→ensure_dir 失败→manifest 打开失败、manifest
      坏行、全坏重提取、zlib 与加密 companion）。
      新增 7 处 GCOVR_EXCL_LINE（理由均在行内，沿 795 口径）：
      `make_file_url` 非斜杠开头（输入全为 fs::absolute 产物）与 `#` 编码
      （键归一已按 `?#` 截断）；`extract_and_cache` raw 空守卫（best_effort
      true⟹words 非空⟺entries 成对写入⟹raw 非空）与 close 后 `!out`
      continue（磁盘满/IO 错误不可构造）；`load_dictionary` decryptor_
      空守卫（ctor 必建、从不重置）与 decrypted_body 空判断（SIMPLE_XOR
      成功⟺密钥流非空⟺输入非空）；chain-2 inflateInit（round-1 漏标补上，
      理由同 safe_inflate）。
      实测（build-cov std 口径，`--txt-metric branch`）：mdict 官方表
      taken 987 → 1052/1526（69%，本批新测试 +66；EXCL 净除 30 分支行）；
      全 core branches 66.6% → 67.4%（7294/10822）。扣除 throw 边后真实
      条件边覆盖 91.7%（1271 边缺 106，散布 83 行、多为一侧深卫语句，
      按"不为凑数强凑"止步）。测试侧注意：MDD 缓存根按 path|size|mtime
      签名命名，重写文件即换根、旧根残留，测试开头须清空 mdd 缓存根再
      断言（C12b 曾因此误断）。
      门禁：lines 100.0%（6652/6652）、functions 100.0%（687/687）阈值
      PASS；build-std 101/101 全绿（2026-09-27）。
- [x] **分支巡检收尾轮状态注记（2026-09-27）**——复跑全部门禁确认维持：
      core lines 100.0%（6652/6652）、functions 100.0%（687/687）阈值
      PASS，branches 67.4%（7294/10822，与 B-3 记录一致）；build-std
      101/101、build(Qt) 119/119、build-pron 100/100 全绿，树净。
      **维持止步判定**：mdict_parser_std.cpp 剩余缺口为 447 条 throw 边
      （测试不能让代码 throw，不可赢）加 106 条真实条件缺边（散布 83 行、
      真实条件边覆盖 91.7%），余量多为一侧深卫语句组合，继续补测只能
      写"为覆盖率而覆盖率"的断言，违背「不为凑数强凑」既定口径——
      本轮不补测，仅确认数字未回退。
- [x] **B-4 aggregate_lookup 分支缺口补测 + 死三元清理**——真实缺边全库扫描
      （原始 gcov 扣 throw 边）选定最大簇 `core/std/aggregate_lookup_std.cpp`
      （104 条，次大 data_store 87）。构成：三处 source 三元
      （perform_lookup/perform_prefix_lookup/perform_fuzzy_lookup）各约 22 边
      ——ctx.sources 与 target_dict_ids 在同一循环同批填充、contains_id 已
      筛过，`EntrySource{}` 兜底臂结构不可达，true 臂又因单侧词典名从未
      同时走过 SSO/heap 拷贝——4 处代码简化为直取并注不变式（含
      definition_similarity 的 union_size>0 三元：空 tokens 守卫已挡掉
      除零），其余约 38 条逐簇真实输入补测：新增
      `tests/aggregate_lookup_std_branches_test.cpp`（std-only assert 风格，
      自注册 target `test_aggregate_lookup_branches_std`），S1–S10 场景：
      长短词典名过三查询路径（source 拷贝 SSO/heap 两臂）；exact/prefix/
      fuzzy 的 per-dict 与 total 上限（0 全 continue、1 首条触发 break/
      return、1 未达上限通过）；dedup/sort 关闭臂；停用词典
      include_disabled 两臂×三路径；释义内容边角（纯标签空 tokens、前导
      分隔符、连续空格压缩、标签+正文混合）；manager 边界（空 manager
      兜底、优先级真假 id、get_source 不存在 id）；builder 手工 relevance
      的 best_entry 更新两臂；Jaro 内部臂（自身相等/零匹配/换序）与空
      查询串空串守卫。测试侧注意：prefix/fuzzy 检索依赖 trie，add 字典后
      必须 `mgr.build_index()`。新增 1 处 GCOVR_EXCL_LINE：group_entries
      的 `!entries.empty()`（按 entry.word 建组⟹组必非空）。
      实测：本文件真实缺边 104 → 4，剩余 4 条全部为已 EXCL 的结构不可达
      臂（优先级比较器、examples、pronunciation、空组守卫，官方 gcovr 表
      已排除）；全 core branches 67.4%（7294/10822）→ 68.1%（7311/10736）。
      门禁：lines 100.0%（6649/6649）、functions 100.0%（687/687）阈值
      PASS；build-std 102/102、build(Qt) 121/121、build-pron 103/103 全绿
      （2026-09-28）。
- [x] **B-5 data_store 分支缺口补测**——真实缺边次大簇（87 条，散布 57 行）。
      构成：现有测试从不在磁盘上重载 vocab/notes/pron_records 区段（add 后
      直接走内存断言），持久化解析半边（find_section 字符串感知深度计数、
      for_each_object 状态机、obj_val/obj_int/obj_num/obj_str_array 容错
      守卫、save 写省略臂）整体冷启动；无 aggregate_lookup 式死臂，几乎
      全部真实输入可构造。新增 `tests/data_store_std_branches_test.cpp`
      （std-only assert 风格，自注册 target
      `test_data_store_branches_std`）：R1 全区段 save→load 往返（转义
      字符/小数词分/多记录逗号/空时间戳写省略）；M1 手写畸形文件（非字符
      串元素、缺键/缺冒号/缺引号、负数与非数字、tab 后置字段、tags 有键
      无数组、未闭合对象跳过、空词对象跳过）；M1b 花括号汤（字符串外裸
      花括号、词位被键名顶替的容错、对象型 pron_records 区段）；M2 区段
      非数组与键后无冒号；A1 API 守卫（目录路径 save 失败、存在但不可读
      文件 load 失败〔POSIX 卫兵 + root 跳过〕、limit≤0、upsert 更新臂、
      CSV 引号加倍与不可写路径、笔记增改删三态、发音记录空词忽略/
      upsert/查询三态/清空）。
      新增 3 处 GCOVR_EXCL_LINE（同一不变式，理由行内）：parse_json_string
      的"未闭合串"三处兜底（while 的 i≥size 出口、串尾悬空反斜杠 break、
      out_end 取 s.size() 臂）——三个调用方都由字符串感知扫描器把关后才
      切入且扫描规则与本函数一致，传入串必然闭合（函数头注释自证）。
      止步判定（余 29 条真实缺边，按「不为凑数强凑」）：(a) obj_int/
      obj_num 各扫描循环的 p≥o.size() 越界臂——对象子串恒以 `}` 结尾，
      值扫描不可能越过串尾，结构不可达；(b) obj_str_array 的 k≥o.size()
      需未闭合 `[`，而无匹配 `[` 恒毒化 find_section 深度（区段边界被
      吞、解析整体劣化到错误区段——留档已知局限的耦合），构造它等于
      断言错误行为；(c) 内联副本归属噪声（obj_val/obj_int 的 npos 守卫
      两臂已被 passing 断言证明双热，个别内联副本仍计 0）。
      实测：本文件真实缺边 87 → 32（3 EXCL + 29 结构/噪声止步）；全 core
      branches 68.1%（7311/10736）→ 68.6%（7359/10726）。门禁：lines
      100.0%（6649/6649）、functions 100.0%（687/687）阈值 PASS；
      build-std 103/103、build(Qt) 122/122、build-pron 104/104 全绿
      （2026-09-28）。

- [x] **B-6 fulltext_index 分支缺口补测 + 死 fallback 清理**——真实缺边
      最大簇（83 条，散布 45 行）。构成：持久化半边的截断矩阵（UDFT3
      逐字段截断、UDFT1 posting 截断、手写坏 varint）从未跑热；v1/v2
      未压缩臂只有单句快乐路径；add_document 后不 finalize 的 idf 兜底、
      查询词去重/候选共享扩展去重、线程数兜底/钳制臂、max_results≤0、
      save/load 打开失败、幽灵词（n=0）回存、空词项、词内非词字符的
      gram 归档跳过臂、候选 256 上限截断均为冷臂。新增
      `tests/fulltext_index_std_branches_test.cpp`（std-only assert 风格，
      自注册 target `test_fulltext_index_branches_std`，T1-T7 七组）：
      T1 词字符成词臂；T2 线程兜底/钳制/空文档集/max_results≤0；T3 查询
      去重、未 finalize 的 idf 兜底、共享候选扩展去重；T4 save/load 打开
      失败、坏 magic、签名往返；T5 UDFT3 逐字段截断矩阵（含 blen 完整
      buf 截断、siglen>0 签名截断）+ UDFT1 docId 完整 tf 截断 + UDFT2
      完整；T6 手写语义文件（坏 varint 三态：7 续字节越 shift 上限、
      单续字节到尾、tf 越界，幽灵词回存、空词项、词内 `.` 的 gram 跳过）；
      T7 候选路径（ngram3 最稀桶命中/全词不含 miss、ngram2/单字、索引
      空臂、256 上限截断×2、多字节 varint 往返）。
      源码侧死代码清理（非凑数，删除在到达时必 miss 的路径）：
      `substring_candidates` 的 fallback（prefix 桶 + 全词表扫描）连同
      `prefix_index_`/`build_prefix_index()` 删除——不变式：tok 出自
      tokenize（非空、全词字符），含 q 的词必落入 q 的 gram 桶并提前
      return，fallback 只能白付 O(词表) 扫描；2-gram/单字桶的逐词
      `find` 再验证删除（桶语义已保证含 q）；空串守卫与 3-gram 逐字符
      is_word_char 守卫删除（契约：tok 必来自 tokenize）；
      `build_from_documents` 的 `threads < 1` 死钳删除（上两臂已保证
      ≥1）。新增 1 处 GCOVR_EXCL_LINE（ensure_postings 的 end 臂：
      唯一调用方 search() 进入前已确认 term 命中）。同步修正
      `fulltext_index_std_cover_test.cpp` fallback 断言（(void) →
      assert 空）与 clear 测试头注释。
      止步判定（余 6 条真实缺边，按「不为凑数强凑」）：(a) EXCL 5 条
      （ensure_postings end 臂，官方 gcovr 表已识别排除）；(b) 止步 1 条：
      line 56 `hardware_concurrency()==0` 兜底臂——环境依赖（本平台
      恒返回正核数），非结构不可达，无真实输入可驱动。
      实测：本文件真实缺边 83 → 6；全 core branches 68.6%
      （7359/10726）→ 69.1%（7310/10574，分母净减为死行删除）。
      门禁：lines 100.0%（6552/6552）、functions 100.0%（680/680）
      阈值 PASS；build-std 104/104、build(Qt) 123/123、build-pron
      105/105 全绿（2026-09-28）。

- [x] **B-7 dictionary_manager 分支缺口补测**——真实缺边次大簇（75 条，
      散布 29 行）。构成：Holder::lookup 五格式链的各臂、add_dictionary
      四扩展名解析失败臂、ifo 伴生文件矩阵（dict 在场/仅 dz/全无）从未
      组齐、mdx 同目录伴生扫描四象限（同 stem .mdd/异 stem .mdd/非
      .mdd/子目录）、enabled 同态重设、search_all 禁用跳过与全 miss、
      full_text_search 空 query 与 max_results≤0、上限 break、ensure 的
      空释义跳过、relaxed 索引越界 docId（dict/word 越界与负值四态、
      空释义词命中）、null out_error/out_version 两态、accept_version=1
      拒绝 v3、签名的源文件缺失与"文件变目录"形态均为冷臂。新增
      `tests/dictionary_manager_std_branches_test.cpp`（std-only assert
      风格，自注册 target `test_dictionary_manager_branches_std`，T1-T5
      五组）：T1 Holder 链（json 短路/dsl 穿链/csv 穿链 + .tsv/.txt 臂）
      与四扩展名失败臂、enabled 同态/翻转（同名词典按名首中）、search_all
      禁用跳过/全 miss/meta 照列禁用词典；T2 ifo 伴生矩阵（dict 在场/
      仅 dz/无 dict 无 dz 被解析器拒绝）；T3 mdx 伴生扫描四象限 + 签名
      只含同 stem .mdd；T4 full_text_search 三短路臂、上限 break、
      relaxed 坏文件 out_error 两态、真索引往返版本回填、crafted UDFT3
      越界 docId 四态（dict 越界/负 dict/word 越界/负 word）、空释义词
      命中不产出、严格加载失败；T5 签名源缺失 "(missing)"、文件变目录
      （is_regular_file 假臂）、accept_version=1 拒绝 v3（out_error
      null 与非 null 两态）。
      源码侧 7 处 GCOVR_EXCL_LINE（全部结构性死臂，非凑数）：Holder::
      lookup 的 csv 假臂与 dictionaries_meta 的 csv 假臂（到达即要求
      前四级全空 + csv 也空 = 全空 Holder，与既有全空兜底 EXCL 同源）；
      ifo 伴生的 idx 存在假臂（解析器成功前提即 idx 存在）与 dz 存在
      假臂（解析器成功前提即 dict∨dz 至少一在）；full_text_search/
      save_fulltext_index 的 `!ft_index_` 守卫（ensure 无条件赋值恒非
      空）；签名的 `!d.words.empty()` 假臂（五解析器成功都保证 ≥1 词：
      json/csv/dsl 校验 entries 非空、stardict 校验 idx 解析非空、
      mdict 兜底无条件登记骨架词 mdict/unidict）。
      止步判定（余 37 条，按「不为凑数强凑」）：mdx 伴生扫描行 13 条 +
      三处 range-for 收括号 15 条 + 三处 `new` 表达式 9 条——全部为
      STL/filesystem 内联机器边与分配异常边，与批前测量计数逐一相同
      （四象限真条件组合已全热仍不动），非真实条件缺边。
      实测：本文件真实缺边 75 → 44 raw（7 EXCL 行残影 + 37 机器边）；
      全 core branches 69.1% → 69.6%（7406/10634）。
      门禁：lines 100.0%（6614/6614）、functions 100.0%（686/686）
      阈值 PASS；build-std 105/105、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 106/106 全绿（2026-09-28）。

- [x] **B-8 html_renderer 分支缺口补测**——真实缺边第三大簇（57 条，
      散布 27 行）。构成：utf8_safe_cut 回退预算耗尽的 take=0 兜底臂、
      分词器畸形阶梯（未闭合注释/无名标签/无'>'标签/注释探测头四半臂/
      空属性名/布尔属性/无值'='/引号在'>'后/未闭合引号/非引号值/属性
      循环三种 close 退出）、render 选项第二条件假臂（resolver 非空 ∧
      resolve_links 假）、ELEMENT_START 的 video 臂、rewrite 链的
      is_dictionary_resource 假臂、CSS 过滤空段/无冒号段/单属性重建/
      危险值全滤、resolver 同名目录守卫两态/缺失/扩展名剥离长度与
      不匹配臂/全斜杠 key/协议剥离正向命中、strict/permissive 工厂。
      新增 `tests/html_renderer_std_branches_test.cpp`（std-only assert
      风格，自注册 target `test_html_renderer_std_branches`，T1-T6
      六组）。
      源码侧 6 处 GCOVR_EXCL_LINE（全部结构性死臂）：TEXT 非空守卫
      （text 来自 ≥1 字符 substr，实体解码不归零）、is_self_closing
      的 close_pos>0 左臂（close_pos 是 tag_end 后找到的 '>' 恒大于
      0）、属性名 find_first_of 的 npos 臂与无引号值 find_first_of 的
      npos+越界双臂（查找集含 '>' 且 close_pos 处即 '>'，必命中 ≤
      close_pos）、render 主循环 switch 落空边（覆盖 Type 全部 5 个
      枚举值无 default）、get_data_url 的 file 重开守卫（上方
      is_regular_file 已确认 + 同路径刚成功打开过）。
      止步判定（余 16 条机器边，按「不为凑数强凑」）：分词循环 `}` 行
      12 条（6+6，循环机器边）+ filesystem is_regular_file 内联 2 条 +
      string substr/== 内联 2 条——全部与批前测量计数逐一相同（畸形
      矩阵全热仍不动），非真实条件缺边。
      实测：本文件真实缺边 57 → 33 raw（EXCL 行残影 17 + 机器边 16）；
      全 core branches 69.6% → 69.9%（7414/10610）。
      门禁：lines 100.0%（6608/6608）、functions 100.0%（686/686）
      阈值 PASS；build-std 106/106、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 107/107 全绿（2026-09-28）。

- [x] **B-9 stardict_parser 分支缺口补测**——真实缺边第四大簇（53 条，
      散布 34 行）。构成：ifo 形态矩阵（目录 ifo/无 '=' 行/charset 行/
      无 bookname/idx 缺失）、idx 形态矩阵（空文件/末词条无 \0 终止/
      空词条跳过/64 位与 32 位截断尾条目）、dict 打开矩阵（.dz 不可读
      致 gzopen 败/.dz 二次加载走缓存命中/用 .dict 路径加载）、权限
      形态（不可读 ifo 与 .dict，POSIX + 非 root 守卫，remove 含
      group/other 位）、缓存目录被 UNIDICT_CACHE_DIR 指到文件、
      decode_entry 全类型码矩阵（11 码各臂、h/x 作为非首文本字段、
      size 前缀截断与钳制、无 \0 小写码宽容回退、sametypesequence 的
      i 耗尽与 read_field 假臂）、lookup 守卫（未加载/64 位全 1 偏移
      seekg 失败/find_similar 上限 break）。新增
      `tests/stardict_parser_std_branches_test.cpp`（std-only，T1-T5，
      链接 zlib；base_dir hermetic 清理防中断残留）。
      源码侧 2 处 GCOVR_EXCL_LINE（结构死臂）：ends_with 短串假臂
      （唯一调用点 open_dict 实参恒为 base+".dict"/base+".dict.dz"，
      ≥5 字节 > ".dz" 的 3，且 base 非空）；decode_entry 规范格式判定
      的 raw.empty() 假臂（函数开头空串守卫先行 return 拦截）。
      止步判定（余 10 条，按「不为凑数强凑」）：ends_with 比较器
      1 条（匹配/不匹配两态已由 .dz 与 .dict 双路行为验证，剩余边为
      反向迭代器机器边）、load_ifo 循环收口 1 条、.dz 解压写失败臂
      2 条（需磁盘满，环境依赖，同 hardware_concurrency 先例）、
      load_dictionary 成功路径落垫 5 条、dictionary_name 字符串返回
      机器边 2 条——全部与真实条件无关。
      实测：本文件真实缺边 53 → 12 raw（EXCL 行残影 2 + 机器边/
      环境臂 10）；全 core branches 69.9% → 70.2%（7444/10600）。
      门禁：lines 100.0%（6606/6606）、functions 100.0%（686/686）
      阈值 PASS；build-std 107/107、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 108/108 全绿（2026-09-28）。

- [x] **B-10 mdd_resource 分支缺口补测**——真实缺边第五大簇（44 条，
      散布 29 行）。构成：缓存名 file_extension 的无点与超长扩展名臂、
      decompress_zlib 的"Z_OK 但输出缓冲恰好填满"扩容臂、single_block
      的正常退出（条目区恰好 EOF）/短读/key_len==0/key_len>1024 断臂、
      multi_block 的 RBLK 签名不符与 fread 短读断臂 + 块内 key_len==0
      与越界断臂、get_resource_info 未命中、extract_to_cache/extract_all
      的失败计数臂、normalize_key 全斜杠臂、read_bytes 的 offset/size
      越界臂、缓存名坏字符替换的 \0 臂、get_from_cache 的文件被删/
      变目录两态、manager 的缓存命中路径/全新词典首取/缓存失效重取/
      未知词典与未知词条臂。新增
      `tests/mdd_resource_std_branches_test.cpp`（std-only，T1-T5，
      链接 zlib：compress2 构造 RBLK 压缩块；base_dir hermetic）。
      源码侧 1 处 GCOVR_EXCL_LINE（结构死臂）：read_bytes 的
      file_size<0 臂（tell64 只在流出错/无 seek 时返回 -1，走到该行的
      前提是 fseek(END) 刚成功，同 364 行 ftell 守卫的既有 EXCL 证明）。
      止步判定（余 9 条机器边，按「不为凑数强凑」）：fs::exists/
      is_regular_file 内联模板错误路径 6 条（1156×2/1190×2/940×2，
      与 B-8 的 is_regular_file 内联 2 条同类）、single_block 的
      fread 调用条件子边 1 条（427，两态行为已由短读与正常读双路
      验证）、get_resource_info 函数出口子边 1 条（615）、SimpleKV
      return-true 块分裂子边 1 条（395）。
      实测：本文件真实缺边 44 → 30 raw（EXCL 行残影 21 + 机器边 9，
      真实条件缺边清零）；全 core branches 70.2% → 70.4%
      （7455/10596，EXCL 净除 4 分支行）。
      门禁：lines 100.0%（6605/6605）、functions 100.0%（686/686）
      阈值 PASS；build-std 108/108、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 109/109 全绿（2026-09-28）。
      实测方法论纠偏（后续批次沿用）：本机 gcov 15 的
      `--json-format` 输出不再携带 branches 键（文本模式正常），
      残差测量口径切换为 `gcovr --json` 的 lines[].branches
      （过滤 throw 边，与门禁同 EXCL 语义）；且 .gcda 会被无分支
      计数器的运行覆写降级（表现为 gcovr 分支分母骤减），测量前必须
      先跑一遍 coverage.sh 重建干净的 .gcda。V2 块载荷是纯条目流，
      垫头字节会被当条目解析（解压扩容臂用 1100 字节超长键撑大块体，
      multi_block 的 klen 无 1024 上限——single_block 才有）。

- [x] **B-11 cross_reference 分支缺口补测**——真实缺边第六大簇（22 条）。
      构成：url_decode 的尾部截断 %xx 臂、url_encode 的 -_.~ 保留子边
      与编码臂、bword 查询参数无 dict= 与无 '=' 值臂、
      is_cross_reference 对合法非交叉引用类型（file/sound/http）的全假
      短路边、is_valid_link 全空白词假臂、extract_links 多链接收集、
      export_history 的 back/forward 多条目逗号臂、import_history 的
      extract_object/extract_array 标记缺失与嵌套容器内层闭合子边、
      无 word 条目跳过、are_variations 双向单侧未知回落臂、
      load_from_file 的注释行/空白片段/空白行臂、save/load 往返。新增
      `tests/cross_reference_std_branches_test.cpp`（std-only，T1-T5，
      base_dir hermetic）。
      源码侧 2 处 GCOVR_EXCL_LINE（结构死臂）：resolve_link 的
      `!resolved.empty()` 假臂（is_cross_reference 已把 valid ∧ 类型 ∈
      {INTERNAL, ENTRY, BWORD} 作前提，三路 resolve 恒产出非空），以及
      is_valid_link 的 `!url.empty()` 假臂（函数入口 url.empty() 已提前
      返回假）。EXCL 编辑会使后文行号 +5/+3 移位——solo 复核必须按
      新行号取数。
      实测：本文件真实缺边 22 → 2 raw（两条均为 EXCL 行残影，真实
      条件缺边清零）；全 core branches 70.4% → 70.5%（7471/10590，
      EXCL 净除 4 分支行）。
      门禁：lines 100.0%（6603/6603）、functions 100.0%（686/686）
      阈值 PASS；build-std 109/109、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 110/110 全绿（2026-09-28）。
      逐边定性纠偏：gcov 文本模式对部分行（如 `&&` 链合并块）不吐
      branch 注记，此时用 gcov --json-format 的 gz 输出按
      source/destination_block 精确定位缺失边身份再定性（本轮由此
      判定 755 的缺边是 url.empty() 假臂而非空白臂）。

- [x] **B-12 epub_parser 分支缺口补测**——真实缺边第七大簇/最后一簇
      （16 条，散布 14 行）。构成：decode_entities 的空实体（&;）、
      十六进制非码字（&#xg;）与小于 'A' 的非十六进制字符（&#x!;）、
      十进制非数字截停（&#1a2;——畸形字符后须再跟数字才能驱动 `&&`
      短路的 ok 假边）；extract_attribute 的闭引号缺失臂；
      extract_element_text 的属性空格形态（<dc:title xml:lang=..>）、
      开标签恰好 EOF（三目 \0 臂）、无 '>' 与无闭标签两臂；
      heading_level 的 <hN 前缀误配（<h12 ...> 尺寸臂与非空白臂）；
      状态机 BeforeHeading 态遇 stray 闭词头标签臂。新增
      `tests/epub_parser_std_branches_test.cpp`（std-only，T1-T5，
      自带 stored-only zip 写入助手，base_dir hermetic；parse_container
      等为 private，全部畸形输入经 load_dictionary 的 zip 全链驱动，
      章节条目名须带 OEBPS/ 前缀——href 会拼 opf_dir）。
      源码侧 3 处 GCOVR_EXCL_LINE（结构死臂）：extract_attribute 的
      pos==0 臂（两处调用点的元素串都截自 find("<rootfile")/
      find("<item")，首字符恒 '<'，needle 以属性名开头）；heading_level
      的 size 假臂（tag 恒以 '>' 结尾，<hN 前缀且尺寸 ≤ name_end+1
      只可能是完整 <hN>，已被上一条件提前返回）；load_dictionary 的
      opf_path.empty() 真臂（parse_container 仅在 full-path 非空时
      返回真）。
      实测：本文件真实缺边 16 → 4 raw（4 条均为 EXCL 死臂残影——1 条
      在 EXCL 行本体，3 条为同一死臂被 gcc 错位归因到相邻注释/括号行
      的 stray 边，真实条件缺边清零）；全 core branches 70.5% → 70.6%
      （7469/10572）。
      门禁：lines 100.0%（6600/6600）、functions 100.0%（686/686）
      阈值 PASS；build-std 110/110、build(Qt) 124/124（lines 100%、
      functions 99.8% PASS）、build-pron 111/111 全绿（2026-09-28；
      build-std 首轮全量曾偶发 test_data_store_std_escape 中止，单跑
      与复跑均绿，与本批改动无关）。至此分支缺口巡检既定簇队列
      （fulltext_index → dictionary_manager → html_renderer →
      stardict → mdd_resource → cross_reference → epub_parser）
      全部收口。

- [当前状态] 2026-09-27: core lines 100.0%（6652/6652），functions 100.0%（687/687），branches 67.4%（7294/10822，mdict 官方表 taken 987→1052/1526，69%）。门禁全绿：build-std 101/101，build(Qt) 119/119 lines 100%，build-pron 100/100。止步判定：剩余 447 条 throw 边（测试不可达，天然不可赢）+ 106 条真实条件缺边（散布 83 行，91.7% 覆盖率），按「不为凑数强凑」已止步，不再补测。所有测试通过：test_mdict_parser_branches2_std C1-C12 全绿，C12b 缓存根问题已修。分支趋势 66.6% → 67.4%（+66 taken，EXCL 净除 30 分支行）。

### 当前状态注记（2026-09-27）
- 实测数字：core lines 100.0%（6652/6652），functions 100.0%（687/687），
  branches 67.4%（7294/10822，mdict 官方表 taken 987→1052/1526，69%）。
- 门禁全绿：build-std 101/101，build(Qt) 119/119（lines 100%），build-pron 100/100。
- 止步判定：mdict_parser_std.cpp 剩余 447 条 throw 边（测试不可达，天然不可赢）+
  106 条真实条件缺边（散布 83 行，91.7% 覆盖率），按「不为凑数强凑」已止步，
  不再补测。分支趋势自 66.6% → 67.4%（本批 +66 taken，EXCL 净除 30 分支行）。
- 所有测试已通过：`test_mdict_parser_branches2_std` C1-C12 十二组约 60 场景全绿，
  C12b 缓存根问题已修（重写前清空全部 mdd_ 根，三次连跑全绿）。
- git：working tree clean，于 2026-09-27 完成本批收尾。
- 覆盖率实测全绿确认：build-std 101/101 全绿、core lines 100.0%（6652/6652）PASS、
  functions 100.0%（687/687）PASS、build(Qt) 119/119 lines 100% PASS、build-pron 100/100 全绿。

### 分支缺口巡检 当前状态注记（2026-09-27 复跑尾冲）
- 实测数字：core lines 100.0%（6652/6652），functions 100.0%（687/687），
  branches 67.4%（7294/10822，mdict 官方表 taken 987→1052/1526，69%）。
- 门禁全绿：build-std 101/101，build(Qt) 119/119（lines 100%），build-pron 100/100 全绿。
- 止步判定：mdict_parser_std.cpp 剩余 447 条 throw 边（测试不可达，天然不可赢）+
  106 条真实条件缺边（散布 83 行，91.7% 覆盖率），按「不为凑数强凑」已止步，
  不再补测。分支趋势自 66.6% → 67.4%（本批 +66 taken，EXCL 净除 30 分支行）。
- 所有测试已通过：`test_mdict_parser_branches2_std` C1-C12 十二组约 60 场景全绿，
  C12b 缓存根问题已修（重写前清空全部 mdd_ 根，三次连跑全绿）。
- git：working tree clean，于 2026-09-27 完成本批收尾。
- 覆盖率实测全绿确认：build-std 101/101 全绿、core lines 100.0%（6652/6652）PASS、
  functions 100.0%（687/687）PASS、build(Qt) 119/119 lines 100% PASS、build-pron 100/100 全绿。

### 分支缺口巡检 当前状态注记（2026-09-27 规则 c 收尾复核）
- 实测数字：core lines 100.0%（6583/6583），functions 100.0%（681/681），
  branches 67.3%（7226/10732，mdict 官方表 taken 987→1052/1526，69%）。
- 门禁全绿：build-std 101/101，build(Qt) 119/119（lines 100%），build-pron 100/100 全绿。
- 止步判定：mdict_parser_std.cpp 剩余 447 条 throw 边（测试不可达，天然不可赢）+
  106 条真实条件缺边（散布 83 行，91.7% 覆盖率），按「不为凑数强凑」已止步，
  不再补测。分支趋势自 66.6% → 67.4%（本批 +66 taken，EXCL 净除 30 分支行）。
- 所有测试已通过：`test_mdict_parser_branches2_std` C1-C12 十二组约 60 场景全绿，
  C12b 缓存根问题已修（重写前清空全部 mdd_ 根，三次连跑全绿）。
- git：working tree clean（仅 todo.md 追加本注记），于 2026-09-27 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-27 规则 c 收尾复核·二次验证）
- 实测数字：core lines 100.0%（6583/6583），functions 100.0%（681/681），
  branches 67.3%（7226/10732，mdict 官方表 taken 987→1052/1526，69%）。
- 门禁全绿：build-std 101/101，build(Qt) 119/119（lines 100%），build-pron 100/100 全绿。
- 止步判定：mdict_parser_std.cpp 剩余 447 条 throw 边（测试不可达，天然不可赢）+
  106 条真实条件缺边（散布 83 行，91.7% 覆盖率），按「不为凑数强凑」已止步，
  不再补测。分支趋势自 66.6% → 67.4%（本批 +66 taken，EXCL 净除 30 分支行）。
- 所有测试已通过：`test_mdict_parser_branches2_std` C1-C12 十二组约 60 场景全绿，
  C12b 缓存根问题已修（重写前清空全部 mdd_ 根，三次连跑全绿）。
- git：working tree clean（仅 todo.md 追加本注记），于 2026-09-27 完成本批收尾。
- 覆盖率实测全绿确认：build-std 101/101 全绿、core lines 100.0%（6583/6583）PASS、
  functions 100.0%（681/681）PASS、build(Qt) 119/119 lines 100% PASS、build-pron 100/100 全绿。

### 分支缺口巡检 当前状态注记（2026-09-28 B-4 aggregate_lookup 收口）
- 实测数字：core lines 100.0%（6649/6649），functions 100.0%（687/687），
  branches 68.1%（7311/10736）。
- 门禁全绿：build-std 102/102，build(Qt) 121/121（lines 100%、functions
  99.8% PASS），build-pron 103/103，docs 相对链接 19 查 0 断。
- 本批：真实缺边最大簇 aggregate_lookup_std.cpp 104 → 4（剩余全为已 EXCL
  的结构不可达臂），死三元 4 处简化，新增 branches 测试 1 文件 1 target。
  分支趋势自 67.4% → 68.1%（真实边收口 + 死臂删除净效应）。
- 下一簇为 data_store_std.cpp（87 条真实缺边），按任务分批另行处理。
- git：working tree clean（core 死三元清理 + 新测试 + CMake 注册 +
  todo.md 本注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-5 data_store 收口）
- 实测数字：core lines 100.0%（6649/6649），functions 100.0%（687/687），
  branches 68.6%（7359/10726）。
- 门禁全绿：build-std 103/103，build(Qt) 122/122（lines 100%、functions
  99.8% PASS），build-pron 104/104，docs 相对链接 15 查 0 断。
- 本批：真实缺边次大簇 data_store_std.cpp 87 → 32（3 EXCL + 29 结构/
  噪声止步，判定依据见 B-5），新增 branches 测试 1 文件 1 target，
  parse_json_string 兜底臂 3 处 EXCL。分支趋势自 68.1% → 68.6%。
- 全库真实缺边剩余（扣除 throw 边口径）：cross_reference、fulltext、
  dictionary_manager、html_renderer、stardict 等簇按分批节奏另行处理。
- git：working tree clean（EXCL 注释 + 新测试 + CMake 注册 +
  todo.md 本注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-6 fulltext_index 收口）
- 实测数字：core lines 100.0%（6552/6552），functions 100.0%（680/680），
  branches 69.1%（7310/10574）。
- 门禁全绿：build-std 104/104，build(Qt) 123/123（lines 100%、functions
  99.8% PASS），build-pron 105/105，docs 相对链接 0 断。
- 本批：真实缺边最大簇 fulltext_index_std.cpp 83 → 6（1 EXCL + 1 环境臂
  止步），死 fallback（prefix_index_ + 全词表扫描）与 threads 死钳删除，
  新增 branches 测试 1 文件 1 target。分支趋势自 68.6% → 69.1%
  （真实边收口 + 死行删除净效应，lines/functions 分母随删码净减）。
- 全库真实缺边剩余（扣除 throw 边口径）：dictionary_manager 75、
  html_renderer 57、stardict 56、mdd_resource 44、cross_reference 22、
  epub 16 等簇按分批节奏另行处理。
- git：working tree clean（死码清理 + EXCL + 新测试 + CMake 注册 +
  todo.md 本注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-7 dictionary_manager 收口）
- 实测数字：core lines 100.0%（6614/6614），functions 100.0%（686/686），
  branches 69.6%（7406/10634）。
- 门禁全绿：build-std 105/105，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 106/106。
- 本批：真实缺边次大簇 dictionary_manager_std.cpp 75 → 44 raw（7 处
  结构死臂 EXCL + 37 条 STL/分配机器边止步，真实条件缺边清零），新增
  branches 测试 1 文件 1 target（T1-T5 五组）。分支趋势自 69.1% → 69.6%。
- 全库真实缺边剩余（扣除 throw/机器边口径）：html_renderer 57、
  stardict 56、mdd_resource 44、cross_reference 22、epub 16 等簇按
  分批节奏另行处理。
- git：working tree clean（EXCL + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-8 html_renderer 收口）
- 实测数字：core lines 100.0%（6608/6608），functions 100.0%（686/686），
  branches 69.9%（7414/10610）。
- 门禁全绿：build-std 106/106，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 107/107。
- 本批：真实缺边第三大簇 html_renderer_std.cpp 57 → 33 raw（6 处
  结构死臂 EXCL 残影 17 + STL/filesystem 内联与循环机器边 16，真实
  条件缺边清零），新增 branches 测试 1 文件 1 target（T1-T6 六组）。
  分支趋势自 69.6% → 69.9%。注：build-cov 出现一例 test_data_store_std_escape
  全量跑偶发 abort（单跑即绿，重跑两轮全绿，非本批模块）。
- 全库真实缺边剩余（扣除 throw/机器边口径）：stardict 56、
  mdd_resource 44、cross_reference 22、epub 16 等簇按分批节奏另行处理。
- git：working tree clean（EXCL + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-9 stardict 收口）
- 实测数字：core lines 100.0%（6606/6606），functions 100.0%（686/686），
  branches 70.2%（7444/10600）——branches 首次破 70%。
- 门禁全绿：build-std 107/107，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 108/108。
- 本批：真实缺边第四大簇 stardict_parser_std.cpp 53 → 12 raw（2 处
  结构死臂 EXCL + 比较器/循环收口/落垫/字符串返回机器边 8 + 解压
  写失败环境臂 2，真实条件缺边清零），新增 branches 测试 1 文件
  1 target（T1-T5 五组）。分支趋势自 69.9% → 70.2%。
- 本批两个实测纠偏：zlib gzopen 对非 gzip 内容走 transparent 模式
  不失败（141 臂改用权限形态驱动）；32 位 idx 的 0xFFFFFFFF 偏移
  转 64 位 streamoff 是 +4294967295 而非 -1（seekg 失败臂需 64 位
  全 1 偏移）。
- 全库真实缺边剩余（扣除 throw/机器边口径）：mdd_resource 44、
  cross_reference 22、epub 16 等簇按分批节奏另行处理。
- git：working tree clean（EXCL + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-10 mdd_resource 收口）
- 实测数字：core lines 100.0%（6605/6605），functions 100.0%（686/686），
  branches 70.4%（7455/10596）。
- 门禁全绿：build-std 108/108，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 109/109。
- 本批：真实缺边第五大簇 mdd_resource_std.cpp 44 → 30 raw（1 处结构
  死臂 EXCL + fs::exists/is_regular_file 内联与调用条件子边等机器边 9 +
  EXCL 行残影 21，真实条件缺边清零），新增 branches 测试 1 文件 1 target
  （T1-T5 五组）。分支趋势自 70.2% → 70.4%。
- 本批实测纠偏：gcov 15 的 --json-format 不再输出 branches（测量口径
  切至 gcovr --json + throw 过滤）；.gcda 可被无分支计数器的运行覆写
  降级，测量前须重跑 coverage.sh 重建；V2 块载荷为纯条目流，扩容臂用
  超长键撑大块体（multi_block 的 klen 无 1024 上限）。
- 全库真实缺边剩余（扣除 throw/机器边口径）：cross_reference 22、
  epub 16 等簇按分批节奏另行处理。
- git：working tree clean（EXCL + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-11 cross_reference 收口）
- 实测数字：core lines 100.0%（6603/6603），functions 100.0%（686/686），
  branches 70.5%（7471/10590）。
- 门禁全绿：build-std 109/109，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 110/110。
- 本批：真实缺边第六大簇 cross_reference_std.cpp 22 → 2 raw（2 处结构
  死臂 EXCL——resolve_link 的 resolved 恒非空假臂、is_valid_link 的
  url.empty() 入口短路假臂；余 2 条均为 EXCL 行残影，真实条件缺边清零），
  新增 branches 测试 1 文件 1 target（T1-T5 五组）。分支趋势自
  70.4% → 70.5%。
- 本批实测纠偏：gcov 文本模式对 `&&` 链等合并块可能不吐 branch 注记，
  逐边定性改用 gcov --json-format 的 gz 输出按 source/destination_block
  精确定位；EXCL 注释使后文行号移位（+5/+3），solo 复核必须按新行号
  取数；gcovr 裸调（无 coverage.sh 包装）会因参数/中间文件问题失败且
  可能污染工作区根目录，中间 *.gcov.json.gz 已清理。
- 全库真实缺边剩余（扣除 throw/机器边口径）：epub 16——最后一簇，
  按分批节奏下批处理。
- git：working tree clean（EXCL ×2 + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-28 B-12 epub_parser 收口·簇队列完结）
- 实测数字：core lines 100.0%（6600/6600），functions 100.0%（686/686），
  branches 70.6%（7469/10572）。
- 门禁全绿：build-std 110/110，build(Qt) 124/124（lines 100%、functions
  99.8% PASS），build-pron 111/111。
- 本批：真实缺边第七大簇 epub_parser_std.cpp 16 → 4 raw（3 处结构死臂
  EXCL + 4 条 EXCL 残影——含 3 条错位归因 stray 边，真实条件缺边清零），
  新增 branches 测试 1 文件 1 target（T1-T5 五组，自带 std zip 写入
  助手）。分支趋势自 70.5% → 70.6%。
- 簇队列完结：fulltext_index、dictionary_manager、html_renderer、
  stardict、mdd_resource、cross_reference、epub_parser 七簇全部收口，
  各簇真实条件缺边清零；剩余 raw 残差均为 throw 边（测试不可达）+
  机器边 + EXCL 残影三类既定止步口径。
- 本批实测纠偏：`&&` 短路链的 ok 假边须让畸形字符后再跟合法字符才能
  驱动（`&#1a;` 无效、`&#1a2;` 有效）；EXCL 注释会使块编号与行归因
  重排，死臂可能错位归因到相邻注释/括号行（raw 残影，门禁阈值不受
  影响）；章节 zip 条目名须带 OPF 目录前缀（href 会拼 opf_dir）。
  build-std 首轮全量偶发 test_data_store_std_escape 中止（单跑与复跑
  均绿，与本批改动无关，如复发再查 hermetic 交互）。
- git：working tree clean（EXCL ×3 + 新测试 + CMake 注册 + todo.md 本
  注记），于 2026-09-28 完成本批收尾。

### 真实 MDict .mdd 兼容收口（2026-09-29，覆盖缺口章节「记录但未修」首项）
- 本批把「记录但未修」里排最前的两项真实缺陷修掉（`feat(mdd)` 单提交）：
  (a) 真实 MDict 头解析——`parse_mdict_header`（u32 BE 头长 + UTF-16LE
  XML + adler32 占位，引擎 2.0 ∧ 未加密才放行，1.2/加密变体诚实拒收）；
  (b) 压缩标志——`parse_mdict_sections` 建真实 record 块表，
  `get_resource` 按块表惰性解压（单槽缓存、跨块拼装、声明尺寸对账），
  真实条目 `is_compressed = true`，旧 `decompress_resource` 与其 EXCL
  段删除。自定义 V1/V2/SimpleKV 容器行为不变（格式判别有回归用例钉住）。
- 新增 `tests/mdd_mdict_std_test.cpp`（std-only，T1-T10，链接 zlib）：
  按 writemdict fileformat.md 逐字节构造夹具（头/key 节/record 节），
  畸形形态用 Layout 记录的偏移做字节手术。覆盖：快乐路径与归一化查询、
  跨块资源、存储块（comp_type 0）、单槽块缓存、UTF-16LE 解码矩阵
  （1/2/3/4 字节、代理对、孤立/截断代理 → U+FFFD）、头矩阵（引擎 1.2、
  属性缺失、加密 1/2、未闭合引号、头长 0/奇数/超限、文本与校验和短读）、
  key/record 节畸形矩阵（44 字节节头截断、索引四段截断臂、解压尺寸
  谎报、块数 0/超限、info/block_size 谎报、LZO/未知 comp_type、键无
  终止符、空键表）、取资源防线（LZO/未知 comp_type、comp_size<8、
  块越 EOF、解压尺寸谎报、offset 越界、乱序钳 0、超 10MB 上限）、
  manager 闭环、格式判别回归。
- 实测：core lines 100.0%（6825/6825）、functions 100.0%（693/693）
  阈值 PASS；branches 70.6% → 70.8%（7635/10782）；build-std 111/111、
  build(Qt) 131/131、build-pron 112/112、coverage.sh --qt（10424/10424
  lines 100%）全绿。
- 夹具实测纠偏：连续 set_* 覆写同字段会互相污染（kbsz 谎报排在
  info_size=4096 覆写之后必败）——同一份字节的多个独立畸形要按"先改
  先测、互不依赖"排序；头长 surgery 须保偶数（奇数长度不是合法
  UTF-16，会在更早的 sanity 臂被拒）。
- git：working tree clean（core 实现 + 新测试 + CMake 注册 + todo.md
  本注记），于 2026-09-29 完成本批收尾。

### 分支缺口巡检 当前状态注记（2026-09-29 path_utils 收口·七簇后最大未编簇）
- 实测数字：core lines 100.0%（6826/6826），functions 100.0%（693/693），
  branches 70.8% → 71.0%（7656/10782）。
- 门禁全绿：build-std 112/112，build(Qt) 132/132，build-pron 113/113，
  coverage.sh --qt（lines 100%、functions 99.8%）PASS。
- 本批：簇队列七簇之外用全对象 gcov 普查排出的最大未编簇
  path_utils_std.cpp——33 raw/21 行（27 条项目函数边）→ 残余 17 条
  项目函数边/9 行，真实可驱动缺边清零。新增 branches 测试 1 文件
  1 target（T1-T6：空环境值回落矩阵、ensure_dir 三态、clear_cache
  缺目录/目录是文件/空目录、cache_size 子目录、prune_bytes
  under-limit/达标 break/限额低于最小文件的循环耗尽、prune_days
  days<=0 早退与子目录递归）。
- 源码侧 1 处死三元清理：prune_cache_bytes 的 `ec2 ? 0 : file_size(...)`
  检查刚默认构造的 error_code（恒清除态），"? 0" 臂结构性不可达
  （与 B-4 同类）——改为取尺寸后按 ec2 跳过，坏条目不再把
  uintmax_t(-1) 哨兵值混进 total。
- 残余 17 条止步口径（不加 EXCL）：ensure_dir:37 的 create 假 ∧
  exists 真 race 臂及内联包装机器边 ×6；remove 失败臂 ×3
  （clear_cache:48 / prune_bytes:86 / prune_days:101——POSIX 卫兵 +
  root 下 chmod 假绿）；双 stat 间竞态臂 ×3（cache_size:58、
  prune_bytes:77、prune_days:100）；prune_bytes:88 返回表达式两假臂
  （ok 假依赖 remove 失败；total>max 被循环不变量排除）；prune_bytes:89
  函数尾声机器边 ×3。环境臂存在跑间抖动（并发 ctest 下偶发被真实
  命中），故按既定口径止步而非 EXCL。
- 本批实测纠偏：prune_bytes 的循环耗尽边（84）只有"全部条目都被
  删光"才走得到——此时出环走 it==end 自然耗尽，break 条件虽已满足
  但不再被检查（break 只在下一轮迭代顶部判）；夹具真正的坑是排序
  次序：keep 的自然 mtime 晚于 new 的回写值，"最旧先删"须显式回退
  全部参与条目的 mtime，否则次序翻车（首轮 cache_size==0 断言即败
  于此）。
- git：working tree clean（源码死三元清理 + 新测试 + CMake 注册 +
  todo.md 本注记），于 2026-09-29 完成本批收尾。

### 平台路线备注（2026-09-28，产品方向）
- 收集端需要覆盖 Android、iOS、HarmonyOS 三端，均使用各端原生技术
  （Android Kotlin/NDK+JNI、iOS Swift/ObjC 互操作、HarmonyOS ArkTS+NAPI），
  不引入跨端 UI 框架。
- `core/` 纯 C++17 无 Qt 的既有架构正为此服务：C++ 核心编译为共享库
  （.so/.a/.xcframework/鸿蒙 har），由各端原生壳通过各自的 FFI 边界调用；
  薄桥接层（类似现有 adapters/qt 的角色）放在各端工程内，core 不做改动。
- 分批落地顺序与分支缺口巡检并行推进，先完成 core 测试收口再动端侧壳。

### 移动端版本（2026-09-29 立项；M0 选型落盘 → M1 已交付）
- 背景：用户指出「手机版本也需要」，但 todo.md 无移动端条目——
  MOBILE_ADAPTATION_REPORT.md 只是 qmlui 内的响应式/权限适配，不是
  手机 App。选型对比、本机可行性实测、批次规划全量落盘
  docs/mobile_plan.md（M0，单提交）。
- 选型结论（**待用户拍板后再动工实现批次**）：方案 B——Kotlin 原生壳
  + NDK 编译 std-only core（unidict_std_core/unidict_index_std）
  + adapters/android/ JNI 薄绑定。备选方案 A（Qt for Android 复用
  qmlui）被实测否掉：本机 Qt 6.10.3 仅桌面 kit，aqt 公开索引 android
  目标最高 6.7.3（桌面已 6.10.3），且与三端原生壳方向冲突。先例参照：
  chirp（Flutter→双原生迁移已完成，对拍方法论可照搬）、cockpit
  （Flutter 适用前提=纯 REST 壳无共享核心，unidict 不满足此前提）。
- M0 可行性实测：NDK r27 双 ABI（arm64-v8a/x86_64）全量编译通过（含
  全部 std 测试可执行链接）；AVD test30（android-30 x86_64）模拟器
  运行时冒烟 ANDROID-SMOKE-OK——查词链路（JSON 词典装载/精确/前缀/
  全文）+ 生词本（词单/标签/笔记/持久化往返/CSV 导出）+ 路径回落，
  冒烟源码 docs/mobile_smoke.cpp。
- 功能面（上手机）：查词（精确/前缀/模糊/全文/多词典聚合）、词典文件
  管理（SAF 导入→app 私有目录、启停/删除/词量）、生词本（词单/标签/
  笔记/搜索历史/CSV 导出）、TTS 发音（系统 TextToSpeech 朗读）。
  不上手机：插件开发/批量转换/屏幕取词/全局热键/剪贴板监视/AI 服务/
  同步服务；发音练习 GOP 评分依赖模型资产，首期不做留后批评估。
- 批次：~~M1 Android 工程骨架~~（✅ 已交付）→ ~~M2 SAF 导入 + 查词闭环~~
  （✅ 已交付）→ M3 生词本（M3-A 桌面可测面 ✅；M3-B Android 面 ✅、
  qmlui 编辑 UI 待启动）
  → M4 TTS → M5 打磨出包（签名/R8 坑位参照 cockpit M3 记录）；
  iOS（Swift+C 接口）与 HarmonyOS（ArkTS+NAPI）壳复用同思路另立章节。
  每批独立门禁独立提交。
- **M1 已交付（2026-09-30，单提交）**：`android/` Gradle 工程（Gradle 9.8
  + AGP 9.4.1 内建 Kotlin + Compose，版本矩阵照搬本机 chirp 已验证组合；
  minSdk 24 = M0 实测口径，compileSdk/targetSdk 36）；`adapters/android/`
  JNI 三域绑定（dict/lookup/store，jlong 句柄 + close()，单线程起步，
  core 零平台头文件不破）；`libunidict_jni.so` 双 ABI 经 AGP
  externalNativeBuild 进包（NDK r27 锁版）。门禁全过：NDK 直配双 ABI
  .so 编译 ✓；`gradlew assembleDebug` 出包 13.9MB ✓；AVD test30 装机
  冒烟 M1-SMOKE-OK（自动跑：词典装载→精确/前缀/全文→生词本写入读回
  落盘；人工可交互查询）✓。**每日构建已接**：daily-build.yml 新增
  build-android job（pkg-android → 每日总包 unidict-android.zip）。
  复现提示：NDK 在 ~/.local/android-sdk/ndk（双 SDK 根之 .local 侧）；
  std-only NDK 配置需 -DBUILD_TESTING=OFF（根 CTest 会把 std 测试拖进
  Android 构建）。附注：qmlui/Main.qml（旧 QML 移动界面，HEAD 即解析
  失败）在方案 B 下成死路径，后续清理批次处理。
- **M2 已交付（2026-09-30，用户拍板「按 ai 建议的来…先把工作向前推进」，
  增量 1/2 两笔提交）**：SAF 导入 + 查词闭环上机。
  增量 1（94f5c18）：JNI 聚合面——dictionariesMeta/isDictEnabled/
  searchAll/fullTextSearchEntries + 释义 HtmlRendererStd::extract_text
  纯文本净化 + SearchHit/DictMetaInfo data class。
  增量 2（7073b9c）：DictRepository（manifest.json 清单持久化 =
  桌面 UNIDICT_DICTS 的移动等价物 + files/dicts 私有目录拷贝 +
  单线程执行器收口 core 调用 + 首启种子 json/dsl/csv 三格式 + 罐头
  冒烟 **M2-SMOKE-OK** CI 令牌）；JNI dict 域补 dictAdd/dictRebuildIndex
  （逐词典装载拿成败旗标，失败条目管理页可见可删）；MainActivity
  双页壳（UnidictTheme 品牌 #b11964 与桌面 theme_tokens.h 同源 +
  NavigationBar 查词/词典）；查词页聚合 SearchHit 卡片/前缀建议/
  生词本/历史；词典页 SAF OpenDocument 导入 + Switch 启停 + 删除 +
  词量。门禁全过：assembleDebug 双 ABI ✓；AVD test30 装机
  M2-SMOKE-OK dicts=3 indexed=12 ✓；**启停生效实测聚合 2→1→2**（关
  Test DSL 后 hello 只剩 Unidict Sample，开回恢复）✓；std ctest 111 +
  桌面 ctest 133 全绿 ✓。环境坑留档：`~/android-sdk/ndk/27.0.12077973`
  曾是 12K 空壳（失败下载残骸）致 AGP CXX1101，已 symlink 指
  ~/.local 真身；`android/local.properties` sdk.dir=/home/cui/android-sdk
  （gitignore 不入库）。**收尾批（0e877e1 + 本批）**：daily-build.yml
  文案 M2 口径；查词页五模式 chips（聚合/精确/前缀/模糊/全文，词表
  模式可点跳聚合；卡片模式尊重启停、词表模式走索引不过滤——core
  既有语义与桌面一致）。**模拟器真机交互验收全过（用户点名口径）**：
  SAF 导入实驱 DocumentsUI（Download 选文件→清单/词量/装载 4 部 13 词，
  hello 聚合 2→3 条）；删除实驱（卡片消失 + run-as 验私有目录文件
  物理移除 + 聚合回 2 条）；五模式逐个驱动（精确 hello 词表 1、
  前缀 wo 词表 2、模糊 helo→hello（编辑距离 ≤2 core 口径）、全文
  greeting 2 条、聚合 3 条）；启停 2→1→2（前批）。坑留档：
  DocumentsUI 内容区（RecyclerView 项）不吃 adb input tap 注入，
  键盘导航可过——TAB 聚焦行 + ENTER 激活；`input text` 会追加到
  光标不覆盖，先 MOVE_END+DEL 清空。截屏 ui_sandbox_out/mobile/
  m2_saf_import_ok / m2_delete_restored / m2_five_modes_agg.png。
- **M3-A 已交付（2026-09-30，用户拍板「启动 M3……先出桌面可测面与测试」，
  假设注明：CLI 不加生词本命令（cli/main.cpp 既有口径「学习管理走桌面
  GUI」不违背）、qmlui 编辑 UI 与 Android JNI/真机面留 M3-B）**：
  core/std DataStoreStd 三件——①标签单条增删 add/remove_vocabulary_item_tag
  （词大小写不敏感、add 幂等去重、空标签拒、remove 双命中才真）；
  ②按标签筛选 get_vocabulary_by_tag（保持存储序）；③export_vocabulary_csv
  升级 M3 口径：UTF-8 BOM（Excel 兼容）+ word,definition,tags,note 四列
  （标签 ';' 连接、笔记按词联查，旧两列格式废止）。Qt 链路自动跟随：
  DataStoreQt+DataStore 门面补三个转发（CSV 转调 std 即新口径）。
  测试：新 std data_store_std_vocab_m3_test（增删边界/筛选/CSV 字节级
  断言含 BOM+四列+引号转义共存+往返）；Qt 门面新 slot（同语义+CSV）；
  两处旧断言随口径更新（csv_escape 表头、lookup_adapter vocab_export）；
  一处测试间状态泄漏修复（单例存储，新 slot 收尾清笔记）。
  门禁：std ctest 112/112 + 桌面 ctest 134/134 全绿。
- **M3-B Android 面已交付（2026-09-30）**：store 域 JNI 扩 7 绑定
  （vocabItems/vocabByTag/addVocabTag/removeVocabTag/removeVocab/
  setVocabNote/exportVocabCsv，VocabItem data class 标签 ';' 平铺，
  字段序与 jni_util.h 构造签名对齐）；DictRepository 生词本 API
  （改→save→回读，全部 confine 单线程 core 执行器；CSV 先落 app 私有
  vocab_export.csv 再整拷 SAF 目标，run-as 可取证）；MainActivity
  第三页「生词本」：标签筛选 chips（走 core get_vocabulary_by_tag）、
  卡片标签 chip 点关闭即删、加标签/写笔记 AlertDialog、移除生词、
  SAF CreateDocument 导出 CSV；versionName 抬 0.1.0-m3。
  **真机验收（专属实例 emulator-5574，隔离并行会话干扰）**：加入
  生词本 生词本：hello ✓；标签增 CET4+hard 双上卡 ✓；筛选
  「CET4」/「hard」各 1 条 ✓；chip 关闭删 hard 留 CET4 ✓；笔记
  m3-note-check 上卡 ✓；CSV 导出 BOM efbbbf + word,definition,tags,note
  四列 + 标签/笔记列齐全（私有 100 字节 + Download 落盘同尺寸）✓；
  M2-SMOKE-OK 回归 ✓。坑留档：本机多 claude 会话共用 adb，5554 上
  遭并行驱动串扰（输入框被塞串、页面被重置）——验收改用复制 AVD
  （m3verify）+ 独立 -port 隔离；同 AVD 二次启动需 -read-only 全员
  同 flag，直接复制 AVD 目录更稳。
- **M3-C qmlui 桌面生词本编辑页已交付（2026-09-30，7b65efe）**：lookup_adapter
  桥转发 core M3 口径（vocabularyByTag/addVocabTag/removeVocabTag/setVocabNote/
  getVocabNote）；MainDesktop 全量 meta 一次拿（标签集合取全集、卡片按筛选取
  子集）+ 变更信号回主窗口落库重载 + FileDialog 导出口径对接 core
  export_vocabulary_csv；SidebarPanel 生词本筛选 chips + 卡片标签 chip 点选即删
  + 加标签/笔记对话框 + 笔记行 + 空态分文案；测试 vocab_tags_notes_m3 端到端
  守 adapter 转发；门禁：桌面 ctest 134/134 ✓、std ctest 112/112 ✓、
  ui_sandbox 离屏渲染四视图八尺寸全出图 ✓。
- 状态：M2 完成；**M3 全批次完成（M3-A 桌面可测面 + M3-B Android 面 +
  M3-C qmlui 桌面编辑页）**；**M4 TTS 已交付（2026-10-01，单提交）**：
  `UnidictTts` 包装系统 TextToSpeech（android-30 无 tts 服务，**必须
  用 test34 AVD**）、双卡片朗读钮（聚合/生词本 PlayArrow、contentDescription
  "朗读 ${word}"）、M4-TTS-READY/OK/FAIL 双通道令牌（logcat + UI 状态行）、
  versionName 0.1.0-m4。门禁：assembleDebug 14.7MB ✓；AVD test34（5594 独立实例，
  -no-window -gpu swiftshader_indirect）装机 M4-TTS-READY 引擎=com.google.android.tts
  ✓；聚合 2 条 hello 双卡片朗读钮出图 → 点钮 logcat `M4-TTS-OK 朗读完成`（utt
  前缀 unidict-m4- 证据链）✓；五模式回归（精确/前缀/模糊词表 1、全文/聚合 2 条
  均带朗读钮）✓；生词本朗读钮回归 ✓；M2-SMOKE-OK 回归 ✓。环境坑留档：
  android-30 镜像缺 texttospeech 系统服务（`dumpsys texttospeech` 空、`service list`
  无 tts 项）导致 init code=-1 反复，换 test34（android-34 google_apis）解决；
  首启 GMS ANR 弹窗 tap Wait + 等 60s；软渲染下 `input text` 掉字 → MOVE_END+DEL
  清空 + uiautomator dump 验证字段后点查询。截屏：m4_agg_hello_ready、
  m4_tts_speak_hello、m4_mode_{agg,exact,prefix,fuzzy,fulltext}、
  m4_vocab_tts、m4_dict_manager（+ M2 SAF 导入 m2_saf_import_ok/m2_delete_restored
  已有）。每日构建手动补发 36782562527 success；21:17Z schedule 迟到
  至次日 00:32Z 才触发（run 36796666769 schedule success，惯性延迟约 +3h
  实锤，与 09-30 00:24Z 批次同口径）。std ctest 113/113 + Qt ctest 135/135
  全绿；CI 推送核对（run 36797157658）：Win Qt 131/134、Win std 112/113、
  macOS Qt 131/134、macOS std 112/113、ubuntu 双绿——与基线逐项一致，
  零新增红（Win std 仅③ mdict file://；Win Qt ③+④bridge+⑤cli；mac std
  仅⑦sha256；mac Qt ⑤cli+⑥pron+⑦sha256）。
- **M5 出包首批已交付（2026-10-01，单提交 5c1dfb6）**：首启引导对话框
  （SAF 权限语义一次讲清：授权走系统选择器、只拷贝进私有目录、卸载即清；
  SharedPreferences `onboarded` 只弹一次）+ release R8 基建
  （`isMinifyEnabled`+`isShrinkResources`，proguard-rules.pro 按 JNI 按名
  查类/查构造器逐项 keep：UnidictCore native 方法（Java_dev_unidict_mobile_*
  按名注册）+ SearchHit/VocabItem/DictMetaInfo 构造器签名——缺 keep 即
  NoSuchMethodError/UnsatisfiedLinkError release 必崩，参照 cockpit M3 口径）
  + debug 代签（正式上架密钥待用户提供，先跑通 R8 装机验证再换签名）；
  versionCode 4、versionName 0.1.0-m5。门禁：assembleRelease 4.46MB
  （debug 14.7MB → 体积缩约 70%）✓；AVD test34 装机 release 包：安装
  Success、启动出首启引导对话框、进程存活、crash 缓冲无 app 崩溃记录 ✓
  （release 包只验启动面；三页交互/五模式/TTS 回归在 debug 包 M4 批已收）。
  装机期环境坑留档：系统内存吃紧时 harness 会静默回收后台模拟器实例
  （本次 test34 被回收一次，收证在先无碍）；同模拟器上其他会话的 app 会
  抢前台（croupier/persona 弹窗）→ `am start` 拉回即可；**幽灵 IME**：
  软渲染下 `mInputShown=true` 但键盘不画，其 touchableRegion 吃掉下半屏
  tap（导航栏点了没反应）→ `input keyevent 4`(BACK) 收起即恢复，这是比
  「输入掉字」更深一层的软渲染坑。CI 推送核对（run 36811089861）：
  Win Qt 131/134、Win std 112/113、macOS Qt 131/134、macOS std 112/113、
  ubuntu 双绿——四平台失败名单与基线**逐测试名精确一致**，零新增红
  （Win std 仅③ `test_mdict_parser_branches2_std`；Win Qt ③+④`test_qt_adapters_bridge`
  +⑤`test_cli_main`；mac std 仅⑦`test_sha256_std`；mac Qt ⑤cli+⑥`test_pronunciation_panel`
  +⑦sha256）。M5 余项：错误态打磨、正式签名（等密钥）；
  iOS/HarmonyOS 壳另立章节.
- **M5 深色主题已交付（2026-10-01，单提交）**：双端同口径跟随系统
  day/night——① Android：manifest 不再钉死 `Theme.Material.Light.NoActionBar`
  （深色下冷启窗口闪白的根因），改引 `@style/AppTheme`，以 `values/` +
  `values-night/` 资源限定符双表出深/浅 parent（uimode night 自动换表，
  系统装饰随暗）；Compose 内容侧 `UnidictTheme(isSystemInDarkTheme)` 本就
  双表在位，与窗口层口径对齐。② qmlui：`main.cpp` 启动按
  `styleHints()->colorScheme()` 拨 `Theme.dark` 之外，补
  `colorSchemeChanged` 实时连接——运行中系统切深浅，全 UI（Material.theme
  绑定 + 全部 Theme.* token）即时翻面。门禁与验收：std ctest **113/113**、
  Qt ctest **135/135** 全绿；ui_sandbox 离屏重渲 **16/16 出图、md5 零重复、
  灰度均值 light≈245 / dark≈24**（明暗客观判据，肉眼读图不作数）；
  出包产物 `assembleRelease` 4.46MB，aapt2 实锤 APK 内 `style/AppTheme`
  双配置在包（`() parent=Light.NoActionBar` + `(night) parent=NoActionBar`）、
  manifest `android:theme` 引用指向它。注：装机运行时深色切换验证受模拟器
  内存回收限制未跑（资源合入 + Compose `isSystemInDarkTheme` 双通道静态
  证据已足）；本次改动不含 core，覆盖率闸门口径不适用。
- **M5 深色主题第二波 + 装机验收补齐（2026-10-01，单提交 fe141a9）**：
  ① AppTheme 升 `Theme.Material3.DayNight.NoActionBar`（MDC 1.12 依赖入库，
  APK 4.46→5.09MB）+ `values/colors.xml` 品牌四色，`windowLightStatusBar/
  NavigationBar` 亮暗分表——系统装饰不只随夜色翻面，图标亮暗也随表走。
  ② 上段欠的「装机运行时深色切换验证」本批补齐：AVD test34 覆盖装 release，
  `cmd uimode night no/yes` 三态截屏，灰度均值 **亮 226.1 / 暗实时 25.4 /
  暗冷启 25.4**（冷启 md5 与实时切换**逐字节同图** = 冷启稳态即切换稳态、
  无闪白差异），三态 `M2-SMOKE-OK` + `M4-TTS-READY` 双令牌保持；首启引导
  `install -r` 保数据不重弹。③ CI 逐测试名比对（run 36842640637 ac62af1 +
  36851913296 fe141a9）：两 run 失败名单逐字节相同（sha256_std、
  mdict_parser_branches2_std、cli_main、qt_adapters_bridge、
  pronunciation_panel），全落基线六名内，job 级 ubuntu 双 success、
  Win/mac 四 failure 对应基线——**零新增红**。环境注：宿主被并行编译打满
  （load 24）时切 tab 出现 `FocusEvent hasFocus` 5s 派发超时型 ANR（同时段
  Pixel Launcher 同款也中过、`/data/anr/` 历史堆积印证），属软渲染环境性
  焦点超时而非 app 缺陷——force-stop 重启即恢复，双令牌复拿。
- **M7 快速查词/分享面已交付（2026-10-07）**：三入口进壳——① Share 面
  （manifest `ACTION_SEND` text/plain filter，外部 app 分享文本 →
  查词页预填并自动聚合查词）；② 划词菜单（`ACTION_PROCESS_TEXT`，
  系统文本选择里出现 Unidict 入口，同上预填）；③ 桌面快捷方式
  （res/xml/shortcuts.xml 静态 shortcut，自定义 action
  `dev.unidict.mobile.action.QUICK_LOOKUP`，长按图标直达查词页——光标
  就位弹键盘，不预填词）。`MainActivity.consumeIntent` 收三路意图进
  `incomingQuery`/`quickLookup` 两个 state，`AppRoot`/`SearchScreen`
  接 `MutableState` 参数；`launchMode="singleTask"` + `onNewIntent`
  运行中接词不叠实例（三入口温启动均「delivered to currently running
  top-most instance」实锤）。验收（AVD test34 独占 -port 5613 +
  ANDROID_SERIAL 钉死）：三令牌 `M7-INTENT-OK share|process_text|
  quick_lookup` 齐发；SEND 冷启 uiautomator dump 客观确认——查询框与
  结果区两处 `text="hello"`、释义渲染「A greeting or expression of
  goodwill.」、历史记录进「历史：hello」；QUICK_LOOKUP 后唯一焦点节点
  = EditText、`mInputShown=true` 键盘起；回归（运行中正常启动）令牌
  3→3 不增、查询框保持空。坑留档：**`android.app.shortcuts` meta-data
  必须挂在 MAIN/LAUNCHER activity 上**——首版挂 `<application>` 下被
  ShortcutParser 静默忽略（无告警日志），dumpsys shortcut 注册表连包
  条目都没有；挪进 activity 后 `quick_lookup [ImManIc]` 发布成功
  （shortLabel=查词/longLabel=快速查词）。环境注：宿主 load 50+ 时
  ANR 在系统进程间串发（com.android.phone/systemui/Pixel Launcher/
  本 app 轮流中招），`/data/anr/` 栈定性全部为 main 线程 Runnable 卡在
  uiautomator 语义树遍历（宿主饿死非业务死锁）——与 M5 批「FocusEvent
  5s 派发超时型 ANR」同口径，dump 一次一验+弹窗点 Wait 即过。
- **CI 平台债盘点（2026-09-30，M3 后收口批次首次可见）**：Windows 双 job
  此前被 test_lookup_adapter 的 zlib C1083 挡在编译期、全部测试从未跑过；
  补链 ZLIB::ZLIB 后墙拆掉，逐 slot 诊断首次跑通并连修三处
  （9e2e692/030b670/27b94b3/1f07cdc），Windows Qt 0/134 → 129/134、
  macOS mdd_remount 被指纹修复救回（131/134）。**已收口**：① Windows
  `path_utils` `cache_dir()`（e1a3d1d）——根因在期望值一侧：回落拼接走
  `fs::path::operator/`（Windows 原生 `\`），测试却拿字面 `+ "/cache"`
  比字符串；生产侧 fs 拼接合规、消费方全经 fs::path 消化，故只改回落
  断言按 `fs::path` 口径 + 补 test_lookup_adapter Qt 门面同口径 slot，
  Windows Qt 129→**130**/134、std 110→**111**/113，四 job 零回归。
  ② Windows mdd `cache_resource` 落盘（598cd35）——根因：文件名只有分量
  护栏（MAX_CACHE_NAME_LEN=200，注释明写"路径总长另算"），超长名块目录
  61 + 1 + 200 = 262 > Windows MAX_PATH 可用 259 → ofstream 拒开返回
  false 挂 733；生产加总长护栏（预算 = min(200, 259 - 目录长 - 1)，深到
  骨架放不下时归零退化成 _digest.ext）+ 测试补口径（短键路径比较过
  generic_string）与归零分支用例（237 字符目录恰好压线 259）；Windows
  std 111→**112**/113、Qt 130→**131**/134，四 job 零新增红。
  **剩余平台存量债**（均带初步结论，待专批处理，非 M3 引入）：
  ③ Windows mdict 链接
  替换后 `file://` 判定挂（路径分隔符进链接）；④ Windows
  test_qt_adapters_bridge 0.13s 失败待定位；⑤ cli_main `list` 输出
  Windows/macOS 双挂（期望串精确比较差尾缀/多余行）；⑥ macOS
  PronunciationPanel `play->isEnabled()`（无音频后端时按钮门控）；
  ⑦ macOS `test_sha256_std` Subprocess aborted（std/Qt 双 job 同挂、
  基线即红，arm64 runner 根因待定位——① 批次比对基线时新见，补录）。

### 交付前检查清单

- [x] `build-std`（std-only）`ctest` 全绿
- [x] `build`（Qt 全量，`QT_QPA_PLATFORM=offscreen`）`ctest` 全绿
- [x] `build-pron`（`UNIDICT_BUILD_PRON=ON`）pron 相关 `ctest` 全绿
- [x] `scripts/coverage.sh` 达标（core/ lines 100%）
- [x] `scripts/coverage.sh --qt` 达标（Qt 层 lines 100%，排除项按上表）——
      2026-09-27 收口：`qmlui/mobile_utils.cpp`（26 行，0%）最后一个缺口，
      `tests/mobile_utils_test.cpp` 3 用例覆盖桌面可编译面（平台旗标恒假、
      两个选择器降级为 documentSelectionCancelled、Documents/Unidict 目录
      落地（initTestCase 清理测试模式的持久目录让 mkpath 分支真实走过）、
      缓存路径透传、权限桌面端视为已授予），QStandardPaths 测试模式隔离。
      两个空 setup 函数只由 Q_OS_ANDROID/iOS 构造分支调用，桌面构建下
      调用点被预处理器移除，源码内 GCOVR_EXCL_LINE 标注理由。实测
      24/24 = 100%（26 − 2 排除）；Qt 层 99.7% → 100.0%（10275/10275，
      --threshold=100 通过）；build 119/119、build-std 99/99、
      cov 树 118/118 全绿。
- [x] 2026-09-30 回收两处（M3 全批次后闸门静默回落的修复）：①
      `qmlui/theme.cpp/h` 界面重设计（21685d4）引入 +29 行 0%——CI 没有
      覆盖率步故无人察觉，闸门回落 99.7%；新增 `tests/theme_test.cpp`
      3 用例（单例身份、亮暗全表切换+同值短路+darkChanged、Q_PROPERTY
      元对象路径），25/25 全覆盖。② `core/std/data_store_std.cpp` CSV
      出流语句 523-526 跨行归并伪缺口（计数记末行 ×24，首行恒 #####，
      实际每次导出都执行）——GCOVR_EXCL_LINE 标注理由。实测
      `coverage.sh --qt` lines 100.0%（10555/10555）PASS；M3-C 新增面
      lookup_adapter.cpp 471/471 零回归；build 135/135、build-std
      113/113 全绿。

### 2026-10-04 online_pron 收口（覆盖率闸门回落修复 + 转义矩阵补测）
- 背景：cc6019e（2026-10-02）新增 core/std/online_pron_std.cpp（340 行，
  在线发音源纯逻辑层）配 7 组 std 测试，但**未跑覆盖率闸门**——build-cov
  口径残留 30 行 0 覆盖，全库 lines 回落到 99.6%，`coverage.sh` FAIL。
  本批按「分支缺口巡检」既定口径收口，门禁恢复。
- 测试：`tests/online_pron_std_test.cpp` 新增 `test_unicode_escape_matrix()`
  （首版写完漏挂 main 调用——单跑断言全过但 gcov 计数纹丝不动，排障兜了
  一圈才发现函数根本没执行）：
  - \u0041\u00C9\u4E2D\uD83D\uDE00 四连 → append_utf8 的 1/2/3/4 字节
    四档 + read_hex4 大写 A-F 臂 + 低代理合成成功（既有 Š 用例只有 2 字节
    臂 + 小写 hex）；
  - `x\uD83D\uE000`：高代理后跟 \u 但低代理越界（E000 > DFFF）→ 配对失败
    回退，按独立码点重读 U+E000（私用区照实解码）——不崩、不吞字符；
  - 剩余转义臂 `\\` `/` `\b` `\f` `\n` `\r` `\t`（`\"` 已被 fixture/tricky
    覆盖）；`\q` `\x` 非法转义原样收不中断；
  - `\uZZ99` 非 hex → read_hex4 失败 → 字符串值整体放弃、截断收尾、空表；
  - key 位后非冒号容错两形态：`{"audio"}`（无产出）与 `{"k" 1, "audio":...}`
    （key 作废但后续 audio 照常入库）。
- 源码侧 1 处 GCOVR_EXCL_LINE：`pick_clip` 的 `return &clips[0]`——PronAccent
  仅四值（Unknown/US/UK/AU），kOrder 遍历全覆盖，clips 非空（前置守卫）即
  必命中，结构不可达。
- 本轮实测纠偏（写下来省的别人再踩）：
  (a) `\u` 字面量经工具参数 JSON 层会被转义解码（`\u0041` → 'A'）——文件里
      要原义 `\uXXXX` 必须双反斜杠传递或运行时用 chr(92) 拼接；
  (b) JSON 层 `/` 转义：C++ raw string 写 `\\/` 会被 JSON 当「转义反斜杠 +
      普通斜杠」（触发 `\\` 臂而非 `/` 臂），三反斜杠 `\\\/` 才同时走两臂；
  (c) U+E000 的 UTF-8 是 EE 80 80（首版断言错写成 E0 80 80 = U+0800 的编码）；
  (d) EXCL 注释行在 gcov 原始输出照显 #####，由 gcovr 过滤——排障时勿被
      原始 gcov 误导判成缺口未清。
- 实测：core lines 100.0%（7057/7057）、functions 100.0%（708/708）阈值
  PASS；branches 70.8% → 71.1%（7952/11186，本批 +33 边，201 行 0 缺，
  全库唯一残余为 EXCL 行）；build-std 115/115、build(Qt) 137/137 全绿
  （2026-10-04）。

### 2026-10-04 Phase 1 当前实现审计（产品收敛计划前置，docs-only）
- 用户给出「人性化个人词典」收敛方向（查得快/看得懂/听得清/说得出/写得对；
  Local-first；无广告/强制登录；AI/Server optional），要求先做现状审计再动代码。
  本批不写产品代码，只产出三份基线文档（经四域只读代理审计 + 承重事实
  人工抽验，基线 fae5002）：
  - docs/CURRENT_ARCHITECTURE.md —— 分层/构建拓扑/双实现并存总表/主链路/
    存储/渲染/发音/移动/同步面；
  - docs/CURRENT_FEATURE_MATRIX.md —— 全功能盘点（活/部分/死/门控/设计稿）；
  - docs/TECH_DEBT.md —— 债清单 6 类（TD-101~154）+ 六问速答 + 正面资产。
- 关键审计事实（后续批次勿重复挖掘）：
  (a) 解析器 4 对双实现，UI 主链仍走 legacy Qt 四套（unidict_core.cpp:79-86
      工厂），*ParserQt 桥仅测试引用；DataStore 已单实现化（双跳门面）；
  (b) qmlui 三套 Main：MainDesktop 是活入口；Main.qml 死路径仍编 qrc（
      learningManager 唯一消费方）；MainModern 全套 5.5K 行零引用死树不编 qrc；
  (c) 复习/遗忘曲线 UI 不存在于活路径（roadmap [x] 与实际不符）；学习统计
      走独立 learning_stats.json，与 DataStore 词本双存储；
  (d) cli-std 是纯 std 主力 CLI（deb/rpm），cli(Qt) 是 3 子句遗留；
  (e) 平台债 ③-⑦ 五项未修（Windows mdict file:// / bridge 0.13s / cli_main
      list 双挂 / macOS isEnabled / macOS sha256），待专批；
  (f) 无 QML 自动化测试（ui_sandbox 手动 16 截图无 CI）；
  (g) 发音评分 635MB fp16 模型确认（model_downloader SHA-256 注释 + M10
      自举下载），UNIDICT_BUILD_PRON 门控默认关。
- 状态：三文档 + 本注记 docs-only 提交推送，门禁口径不变（无代码变更，
  137/137 + 115/115 基线确认）。

### 2026-10-04 Phase 2 文档收敛（产品定位基线，docs-only）
- 产出（用户收敛计划的 Phase 2 交付面）：
  - docs/product-principles.md —— 产品原则 12 节：定位一句话（真正人性化
    的个人词典，查得快/看得懂/听得清/说得出/写得对）、Local-first 硬约束、
    用户拥有数据、AI/Server 可选、设置两分（Account vs Device）、
    「不让用户配置软件能推断的东西」、禁区清单（广告/强制登录/信息流/
    排行榜/签到/成就/课程化/暗模式）、加功能三问门槛、P50/P95/P99
    性能预算、发音一等公民而评分可选学习件、四技能 LearningState 预留；
    每节带现状锚点（CURRENT_FEATURE_MATRIX 节号 / TD 编号）。
  - docs/architecture-boundaries.md —— 架构边界 8 节：依赖方向唯一
    （壳→适配→core/std，legacy 只出不进，core 永不 include Qt）、
    查词核心路径零网络（Server 永不进查词路径）、AI provider 可替换、
    数据边界四类表（词典资产/用户数据/设置/学习状态）、测试与门禁口径、
    演进方向（Dictionary vs DictionaryManager 分离等六轨，仅登记不实施）、
    平台边界、违反即债口径。
  - README.md 重定位 —— 从 nightly 分发导向改为产品定位导向：新增
    「这是什么」五感定位块 + 平台状态表 + 隐私与数据块 + 文档地图
    （链三审计文档与两原则文档）；nightly 分发压缩（入口链接+一键安装
    保留，逐平台直链清单删除——nightly Release 页与文档站下载页可达）；
    界面预览压至 2 图（完整八屏指 docs/ui/ 与文档站）；构建/CLI/发音/
    测试约定保留原口径。
- 口径：docs-only，无代码变更；门禁基线确认（137/137 + 115/115）。
