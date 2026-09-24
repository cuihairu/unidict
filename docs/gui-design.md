# GUI 技术选型定稿：Qt Widgets

> 状态：已定案（2026-09-23）。第一个硬目标：**产出 Windows 上可运行的版本**。
> 本文记录选型结论、理由、复审条件与落地路径，后续不再反复争论技术栈。

## 1. 结论

- GUI 框架定为 **Qt 6（Qt Widgets）**，动态链接（LGPLv3 合规）。
- 第一个交付里程碑不是功能完整度，而是 **Windows 可运行包**（exe + Qt DLL 打包），
  通过 CI Windows job 构建并上传 artifact 交付；在此之前不启动任何新的大功能。
- 检索引擎保持 **std-only 独立静态库**（`unidict_std_core` / `unidict_index_std`），
  Qt 只做壳。引擎不依赖任何 UI 框架，可以被别的项目（CLI、服务端、其他 GUI）直接复用。

## 2. Qt Widgets vs Tauri：对比与定案理由

| 维度 | Qt 6 Widgets（选定） | Tauri 2 |
| --- | --- | --- |
| 渲染/内存 | 原生控件 + 光栅绘制，无 WebView 栈；空窗口内存占用小 | 每窗口挂一个 WebView2 进程；词典常驻工具，内存差会被放大 |
| 大列表性能 | `QListView` + 自定义 model 天然虚拟化，十万级词条滚动流畅 | 需要自行做虚拟滚动（DOM 大列表是前端老难题） |
| 即时补全 | `QCompleter` 直接接到词条前缀索引，即插即用 | 需要自己造补全组件与防抖管线 |
| 稳定性 | 二十余年工业级 API，6.x LTS 长期支持；API 语义稳定 | 框架年轻（1.x→2.x 已有破坏性变更）；WebView2 随系统更新版本漂移，渲染行为不可控 |
| 语言栈 | 全程 C++，与现有 core/adapters 零桥接成本 | Rust 外壳 + C++ 引擎，必须维护 Rust↔C++ FFI 桥，双语言构建链 |
| 离线场景契合 | 无网络依赖，WebView2 运行时也不需要 | 依赖系统 WebView2（Win10 旧版本需另装运行时） |
| 美化上限 | QSS 足够桌面工具级观感；复杂富文本渲染稍弱 | HTML/CSS 表现力强（词典释义排版是唯一亮点项） |

**定案理由总结**：词典工作台的核心交互是「高频输入 → 即时补全 → 大列表快速滚动 →
稳定常驻」，这四项全部压在 Qt Widgets 的强项上；Tauri 唯一明显的优势（HTML 排版
释义的视觉上限）用 `QTextBrowser` 支持的 HTML 子集已经够用，真需要更强时也可以在
Widgets 里内嵌渲染层解决，不构成换栈理由。

## 3. Tauri 复审条件（出现以下任一情况再议，否则不再讨论）

1. 产品形态转向「释义页需要重 Web 装修」（复杂 CSS 动效、Web 生态排版组件成为刚需）。
2. 移动端（iOS/Android）优先级反超桌面，且接受 Tauri Mobile 的成熟度风险。
3. 团队后续引入了专职 Rust 工程师，FFI 桥的维护成本不再是额外负担。

## 4. 许可证方案（LGPLv3，动态链接）

- 使用的模块：Qt Core / Gui / Widgets / Test（+ qmlui 遗留的 Quick/Qml、TTS），
  均为 LGPLv3 授权。
- **动态链接即合规**：Windows 发布用 `windeployqt` 把所需 Qt DLL 与 exe 一起打包，
  用户侧不需要安装 Qt；这是 LGPL 的标准发布形态。
- **静态链接才需要商业授权**——我们不做静态链接，规避此成本。
- 合规义务（动态链接下仍需履行，落地在发布包的 `LICENSE-THIRD-PARTY.txt`）：
  1. 声明使用了 Qt 及其版本；
  2. 附上 LGPLv3 许可证文本；
  3. 说明用户可以通过重新链接/替换 Qt DLL 的方式使用修改版 Qt
     （保留对 Qt 库的替换可能性——动态链接本身已满足主体要求，附说明即可）。

## 5. 架构定案：引擎库化，Qt 只做壳

依赖方向单向、无环，与 `herald` 的库化思路一致：

```
cli / gui / qmlui            ← 可执行壳（允许 Qt）
        ↓
adapters/qt                  ← 薄桥接层（QString↔std::string、Qt 容器适配）
        ↓
core/std  (unidict_std_core + unidict_index_std)   ← 引擎静态库，零 Qt、零 UI
        ↓
zlib（唯一第三方依赖）
```

- 引擎（解析、索引、检索、归一化、全文）全部在 `core/std`，CMake target 独立，
  任何新 UI/项目直接链接 `unidict_std_core` 即可，不经过 Qt。
- `core/` 下的 legacy Qt 接口层（`unidict_core_qt`）是过渡产物，逐步把壳的调用
  迁到 `adapters/qt` → `core/std` 链路，最终目标：壳不直接 include legacy 核心。
- **CMake 开关**：`UNIDICT_BUILD_QT_GUI`（Widgets 壳）与 `UNIDICT_BUILD_QT_QMLUI`
  （Quick 壳）分离，均默认跟随 `UNIDICT_BUILD_QT_APPS`。这样只有 Widgets 模块的
  Qt 环境（或只需验证引擎的 CI）不必安装 Quick/Qml 也能构建 GUI。

## 6. Qt6 依赖获取：两条路，先走通的那条

| 路径 | 现状 | 用途 |
| --- | --- | --- |
| **aqtinstall**（`install-qt-action`） | **已验证走通**：CI 用 `aqt install-qt` 装 6.6.3（含 Widgets/Quick；addon 需补 `qtspeech qtmultimedia`，因 TextToSpeech 的 CMake config `find_dependency(Qt6Multimedia)`） | CI、Windows 打包机首选 |
| vcpkg `qtbase` | 备选：`vcpkg install qtbase --triplet x64-windows` 源码编译耗时长（数十分钟级），且与 aqt 官方二进制混用易产生 zlib/CRT 冲突 | 仅当 aqt 镜像不可达时的兜底 |
| 本地 Linux 开发机 | Qt 6.10.3（`/home/cui/Qt/6.10.3/gcc_64`）含 Widgets、**不含 Quick/Qml/TTS** | 本机构建 gui + std-only 引擎验证 |

结论：**依赖统一走 aqtinstall 官方二进制**（CI 已通），Windows 本地开发者用
[Qt 在线安装器](https://www.qt.io/download-qt-installer) 或 aqt 装同一版本，
vcpkg 只负责 zlib；不引入 vcpkg qtbase 混装。

## 7. Windows 产出路径

- **不做交叉编译**：MSVC 工具链 + Qt 的交叉编译组合维护成本高、可复现性差；
  MinGW 交叉编译出的二进制与官方 Qt 预编译库（MSVC ABI）不兼容。
- **采用 CI Windows job**：`windows-latest` + aqt Qt 6.6.3 + MSVC + Ninja 构建
  `unidict_gui`，`windeployqt` 收集 DLL，产出 zip 通过 `upload-artifact` 交付。
- artifact 是开发产物分发，不走 GitHub Release / tag（发版动作仍在禁用清单）。

## 8. 里程碑

- **M1 最小可运行窗口**（✅ 完成）：词典目录加载 + 搜索框即时查询 + 结果列表 + 释义面板，
  本机（Linux/Qt 6.10）与 CI（三平台 Qt 6.6.3）构建通过。
- **M2 检索体验**（✅ 完成 2026-09-24）：QCompleter 词条即时补全、全文检索结果融合
  （`DictionaryManager::fullTextSearch` 组合 std 倒排索引，精确未命中时回落展示，
  锚点点击回查）、词典多选与优先级（词典管理对话框 启用/禁用 + 上移/下移）。
  遗留优化项：QCompleter 数据源仍为全量词表（上限 20 万），接前缀索引按需查询待做。
- **M3 工程化**（✅ 完成 2026-09-24）：设置持久化（主题/剪贴板取词开关 QSettings 记忆）、
  剪贴板取词入口（复用 ClipboardMonitor 轮询过滤，取词后弹窗回填查询）、
  错误提示与空态（释义面板 placeholder + 词典加载失败 QMessageBox）。
- **M4 Windows 打包**（✅ 完成）：windeployqt 收集 DLL + vcpkg zlib 打 zip，
  CI `upload-artifact` 产出 `unidict-gui-windows-*`（daily-windows 稳定 success）。
