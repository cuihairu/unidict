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

### 演进记录口径（2026-10-08 定，P-11 UI 重设计 D3 批）

- **只记大版本节点，不逐批全量**：合成图一行 = 一个重设计里程碑
  （10-01 旧稿 → 10-05 欧路重排 → D3 结果面板卡化…），命名
  `<节点>-<屏>-<主题>.png`；`docs/ui/` 八屏始终保持「实现即原型」
  现行态，每批重渲换新，不在此处重复留档。
- **每节点只截受影响屏**：D3 动的是结果面板（右栏全局可见，八屏皆
  有它，但语义上是「结果面板」一处演进）——只出 `D3-result-*.png`
  两张，其余屏以 `docs/ui/` 滚动态为准。
- **采集与度量口径不变**：ui_sandbox 离屏软件渲染（确定性，同码
  同字节）；验收用客观度量（md5 判重/灰度均值判主题/像素差判局部
  元素），肉眼读图只做布局理解不做验收结论。

### D3 结果面板卡化（2026-10-08，`D3-result-light/dark.png`）

- 左 = D2 基线（外层硬边框盒 + 平铺分组头），右 = D3 现实现
  （外盒退场，每词典一张 radiusL 圆角卡：卡头=词典名 + 词条数
  描边胶囊 chip + 120ms 旋转折叠箭头 + 悬停 ghost 面；卡内词条行
  = 层级标记蓝链 + 右侧本条朗读/复制轻动作）。
- 客观度量：docs/ui 八屏全量换新（md5 全变——右栏结果面各屏皆见）；
  ui_click_audit S8 分组折叠 / S9 词头链层级标记全过（契约逐字节保留）。

## gui（`gui/` 子目录 12 张：home/result/examples/history/vocab/manage × light/dark）

- **设计基准是 `docs/gui-ui-structure.md`**（Qt Widgets 一屏一焦点），
  与 `docs/ui/` 的 QML 原型稿非同一设计基准——双端同属「按各自设计稿
  实现+全接线」验收，不逐像素互比。
- 客观度量（2026-10-05 采集复验）：12 张 md5 全唯一；亮暗灰度均值
  250 vs 45（主题可区分性达标）。图由 `unidict_gui_sandbox` 离屏采集，
  存储钉临时目录不碰真实用户数据。
- 已知采集伪象：离屏容器缺 IPA 字体时音标行呈豆腐块，桌面真机正常。
