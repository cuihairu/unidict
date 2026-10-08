# Unidict 崩溃采集简档（Crashpad）

> 2026-10-08 首批。选型已钉（用户拍板）：Google **Crashpad** 直集成；Breakpad 不进产物，只作历史对照与宿主机还原工具（dump_syms / minidump_stackwalk）。

## 一、目标与边界

**目标**：桌面端崩溃时由独立 handler 进程产出 minidump，先本地落盘；开发者可用符号表离线还原出带函数名的调用栈，作为用户报障的诊断凭据。

**边界**：

| 面 | 本批口径 |
| --- | --- |
| 接入面 | 仅 `qmlui` 桌面壳（unidict_qml）——正式桌面交付面；gui（演示壳）、cli/cli-std（开发工具）不接，核心转储已够用 |
| Android | **不接**。Crashpad 官方不支持 Android；Android 壳的 native 崩溃走系统 tombstone，Java 异常走系统 ANU/crash 面报错对话框。后续若需要 native 采集再单独评估（breakpad 或 crashpad-android 移植），不在本批 |
| std-only 构建 | 不受影响。`UNIDICT_BUILD_CRASH` 挂在 `UNIDICT_BUILD_QT_QMLUI` 之下，std 构建（QMLUI=OFF）完全不触 crashpad |
| 数据外发 | **默认关**。上传 URL 留空 = 只落盘不外发；未来若开属新外部端点，须显式用户授权 + 隐私声明（见第七节） |

## 二、选型与版本

- **Crashpad**（backtrace-labs 维护 fork @ `7b9686b`）+ **mini_chromium**（@ `9cdc2a7`）+ **linux-syscall-support**（@ `e1e7b0a`），经 [TheAssemblyArmada/crashpad-cmake](https://github.com/TheAssemblyArmada/crashpad-cmake)（@ `80573adc`）的 CMake 封装以 **FetchContent** 接入，四个 commit 全部钉死保证可复现。
- 封装选它的理由：把 mini_chromium/lss 依赖与 `crashpad_handler` 可执行目标都建好，自有 CI 覆盖 Linux/macOS/Windows 三平台；本机已 spike 验证（cmake 4.2 + gcc 15.2 编译通过）。
- **为什么 Crashpad 而不是 Breakpad**（历史对照）：

| | Crashpad | Breakpad |
| --- | --- | --- |
| handler 位置 | 独立进程，主进程崩溃不影响写 dump | 进程内，崩溃瞬间自身也可能损坏 |
| 崩溃数据库 | 内置（状态机 new/pending/completed、settings） | 无，自行管理 |
| 维护状态 | Chromium 活跃维护 | 维护态（2009 年面向 32 位时代） |
| minidump 格式 | 标准 minidump + 扩展 | 标准 minidump |

  两者 dump 格式兼容，所以还原工具链直接用 Breakpad 的 `dump_syms` + `minidump_stackwalk`（成熟且用户点名），它们只在宿主机/CI 上跑，**不进产物、不进主 CMake**。

## 三、进程模型与初始化点

```
unidict_qml (主进程)
   │  main() 早期：crash::init(crashDir, appDir)
   │     └─ CrashpadClient::StartHandler → fork+exec crashpad_handler
   ├──────────────► crashpad_handler (独立子进程)
   │                  常驻监听 socket；主进程 SIGSEGV/SIGILL/abort 时
   │                  由内核信号 → 客户端 stub 通过 ptrace 让 handler
   │                  跨进程读取寄存器/内存 → 写 minidump 进崩溃库
   └─ 正常业务继续跑，与 handler 生命周期解耦（restartable=true）
```

- **初始化点**：`qmlui/main.cpp`，`QGuiApplication` 构造、应用属性设置之后立即调用——尽早初始化覆盖后续所有代码；代价是 QGuiApplication 构造段自身崩溃不覆盖（接受，量级极小）。
- Android 构建编译隔离：`#ifdef UNIDICT_ENABLE_CRASH` 包住接入代码 + CMake 选项在 ANDROID 下强制 OFF。
- 失败语义：handler 缺失或启动失败 → init 返回假，应用**照常运行**（采集是增强不是依赖），qWarning 说明原因。

## 四、dump 落盘目录

- 根目录：`QStandardPaths::AppDataLocation + /crash`。当前组织名是占位值（`YourCompany`），实际路径即：
  - Linux：`~/.local/share/YourCompany/Unidict/crash/`
  - Windows：`%APPDATA%\YourCompany\Unidict\crash\`
  - macOS：`~/Library/Application Support/YourCompany/Unidict/crash/`
  - 组织名改正式名会迁移 QSettings 既有数据（设置/同步绑定），不在本批顺手改，落盘路径跟现状走。
- Crashpad 崩溃库布局（crashpad 自管，勿手工写）：

```
crash/
├── settings.dat          # 库元数据
└── crashpad/
    ├── new/              # 正在写入
    ├── pending/          # 待处理（无上传时最终归宿之一）
    └── completed/        # 已完成
```

- 保留策略：首批不做手动清理，crashpad 库自管；后续若磁盘敏感再加按数量/年龄清理（todo 注记，不做产品 UI）。

## 五、独立 handler 进程打包

- **构建期**：`crashpad_handler` 目标由封装提供；`unidict_qml` POST_BUILD 把它拷到 `$<TARGET_FILE_DIR:unidict_qml>`，与主程序同目录。
- **安装期**：`install(TARGETS crashpad_handler DESTINATION bin)`，走既有 install/CPack 树（deb/rpm/NSIS/DMG 同规则）。
- **运行期**：handler 路径 = `QCoreApplication::applicationDirPath() + /crashpad_handler`（显式传给 StartHandler，不依赖隐式查找）；缺失时 init 报假并写明原因。
- 源码构建用户：crashpad 构建进默认构建（`UNIDICT_BUILD_CRASH` 默认 ON），无须额外开关；不想编译 crashpad 的环境 `-DUNIDICT_BUILD_CRASH=OFF`。

## 六、符号表管理

**编译**：接入符号 = 给 `unidict_qml` 目标追加 `-g`（crash 选项开启时；不动全局构建类型）。crashpad 自身代码无需符号（栈还原关心的是 unidict 帧）。

**分离**（分发口径：主程序去符号，符号文件留档不进分发包）：

- Linux（`scripts/symbols_split.sh`）：
  ```bash
  objcopy --only-keep-debug unidict_qml unidict_qml.debug   # 1. 抽符号
  strip --strip-debug unidict_qml                            # 2. 主程序去符号
  objcopy --add-gnu-debuglink=unidict_qml.debug unidict_qml  # 3. 挂回链
  ```
- Windows：MSVC 的 `.pdb` 天然分离，RelWithDebInfo 构建即得，分发只带 exe、pdb 留档。
- macOS：`dsymutil unidict_qml` 出 `.dSYM` 留档。

**还原栈**（宿主机工具，`scripts/build_stackwalk_tools.sh` 从 google/breakpad 源码编译到 `build-tools/`，gitignore，不进主 CMake）：

```bash
dump_syms unidict_qml > unidict_qml.sym                    # ELF+DWARF → breakpad 符号
# 按 MODULE 行的 uuid 摆放：symbols/<name>/<uuid>/<name>.sym
minidump_stackwalk <dump>.dmp symbols/ > stack.txt          # → 带函数名/文件行号的调用栈
```

- 版本对应：minidump 里记录各模块 debug uuid，stackwalk 按它自动匹配符号文件；**dump 必须配对应版本的符号**。分发流程约定：出包时跑 symbols_split，符号文件连同版本号归档（CI artifact 或本地留档），报障时按用户版本取符号。

## 七、上传留位（默认关）

- `CrashpadClient::StartHandler` 的 url 参数传**空串** → handler 只落盘不外发。当前代码即此状态，没有任何网络出口。
- 代码留位：`crash::init` 预留 uploadUrl 参数（默认空）。未来若开，前置条件（缺一不可）：
  1. 用户在设置页**显式授权**（默认 OFF，对齐在线发音/AI 的「开在线是显式动作」口径）；
  2. 端点与归属拍板（属新增外部服务，在 server_plan 之外，须单独评审）；
  3. 隐私声明写明外发内容——minidump 含进程内存片段，**可能包含用户词典内容与生词本数据**，这是数据外发门槛要过审的点。

## 八、与既有日志/监控打通

- 项目无中央日志框架，既有日志渠道 = Qt 默认 `qInfo`/`qWarning`（stderr）。打通即挂上这条渠道：
- **初始化上报**：init 成功 → `qInfo("崩溃采集已启用: %s", dbPath)`；失败 → qWarning 带原因（handler 缺失/启动失败）。
- **历史崩溃检出**：每次启动扫描崩溃库（new/pending/completed 三目录），检出上次运行遗留的 dump 时 `qWarning` 明示数量与路径——用户报障时可自行附上，开发者按第六节还原。后续若要 UI 面（设置页显示「检测到 N 份崩溃报告」）再议，本批只做日志面。
- **诊断开关**：`UNIDICT_CRASH_TEST=nullderef` 环境变量在 init 之后触发空指针解引用样例（专供验收/诊断，常驻代码，注释写明）。

## 九、验收口径

1. 配置期 crashpad 进构建，`build/Release/`（或安装 bin/）带 `crashpad_handler`。
2. `UNIDICT_CRASH_TEST=nullderef` 启动 unidict_qml → 进程崩溃，崩溃库里出现 minidump（下一进程启动日志报出）。
3. `dump_syms` + `minidump_stackwalk` 还原出**含 unidict 函数名**的调用栈（符号化有效性的硬验证）。
4. 三门全绿；build-std 构建不触 crashpad；coverage 口径不变（adapters/crash 不在 core/ 统计内）。

## 十、风险与缓解

| 风险 | 缓解 |
| --- | --- |
| gcc 15 / cmake 4 对 2022 年代封装的兼容 | 本机 spike 已过（crashpad_handler 可执行产出）；CI 编译器版本更老（gcc 13 / MSVC），风险面更小 |
| googlesource（lss）网络间歇 | 本机已验可达；FetchContent 缓存后不重拉；CI 网络独立 |
| CI 时长增加（crashpad 首编约几分钟） | 一次性成本，可接受；若 windows-latest 出编译问题，兜底=降默认值 OFF 并跟进修封装 |
| crashpad_handler 常驻进程 | 内存占用个位数 MB，与主进程解耦 |
