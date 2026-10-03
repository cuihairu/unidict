# BUGS

用户报告与自查缺陷登记。格式：现象 / 根因 / 修复 / 验收。修复完成即勾，
带后续验收项的写明口径。

## BUG-008 Android 启动图标缺失（APK 没有 android:icon）✅修复（2026-10-03 登记并修复）

**现象**：BUG-006 接完 Windows/macOS/Linux 三平台图标后复查分发面，发现
Android 侧还空着——`android/app/src/main/AndroidManifest.xml` 的
`<application>` 没有 `android:icon`，启动器只能取系统默认图标（绿色
机器人或空白底），跟其它平台的品牌图标对不上。

**根因**：Android 端从未做过启动图标资产，也没在 manifest 声明图标资源。
BUG-006 的 `assets/icons/`（ico/icns/png）是桌面格式，`.ico` 能塞进
`mipmap-*` 但不合适（启动器按密度桶取 PNG，API 26+ 还要自适应图标 XML），
所以要单独生成。

**修复**（与桌面资产同源，同一份 `docs/logo.svg`）：
- legacy 图标 `mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher.png` 与
  `ic_launcher_round.png`：48…192px 五档，字形按 alpha 内容框裁切后居中、
  占方图 78%，方版四角不透明、圆版圆形 alpha 遮罩。
- 自适应前景 `mipmap-*/ic_launcher_foreground.png`：108dp 画布
  （48…432px），字形比例 66/108 落在规范安全区内。
- `mipmap-anydpi-v26/ic_launcher{,_round}.xml`：API 26+ 自适应图标，
  背景 `@color/ic_launcher_background`、前景 `@mipmap/ic_launcher_foreground`。
- `values/ic_launcher_colors.xml`：自适应层底色 `#F5EFF3`（品牌色 #b11964
  的 5% 淡底）——字形保持 logo 原色不反白，淡底只为在白底启动器上能看出
  图标边界。
- `AndroidManifest.xml`：`<application>` 补 `android:icon`/`android:roundIcon`。
- `tools/build_icons.py` 增 `write_android_assets()`/`report_android()`，
  与 ico/icns 同一函数入口生成，`--check` 一并自检 Android 尺寸集。

**验收（2026-10-03，本地 release APK 实测）**：
- `aapt2 dump badging`：`label='Unidict' icon='res/BW.xml'`；
  `aapt2 dump xmltree`：`android:icon=@0x7f0d0000`、`android:roundIcon=@0x7f0d0002`
  （资源名经 `aapt2 dump resources` 解析，未按位置猜）。
- 资源表 `mipmap/ic_launcher`、`ic_launcher_round` 六变体
  （anydpi-v26 + 五档密度）、`ic_launcher_foreground` 五变体齐全，
  **R8 + resource shrinking 之后仍在**。
- **15 帧 PNG 与仓库源资产像素级一致，不一致帧 0**（`aapt2` 会重编码 PNG，
  文件字节长度有差但解码像素相同——按字节比会误判）。
- 前景字形 bbox：mdpi `(21,21,87,87)`、xxxhdpi `(84,84,348,348)`，
  与 66/108 安全区恰好重合（字形主色 `#b11964`，未反白/未改色）。
- `python3 tools/build_icons.py` 重跑后 `assets/icons/` 无 diff（生成可重复）；
  ctest 136/136 绿。留档 `ui_sandbox_out/bug008/android_launcher_icon.log`。
- 留待真机：真机/模拟器启动器上的**实际显示观感**与不同厂商主题遮罩下的
  观感（本轮只证「图标资源在 APK 里且像素/结构正确」）。

## BUG-007 macOS .app 的 CFBundleIdentifier/CFBundleName 为空 ✅修复（2026-10-03 登记并修复）

**现象**：nightly 产物 `unidict_qml.app/Contents/Info.plist` 里
`CFBundleIdentifier` 与 `CFBundleName` 是空串（同批 `CFBundleExecutable`/
`CFBundleIconFile`/`CFBundlePackageType` 均正常）——做 BUG-006 产物核验时
顺带发现。

**影响**：图标与可执行文件定位不受影响（BUG-006 口径仍成立）；但 Launch
Services 侧没有应用身份可用于归一（多版本共存、偏好设置归属、「打开方式」
列表、后续签名与公证都要用它）。

**根因**：`qmlui/CMakeLists.txt` 的 APPLE 分支只设了 `MACOSX_BUNDLE`，
没设 `MACOSX_BUNDLE_IDENTIFIER`/`MACOSX_BUNDLE_BUNDLE_NAME`，CMake 生成的
默认 Info.plist 这两键填空串。

**修复**：两个属性按仓库既有口径填 `com.unidict.unidict`（与
`cmake/BuildOptions.cmake` 的 `CPACK_BUNDLE_IDENTIFIER` 一致，不新造域名）
与 `Unidict`。

**验收**：本地 Linux 构建不受影响（APPLE 分支不生效）ctest 全绿；产物面
待下一轮 nightly 对拍 Info.plist 两键（记在验收待办，不先勾）。

## BUG-006 二进制图标没换（SVG 不能直接当 OS 图标）✅修复（2026-10-03 登记并修复）

**现象**：品牌图标只有 SVG（docs/logo.svg），各平台二进制/桌面对象
不带图标——Windows exe 默认通用图标、macOS .app 默认图标、Linux 无
.desktop 集成。

**根因**：从未生成多尺寸光栅资产，也从未接线——无 .ico/.icns/.rc/
.desktop，CMake 的 WIN32/MACOSX_BUNDLE 目标都没挂图标资源。

**修复**（资产 + 三平台接线）：
- 资产：docs/logo.svg（1024 viewBox 单 path）矢量直渲 1024 master +
  Lanczos 下采样 → `assets/icons/`：`unidict.ico`（16/24/32/48/64/128/256
  七帧，16/24 为 BMP-in-ICO 32bpp XOR+AND 掩码以兼容老 shell，32 及以上
  为 PNG-in-ICO 软 alpha——首版 16-128 是 8bpp 调色板 + 1bit 硬掩码，
  任务栏/资源管理器边缘锯齿，度量见下）、`unidict.icns`
  （ic11/12/07/08/09/10 = 32/64/128/256/512/1024 六帧 PNG-in-ICNS）、
  `unidict_256/512.png`（Linux hicolor）。
- 可重复生成：`tools/build_icons.py`（`--check` 只校验不写盘）。SVG 光栅化
  后端按可用性择优 resvg → rsvg-convert → inkscape → ImageMagick；ICO/ICNS
  容器由脚本按 spec 手工拼装并回读自检（尺寸集/PNG 负载/容器声明长度）。
  本机无 resvg/inkscape/rsvg-convert，实跑走 ImageMagick 内建 SVG 渲染器
  （本仓 logo 是单 path、无文字/渐变依赖，换后端不改变像素语义）。
- Windows：`qmlui/unidict.rc` + `gui/unidict.rc`（IDI_ICON1）编进 PE
  资源段（CI MSVC 自动 rc.exe）。
- macOS：`MACOSX_BUNDLE_ICON_FILE` + icns 落 Contents/Resources
  （默认 Info.plist 自动注入 CFBundleIconFile；macdeployqt 保留）。
- Linux：`assets/linux/unidict.desktop` + hicolor 256/512 install 规则
  （顶层 CMakeLists），并补 `install(TARGETS unidict_qml)`（RUNTIME
  bin；INSTALL_RPATH 清空绕 Qt6 link 线与 install 前缀的 RPATH_CHANGE
  校验冲突）。

**验收（2026-10-03）**：
- **Windows exe 资源段（nightly run 37099269267 产物实测，非本地推断）**：
  解析 `unidict-windows-x64.zip` 内 `unidict_qml.exe` 的 PE 资源目录 →
  `RT_ICON` 7 条 + `RT_GROUP_ICON` 1 条，帧尺寸集 **[16,24,32,48,64,128,
  256]**，subsystem=2 (WINDOWS_GUI)。对照 `unidict_cli.exe`
  （subsystem=3）只有 RT_MANIFEST、无图标组——说明图标确实来自本项目
  rc 资源而非 Qt/Qt 工具链。留档 `ui_sandbox_out/bug006/pe_resources.txt`
  （解析脚本 `/tmp/opencode/pe_icon_check.py`，纯 PE 结构解析，无 Windows）。
- **macOS bundle（同一 run 产物）**：`unidict_qml.app/Contents/Resources/
  unidict.icns` 存在，magic=icns、声明长度=实际长度=151332B、六帧
  （ic11/12/07/08/09/10）齐全；`Contents/Info.plist` 含
  `CFBundleIconFile = unidict.icns`（Finder/启动台取图标即此键）。
- **Linux**：`cmake --install --prefix` 干净完成 → bin/unidict_qml +
  share/applications/unidict.desktop + share/icons/hicolor/{256,512}/
  apps/unidict.png 布局正确。
- **帧级客观度量**（`ui_sandbox_out/bug006/icon_verify.log`）：
  同尺寸 ICO 帧与 ICNS 帧**像素缓冲 md5 逐帧一致**（32/64/128/256 四档），
  ICO 256 帧与 `unidict_256.png` 像素一致；三平台资产同源于一次下采样。
  alpha 软硬度量（非透明像素 vs 全不透明像素）：现 **七帧全部软 alpha**
  （如 32px 非透明 713 / 全不透明 157），首版 16-128 帧两者相等（硬 1bit
  掩码）——这是本次资产重做的直接原因。非透明覆盖率随尺寸收敛
  69.6%→38.1%（1024 master 基准 38.09%），无空白帧、无裁切。
- 帧对照图（可直接看图核验，非文字断言）：
  `ui_sandbox_out/bug006/icon_frames_showcase.png` —— 三平台全部帧
  一次排开，16-64px 帧按 5× 最近邻放大以便看单像素结构。
- `python3 tools/build_icons.py --check` 自检通过；ctest 136/136 绿。
- 留待真机：Windows 资源管理器/任务栏、macOS Finder 图标的**实际显示
  观感**（本轮只证「图标资源在产物里且结构正确」，像素观感需 Win/mac 屏
  对拍）。

## BUG-005 顶栏 125170 词条但简单词查不出 ✅修复（2026-10-03 登记并修复）

**现象**：GUI 加载词典显示「2 本词典 · 125170 词条」，查简单词
（good/the 等英文高频词、大写形态 Hello/QT）全部查不出。

**根因（双）**：
1. **JsonParser::lookup 大小写敏感**——四个 parser 里唯一精确匹配的
   （stardict/mdict/epub 都查小写折叠键表）。键表 `m_lowerWords` 早已
   在加载期构建（prefixSearch 在用），查词侧漏了折叠回退：Hello 查不到
   词头 hello，只给 Did-you-mean 提示。
2. **查询链不走释义全文索引**——CC-CEDICT 是汉英词典，词头全是汉字，
   英文只存在于释义文本里；`searchWord/searchAll` 只查词头，
   `fullTextSearch`（倒排）有数据有测试但从未接入查询链 → 计数对
   （getWordCount 求和 125170）、英文查询键在词头索引里不存在。

**修复**（core 双点）：
- `json_parser.cpp` lookup：精确 miss → `m_lowerWords` 折叠键回退，
  与其余 parser 同口径。
- `unidict_core.cpp` searchWord/searchAll：词头全 miss →
  `fullTextSearch` 兜底（限 12 条，倒排懒构建+进程内缓存），命中条目
  `metadata.matchType = "fulltext"` 供 UI/CLI 标注「释义匹配」。

**验收（2026-10-03）**：
- **用户词典抽 10 词真查（`ui_sandbox_out/bug005/lookup10.log`，10/10 命中）**：
  源词典 = 用户加载的随包 CC-CEDICT（`dictionaries/ccedict-zh-en.json`，
  125166 词头）+ demo 4 词 = 顶栏 125170 同集；固定随机种子 20261003 从
  **该词典条目本身**抽样（大写形态与短语取自其释义文本，不是外部词表）：
  词头 4（中英/码分多址/马龙区/一时瑜亮）、大写 3（LING/EMERGENCY/NUMBER）、
  短语 3（yao yao/yao ling/ling the）。判据为客观三条件：无「未找到」+
  有释义行 + 大写/短语回显含查询 token。
- CLI 双词典抽词真查：Hello/QT/UNIDICT（大写）命中、good/the 释义匹配
  命中、你好/hello 词头命中——修复前 good/the/Hello/QT/UNIDICT 五类全 miss。
- GUI offscreen 复现用户原样场景（顶栏 125170）：查 good 出 CC-CEDICT
  释义匹配卡片（截图 `ui_sandbox_out` 临时档）。
- 回归测试 `bug005CountAndLookupConsistency`（core_lookup_tests）：
  计数与可查一致性（词头逐一可查）+ 大小写互通 + 全文兜底 matchType
  + 词头命中不掺兜底。
- 词头命中路径无回归：CLI 2.0s（原水平）；BUG-003 八屏 md5 复测 8/8。
- 已知取舍：全文兜底首查多 ~3s（125k 词倒排在查询线程懒构建，进程内
  缓存；GUI 长驻只付一次，CLI 每进程冷启动）。异步化/索引落盘复用
  留后续优化。

## BUG-003 界面与原型不一致（第二轮报告：上一轮未达标）✅修复+离屏验收通过（2026-10-02 登记，2026-10-03 修复）

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

**修复落盘（2026-10-03，2646422）**：
- 根因①回退：两笔 Revert（944ae37 / 7f5657c）恢复顶/底栏原型口径
  （elevation 1、边距 12、间距 10、原型字号）。
- 根因②修复：`toolsDrawer` 显式定高 535（实测底沿落 y519，与原型
  抽屉底沿逐像素对齐——首版 519 实得 503 差 16px，校准后归零）；
  语音 tab 增高内容包进 `ScrollView` 兜底，不再撑高抽屉。
- **验收（离屏面）**：ui_sandbox 1200×760 × 8 屏 vs `docs/ui/`
  逐像素容差 8 **全部归零、md5 8/8 逐字节一致**；ctest 136/136 绿。
  对照图 `docs/ui/compare/` 八张已重制（2026-10-03 口径）。
- 待办：Windows/macOS 产物与真机双端走查回传截图（同 BUG-002 口径，
  需对应环境）。

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
