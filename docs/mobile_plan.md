# Unidict 移动端版本选型与批次规划

> 2026-09-29 首批（选型设计 + 可行性验证）。本机可行性实测由本批完成，
> 实测环境：Linux x86_64 主机，NDK r27.0.12077973，SDK build-tools 36 /
> platform android-36，模拟器 AVD test30（android-30 google_apis x86_64），
> Qt 6.10.3（仅 gcc_64 桌面 kit），aqt 3.3.0，OpenJDK 21。
> 前置报告：MOBILE_ADAPTATION_REPORT.md（qmlui 内响应式改造，**不是**手机 App）。

## 0. 背景与问题

用户指出「手机版本也需要」，但 todo.md 无移动端条目。现状盘点：

- `MOBILE_ADAPTATION_REPORT.md` 记录的是 **qmlui 内的移动友好改造**——
  响应式布局（`ResponsiveLayout.qml`）、触控目标、`MobileUtils` 平台检测
  与文档选择器 intent 桥、AndroidManifest/Info.plist 模板。它让 qmlui 在
  小屏上好看，但从未产出可安装的手机 App，也没有选型结论。
- `core/` 自 2026-09 起维持 **std-only 无 Qt**（AGENTS.md 架构约束），
  `unidict_std_core` + `unidict_index_std` 两个静态库覆盖查词全链路
  （词典装载/解析 mdx·mdd·stardict·dsl·csv·json·epub、索引、全文、聚合、
  生词本 DataStore、文本归一、发音纯逻辑层）——这正是可上手机的资产。
- 平台方向已有既定口径（2026-09-28）：Android / iOS / HarmonyOS 三端
  **原生壳** + `core/` 共享库；桌面 Qt（adapters/qt、gui、qmlui）只是
  适配层之一。

本机两个先例项目的技术栈决策记录（选型时参照）：

| 项目 | 移动端决策 | 关键记录 |
|------|-----------|----------|
| **cockpit** | **Flutter**（2026-09-22，用户指定） | 纯 REST 客户端、无共享 C++ 资产，直连既有 `/api/*`；M1-M6 交付完整（112 测试、CI 出 APK、R8/签名收口）。**适用前提：壳即全部，没有需要复用的原生核心。** |
| **chirp** | Flutter → **Android/iOS 双原生迁移**（2026-09-28 启动，2026-09-29 移除 Flutter 应用与 CI 腿） | 决策原文：「Android/iOS/HarmonyOS 三端从 Flutter 统一栈切换为各端原生实现。路径：先把与传输无关的纯协议核心逐端原生化（可独立测试、可字节级对拍），再补各端壳层」；门禁 Android `make test` 84/84、iOS `swift test` 81/81。**适用前提：有共享核心要携带，跨端 UI 栈被证明是负担。** |

unidict 与 chirp 同构（有重共享核心、目标三端），与 cockpit 异构（core
正是产品本体，不可能在壳里重写）。先例指向明确。

## 1. 选型对比

### 方案 A：Qt for Android 复用 qmlui

把现有 QML 界面打包上 Android（qmlui 已有 Android 分支：
`find_package(Qt6 ... AndroidExtras)`、`mobile/android/AndroidManifest.xml`）。

| 维度 | 实测/事实 |
|------|-----------|
| 构建链 | **本机不可行**：Qt 6.10.3 仅装 `gcc_64`；`aqt list-qt linux android` 索引最高 **6.7.3**（桌面已到 6.10.3）。走 A 要么把整个 Qt 降级到 ≤6.7（qmlui 声明依赖 TextToSpeech / QuickDialogs2 / QuickControls2，跨 3 个大版本 API 兼容性风险全落在 UI 回归上），要么依赖商业许可通道的新 Android kit。**注**：aqt 索引只反映公开下载通道，若认真走 A 需先复核 download.qt.io 官方安装器是否有 6.10 Android 开源包。 |
| UI 复用 | 名义上复用 qmlui，但移动 UX（底部导航、SAF 导入流、触控查询页）仍要重做；桌面两套主界面（Main/MainDesktop，MainModern 死树已删）的复杂度会被一起背上。 |
| 平台集成 | SAF/TTS/分享接收/桌面小组件都要写 Java/JNI 侧代码穿过 Qt 的平台桥——Qt 不省平台工作，只多一层翻译。 |
| 体积 | Qt Quick + QuickControls2 + TextToSpeech + QuickDialogs2 全套运行时，APK 起步几十 MB。 |
| 三端路线 | **只解 Android 一端**。iOS 需另配 Qt iOS kit + macOS 构建机；HarmonyOS 无官方开源 Qt 后端（MOBILE_ADAPTATION_REPORT 里「Qt for HarmonyOS：华为官方支持」的说法与公开事实不符）。与既定三端原生壳方向直接冲突。 |

### 方案 B：Kotlin 原生壳 + NDK 编译 std-only core + JNI 薄绑定（推荐）

新 Android 工程（Kotlin + Jetpack Compose），`unidict_std_core` 以静态库
进包，`adapters/android/` 放 C++ JNI 胶水（core 保持零平台头文件）。

| 维度 | 实测/事实（本批验证） |
|------|----------------------|
| 构建链 | **本机已通**：NDK r27 + CMake 工具链文件，arm64-v8a 与 x86_64 双 ABI 全量编译通过，产出 `libunidict_std_core.a` / `libunidict_index_std.a`；std 测试套件可执行文件在 Android 目标下也全部链接成功（zlib 用系统 `-lz`，`USE_ZLIB` 语义不变）。 |
| 运行时 | 模拟器实测：android-30 x86_64 镜像上，冒烟程序驱动查词全链路（JSON 词典装载→精确/前缀/全文检索）+ 生词本（词单/标签/笔记/持久化往返/CSV 导出）+ 路径环境回落，`ANDROID-SMOKE-OK`。std::filesystem / 异常 / STL 在 bionic 下工作正常（minSdk 24）。 |
| 平台集成 | 正路：SAF = `ActivityResultContracts.OpenDocument`，TTS = `android.speech.tts.TextToSpeech`，剪贴板/分享/小组件原生 API 直达，无需翻译层。 |
| 体积 | core 静态库 MB 级（无 UI 运行时），APK 净增量小。 |
| 三端路线 | 一套思路三次复用：Android=Kotlin+JNI、iOS=Swift（core 静态库+C 接口）、HarmonyOS=ArkTS+NAPI。chirp 的对拍方法论（双原生包逐名比对协议向量）直接照搬。 |
| 代价 | UI 全新写（本来也要写——移动查词 UX 与桌面三套主界面无复用价值）；JNI 绑定层要维护（薄胶水，纯数据进出）。 |

### 方案 C：双方案并行

否。双倍构建/测试/维护成本，无对拍收益；两个先例项目都是单栈收敛。

### 结论

**选方案 B**。与 2026-09-28 既定平台方向（三端原生壳 + core 共享库）、
chirp 先例（跨端 UI 栈在携带共享核心的场景被证明是负担）、本机实测
（NDK 链路当天全通，Qt Android 链路当场断）三方一致。

## 2. 移动端功能面（明确边界）

**上手机（按批次）**：

1. **查词**——词典装载/启停/删除，精确/前缀/模糊/通配符检索，多词典
   聚合展示，全文检索（复用 `aggregate_lookup_std` / `fulltext_index_std`）。
2. **词典文件管理（SAF）**——`OpenDocument` 导入 .mdx/.mdd/.stardict/
   .dsl/.csv/.json/.epub → 拷贝进 app 私有目录；词典列表（词量/启用态）；
   删除。桌面 UNIDICT_DICTS 环境变量的移动等价物 = 导入清单持久化。
3. **生词本**——词单/分组标签/笔记/搜索历史/CSV 导出（`DataStoreStd`
   绑定面，文件随 app 数据目录）。
4. **发音 TTS**——系统 `TextToSpeech` 朗读词条（引擎选择/语速），词条页
   发音按钮。

**不上手机（桌面专属，明确排除）**：

- 插件开发与插件管理（`plugin_manager`）
- 批量转换工具
- 屏幕取词 / 全局热键 / 剪贴板监视（桌面语义，无移动等价物）
- AI 服务与翻译引擎（`ai_service`）
- 同步服务（后续批次再评估，先留边界）
- 发音练习 / GOP 评分（依赖 onnxruntime + 模型资产下载链路，体量大，
  首期不做；TTS 朗读已覆盖「听发音」诉求）

## 3. 批次拆解

每批独立门禁、独立提交、独立可验收（对齐 chirp 分端推进节奏）：

| 批次 | 内容 | 验收门禁 |
|------|------|----------|
| **M0（本批）** | 选型文档 + 可行性验证（NDK 双 ABI 编译通过、模拟器运行时冒烟通过、Qt Android 链路断点记录在案） | 文档入 docs/ + todo.md 章节 |
| **M1** | Android 工程骨架：Gradle + Compose 空壳、`adapters/android/` JNI 绑定层（dict/lookup/store 三个域）、core .so 打包、冒烟页（载示例词典查词） | `gradle assembleDebug` 出包；模拟器装机冒烟页可查词 |
| **M2** | 查词链路：SAF 导入/词典管理页 + 查词页（精确/前缀/模糊/全文）+ 词条渲染（首期纯文本，`html_renderer_std` 输出净化后 WebView 留后批评估） | 导入真实 .mdx/.json 词典并查询；词典启停生效 |
| **M3** | 生词本 + 历史 + 笔记：DataStoreStd 绑定面全量上壳，词单页/标签/CSV 导出（SAF CreateDocument） | 生词增删改查 + 持久化往返 |
| **M4** | TTS 发音：词条页发音按钮、引擎/语速设置 | 模拟器+真机各验一次朗读 |
| **M5** | 打磨与出包：深色主题、错误态、首启引导（SAF 权限语义）、release 签名/R8/体积（坑位清单参照 cockpit M3 记录） | release 包装机可跑 |
| **M7（已交付 2026-10-07）** | 快速查词/分享面：Share 接词 + 划词菜单（SEND/PROCESS_TEXT → 查词页预填自动聚合查词）+ 桌面静态快捷方式（长按图标直达查词页、光标就位）；singleTask + onNewIntent 运行中接词不叠实例 | 三入口 logcat 令牌 `M7-INTENT-OK` + dump 验预填/焦点/键盘 + 正常启动回归零令牌 |
| （后续） | iOS 壳（Swift + core 静态库 + C 接口）、HarmonyOS 壳（ArkTS + NAPI）——各自另立章节，复用 M2+ 的绑定面设计与对拍方法 | 另批规划 |

## 4. JNI 绑定面设计约定（M1 起生效）

- **位置**：`adapters/android/`（C++ JNI 胶水）。`core/` 继续零平台头
  文件——JNI 头只在胶水里出现（AGENTS.md 约束不破）。
- **域拆分**：`dict_jni.cpp`（装载/启停/删除/元信息）、`lookup_jni.cpp`
  （精确/前缀/模糊/全文/聚合）、`store_jni.cpp`（生词本/历史/笔记）。
- **数据形态**：UTF-8 `std::string` ↔ `jstring`；结构体平铺成
  Kotlin `data class` 字段，避免 `jobject` 频繁往返；列表一次性转
  `Array`/`List`。
- **生命周期**：C++ 管理对象以 `jlong` 句柄持有，配 `close()` 显式释放；
  壳层单线程调用起步（core 无内部锁假设），后续要多线程再在胶水层加。
- **行为规范**：std 测试套件（112 ctest）即 core 行为规范；JNI 面每个
  域配最小冒烟断言（M0 的 android_smoke.cpp 是模板），不复制测试矩阵。

## 5. 本批可行性验证记录（M0 实测）

1. **NDK 交叉编译**：`cmake -B <dir> -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI={arm64-v8a,x86_64} -DANDROID_PLATFORM=android-24` + std-only 配置（四个 `UNIDICT_BUILD_*` 关）→ 双 ABI 全量编译通过，静态库与全部 std 测试可执行文件链接成功。
2. **模拟器运行时冒烟**：AVD test30（android-30 google_apis x86_64）无头启动；`/tmp/asmoke/android_smoke.cpp`（NDK clang++ 直链静态库 + `-lz`）经 `adb push` 到 `/data/local/tmp` 执行，覆盖查词链路 + 生词本持久化 + 路径回落，输出 `ANDROID-SMOKE-OK`。
3. **Qt for Android 链路**：本机 Qt 6.10.3 仅桌面 kit；`aqt list-qt linux android` 公开索引最高 6.7.3（桌面 6.10.3）——方案 A 在本机的现实成本：降级 Qt 三个大版本或依赖商业通道。未实测降级兼容性（不做 A，不投入）。

实测环境注意（复现提示）：本机有两处 SDK 根（`~/android-sdk` 与
`~/.local/android-sdk`），emulator 对 `ANDROID_SDK_ROOT`/`ANDROID_HOME`
的搜索顺序会让 AVD 的 `image.sysdir.1` 解析到空根——用
`-sysdir <SDK>/system-images/android-30/google_apis/x86_64` 显式指路可绕。
