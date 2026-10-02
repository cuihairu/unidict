# 快速上手

两条路：**一键安装**（用每日构建的成品包，推荐）或 **源码构建**（开发/定制）。装好后用同一条冒烟命令验证——能输出释义就是好的。

## 一键安装

::: code-group

```bash [Linux / macOS]
curl -fsSL https://raw.githubusercontent.com/cuihairu/unidict/main/install.sh | bash
```

```powershell [Windows]
irm https://raw.githubusercontent.com/cuihairu/unidict/main/install.ps1 | iex
```

:::

脚本自动识别操作系统与 CPU 架构，从**滚动 nightly Release** 下载对应平台包安装；重跑一次即升级。Linux 上优先走发行版系统包（deb/rpm），卸载交给包管理器（`sudo apt remove unidict` / `sudo dnf remove unidict`）。

Windows 安装器装到 `Program Files\Unidict`，开始菜单有 Unidict 快捷方式；桌面 QML 界面双击即用，随包的示例词典自动加载。

## 冒烟验证

::: code-group

```bash [Linux / macOS]
UNIDICT_DICTS=dict.json unidict_cli_std hello
# 预期输出:
# hello: A greeting or expression of goodwill.
```

```powershell [Windows]
set UNIDICT_DICTS=dict.json && unidict_cli_std.exe hello
```

:::

安装位置与各平台差异见[下载与安装](/install)。

## 源码构建

要求 CMake 3.16+、C++17 编译器、zlib。核心开发推荐 std-only 构建（无 Qt 依赖，编译快）：

```bash
cmake -B build-std -S . \
  -DUNIDICT_BUILD_QT_CORE=OFF -DUNIDICT_BUILD_ADAPTER_QT=OFF \
  -DUNIDICT_BUILD_QT_APPS=OFF -DUNIDICT_BUILD_QT_TESTS=OFF
cmake --build build-std -j
ctest --test-dir build-std -R _std --output-on-failure
```

Qt 全量构建（桌面 QML 应用、Widgets 演示、Qt 测试）需要 Qt 6.10+：

```bash
cmake -B build -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 下一步

- 用自己的词典：[词典管理与导入格式](/dictionaries)
- 换着花样查：[查词用法](/search)
- 查过的词留下来：[生词本](/vocabulary)
