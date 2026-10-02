# BUGS

用户报告与自查缺陷登记。格式：现象 / 根因 / 修复 / 验收。修复完成即勾，
带后续验收项的写明口径。

## BUG-001 Windows 双击启动 GUI 弹 terminal 控制台窗口 ⏳修复待产物验收（2026-10-02）

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

## BUG-002 GUI 实现与 Qt Design 原型不一致 🔄处理中（2026-10-02）

**现象**：用户对照仓库 `docs/ui/` 的原型图（Qt Design mockup，desktop_plan
里「GUI 外观精确复刻」的那套设计稿）后判定「GUI 界面和原型根本不是
一个东西」。

**口径**：原型图是唯一标准。逐屏对照重做——布局、控件位置、字体字号、
配色、间距全部对齐设计稿，验收标准是「长一样」，不是「功能差不多」。
交付物：每屏「原型图 vs 实现截图」左右对照图供用户验收；设计稿没覆盖
的部分列清单请用户定，不自行发挥。

**进度**：见后续提交记录（本条登记先行，修复随各屏提交更新）。
