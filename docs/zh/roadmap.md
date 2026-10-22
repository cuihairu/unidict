> 语言: **简体中文** | [English](../roadmap.md)

# Unidict 开发路线图

本文档给出 Unidict 的详细开发计划，按模块与特性组织。

> 处置说明（2026-10-10，关闭 TD-143）：与产品定位冲突的条目按 todo.md §A 的
> 禁做清单处置，并在下方以删除线标出（「按 §A 放弃」）。已交付条目打勾并附
> 交付记录。

##  MVP（最小可用产品）优先级
1.  **核心查词功能** —— 支持 2-3 种主流词典格式（如 MDict、StarDict）。
2.  **基础 UI** —— 干净、快速的搜索与展示界面。
3.  **生词本** —— 保存与复习单词的基础功能。
4.  **跨平台支持** —— 初步支持 Windows、macOS 与 Linux。
5.  **离线优先** —— 核心功能必须能在无网络连接时工作。

---

## 核心架构
- [x] 跨平台框架选型与搭建（C++/Qt）
- [x] 数据库 schema 设计（基于 JSON 的 DataStore MVP）
- [x] 同步引擎设计（账号、增量同步、E2EE）
      （设计文档：docs/design/sync-engine.md —— S3 兼容 blob 后端 +
      轻量账号服务、基于状态的 pull-merge-push 加 HLC、按 collection 的
      字段级合并、monocypher E2EE 密钥层级；实现分阶段 S1–S5，未启动）
- [x] 插件系统架构（扩展 -> 解析器工厂注册表）
- [x] 词条渲染管线（HTML/CSS 子集、链接处理、资源解析）
      （首期仅 MDX：HtmlRendererStd 白名单净化；entry://bword://
      链接 → #w: 页内锚点；img/audio 相对 src → res:///<key>?dict=<id>
      经 QTextBrowser::loadResource 由同名 .mdd 供给；其余格式
      仍为纯文本）
- [x] 文本归一化策略（Unicode/大小写/变音符号折叠，可配置）
      （策略文档：docs/design/text-normalization.md —— 核心折叠已落地
      为 text_norm_std v2（表驱动、无 ICU、Options 开关、用于缓存失效的
      fold-key 版本号）；本轮：RFC 3629 越界解码拒绝 + astral-plane
      测试矩阵；2026-10-16：面向 UI 的 profile 配置已交付 —— 设置抽屉中
      的三档 profile（tolerant/conservative/exact），textnorm/profile
      持久化，变更时引擎 re-key + 解析器 fold-cache 切换，48eff28）

## 词典支持与管理
- [x] **多格式词典支持**
  - [x] StarDict (.ifo/.idx/.dict/.dict.dz)
  - [x] MDict (.mdx/.mdd)（同名 .mdd 自动检测并挂接；资源
        供给渲染管线；真实世界兼容性持续跟进）
  - [x] DSL
  - [x] EPUB（最小实现：zip 容器 + OPF + 基于标题的词条抽取；真实排版兼容性持续跟进）
        (Qt 面 EpubParser + std 面 EpubParserStd 双实现，DictionaryManager
        两面均接线；真 deflate epub（mimetype stored 首条、子目录 href、
        实体/嵌套标签变体）经 CLI 双面端到端查词验收，std 面
        manager 级回归见 test_dictionary_manager_std)
  - [x] 自定义 JSON 格式
  - [x] CSV/TSV/纯文本（简单自定义格式）
- [ ] **词典管理**
  - [ ] 在线词典商店浏览/下载（暂缓 —— P-10，仅用户驱动）
        (已裁决维持不做——P-10 已于 2026-10-14 巡检授权关闭（todo.md:31），随关不重开；
        复核 PATROL1016：解锁条件「用户显式推进」已裁决为不做，结论仍成立)
  - [x] 本地词典导入（路径/环境变量 + scan-dir）
  - [x] 本地词典导出/打包（core/std/dictionary_export_std +
        cli-std `--export-dict <name> <out.json>`：把已加载词典打包为
        项目自定义 JSON 格式；round-trip 已验证 ——
        json_parser_std 增加了转义感知的字符串抽取与解码，以及
        字符串感知的对象扫描，可原样读回含引号、反斜杠、换行、
        制表符与裸花括号的词条）
  - [x] 词典优先级设置（UI + 持久化排序）
  - [x] 词典分组/profile（如 EN-EN 与 EN-ZH）
  - [x] 损坏词典检测（加载失败诊断 + 隔离：
        解析失败隔离至显式重试、文件缺失自愈；GUI 词典管理 ⚠ 行 +
        重试/移除、CLI --list [FAILED]）

## 查词功能
- [x] **基础搜索**
  - [x] 精确匹配
  - [x] 模糊搜索
  - [x] 全文搜索（倒排索引 + TF/IDF + 持久化；经 qmlui 词典页 rebuild
        按钮手动重置索引 —— 下次查询时懒重建）
  - [x] 通配符搜索
  - [x] 正则搜索
- [x] **快速访问**
  - [x] 文内取词（mouseselect/hotkey）—— 由剪贴板监听器
        与全局热键承载（鼠标悬停取词按 §A 放弃）
  - [x] 剪贴板监听（Qt Widgets GUI 工具栏开关；ClipboardMonitor
        轮询 + 过滤，自动置前窗口并填入查询词）
  - [x] 全局热键（三桌面平台全覆盖：Windows RegisterHotKey
        + WM_HOTKEY；Linux 在 root 窗口上 XGrabKey + QSocketNotifier
        泵 X 连接 fd —— X11/xcb 会话，Wayland 注册时如实
        失败；macOS 经 global_hotkeys_mac.mm 桥接 Carbon
        RegisterEventHotKey）
  - ~~鼠标悬停取词~~（按 §A 放弃 —— 快速取词由剪贴板/热键承载）
- ~~**视觉取词**~~（按 §A 放弃 —— 非核心，属另一种产品形态）
  - ~~截图/相机 OCR~~
  - ~~PDF 取词~~

## 智能特性
- [ ] **AI 集成**
  - [ ] LLM 集成（provider 认证、流式、缓存、prompt）
        （暂缓 —— 需要外部 AI 服务的 auth/基建；P-9
        命令桥 provider 已覆盖本地/CLI 用途，无需它）
        (复核(巡检 R1)：维持挂起——需外部服务 auth/凭据与计费决策，非巡检可代决；
        解锁条件 = 用户给出可用 provider 凭据并明确计费口径)
        (复核 PATROL1016：解锁条件不变——凭据与计费口径仍未到位，维持挂起)
  - [x] AI 翻译（经外部命令桥）
  - [x] AI 语法检查与润色（经外部命令桥）
  - [x] AI 情境造句（外部命令桥 `sentences`
        子命令 + 启发式 mock 兜底（自标注
        "[Mock sentences]"）；桌面词条卡 ✨ 造句 轻量动作配只读
        结果弹窗，经 ui_click_audit S20 审计）
- [x] **语音特性**
  - [x] TTS 朗读（Qt TextToSpeech；音色选择/预设）
  - ~~语音搜索~~（按 §A 放弃 —— TTS 输出面已有；语音输入
        查词不做）
  - [x] 发音练习与评分 —— 已交付 M1–M9（本地 ONNX 评分、
        离线优先；录音基建、回放跟读循环、ARPAbet+GOP 评分
        内核、onnxruntime 适配 + CLI --pron-score、变体容忍、
        混淆定位、位置感知变体、笔记本标签、练习历史；
        计划与各里程碑记录见
        [docs/pronunciation-plan.md](pronunciation-plan.md)）

## 学习特性
- [x] **词汇管理**
  - [x] 生词本（基础 CRUD + 导出 CSV）
  - [x] 生词本标签/分组（按词标签持久化在数据
        store 中；GUI 收藏面板分组过滤下拉 + 右键设置标签）
  - [x] 搜索历史
  - ~~学习进度追踪（分析、连续打卡、目标）~~（按 §A 放弃 ——
        游戏化：连续打卡/目标不做；复习调度由下方
        遗忘曲线条目覆盖）
  - [x] 遗忘曲线算法（基础的计划复习）
        （与上方闪卡复习同一暂缓口径：先做基础功能，UI 保持
        预留且未接线）
- [x] **记忆系统**
  - [x] Anki 式闪卡复习（基础）
        （用户 2026-10-10 决定：复习/游戏化暂缓 —— 先做词典
        基础功能；复习 UI 按 TD-114 保持预留，LearningState
        字段留在数据模型中 —— 不删除、也不接入任何在用界面）
  - ~~自定义复习计划（用户可配算法）~~（按 §A 放弃
        —— 与「简单可预期」定位冲突）
  - ~~学习统计与可视化~~（按 §A 放弃）
  - ~~成就/游戏化系统~~（按 §A 放弃）
- [x] **笔记系统**
  - [x] 为词典词条添加笔记（按词笔记持久化在数据
        store 中，经工具栏 笔记 按钮 upsert；空文本即移除笔记；
        显示在词条视图末尾）
  - [x] Markdown 支持（GitHub 方言，经 QTextDocument::setMarkdown，
        纯文本转义兜底）
  - [x] 导出笔记（PDF/HTML）（std 导出器 core/std/notes_export_std →
        独立 HTML，词/文本转义、换行→<br>、UTC 时间戳；
        Qt PDF 经 DataStoreQt::exportNotesPdf（QPdfWriter+QTextDocument）；
        桌面侧栏 「导出笔记」 按钮、按后缀路由 .pdf/.html；
        cli-std --export-notes）
  - [x] 笔记内搜索（DataStoreStd::search_notes —— 对笔记文本的
        不区分大小写子串匹配，空查询 = 全部；lookup_adapter
        searchNotes → 桌面生词 tab 笔记搜索框，移动端生词
        过滤同样匹配笔记内容）

## 翻译特性
- ~~**在线翻译引擎**~~（按 §A 放弃 —— 翻译走
  AI 外部命令桥，已交付）
  - ~~Google Translate~~
  - ~~DeepL~~
  - ~~自定义 API 集成~~
- ~~**文档翻译**~~（按 §A 放弃 —— PDF/Word 抽取已放弃）
  - ~~PDF/Word 翻译（保留格式）~~
  - ~~双语并排视图~~

## 用户界面
- [ ] **主界面**
  - [x] 现代 UI/UX 设计（QML）
  - [x] 亮/暗主题（基础）
  - [ ] 自定义主题/配色（暂缓 —— 设计系统决策：
        每个 accent 预设都必须通过 WCAG 对比度机器
        回归（theme_tokens_contrast_test）；调色板选择属
        产品/设计决策，需用户指示）
        (已代决(巡检 R1)：维持现状——不扩自定义色板；§A 极简口径 + 无驱动需求，
        调色板属审美取舍，非技术债；如需再启，先定色板再走 contrast 回归)
        (复核 PATROL1016：维持现状——contrast 闸门仍在（tests/CMakeLists.txt:43），
        重启前置「先定色板」口径不变)
  - [x] 字体与布局自定义（释义区字体经 QFontDialog 选择，
        持久化在 QSettings；列表/工具栏跟随系统主题）
- [x] **交互**
  - [x] 快速、响应灵敏的搜索
  - [x] 键盘快捷键完备（应用内 Shortcut 集合 —— Ctrl+K 聚焦
        搜索、Ctrl+1/2/3 切换 tab、Enter/Esc、←/→ 词条导航；设置抽屉
        快捷键 区有可见速查表；全局快速取词热键走 P-5，
        各平台状态如实标注）
  - ~~手势支持（移动端）~~（按 §A 放弃 —— 移动端手势取词
        不做）

## 平台专项
- [ ] **桌面（Windows、macOS、Linux）**
  - [x] 系统托盘/菜单栏集成（QSystemTrayIcon：关闭即隐藏到托盘
        + 托盘菜单 显示主窗/开关/退出，双击恢复，首次隐藏
        气泡提示；无托盘可用时退化为普通关闭；
        qmlui 对齐 F11 2026-10-12 —— TrayController 共享形态，菜单
        项 显示主窗/关闭时隐藏到托盘/开机自启/退出，QML onClosing
        经 shouldHideToTray 拦截关闭）
  - [x] 登录自启（Windows：HKCU Run 注册表键 + 托盘/qmlui
        开关；Linux：XDG autostart .desktop，macOS：launchd LaunchAgent
        plist —— F9 2026-10-12，qmlui 取词 tab 开关 + gui 托盘共用
        同一个 StartupLauncher，带 UNIDICT_AUTOSTART_DIR 测试缝）
  - [ ] 原生通知（暂缓 —— 目前尚无具体触发场景；
        需先有产品方向）
        (已代决(巡检 R1)：维持 defer——现无具体触发场景（同步为手动、无后台任务），
        无场景即无通知语义，待出现真实触发点再启)
        (复核 PATROL1016：维持 defer——同步默认关实证（sync_manager_qt.cpp:85
        kKeyEnabled 默认 false）、仍无后台任务面，触发场景不存在的前提成立)
- [ ] **移动端（Android、iOS）**
      (已代决(巡检 R1)：维持 D 记录口径——Android 面已交付；iOS/HarmonyOS 维持
      「低优先级填空」不排期（iOS 走 macOS runner、鸿蒙编译过即可），启动须用户显式推进)
      (复核 PATROL1016：维持 D 记录——无用户推进信号，iOS/HarmonyOS 不排期口径不变)
  - [x] 悬浮取词控件（Android 壳 M8-A：FloatingLookupService
        前台服务，带可拖动悬浮球（SYSTEM_ALERT_WINDOW，
        由设置开关引导开启），展开为迷你查词面板 —— 聚合最佳命中
        释义 + 朗读/加入生词/到主界面/收起；共享进程级单例
        DictRepository；经 M8-FLOAT-OK token 在模拟器上端到端
        验收，含对靠屏幕边缘打开面板的离屏钳位修复）
  - [x] 分享菜单集成（Android 壳 M7：SEND/PROCESS_TEXT 分享与
        文本选择菜单预填查词页；launcher 长按快捷方式
        直达查词）
  - [x] 桌面 widget（M7 静态快捷方式 + M8-B 可缩放
        AppWidget：显示最近查过的词，点按打开快速查词，
        经 launcher 手柄 H|V 缩放；模拟器验收 —— bind/refresh
        经 M8-WIDGET-OK token，搜索后实时刷新文本，缩放
        拖拽已提交）

## 高级特性
- [ ] **数据同步**
  - [ ] 多设备同步（云端）（暂缓 —— P-10，仅用户驱动）
        (已裁决维持不做——随 P-10 于 2026-10-14 巡检授权关闭；B 系列 binlog 同步
        已覆盖，云端 S1–S5 需账号基建；§A 口径不动。
        复核 PATROL1016：解锁条件已裁决为不做，结论仍成立)
  - [x] 增量同步算法（op-log 中继 MVP：B5 C++ relay，含 op_id
        排序/幂等、offset 拉取、快照；云端 S1–S5 设计
        仍按 §A 暂缓）
  - [x] 冲突预览与合并（基于文件的同步 MVP）
- [x] **插件系统**
  - ~~第三方插件支持（动态加载 + API/ABI 版本化）~~
        （按 §A 放弃 —— 无第三方运行时；开发者词典走
        std 解析器工厂注册表）
  - ~~JavaScript/QML 插件引擎~~（按 §A 放弃）
  - ~~插件商店/仓库~~（按 §A 放弃）
  - [x] 开发者 API（解析器工厂注册；内建）
- [x] **导入/导出**
  - ~~Anki 牌组导出（apkg/AnkiConnect）~~（按 §A 放弃）
  - [x] CSV 导出
  - [x] 全量数据备份与恢复（B7 自救已交付：
        密码加密备份文件（PBKDF2 + XChaCha20-Poly1305，
        sync_backup_std），覆盖 words/notes/tags/history/prefs/已装
        词典；恢复经设置抽屉 同步备份 tab 合并缺失词条，
        经 ui_click_audit S18 审计；发音练习记录
        按设计留在设备本地）

## 隐私与安全
- ~~**数据保护**~~（按 §A 放弃 —— 明确的无日志立场
  已足够；不做虚假安全承诺）
  - ~~本地数据加密~~
  - ~~隐私模式~~
  - ~~安全数据擦除~~

---
## 分阶段推进计划
- **v1.1**：引入 OCR 与语音特性。（语音/发音已完成；OCR 按 §A 放弃）
- **v1.2**：集成 AI 翻译与写作辅助。（AI 桥 MVP 已完成）
- **v1.3**：打磨同步引擎并启动插件系统。（同步打磨暂缓 —— P-10；插件运行时按 §A 放弃）
- **v2.0**：引入社区特性与高级学习分析。（按 §A 放弃）
