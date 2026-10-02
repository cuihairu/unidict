# 原型 vs 实现对照

每张图：**左 = `docs/ui/` 原型稿，右 = 当前实现离屏截图**（1200×760，
红/灰竖条分隔）。判据：单屏 md5 两侧逐字节一致（home/result/vocab/
settings × light/dark 八张全 MATCH，2026-10-02 复验）——实现即原型。

实现截图由 `unidict_ui_sandbox`（`-DUNIDICT_BUILD_UI_SANDBOX=ON`）对
QML 应用（MainDesktop.qml）离屏采集；每日构建 Windows/macOS 包的 GUI
主程序 `unidict_qml` 加载的正是这套 QML（BUGS.md BUG-002 修复记录）。
