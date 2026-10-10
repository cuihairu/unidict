<div align="center">
  <p><a href="README.md">English</a> | <b>简体中文</b></p>
  <img src="docs/logo.svg" width="64" height="64" alt="Unidict logo"/>
  <h1>Unidict</h1>
  <p>
    <a href="https://github.com/cuihairu/unidict/actions/workflows/ci.yml"><img src="https://github.com/cuihairu/unidict/actions/workflows/ci.yml/badge.svg" alt="CI"/></a>
    <a href="https://codecov.io/gh/cuihairu/unidict"><img src="https://codecov.io/gh/cuihairu/unidict/graph/badge.svg" alt="Codecov 覆盖率"/></a>
    <a href="https://github.com/cuihairu/unidict/releases/tag/nightly"><img src="https://img.shields.io/github/v/release/cuihairu/unidict?label=nightly&sort=date&logo=github&color=b11964" alt="nightly Release"/></a>
    <a href="https://cuihairu.github.io/unidict/"><img src="https://img.shields.io/website?url=https%3A%2F%2Fcuihairu.github.io%2Funidict%2F&up_message=%E5%9C%A8%E7%BA%BF&down_message=%E7%A6%BB%E7%BA%BF&label=%E6%96%87%E6%A1%A3%E7%AB%99&color=b11964" alt="文档站"/></a>
    <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux%20%7C%20Android-3d4451" alt="平台支持"/>
  </p>
  <p>个人词典——查得快、看得懂、听得清、说得出、写得对。</p>
  <p>离线本地优先 · 无广告 · 无强制登录 · AI 与云端服务可选</p>
  <p><b>uni = universal</b>：不止英文查词——中文词典与其他语种同样是目标（名字里的「uni」即取此义，中文谐音：有你 · 友你 · 优你）。</p>
  <p>基于 C++17：核心库完全不依赖 Qt（Qt 仅用于桌面与应用层壳）。</p>
</div>

## 这是什么

把你的 StarDict / MDict / DSL / JSON / CSV 词典变成**你自己的词典**：

- **查得快** —— 精确、前缀、模糊、通配符、正则、全文六种检索 + 多词典聚合分组去重
- **看得懂** —— 释义按词典折叠分组、净化渲染、例句/词组/近义联想交叉引用
- **听得清** —— 本地 TTS 离线朗读；可选在线人声（美/英/澳口音，默认关，只发查询词）
- **说得出** —— 可选本地发音评分（逐音素反馈；实验性、默认不装）
- **写得对** —— 生词本 + 标签 + 笔记 + 历史置顶，CSV 导出；备份即是文件

产品原则与禁区见 [docs/product-principles.md](docs/product-principles.md)；
架构边界规则见 [docs/architecture-boundaries.md](docs/architecture-boundaries.md)。

## 平台状态

| 平台 | 状态 |
|---|---|
| Windows / macOS / Linux 桌面 | nightly 分发（GUI + CLI） |
| Android | 原生壳交付到 M5 + M7（词典导入、查词、生词本/历史/笔记、TTS、深色主题、APK 打包、分享接词）；发布签名待密钥 |
| iOS / HarmonyOS | 未开始 |

## 每日构建（nightly）

每天 05:17（北京时间）自动构建并发布到**滚动 nightly Release**（固定 tag `nightly`，每次清旧传新，无需登录匿名下载）：

- **取件入口：<https://github.com/cuihairu/unidict/releases/tag/nightly>** · 备用镜像：[文档站下载页](https://cuihairu.github.io/unidict/download)
- 包内含 `VERIFY.md` 验证指引与各平台 `PLATFORM-NOTES.txt`

一键安装（自动识别 OS/架构、覆盖安装即升级、装完跑冒烟验证）：

```bash
# Linux / macOS（Apple Silicon）
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell
# Windows（PowerShell）
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

## 界面预览

桌面 QML 应用（完整亮/暗八屏见 `docs/ui/` 与[文档站](https://cuihairu.github.io/unidict/)）：

| <img src="docs/ui/result-light.png?v=d2" width="400" alt="查词结果·亮色"/><br><sub>查词结果 · 多词典聚合释义，支持朗读/收藏/复制</sub> | <img src="docs/ui/result-dark.png?v=d2" width="400" alt="查词结果·暗色"/><br><sub>查词结果 · 暗色</sub> |
| --- | --- |

> 展示图即当前实现态（「实现即原型」，D2 查询命令条/header 已落地）；界面重设计进行中——结果面板视觉升级（D3）排队中，截图随批次换新。`?v=` 参数只用于击穿 GitHub README 图片缓存，文件本体路径不变。

## 快速开始

```bash
# 开箱查真词典：仓库自带 CC-CEDICT 汉英词典（12.5 万词头，CC BY-SA 4.0）
UNIDICT_DICTS="dictionaries/ccedict-zh-en.json" build-std/Release/unidict_cli_std 词典
UNIDICT_DICTS="dictionaries/ccedict-zh-en.json" build-std/Release/unidict_cli_std -m prefix -p dictiona

# 指定词典与搜索模式（exact/prefix/fuzzy/wildcard/regex/fulltext；
# 查询词用位置参数，fulltext 也可用 --pattern）
unidict_cli_std -d dict.mdx -m prefix inter
unidict_cli_std -d dict.ifo -m fuzzy helo
unidict_cli_std -d dict.mdx -m fulltext --pattern "annual meeting"

# 词典管理 / 加密 MDict / 索引运维
unidict_cli_std --scan-dir ./dictionaries --list-dicts-verbose
unidict_cli_std -d encrypted.mdx --mdict-password <pw> hello
unidict_cli_std --index-save index.bin --index-load index.bin
```

完整选项见 `unidict_cli_std --help`。CLI 定位为 man 式纯查词与诊断：生词本、历史、笔记等学习管理集中在桌面 GUI，CLI 不提供入口、查词也不写入历史。

### 更多开源词典（一键拉取）

除内置 CC-CEDICT 外，`scripts/fetch_sample_dicts.sh` 可下载 5 个开源词典到本地（资产不入库）：ECDICT 英汉 ~340 万词（MIT）、WikDict 汉/英双向（CC BY-SA 4.0，繁体词头）、FreeDict 英德/英中（GPL-3.0）。合计 6 个词典开箱可用：

```bash
scripts/fetch_sample_dicts.sh                       # 全部拉取（约 100MB）
UNIDICT_DICTS="$(scripts/fetch_sample_dicts.sh --print-env)" \
  build-std/Release/unidict_cli_std lobster         # 内置 + 已拉取词典联合查词
build-std/Release/unidict_cli_std --scan-dir dictionaries/downloaded -m prefix car
```

各词典署名与许可见 `dictionaries/downloaded/ATTRIBUTION.md`（拉取时自动生成）。

环境变量：`UNIDICT_DICTS`（词典列表）、`UNIDICT_DICT_DIR`（词典目录）、`UNIDICT_DATA_DIR`/`UNIDICT_CACHE_DIR`（数据与缓存目录）、`UNIDICT_MDICT_PASSWORD`（MDict 默认密码）。

## 构建与测试

要求：CMake 3.16+、C++17 编译器、zlib；Qt 6（Core/Gui/Widgets，QML 应用另需 Qml/Quick/QuickControls2/TextToSpeech）。

```bash
# Qt 全量构建
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure

# std-only（无 Qt，核心开发推荐）
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

发音评分（实验性，默认不构建）：

```bash
cmake -B build-pron -S . -DUNIDICT_BUILD_PRON=ON \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
# 模型（~635MB）与词表不进 git，运行时指定：
unidict_cli_std --pron-model model.onnx --pron-vocab vocab.json \
    --pron-phones "K AE T" --pron-score cat.wav
```

详见 [docs/pronunciation-plan.md](docs/pronunciation-plan.md)。
测试约定：`core/` 新测试不依赖 Qt（`tests/test_<module>_std.cpp`，`<cassert>`+`main()`）；Qt 桥接层测试用 Qt Test；测试只增不减；提交前两个构建形态 ctest 全绿 + coverage 门禁（`scripts/coverage.sh`）。

## 目录布局

- `core/`：std-only 核心库（解析器、索引引擎、全文检索、数据存储、聚合、交叉引用、渲染）；legacy Qt 核心已退役
- `adapters/qt/`：Qt 桥接（把 std 核心桥给 Qt 应用）；`adapters/android/`：JNI 胶水；`adapters/pron/`：发音评分推理壳
- `cli-std/`：std-only 主力 CLI（deb/rpm；Qt CLI 已退役）
- `gui/`：Qt Widgets 桌面；`qmlui/`：QML 桌面应用
- `tests/`：Qt Test 与 std-only（cassert）双轨测试
- `docs/`：文档（见下方地图）

## 文档地图

- 现状基线审计（2026-10-04）：[CURRENT_ARCHITECTURE](docs/CURRENT_ARCHITECTURE.md) · [CURRENT_FEATURE_MATRIX](docs/CURRENT_FEATURE_MATRIX.md) · [TECH_DEBT](docs/TECH_DEBT.md)
- 产品方向：[product-principles](docs/product-principles.md) · [architecture-boundaries](docs/architecture-boundaries.md) · [roadmap](docs/roadmap.md)
- 使用：[USER_GUIDE](docs/USER_GUIDE.md) · 开发：[dev](docs/dev.md) · 设计：[sync-engine](docs/design/sync-engine.md) · [text-normalization](docs/design/text-normalization.md)
- 在线文档站：<https://cuihairu.github.io/unidict/>

## 隐私与数据

- **本地优先**：查词、生词本、历史、笔记全部留在本机；查词核心链路不发网络请求。
- **唯一默认外发**：在线发音（默认关，启用时仅发送查询词，UI 有明示文案）。
- **无广告、无强制登录、无遥测**；词典文件是用户自己的资产，仓库不提交任何词典数据；AI 与云端服务均可选。

### 个人词库同步的设计说明(设计中)

跨设备同步个人词库(生词本/收藏/笔记/偏好)采用**动态密码配对 + 端到端加密**:

- **不需要账号**:一端生成短时效**动态密码**,另一端输入即加入同步组;组内设备同等权限、自由进出,设备清单互见(只能改自己的备注)。
- **端到端加密**:建组设备本地生成**组密钥**,密钥不上传;新设备通过 PAKE 协议用动态密码换出同一把组密钥(短码不作密钥,防爆破与中间人)。词库指令流全部加密后才上云——**服务端只见密文,无法读取你的词库**。
- **同步语义**:所有变更以指令形式同步(增词/删词/改笔记…),多设备回放即一致,不丢更新。
- **代价与自救**:没有账号找回——**设备全部丢失则词库随之丢失**;请用「导出加密备份」把密文备份保存到本地或网盘。
- **默认关闭**:同步功能默认不开启,显式开启时会明示同步范围(哪些词条上云)。

## 贡献

仓库纪律与流程见 [AGENTS.md](AGENTS.md)（构建/测试/提交约定、Conventional Commits、门禁口径）。

## 许可证

MIT，见 [LICENSE](LICENSE)。
