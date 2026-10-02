# BUGS

用户报告与自查缺陷登记。格式：现象 / 根因 / 修复 / 验收。修复完成即勾，
带后续验收项的写明口径。

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

**设计稿缺失清单（待用户定，不自行发挥）**：
1. 原型设置屏右栏「词典」tab 的具体设计——现有截图停在「取词」tab；
   QML 应用目前无词典导入/管理 UI（只认 UNIDICT_DICTS 环境变量），
   「添加自己的词典」入口没有设计稿可依。
2. 顶栏「历史 生词本 设置」与左栏「结果 历史 生词本」tabs 的交互关系
   （重复入口的跳转行为）原型未给说明，现实现按静态稿复刻两处入口。
3. 原型未覆盖的动态态（查词加载中、错误提示、空词典首启引导）无设计稿。
