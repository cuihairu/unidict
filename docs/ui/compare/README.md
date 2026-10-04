# 实现截图留档（BUG-010 逐屏对照）

BUG-010 验收的截图侧证据（接线侧证据=双端真点清单，见 BUGS.md）。
采集：`-DUNIDICT_BUILD_UI_SANDBOX=ON` 后跑

```bash
UNIDICT_DICTS=examples/dict.json build/Release/unidict_ui_sandbox <出图目录>
UNIDICT_DICTS=qmlui/dev/fixtures/click_audit_dict.json \
    build/Release/unidict_gui_sandbox <出图目录>
```

## qmlui（`home/result/vocab/settings × light/dark` 八张，1200×760）

- **设计基准 = `docs/ui/`**（2026-10-01 稿；2026-10-04 起实现按欧路标准
  重排——结果区配色/层级/注释弱化（759f93e）+ 接线修复（05dab90）——
  基线于 2026-10-05 刷新为现实现输出，即「实现即原型」口径的现行基准）。
- 本目录八张合成图：**左 = 10-01 旧稿，右 = 10-05 现实现**（红竖条
  分隔），留设计演进记录；此后逐屏走查以 `docs/ui/` 新基线为对照面。
- 每日构建 Windows/macOS 包的 GUI 主程序 `unidict_qml` 加载的正是这套
  QML（BUGS.md BUG-002 修复记录）；抽屉定高等历史修复见 BUG-003。

## gui（`gui/` 子目录 12 张：home/result/examples/history/vocab/manage × light/dark）

- **设计基准是 `docs/gui-ui-structure.md`**（Qt Widgets 一屏一焦点），
  与 `docs/ui/` 的 QML 原型稿非同一设计基准——双端同属「按各自设计稿
  实现+全接线」验收，不逐像素互比。
- 客观度量（2026-10-05 采集复验）：12 张 md5 全唯一；亮暗灰度均值
  250 vs 45（主题可区分性达标）。图由 `unidict_gui_sandbox` 离屏采集，
  存储钉临时目录不碰真实用户数据。
- 已知采集伪象：离屏容器缺 IPA 字体时音标行呈豆腐块，桌面真机正常。
