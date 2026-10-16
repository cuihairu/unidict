> 语言: **简体中文** | [English](./CURRENT_FEATURE_MATRIX.en.md)

# Unidict 当前功能矩阵

> Phase 1 · 现状基线 · 审计日 2026-10-04 · 基线提交 fae5002
> 局部修订 2026-10-12：主题（部分→活，三态手动切换）、搜索历史（+逐条删除）、词典优先级（+qmlui 行内重排）、生词本（+CLI CSV 进/出）、真点审计计数 57→121；巡检续批③：释义字号（qmlui 外观 tab 四档，ui/def_font_size）、词典移除（qmlui 词典 tab ✕ 会话面）、审计计数 121→124；续批④：开机自启 Linux/macOS 实装（XDG/LaunchAgent + qmlui 取词 tab 开关）、审计计数 124→126；续批⑤：全局热键三平台实装 F10（Windows/macOS/Linux-X11，Wayland 诚实失败）、qmlui 托盘 parity F11、全文索引重建按钮 F12、审计计数 126→134
> 局部修订 2026-10-13：§6 桌面集成三行刷新（托盘行补 qmlui TrayController 面、自启部分→活、热键部分→活三平台）；§1 增全文索引重建按钮行（F12）；§6 P-5 行补 Windows quick_lookup 热键焦点窗选区读取（F13a，UIA 触发式，空回落剪贴板）；§7 剪贴板取词（Android 壳）「未建→不做」（mobile_plan §2 明确排除：桌面语义，无移动等价物）；§1 DSL 部分→活（parser_registry_std 已注册）；§9 relay 部分→活（B1-B7 全交付）、AI 行 Live UI 面纠正（桌面 qmlui，非移动端）、插件部分→活（内建注册表为最终形态）；§10 基准部分→活（TD-134 收口）、本地加密行 roadmap 注对齐划线废止口径；§11 发音/复习两注对齐 TD-143 对账后 roadmap 现状
> 局部修订 2026-10-16：§2 文本卷叠行补三档 profile 与 qmlui 设置面（48eff28）
> 状态图例：**活** = 用户路径可达；**部分** = 可用但有缺口/仅某面；**死** = 无引用或不可达；
> **门控** = 编译开关默认 OFF；**设计稿** = 仅文档级；**未建**。

## 1. 词典格式与词典管理

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| StarDict (.ifo/.idx/.dict/.dict.dz) | 活 | core/std/stardict_parser_std（单实现；legacy 已退役） | UI 主链走 std 面（qmlui/gui 均 DictionaryManagerStd） |
| MDict .mdx / .mdd | 活 | core/std/mdict_parser_std + std/mdd_resource | sibling .mdd 自动附着、资源进渲染管线；v2 忠实读取（Encoding 码表/加密 key 块）+ UTF-16 头全量中文标题（std 面） |
| StarDict 资源（图片/音频） | 活 | std/stardict_resource_std + manager 便利方法 | .ifo res 键（目录/清单）+ 散装媒体兜底；qmlui 渲染回退链（mdd 优先） |
| MDict 加密词典 | 活 | core/std/mdict_crypto + decryptor | GUI 密码设置（[encrypted] 检测） |
| DSL | 活 | core/std/dsl_parser_std | parser_registry_std 注册 .dsl；qmlui/cli-std 经 DictionaryManagerStd 同链可用 |
| EPUB | 活 | std 面单口径（legacy 面已退役 927f376） | 真 deflate 端到端 + EPUB2 text/html/dt-dd 版式/href 编码归一（std 面） |
| JSON custom | 活 | std 面单口径（legacy 面已退役 927f376） | 环境变量/扫描目录导入 |
| CSV/TSV/plain | 活 | core/std（custom 格式族） | cli-std 面 |
| 词典优先级/分组/启用/移除 | 活 | Drawer「词典」tab + gui 工具栏分组下拉 + qmlui 行内 P↑/P↓（normalize-on-move）+ ✕ 移除（qmlui 会话面：env 驱动无状态文件，重启随 UNIDICT_DICTS 回归） | 设置可持久化（qmlui 移除除外） |
| 本地词典导出/打包（JSON） | 活 | core/std/dictionary_export_std + cli-std --export-dict | 项目自定义 JSON 格式；round-trip 全保真（json_parser 转义解码+字符串感知扫描配套） |
| 损坏词典隔离/重试 | 活 | GUI ⚠ 行 + CLI --list [FAILED] | 缺失文件自愈 |
| 全文索引状态行 + 重建按钮 | 活 | qmlui 词典 tab 状态行（按需建 / 已建 N 词条文档）+「重建」按钮（dictIndexRebuildButton，loadedDictionaries>0 才可用；F12） | reset 丢内存索引，下次全文检索惰性重建；审计 S35 三查（惰建触发/真点 docs 13→0/状态行回惰建态） |
| 在线词典库浏览/下载 | 设计稿 | docs/server_plan.md | 元数据目录 + 匿名分发 |

## 2. 检索

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| exact / prefix / fuzzy / wildcard / regex / fulltext | 活（core 层全实现） | DictionaryManagerStd 6 模式；IndexEngineStd 4 模式 | exact 走词表 |
| UI 曝光：模式下拉 | 部分 | SidebarPanel.qml:55-61，5 态 [自动/前缀/模糊/通配符/正则] | 默认「自动」= prefix 优先、空时 fuzzy 兜底；**全文不在下拉里**（是内容 Tab） |
| 全文检索 UI | 活 | EntryResultsPane「全文检索」tab | relatedLookup 同形键复用 |
| 输入建议/补全 | 活 | gui QCompleter；qmlui 搜索框建议 | qmlui 建议三段分组（精确/前缀/词条，MainDesktop suggestSection 计算段键 + ListView section 段头；UI 批一 2026-10-11） |
| 聚合分组/去重/相关性 | 活 | aggregateLookup = manager.search_grouped 包装（五层降级+组内去重；TD-104 双套已于 2026-10-10 消费面扫描退役根除） | UI/CLI 单口径（DictionaryManagerStd）；qmlui 词典 tab 多词典命中出 chip 概览（点选 solo 聚焦单词典、再点取消），分组走 GroupCard 折叠卡（UI 批一 2026-10-11） |
| 文本卷叠 | 活 | core/std/text_norm_std v2（表驱动，无 ICU） | case/diacritics/punctuation/ligatures 独立开关，fold-key 版本化；三档 profile（宽容=全折/谨慎=保留重音/精确=原样字节）qmlui 外观 tab 三 radio（textnorm/profile，启动装载前落档、换档即时重灌），engine 与 json/stardict/mdict 解析器折叠缓存同口径（48eff28） |

## 3. 渲染与内容

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| HTML 净化渲染 + 资源解析 | 活 | html_renderer_std + QTextBrowser::loadResource | 白名单 sanitize；res:// 从 sibling .mdd 取料 |
| 交叉引用 | 活 | cross_reference_std | 近义联想/词组/相关 |
| 词条笔记（Markdown） | 活 | gui 工具栏笔记按钮；qmlui 生词本笔记 | QTextDocument::setMarkdown |

## 4. 发音

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| TTS 本地发音 | 活 | qmlui Drawer「语音」tab | voice/preset/rate/pitch/volume + 自动播放延迟 |
| 在线发音源（三态+口音） | 活 | PronunciationSourceStd → FreeDictionarySource | dictionaryapi.dev；仅发查询词（UI 明示） |
| 录音/波形/回放 | 活 | gui：audio_recorder / waveform_widget / pcm_playback | 16k/mono/16bit 薄壳 |
| 发音评分 M1–M9 | 门控 | adapters/pron + core/std 发音全家 | UNIDICT_BUILD_PRON（默认 OFF）；635MB fp16 模型不入库；M10 自举下载（cli-std + curl） |
| 口音选择（评分参考切字段） | 门控 | gui 口音选择器 + QSettings | M5 |

## 5. 学习与个人数据

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 生词本 CRUD+标签+笔记+CSV 导出 | 活 | MainDesktop 生词本 tab（lookup_adapter 22 个 Q_INVOKABLE → DataStore） | gui 右键菜单；qmlui 卡片「+标签」「笔记」对话框 + 标签筛选 chips；真点审计 124 项（qmlui/dev/ui_click_audit.cpp）；cli-std --import-vocab/--export-vocab 同 CSV 口径（导入导出批 2026-10-12） |
| 笔记内检索（Search within notes） | 活 | data_store_std search_notes → data_store_qt getNotesByText → lookup_adapter searchNotes | 大小写不敏感子串（非 ASCII 原样参与），空 query=全量；桌面生词本独立搜索框、移动端并入 vocabFilter |
| 笔记导出（Export notes PDF/HTML） | 活 | core/std/notes_export_std（HTML）+ data_store_qt exportNotesPdf（QPdfWriter）→ lookup_adapter → 桌面「导出笔记」按钮按后缀分流 | HTML：转义（&<>"'）+ 换行转 <br> + UTC 时间戳（0 留空）；PDF：Qt 排版；cli-std --export-notes 同口径 |
| 搜索历史 + 置顶 + 删除 + 清空 | 活 | MainDesktop 历史 tab | limit 100；同词重查命中计数 hit_count（大小写不敏感去重累加，行尾 ×N 胶囊、count>1 才显示；store JSON 紧凑写 ==1 省略、读侧兜回 1，UI 批一 2026-10-11）；📌 置顶块语义（置顶块浮首，UI 批 2026-10-12）；✕ 逐条删除（core 大小写不敏感删，2026-10-12）；整页清空 |
| 学习统计/进度/成就/激励语/每日目标 | 死 | learning_manager（仅死 Main.qml 消费） | learning_stats.json 独立存储（TD-113） |
| 复习（flashcard） | 死 | 仅死 Main.qml（modern 死树已删，TD-111） | roadmap 勾选与实际不符 |
| 遗忘曲线调度 | 死 | 同上 | 活路径无复习入口 |
| 发音不稳标签联动（词分<0.6） | 门控 | gui 收藏面板（M8） | 只动已收藏的词 |

## 6. 桌面集成

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 系统托盘（close-to-tray + 菜单 + 恢复） | 活 | gui main.cpp（QSystemTrayIcon）+ qmlui TrayController（F11 parity：显示主窗/关闭时隐藏到托盘/开机自启/退出，双击恢复，首隐气泡） | 无托盘时回退普通关闭（qmlui 关窗即退出） |
| 开机自启 | 活 | gui 托盘菜单 + qmlui 取词 tab 开关（F9，共用 StartupLauncher：Windows HKCU Run / Linux XDG autostart / macOS launchd，UNIDICT_AUTOSTART_DIR 测试缝） | |
| 全局热键 Ctrl+Alt+U | 活 | qmlui global_hotkeys 三桌面平台（F10：Windows RegisterHotKey / Linux X11 XGrabKey——Wayland 会话诚实失败 / macOS Carbon）+ Drawer「快捷键」tab | Linux 需 X11 头（UNIDICT_HAS_X11），Wayland 注册即败明示 |
| 剪贴板取词 | 活 | qmlui clipboard_monitor（轮询+过滤+自动置词） | 开关/间隔/词长/排除模式可配 |
| 划词取词（X11 主选区） | 部分 | qmlui selection_monitor（xcb 钉定：非 xcb 平台开关隐藏、start no-op） | 选中即查，命中分流同剪贴板（取词窗/主窗）；Win/mac Selection 回落剪贴板故不做（会双触发），pro_dictionary_gap P1 收尾 |
| P-5 悬浮取词窗 | 活 | qmlui QuickLookupPane（剪贴板取词/quick_lookup 热键 → 贴光标浮窗） | 取词/朗读/生词本/主窗打开；失焦即收；开关持久化（quicklookup/enabled）；Windows 热键先读焦点窗选区（F13a win_selection_reader，UIA 触发式，空回落剪贴板） |
| 主题 | 活 | qmlui Theme 三态（0 跟随系统/1 浅/2 深；gui 同语义同键 ui/theme；设置「外观」tab themeModeRadio0/1/2 即时翻面 + 落盘记忆） | qmlui 跟随系统 + 手动覆盖；gui QPalette 深色 |
| 释义字体设置 | 活 | gui QFontDialog（QSettings 持久化）；qmlui 外观 tab 四档 radio 12/14/16/18（ui/def_font_size 落盘，档位即时翻面重建当前查询面） | |
| 快捷键 Ctrl+1/2/3 切 tab | 活 | MainDesktop.qml:1221 | |

## 7. 移动端（Android）

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 查词（exact/prefix/fuzzy/searchAll） | 活 | JNI lookup_jni + Compose 壳（M0–M3-C） | |
| 词本/历史 | 活 | JNI store_jni | |
| TTS 发音（M4） | 活 | UnidictTts.kt（系统 TextToSpeech） | 7761c04 上机，M4-TTS-OK 令牌验收；词条页/生词本卡片朗读 |
| 打磨出包（M5：深色/错误态/首启引导/R8） | 活 | MainActivity.kt M5 首启引导 + build.gradle.kts R8 | |
| 分享接词/划词菜单/快捷方式（M7） | 活 | MainActivity singleTask + SEND/PROCESS_TEXT | 三入口 logcat 令牌验收 |
| 剪贴板取词（Android 壳） | 不做 | — | mobile_plan §2 明确排除：剪贴板监视=桌面语义，无移动等价物；桌面剪贴板取词已活（qmlui） |
| 悬浮取词窗（Android M8-A） | 活 | FloatingLookupService（overlay 球 + 迷你查词面板） | M8-FLOAT-OK 全链令牌验收；拖拽越屏回钳 |
| 桌面可缩放 widget（Android M8-B） | 活 | QuickLookupWidgetProvider（最近一词 + 点按直达查词，H\|V 可缩放） | 模拟器实测：放置/刷新令牌 M8-WIDGET-OK、查词后桌面实时更新、缩放手柄拖拽落定 |
| iOS / HarmonyOS | 未建 | | |
| QML 移动壳 | 死 | Main.qml + qmlui/mobile/ | 被原生壳取代，仍编 qrc |

## 8. CLI

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| cli-std 全功能（六格式加载、六模式查词、--mdict-password、--list-dicts、--drop-dict、--scan-dir、--where/--all、索引运维、全文索引运维、缓存五件套、--list-plugins、--pron-* 全家、模型自举下载） | 活 | cli-std/（零 Qt，deb/rpm） | 主力 CLI |
| cli（Qt）诊断面（--dict/--dict-dir/--list） | 活（遗留） | cli/ | 功能面严重不对等（TD-118） |

## 9. 同步 / AI / Server / 插件

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 本地文件级同步（合并+预览+选择性应用） | 活 | adapters/qt/sync_service_qt | 无账号无云端 |
| 同步中转 relay（协议 v1 + dev/Worker 双参考实现 + 契约测试） | 活 | server/sync_relay/ + adapters/qt/sync_manager_qt | B1-B7 全交付（todo.md B 系列）：客户端 SyncEngineStd 已接（断点续传 + 设备面），SPAKE2+/加密备份齐；同步默认关、中转只见密文 |
| AI 翻译 / 语法检查 | 活 | ai_service_qt（外部命令桥，UNIDICT_AI_CMD） | 桌面 qmlui 面；无 streaming/provider；mobile_plan §2 明确排除 AI 上移动 |
| AI 语境造句 | 活 | ai_service_qt sentences 子命令 + heuristic mock 自标 [Mock sentences] | 桌面词条卡「✨ 造句」轻入口 + 只读弹层，ui_click_audit S20 |
| 字典库服务（账号/目录/分发/同步） | 设计稿 | docs/server_plan.md + design/sync-engine.md | 云级 S1–S5 未实现 |
| 插件（解析器工厂注册表） | 活 | cli-std --list-plugins | 内建注册表为最终形态；动态加载按 §A 废止（legacy plugin_manager 已退役） |

## 10. 导出 / 安全 / 其它

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 生词本 CSV 导出 | 活 | MainDesktop + cli-std | |
| 笔记导出 HTML / PDF | 活 | notes_export_std + DataStoreQt::exportNotesPdf → 桌面「导出笔记」按钮按后缀分流；cli-std --export-notes | roadmap Note-Taking 已交付（c0408fa） |
| 全量备份还原（口令加密） | 活 | sync_backup_std → 设置抽屉「同步备份」tab | B7 交付；pron 练习记录按设计留在本机 |
| Anki 导出（apkg/AnkiConnect） | 未建 | roadmap [ ] 废止（§A） | |
| 本地加密 / 隐私模式 / 安全擦除 | 未建 | roadmap 划线废止（§A） | 明示无日志口径已够，不做假安全承诺 |
| 隐私口径（在线发音仅发查询词） | 活 | online_pron_std 头注释 + UI 明示文案 | |
| 性能基准 | 活 | benchmark.sh（仓根；12 类 CLI 操作，3–20 次/类） | TD-134 已收口（38a1355）：CSV 出库 + .gitignore，需时现跑 |
| 主题对比度机器回归 | 活 | tests/theme_tokens_contrast_test | WCAG AA 数值锁定 |

## 11. 与 roadmap.md 对照的差（选区外）

- roadmap「Basic Search」全部 [x] 与 core 层事实一致；「全文检索」在 UI 的面是内容 Tab 而非模式，roadmap 未表达此差异。
- roadmap「复习/遗忘曲线 [x]」为死路径（见第 5 节）；TD-143（9ef59a2）对账后 roadmap 已加「review UI stays reserved per TD-114 … not wired to any live screen」明示，勾选与实现面已一致，残余仅「[x] 而无活入口」的产品口径。
- roadmap「发音」条目 TD-143（9ef59a2）已重写为 M1–M9 交付行，旧注「停在 M3」已消。
- roadmap「AI translation/grammar [x]」属实（外部命令桥）。