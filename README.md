<div align="center">
  <img src="docs/logo.svg" width="180" alt="Unidict logo"/>
  <h1>Unidict</h1>
  <p>基于 C++17 的开源离线词典工作台：核心库不依赖 Qt，Qt 仅用于适配器与应用层。</p>
</div>

## 当前状态

核心链路可用：离线加载多格式词典、多种检索模式、全文检索、生词本与历史、聚合查询。
桌面端提供 QML 应用（含剪贴板取词、全局热键、TTS 发音、AI 外部命令桥接）与 Qt Widgets 演示。

- **StarDict**（.ifo/.idx/.dict/.dict.dz）
- **MDict**（.mdx/.mdd，多种块布局，支持 SimpleXOR 加密，密码可经参数或 `UNIDICT_MDICT_PASSWORD` 提供）
- **DSL / JSON / CSV / TSV / 纯文本**
- **检索**：精确、前缀、模糊、通配符、正则、全文检索（倒排索引 + TF/IDF，UDFT1/2/3 持久化与版本协商）
- **学习**：搜索历史（置顶/过滤/导入导出）、生词本（CRUD + CSV 导出）、基础复习调度

## 目录布局

- `core/`：std-only 核心库（解析器、索引引擎、全文检索、数据存储、聚合查询、交叉引用、HTML 渲染）
- `adapters/qt/`：Qt 适配器（把 std 核心桥接给 Qt 应用）
- `cli/`：Qt 版命令行；`cli-std/`：std-only 命令行
- `gui/`：Qt Widgets 演示；`qmlui/`：QML 桌面应用
- `tests/`：Qt Test 与 std-only（cassert）双轨测试
- `docs/`：用户指南、开发笔记、路线图（`docs/roadmap.md`）

## 构建

要求：CMake 3.16+、C++17 编译器、zlib；Qt 6（Core/Gui/Widgets，QML 应用另需 Qml/Quick/QuickControls2/TextToSpeech）。

Qt 全量构建：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure
```

std-only（无 Qt，推荐用于核心开发）：

```bash
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

## 命令行用法

```bash
# 加载词典查词
UNIDICT_DICTS="examples/dict.json" build-std/Release/unidict_cli_std hello

# 指定词典与搜索模式
unidict_cli_std -d dict.mdx -m prefix -p inter
unidict_cli_std -d dict.ifo -m fuzzy -p helo
unidict_cli_std -d dict.mdx -m fulltext --pattern "annual meeting"

# 词典管理
unidict_cli_std --scan-dir ./dictionaries --list-dicts-verbose

# 加密 MDict
unidict_cli_std -d encrypted.mdx --mdict-password <pw> hello

# 索引与缓存
unidict_cli_std --index-save index.bin --index-load index.bin
unidict_cli_std --cache-size --clear-cache
```

完整选项见 `unidict_cli_std --help`（覆盖词典加载、多模式查词与全文索引/缓存维护等诊断入口）。

CLI 定位为 man 式纯查词与诊断：生词本、历史、笔记等学习管理功能集中在桌面 GUI，CLI 不提供入口、查词也不写入历史。

## 发音评分（实验性，M3）

本地离线发音评分：onnxruntime 跑 wav2vec2-espeak-ctc 声学模型，CTC 强制
对齐 + GOP 逐音素打分（分层与模型选型见
[docs/pronunciation-plan.md](docs/pronunciation-plan.md)）。构建开关：

```bash
cmake -B build-pron -S . -DUNIDICT_BUILD_PRON=ON \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
```

开启后首次配置会下载 onnxruntime 预编译包（可 `UNIDICT_PRON_ORT_URL`
换镜像）；模型（~635MB）与词表**不进 git**，运行时指定：

```bash
unidict_cli_std --pron-model model.onnx --pron-vocab vocab.json \
    --pron-phones "K AE T" --pron-score cat.wav
```

输入 wav 须为 16kHz/单声道/16bit；目标音素用 ARPAbet（39 音素域）。
输出逐音素 GOP（含帧区间换算的毫秒位置与 espeak 对照）与词分。

环境变量：`UNIDICT_DICTS`（词典列表）、`UNIDICT_DICT_DIR`（词典目录）、`UNIDICT_DATA_DIR`/`UNIDICT_CACHE_DIR`（数据与缓存目录）、`UNIDICT_MDICT_PASSWORD`（MDict 默认密码）。

## 桌面应用

- `build/qmlui/unidict_qml`：QML 应用。导入 `.ifo`/`.mdx` 文件或扫描目录；搜索、生词本、复习、设置；剪贴板取词与全局热键；TTS 发音。
- `build/gui/unidict_gui`：Qt Widgets 演示。

词典顺序与启用状态自动持久化，下次启动恢复。

## 测试约定

- `core/` 的新测试不依赖 Qt：`tests/test_<module>_std.cpp`，`<cassert>` + `main()`
- Qt 桥接层测试使用 Qt Test：`tests/<unit>_test.cpp`
- 测试只增不减；提交前确保两个构建形态的 ctest 全绿

## 路线图

功能差距与优先级见 [docs/roadmap.md](docs/roadmap.md) 与 [docs/pro_dictionary_gap.md](docs/pro_dictionary_gap.md)。

## 许可证

MIT，见 [LICENSE](LICENSE)。
