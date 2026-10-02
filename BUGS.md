# BUGS

用户报告与自查缺陷登记。格式：现象 / 根因 / 修复 / 验收。修复完成即勾，
带后续验收项的写明口径。

## BUG-003 界面与原型不一致（第二轮报告：上一轮未达标）❗待修复（2026-10-02 登记）

**现象**：用户第二轮反馈「界面还是对不上原型」，明确上一轮（BUG-002：
换发 unidict_qml + md5 同图自证）没有达标，不接受"功能差不多"交差。

**升级做法（用户指定）**：逐屏对照 `docs/ui/` 的 Qt Design 设计稿**重做**，
每屏交「原型图 vs 实现截图」左右对照图，差一处改一处，直到与原型一致；
设计稿未覆盖的细节列清单问用户，不自行发挥。

**排查方向**：
1. 本地 offscreen 截图 vs 原型的逐像素差（此前 md5 一致的结论需在当前
   HEAD 复测——中间隔了 M6 语音 tab 新控件等改动）；
2. **分发面环境差**：Windows 包 windeployqt 是否带 Material 样式插件
   （缺插件会静默回落 Default 样式，观感完全不同）、字体差异；
3. 运行态 vs 静态稿：空历史/空生词本/未加载词典提示等动态态原稿未定义。

**验收**：八屏（home/result/vocab/settings × 亮暗）每屏一张
「原型 | 实现」并排对照图入 `docs/ui/compare/`，客观度量（像素差）
达标；双端真实走查截图回传。

**客观度量与根因定位（2026-10-03，ui_sandbox offscreen 1200×760 ×8 屏
vs `docs/ui/`，逐像素容差 8）**：

- **基线（nightly 3973dfe）**：home/result/vocab × 亮暗共 **6 屏 md5
  逐字节一致**；settings 两屏各差 1094px（bbox x781-1171 / y519-683，
  全部在抽屉下部空白带）。
- **根因①（顶/底栏，9380689）**：elevation 2 / 边距 16 / spacing 12 与
  原型（1 / 12 / 10）不符——状态栏文字 ±4px、顶栏 logo +4 / 计数 +6 /
  右侧 tabs −4/−6/−8 位移（按位移量对齐后残差归零，纯参数差不涉及
  阴影渲染）。
- **根因②（settings 抽屉，fa00d2d）**：语音 tab 新增 4 控件使
  StackLayout 隐式高度增大，无显式高度的 Drawer 从内容自适应
  （≈505px，原型口径：抽屉下方露出主窗格）变为全高，盖住了原型中
  可见的列表分隔线与滚动条。隔离 worktree 仅撤该 hunk 复测 →
  **8/8 屏 md5 一致**（含根因①一并回退）。
- **6aa30a7 度量**：与原型差 2.8%-5.5%/屏（25k-50k px，合计 29.9 万
  px），较修复前（3.7k-4.6k px/屏）扩大一个数量级——字号/边距/间距
  全面增大，方向与 `docs/ui/` 设计稿相反。按本缺陷验收口径（对照
  docs/ui）**未达标，不勾选**。对照图：`ui_sandbox_out/bug003/
  compare_6aa30a7/`（现状）与 `compare_reverted/`（回退实验，8/8 一致）。
- **口径提醒**：若以重渲 `docs/ui/*.png` 来达成「一致」，即回到第一轮
  被否的「md5 同图自证」（用户第二轮明确不接受）；设计稿若要更新，
  应先出新旧设计对照并留档，再重制原型图。

## BUG-004 开箱无真实词典可查 ✅修复+双端走查通过（2026-10-02 登记，2026-10-03 验收）

**现象**：用户反馈装完开箱一个词都查不到（或只有 4 词演示样本，
够不上"真实可用"）。

**现状（修复前）**：随包 `examples/dict.json` 仅 4 词；Android 首启
种子为三格式演示词典共 12 词。

**要求（用户指定）**：首次安装/启动**内置至少一个真实可用词典**，
许可合规优先——CC-CEDICT/CC 授权词典数据打包内置（不许无授权数据）；
桌面与 Android 双端内置；查词冒烟：装完直接查一个单词出释义。

**修复**（两笔提交：perf 装载提速 + feat 内置词典）：
- 资产：CC-CEDICT（CC BY-SA 4.0，MDBG 导出）经 `tools/build_ccedict_dict.py`
  转项目 JSON 格式，出 `dictionaries/ccedict-zh-en.json`（125,166 条
  简体词头）+ `dictionaries/CC-CEDICT-ATTRIBUTION.md` 署名/许可/取舍说明
  （繁体不命中为既定取舍，源文件地址在署名里，用户可自行导入繁体版）。
- 装载性能（惠及所有大词典用户）：`fold_key` 汉字带 0x3400-0x9FFF 快速
  通道（五张折叠表键全在带外，带边界测试）；JSON 解析改 string_view
  直扫（免对象子串/免逐调用 pattern 构造/词数 reserve）——125k 词
  全程 CLI 约 2.1s。
- 桌面分发：`main.cpp` 兜底（UNIDICT_DICTS 未设）收集 exe 同目录 /
  macOS `Contents/Resources` 全部随包词典；CMake POST_BUILD 落位；
  daily-build Windows zip/安装器、macOS bundle、Linux zip 全部 staging
  词典+署名文件，Windows 安装器验证加「计算机→computer」CEDICT 冒烟。
- Android：`assets.srcDir("../../dictionaries")` + 首启种子列表首位加
  `ccedict-zh-en.json`（APK 自带，与演示样张同机制）。

**验收（2026-10-03 双端真实走查）**：
- 桌面：全新环境（无 UNIDICT_DICTS）起 `unidict_qml`，启动日志
  `已加载随包词典: …/ccedict-zh-en.json:…/dict.json`；顶栏
  `2 本词典 · 125170 词条`；查「你好」出释义
  `[ni3 hao3] hello; hi`（来源标注 CC-CEDICT 汉英词典），状态行
  「找到 1 个词典结果」。截图 `ui_sandbox_out/b004_desktop_*.png`。
- Android：全新安装（清数据首启）→ 引导 → `M2-SMOKE-OK dicts=4`；
  查「你好」出 `聚合结果 1 条` + CC-CEDICT 卡片 `[ni3 hao3] hello; hi`；
  词典页 `已装载 4 部`、CC-CEDICT `125166 词`。截图
  `ui_sandbox_out/b004_android_{result,dicts}.png`。
- CLI 冒烟：`UNIDICT_DICTS=ccedict-zh-en.json unidict_cli_std 你好` →
  `你好: [ni3 hao3] hello; hi`；Windows 安装器 CI 验证含同口径冒烟。
- 许可：词典与署名文件进包（AGENTS「不提交词典资产」按用户 BUG-004
  明令覆盖，CC BY-SA 4.0 署名齐备）。

**留档（非本缺陷）**：克隆模拟器（test30 副本，-no-audio）上
M4-TTS-FAIL code=-1，为该环境 TTS 引擎未初始化，与词典/装载无关；
真机与既有验收面口径不变。

## BUG-001 Windows 双击启动 GUI 弹 terminal 控制台窗口 ✅修复+产物验收通过（2026-10-02）

**现象**：Windows 下双击 `unidict_gui.exe`（每日构建包）启动全程带着一个
黑色控制台窗口，GUI 程序不该有。

**根因**（两处叠加）：
1. `gui/CMakeLists.txt` 用裸 `add_executable(unidict_gui ...)`——缺
   `WIN32` 属性即 console 子系统，Windows 装载器为 console 程序分配
   控制台。`qmlui/CMakeLists.txt` 桌面分支同病。
2. AI 桥 `adapters/qt/ai_service_qt.cpp` 的 `QProcess::start` 拉
   外部命令：GUI 进程拉控制台子进程时每次执行闪一个 terminal 窗口。

**修复**（单提交）：
- 两个桌面目标 `add_executable(... WIN32 ...)`（GUI 子系统；Qt6 对
  `WIN32_EXECUTABLE` 目标自动提供 WinMain 入口，无需手写）。
- AI 桥 Windows 下 `setCreateProcessArgumentsModifier` 加
  `CREATE_NO_WINDOW`，子进程静默拉起，输出仍走管道。

**验收**：CI Windows 构建绿；取每日构建产物验 PE 头
subsystem=2（IMAGE_SUBSYSTEM_WINDOWS_GUI）；双击启动（含触发 AI 桥）
零控制台窗口——后一项待 Windows 真机，先以 PE 头 + 代码口径留档。

**产物验收（2026-10-02 run 36982676557）**：安装器 Verify installer 步骤
通过——静默装 + 文件/开始菜单快捷方式/卸载器齐 + `unidict_qml.exe`
PE subsystem = 2 + CLI 冒烟，全链绿（首次跑出的 0 是验收脚本自身偏移
错误 +68→+92，非产物问题，已修）。真机双击走查仍留待用户顺手确认。

**收口补验（2026-10-02 run 37049418211，nightly 刷新到 3973dfe）**：
- windeployqt 样式插件完整进包：Windows zip 内
  `qml\QtQuick\Controls\Material\`（71 项 QML +
  `qtquickcontrols2materialstyleplugin.dll` +
  `Qt6QuickControls2Material(.StyleImpl).dll`）——BUG-003 排查方向 2
  的「缺 Material 插件静默回落 Default 样式」风险在 Windows 分发面
  排除（macOS 同链 `macdeployqt -qmldir=qmlui`，同口径）。
- 包内主程序对：`unidict_qml.exe`（GUI）+ `unidict_cli.exe` /
  `unidict_cli_std.exe`；安装器 Verify 步骤绿（静默装/开始菜单/
  卸载器/PE subsystem=2/样本+CEDICT 冒烟）。
- 包格式命名按本文件口径：`unidict-windows-x64.zip` /
  `unidict-windows-x64-setup.exe`；nightly Release 13 资产清旧传新，
  BUILD_INFO commit=3973dfe。
- 随包词典（BUG-004 联动）：zip 内 `ccedict-zh-en.json`（11.7MB）+
  `CC-CEDICT-ATTRIBUTION.md`。
- 真机双击零控制台窗口一项仍留用户顺手确认（PE subsystem 已=2）。

## BUG-002 GUI 实现与 Qt Design 原型不一致 ✅修复待产物验收（2026-10-02）

**现象**：用户对照仓库 `docs/ui/` 的原型图后判定「GUI 界面和原型根本
不是一个东西」。

**排查（客观度量）**：`docs/ui/` 四屏原型图（home/result/vocab/settings ×
亮暗）与 QML 应用（`MainDesktop.qml`）离屏截图 **md5 逐字节一致**（四屏
全 MATCH）——QML 实现就是原型本体，零像素差。

**根因：分发面发错了程序**。每日构建 Windows/macOS 包的 GUI 主程序一直是
`unidict_gui`（`gui/` Qt Widgets 演示，另一套完全不同的界面），而与原型
同源的 QML 应用 `unidict_qml` 被构建开关 `-DUNIDICT_BUILD_QT_QMLUI=OFF`
排除在分发外。用户按 VERIFY.md 双击 `unidict_gui` → 看到的与原型自然
「根本是两个东西」。

**修复**（单提交）：
- 分发 GUI 主程序换成 `unidict_qml`：Windows `unidict_qml.exe`（windeployqt
  `--qmldir` 收 Quick/Material/Dialogs 插件）；macOS `unidict_qml.app`
  bundle（新增 MACOSX_BUNDLE，macdeployqt `-qmldir`）；Widgets demo 不再进包
- 双击即有词典：`UNIDICT_DICTS` 未设时 main.cpp 兜底加载 exe 同目录 /
  .app `Contents/Resources` 的随包 dict.json（env 显式设置仍优先）
- VERIFY.md / PLATFORM-NOTES / Release notes / install.sh / install.ps1
  文案同步

**验收**：本地走查过——offscreen 运行日志命中「已加载随包词典」，
env 设置时不触发兜底；对照图 `docs/ui/compare/`（原型 | 实现左右并排，
md5 同图为判据）；CI Windows/macOS 构建绿 + 每日构建 dispatch 后产物
内 `unidict_qml` 实跑。

**产物验收（2026-10-02 run 36982676557）**：Windows 安装器内
`unidict_qml.exe` 安装/启动路径验证通过（Verify installer 绿）；macOS
bundle 同班构建绿。文档站「下载」Tab 直链表与 Release 资产名一致。

**产物验收收口·UI 面（2026-10-03，nightly 3973dfe）**：以同提交离屏
渲染对照 `docs/ui/` 原型——home/result/vocab × 亮暗 **6/8 屏 md5 逐字节
一致**；settings 两屏 1094px 差为开发树 fa00d2d 引入（抽屉高度问题，
见 BUG-003 根因②），**非分发面回归**（包内 Material 样式插件齐备，
见上方收口补验）。Linux 包为 CLI 面（仅 `unidict_cli_std`，无 GUI），
Win/mac 产物 GUI 逐屏走查仍需对应环境/真机。BUG-002 口径维持 ✅。

**设计稿缺失清单（待用户定，不自行发挥）**：
1. 原型设置屏右栏「词典」tab 的具体设计——现有截图停在「取词」tab；
   QML 应用目前无词典导入/管理 UI（只认 UNIDICT_DICTS 环境变量），
   「添加自己的词典」入口没有设计稿可依。
2. 顶栏「历史 生词本 设置」与左栏「结果 历史 生词本」tabs 的交互关系
   （重复入口的跳转行为）原型未给说明，现实现按静态稿复刻两处入口。
3. 原型未覆盖的动态态（查词加载中、错误提示、空词典首启引导）无设计稿。
