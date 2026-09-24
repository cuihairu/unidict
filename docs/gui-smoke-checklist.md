# Windows GUI 冒烟验证清单

> 每次拿到新的 Windows artifact（CI 产物，非 Release）后过一遍这张清单。
> 全绿说明"Windows 可运行版"这一硬目标仍然成立；任何一条挂了先记下
> artifact 的 commit（`BUILD-INFO.txt` 里有），再复现定位。

## 拿到 artifact

```bash
# 找最近的成功 run，下载 unidict-gui-windows artifact
gh run list --workflow ci.yml --limit 5
gh run download <run-id> -n unidict-gui-windows -D ./win-smoke
cd win-smoke && unzip ../unidict-gui-windows.zip   # artifact 本身是 zip
```

zip 内容：`unidict_gui.exe` + windeployqt 收集的 Qt DLL（含
`platforms/qwindows.dll`）+ vcpkg 的 zlib1g.dll + `BUILD-INFO.txt`
（构建时间与 commit）。目录结构别打乱，DLL 就近加载。

## 启动与窗口

- [ ] 双击 `unidict_gui.exe` 直接出主窗口（SmartScreen 会拦未签名 exe：
      "更多信息 → 仍要运行"，属于预期，不是打包问题）
- [ ] 窗口标题 Unidict；无 DLL 报错弹窗（若报缺 Qt6Xxx.dll，说明
      windeployqt 没带全，回去查 CI 的 Package 步骤）
- [ ] 拖动/缩放窗口正常；关掉重开后位置尺寸恢复（窗口几何记忆）
- [ ] 工具栏主题按钮循环三态：跟随系统 → 浅色 → 深色，重启后记忆

## 词典与查询（核心路径）

- [ ] 首次启动状态栏显示"0 部词典 · 就绪"；点"词典管理…"添加一个
      JSON 词典（`examples/dict.json` 可用）或 StarDict 目录
- [ ] 输入词条回车 → 释义区出结果，来源词典名置灰显示
- [ ] 输入**不存在的词** → 出"相近词条"列表 + "全文命中（释义中出现
      该词）"区块；点全文命中里的词条能回查
- [ ] 输入词前缀 → 弹出补全列表（Unfiltered 弹窗）；回车/点击选中即查询
- [ ] 释义里的 HTML（MDX 词典）渲染正常、无脚本执行窗口弹出

## 取词与侧栏

- [ ] 工具栏"剪贴板取词"开关打开后，在别的应用里复制一个单词 →
      Unidict 窗口弹出并自动查询；关闭开关后不再触发（开关状态重启记忆）
- [ ] 工具栏"全局热键"开关打开后，在别的应用里按 Ctrl+Alt+U →
      Unidict 窗口弹出且输入框聚焦；关闭开关后热键失效（开关状态重启记忆；
      仅 Windows，Linux/macOS 按钮置灰属预期）
- [ ] 查询过的词出现在"历史"侧栏；双击回填；右键可置顶/删除
- [ ] "☆ 收藏当前词"入"收藏"侧栏；右键移除

## 词典管理对话框

- [ ] 添加文件（.ifo/.mdx/.json 过滤器）与添加目录（递归扫描）
- [ ] 启用/禁用某词典后查询结果即时变化；上移/下移改优先序
- [ ] "设置分组标签…"给词典打标签（逗号分隔，留空清除）；列表行尾
      显示"分组: xx/yy"
- [ ] 移除词典；关闭对话框后状态栏计数刷新

## 分组过滤（profile）

- [ ] 给两部词典分别打不同分组（如 en / zh），主窗口工具栏下拉框出现
      这两个分组；选"en"后查询与补全只出 en 组词典的结果
- [ ] 切回"全部分组"后结果恢复完整；切换后直接回车重查当前输入框
- [ ] 选中某分组重启 Unidict，分组选择被记忆；若该分组的词典全被
      移除，重启后回落"全部分组"

## 已知不验证项（本机 Linux offscreen 冒烟覆盖不到）

- 深浅色"跟随系统"的实际联动（需要真实桌面会话）
- 全局热键的真实触发（需要前台有其他应用接收按键；上面勾选项在
  Windows artifact 上人工过）
- 多显示器下几何恢复的边角（Qt saveGeometry 自带，异常时删
  `%APPDATA%/Unidict` 下的配置即可复位）

## 历史坑位（出现过的问题，复查时留意）

- Windows Git Bash 没有 zip 命令，CI 打包用 PowerShell
  `Compress-Archive`（曾因 `zip: exit 127` 挂过一次）
- Qt 6.10 的 offscreen 平台插件在 Windows 上静默崩溃，所以 Qt 测试在
  Windows runner 上走默认 qwindows 主路径
- vcpkg zlib DLL 必须拷进 dist，缺了会在加载 .dict.dz/.mdx 压缩块时
  报错（解压失败 ≠ 词典损坏）
