# Unidict 当前功能矩阵

> Phase 1 · 现状基线 · 审计日 2026-10-04 · 基线提交 fae5002
> 状态图例：**活** = 用户路径可达；**部分** = 可用但有缺口/仅某面；**死** = 无引用或不可达；
> **门控** = 编译开关默认 OFF；**设计稿** = 仅文档级；**未建**。

## 1. 词典格式与词典管理

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| StarDict (.ifo/.idx/.dict/.dict.dz) | 活 | core/stardict_parser(std 双实现) | UI 主链走 legacy 面 |
| MDict .mdx / .mdd | 活 | core/mdict_parser + std/mdd_resource | sibling .mdd 自动附着、资源进渲染管线；v2 忠实读取（Encoding 码表/加密 key 块）+ UTF-16 头全量中文标题（std 面） |
| StarDict 资源（图片/音频） | 活 | std/stardict_resource_std + manager 便利方法 | .ifo res 键（目录/清单）+ 散装媒体兜底；qmlui 渲染回退链（mdd 优先） |
| MDict 加密词典 | 活 | core/std/mdict_crypto + decryptor | GUI 密码设置（[encrypted] 检测） |
| DSL | 部分 | core/std/dsl_parser_std | 仅 std/cli-std 面；UI 主链 factory 未注册 |
| EPUB | 活 | legacy + std 双面接线 | 真 deflate 端到端 + EPUB2 text/html/dt-dd 版式/href 编码归一（std 面） |
| JSON custom | 活 | legacy + std 双面 | 环境变量/扫描目录导入 |
| CSV/TSV/plain | 活 | core/std（custom 格式族） | cli-std 面 |
| 词典优先级/分组/启用 | 活 | Drawer「词典」tab + gui 工具栏分组下拉 | 设置可持久化 |
| 本地词典导出/打包（JSON） | 活 | core/std/dictionary_export_std + cli-std --export-dict | 项目自定义 JSON 格式；round-trip 全保真（json_parser 转义解码+字符串感知扫描配套） |
| 损坏词典隔离/重试 | 活 | GUI ⚠ 行 + CLI --list [FAILED] | 缺失文件自愈 |
| 在线词典库浏览/下载 | 设计稿 | docs/server_plan.md | 元数据目录 + 匿名分发 |

## 2. 检索

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| exact / prefix / fuzzy / wildcard / regex / fulltext | 活（core 层全实现） | DictionaryManagerStd 6 模式；IndexEngineStd 4 模式 | exact 走词表 |
| UI 曝光：模式下拉 | 部分 | SidebarPanel.qml:55-61，5 态 [自动/前缀/模糊/通配符/正则] | 默认「自动」= prefix 优先、空时 fuzzy 兜底；**全文不在下拉里**（是内容 Tab） |
| 全文检索 UI | 活 | EntryResultsPane「全文检索」tab | relatedLookup 同形键复用 |
| 输入建议/补全 | 活 | gui QCompleter；qmlui 搜索框建议 | |
| 聚合分组/去重/相关性 | 活 | aggregateLookup = manager.search_grouped 包装（五层降级+组内去重；TD-104 双套已于 2026-10-10 消费面扫描退役根除） | UI/CLI 单口径（DictionaryManagerStd） |
| 文本卷叠 | 活 | core/std/text_norm_std v2（表驱动，无 ICU） | case/diacritics/punctuation/ligatures 独立开关，fold-key 版本化 |

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
| 生词本 CRUD+标签+笔记+CSV 导出 | 活 | MainDesktop 生词本 tab（lookup_adapter 22 个 Q_INVOKABLE → DataStore） | gui 右键菜单；qmlui 卡片「+标签」「笔记」对话框 + 标签筛选 chips；真点审计 57 项（qmlui/dev/ui_click_audit.cpp） |
| 笔记内检索（Search within notes） | 活 | data_store_std search_notes → data_store_qt getNotesByText → lookup_adapter searchNotes | 大小写不敏感子串（非 ASCII 原样参与），空 query=全量；桌面生词本独立搜索框、移动端并入 vocabFilter |
| 笔记导出（Export notes PDF/HTML） | 活 | core/std/notes_export_std（HTML）+ data_store_qt exportNotesPdf（QPdfWriter）→ lookup_adapter → 桌面「导出笔记」按钮按后缀分流 | HTML：转义（&<>"'）+ 换行转 <br> + UTC 时间戳（0 留空）；PDF：Qt 排版；cli-std --export-notes 同口径 |
| 搜索历史 + 置顶 + 清空 | 活 | MainDesktop 历史 tab | limit 100 |
| 学习统计/进度/成就/激励语/每日目标 | 死 | learning_manager（仅死 Main.qml 消费） | learning_stats.json 独立存储（TD-113） |
| 复习（flashcard） | 死 | 仅死 Main.qml（modern 死树已删，TD-111） | roadmap 勾选与实际不符 |
| 遗忘曲线调度 | 死 | 同上 | 活路径无复习入口 |
| 发音不稳标签联动（词分<0.6） | 门控 | gui 收藏面板（M8） | 只动已收藏的词 |

## 6. 桌面集成

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 系统托盘（close-to-tray + 菜单 + 恢复） | 活 | gui main.cpp（QSystemTrayIcon） | 无托盘时回退普通关闭 |
| 开机自启 | 部分 | gui（Windows HKCU Run + 托盘菜单开关） | Linux/macOS 为 stub |
| 全局热键 Ctrl+Alt+U | 部分 | qmlui global_hotkeys + Drawer「快捷键」tab | 仅 Windows（RegisterHotKey + WM_HOTKEY）；Linux/macOS stub |
| 剪贴板取词 | 活 | qmlui clipboard_monitor（轮询+过滤+自动置词） | 开关/间隔/词长/排除模式可配 |
| 划词取词（X11 主选区） | 部分 | qmlui selection_monitor（xcb 钉定：非 xcb 平台开关隐藏、start no-op） | 选中即查，命中分流同剪贴板（取词窗/主窗）；Win/mac Selection 回落剪贴板故不做（会双触发），pro_dictionary_gap P1 收尾 |
| P-5 悬浮取词窗 | 活 | qmlui QuickLookupPane（剪贴板取词/quick_lookup 热键 → 贴光标浮窗） | 取词/朗读/生词本/主窗打开；失焦即收；开关持久化（quicklookup/enabled）；热键注册仅 Windows 生效 |
| 主题 | 部分 | qmlui 跟随系统亮暗；gui QPalette 深色 | 无手动切换 UI；ui/theme 键仅 gui 消费 |
| 释义字体设置 | 活 | gui QFontDialog（QSettings 持久化） | |
| 快捷键 Ctrl+1/2/3 切 tab | 活 | MainDesktop.qml:1221 | |

## 7. 移动端（Android）

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 查词（exact/prefix/fuzzy/searchAll） | 活 | JNI lookup_jni + Compose 壳（M0–M3-C） | |
| 词本/历史 | 活 | JNI store_jni | |
| TTS 发音（M4） | 活 | UnidictTts.kt（系统 TextToSpeech） | 7761c04 上机，M4-TTS-OK 令牌验收；词条页/生词本卡片朗读 |
| 打磨出包（M5：深色/错误态/首启引导/R8） | 活 | MainActivity.kt M5 首启引导 + build.gradle.kts R8 | |
| 分享接词/划词菜单/快捷方式（M7） | 活 | MainActivity singleTask + SEND/PROCESS_TEXT | 三入口 logcat 令牌验收 |
| 剪贴板取词（Android 壳） | 未建 | — | 桌面剪贴板取词已活（qmlui）；Android 壳待做 |
| 悬浮取词窗 / 桌面可缩放 widget（Android） | 门控 | roadmap 缓办注记 | adb 可用已复核（2026-10-10），门控解除，可推进 |
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
| 同步中转 relay（协议 v1 + dev/Worker 双参考实现 + 契约测试） | 部分 | server/sync_relay/ | B1 参考面已交付；客户端同步引擎（B2 起）未接 |
| AI 翻译 / 语法检查 | 活 | ai_service_qt（外部命令桥，UNIDICT_AI_CMD） | 无 streaming/provider；Live UI 面 = 仅移动端（Android 壳未接，见下） |
| AI 语境造句 | 活 | ai_service_qt sentences 子命令 + heuristic mock 自标 [Mock sentences] | 桌面词条卡「✨ 造句」轻入口 + 只读弹层，ui_click_audit S20 |
| 字典库服务（账号/目录/分发/同步） | 设计稿 | docs/server_plan.md + design/sync-engine.md | 云级 S1–S5 未实现 |
| 插件（解析器工厂注册表） | 部分 | plugin_manager（legacy）+ --list-plugins | 仅内建注册；动态加载未建 |

## 10. 导出 / 安全 / 其它

| 功能 | 状态 | 位置 | 备注 |
|---|---|---|---|
| 生词本 CSV 导出 | 活 | MainDesktop + cli-std | |
| 笔记导出 HTML / PDF | 活 | notes_export_std + DataStoreQt::exportNotesPdf → 桌面「导出笔记」按钮按后缀分流；cli-std --export-notes | roadmap Note-Taking 已交付（c0408fa） |
| 全量备份还原（口令加密） | 活 | sync_backup_std → 设置抽屉「同步备份」tab | B7 交付；pron 练习记录按设计留在本机 |
| Anki 导出（apkg/AnkiConnect） | 未建 | roadmap [ ] 废止（§A） | |
| 本地加密 / 隐私模式 / 安全擦除 | 未建 | roadmap [ ] | |
| 隐私口径（在线发音仅发查询词） | 活 | online_pron_std 头注释 + UI 明示文案 | |
| 性能基准 | 部分 | scripts/benchmark.sh（12 类 CLI 操作 ×10 次） | CSV 为单次残留，无趋势（TD-134） |
| 主题对比度机器回归 | 活 | tests/theme_tokens_contrast_test | WCAG AA 数值锁定 |

## 11. 与 roadmap.md 对照的差（选区外）

- roadmap「Basic Search」全部 [x] 与 core 层事实一致；「全文检索」在 UI 的面是内容 Tab 而非模式，roadmap 未表达此差异。
- roadmap「发音」条目停在 M3 级别一带而过，实际 M1–M9 全实装（建议收敛期重写该条目）。
- roadmap「复习/遗忘曲线 [x]」为死路径（见第 5 节）。
- roadmap「AI translation/grammar [x]」属实（外部命令桥）。